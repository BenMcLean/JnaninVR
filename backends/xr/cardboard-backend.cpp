/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define FORBIDDEN_SYMBOL_EXCEPTION_time_h
#define FORBIDDEN_SYMBOL_EXCEPTION_printf

#include "common/scummsys.h"

#if defined(USE_CARDBOARD) && defined(ANDROID_BACKEND)

#include "common/debug.h"
#include "common/system.h"
#include "common/textconsole.h"

#include <math.h>
#include <string.h>
#include <time.h>

#include <cardboard.h>

#include "backends/platform/android/jni-android.h"
#include "backends/xr/xr-android.h"
#include "backends/xr/xr-backend.h"
#include "backends/xr/xr-host.h"

namespace XR {

namespace {

// How far ahead the head pose is predicted, to cover the time until the frame is on screen
const int64 kPredictionNs = 50000000;
// The eye images are rendered at this fraction of the half display size
const float kEyeScale = 0.75f;
const uint32 kParamsRetryMs = 1000;

// OpenGL ES 2.0 shaders, the Android port creates an ES 2 context
const char *const kVertexShader =
	"uniform mat4 uView;\n"
	"uniform mat4 uProj;\n"
	"uniform vec3 uCenter;\n"
	"uniform vec3 uRight;\n"
	"uniform vec3 uUp;\n"
	"attribute vec2 aCorner;\n"
	"varying vec2 vUV;\n"
	"void main() {\n"
	"	vUV = aCorner * 0.5 + 0.5;\n"
	"	vec3 world = uCenter + aCorner.x * uRight + aCorner.y * uUp;\n"
	"	gl_Position = uProj * (uView * vec4(world, 1.0));\n"
	"}\n";

const char *const kFragmentShader =
	"precision mediump float;\n"
	"uniform sampler2D uTex;\n"
	"varying vec2 vUV;\n"
	"void main() {\n"
	"	gl_FragColor = vec4(texture2D(uTex, vUV).rgb, 1.0);\n"
	"}\n";

// The corners of the screen, as a triangle strip
const float kCorners[8] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };

int64 bootTimeNs() {
	struct timespec ts;
	clock_gettime(CLOCK_BOOTTIME, &ts);
	return (int64)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

/** The OpenGL projection matrix (column major) of a frustum given as tangents at distance 1. */
void projection(const EyeFov &fov, float nearZ, float farZ, float *m) {
	memset(m, 0, 16 * sizeof(float));
	m[0] = 2.0f / (fov.tanRight - fov.tanLeft);
	m[5] = 2.0f / (fov.tanUp - fov.tanDown);
	m[8] = (fov.tanRight + fov.tanLeft) / (fov.tanRight - fov.tanLeft);
	m[9] = (fov.tanUp + fov.tanDown) / (fov.tanUp - fov.tanDown);
	m[10] = -(farZ + nearZ) / (farZ - nearZ);
	m[11] = -1.0f;
	m[14] = -2.0f * farZ * nearZ / (farZ - nearZ);
}

/** The matrix (column major) taking the tracking space into an eye: the inverse head rotation, then the eye offset. */
void viewMatrix(const Quat &head, const float *eyeFromHead, float *m) {
	const Quat inverse = conjugate(head);
	memset(m, 0, 16 * sizeof(float));
	for (int col = 0; col < 3; col++) {
		float axis[3] = { 0.0f, 0.0f, 0.0f };
		axis[col] = 1.0f;
		rotateVector(inverse, axis);
		m[col * 4 + 0] = axis[0];
		m[col * 4 + 1] = axis[1];
		m[col * 4 + 2] = axis[2];
	}
	m[12] = eyeFromHead[12];
	m[13] = eyeFromHead[13];
	m[14] = eyeFromHead[14];
	m[15] = 1.0f;
}

} // End of anonymous namespace

/**
 * Cardboard viewers: the phone is the display and the head tracker, the
 * Cardboard SDK warps the two eye images for the viewer lenses.
 * Gamepad and touch events stand in for controllers, the gaze is the pointer.
 */
class CardboardBackend : public Backend {
public:
	CardboardBackend();

	bool init(GLFuncs &gl) override;
	void shutdown() override;

	bool pollEvents() override;
	bool isRunning() const override { return _ready; }
	uint32 framePeriodMs() const override { return 16; }

	bool beginFrame(Frame &frame) override;
	bool endFrame(const Frame &frame, SubmitMode mode, const Theatre &theatre) override;
	void present(const Theatre &theatre) override;
	bool wantsExternalInput() const override { return true; }

	bool prepareEyes() override { return _ready; }
	void getEyeSize(int &width, int &height) const override { width = _eyeW; height = _eyeH; }
	bool acquireEye(int eye, GLuint &texture) override;
	bool releaseEye(int) override { return true; }

private:
	GLFuncs *_gl;

	CardboardHeadTracker *_tracker;
	CardboardLensDistortion *_lens;
	CardboardDistortionRenderer *_renderer;
	int _paramsChanges;
	uint32 _lastTry;

	bool _ready;
	int _dispW, _dispH;
	int _eyeW, _eyeH;
	GLuint _tex[2];
	EyeTarget _target;
	GLuint _program;
	GLint _uView, _uProj, _uCenter, _uRight, _uUp, _uTex;

	float _eyeFromHead[2][16];
	EyeFov _fov[2];

	SubmitMode _mode;
	Quat _headOrientation;

	bool createParams();
	bool createResources();
	void destroyResources();
	bool createProgram();
	void drawTheatre(int eye, const Theatre &theatre);
};

CardboardBackend::CardboardBackend() : _gl(nullptr), _tracker(nullptr), _lens(nullptr), _renderer(nullptr),
		_paramsChanges(0), _lastTry(0), _ready(false), _dispW(0), _dispH(0), _eyeW(0), _eyeH(0),
		_program(0), _uView(-1), _uProj(-1), _uCenter(-1), _uRight(-1), _uUp(-1), _uTex(-1),
		_mode(kSubmitNothing) {
	_tex[0] = _tex[1] = 0;
	memset(_eyeFromHead, 0, sizeof(_eyeFromHead));
	memset(&_fov, 0, sizeof(_fov));
	_headOrientation.x = _headOrientation.y = _headOrientation.z = 0.0f;
	_headOrientation.w = 1.0f;
}

bool CardboardBackend::init(GLFuncs &gl) {
	_gl = &gl;

	jobject context = Host::getActivity();
	if (!context) {
		warning("Cardboard: could not get the Android activity");
		return false;
	}
	Cardboard_initializeAndroid(JNI::getJavaVM(), context);

	_tracker = CardboardHeadTracker_create();
	if (!_tracker)
		return false;
	CardboardHeadTracker_resume(_tracker);

	// The lens distortion is made in pollEvents(), once the display size is known
	return _target.create(gl);
}

bool CardboardBackend::createParams() {
	// The viewer scanned by the user, or else the original Cardboard
	uint8_t *encoded = nullptr;
	int size = 0;
	CardboardQrCode_getSavedDeviceParams(&encoded, &size);
	if (size == 0)
		CardboardQrCode_getCardboardV1DeviceParams(&encoded, &size);
	_paramsChanges = CardboardQrCode_getDeviceParamsChangedCount();

	Host::getDrawableSize(_dispW, _dispH);
	if (_dispW <= 0 || _dispH <= 0) {
		CardboardQrCode_destroy(encoded);
		return false;
	}

	_lens = CardboardLensDistortion_create(encoded, size, _dispW, _dispH);
	CardboardQrCode_destroy(encoded);
	return _lens != nullptr;
}

bool CardboardBackend::createProgram() {
	GLFuncs &gl = *_gl;
	const GLuint shaders[2] = { gl.createShader(GL_VERTEX_SHADER), gl.createShader(GL_FRAGMENT_SHADER) };
	const char *sources[2] = { kVertexShader, kFragmentShader };

	_program = gl.createProgram();
	bool ok = _program != 0;
	for (int i = 0; i < 2; i++) {
		gl.shaderSource(shaders[i], 1, &sources[i], nullptr);
		gl.compileShader(shaders[i]);
		GLint compiled = 0;
		gl.getShaderiv(shaders[i], GL_COMPILE_STATUS, &compiled);
		if (!compiled) {
			warning("Cardboard: shader %d did not compile", i);
			ok = false;
		}
		if (ok)
			gl.attachShader(_program, shaders[i]);
	}
	if (ok) {
		gl.bindAttribLocation(_program, 0, "aCorner");
		gl.linkProgram(_program);
		GLint linked = 0;
		gl.getProgramiv(_program, GL_LINK_STATUS, &linked);
		ok = linked != 0;
	}
	gl.deleteShader(shaders[0]);
	gl.deleteShader(shaders[1]);
	if (!ok) {
		warning("Cardboard: could not build the screen shader");
		return false;
	}

	_uView = gl.getUniformLocation(_program, "uView");
	_uProj = gl.getUniformLocation(_program, "uProj");
	_uCenter = gl.getUniformLocation(_program, "uCenter");
	_uRight = gl.getUniformLocation(_program, "uRight");
	_uUp = gl.getUniformLocation(_program, "uUp");
	_uTex = gl.getUniformLocation(_program, "uTex");
	return true;
}

bool CardboardBackend::createResources() {
	GLFuncs &gl = *_gl;

	CardboardOpenGlEsDistortionRendererConfig config;
	config.texture_type = kGlTexture2D;
	_renderer = CardboardOpenGlEs2DistortionRenderer_create(&config);
	if (!_renderer)
		return false;

	for (int eye = 0; eye < 2; eye++) {
		CardboardMesh mesh;
		CardboardLensDistortion_getDistortionMesh(_lens, (CardboardEye)eye, &mesh);
		CardboardDistortionRenderer_setMesh(_renderer, &mesh, (CardboardEye)eye);

		CardboardLensDistortion_getEyeFromHeadMatrix(_lens, (CardboardEye)eye, _eyeFromHead[eye]);

		// Half angles in radians from the view direction: left, right, bottom, top
		float fov[4];
		CardboardLensDistortion_getFieldOfView(_lens, (CardboardEye)eye, fov);
		_fov[eye].tanLeft = -tanf(fov[0]);
		_fov[eye].tanRight = tanf(fov[1]);
		_fov[eye].tanDown = -tanf(fov[2]);
		_fov[eye].tanUp = tanf(fov[3]);
	}

	_eyeW = (int)(_dispW / 2 * kEyeScale);
	_eyeH = (int)(_dispH * kEyeScale);

	{
		GLStateSaver saver(gl);
		gl.genTextures(2, _tex);
		for (int eye = 0; eye < 2; eye++) {
			gl.bindTexture(GL_TEXTURE_2D, _tex[eye]);
			gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _eyeW, _eyeH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
	}
	_target.resize(gl, _eyeW, _eyeH);

	if (!_program && !createProgram())
		return false;

	debug(1, "Cardboard: ready, display %dx%d, eyes %dx%d", _dispW, _dispH, _eyeW, _eyeH);
	return true;
}

void CardboardBackend::destroyResources() {
	if (_gl && _tex[0])
		_gl->deleteTextures(2, _tex);
	_tex[0] = _tex[1] = 0;
	if (_renderer)
		CardboardDistortionRenderer_destroy(_renderer);
	_renderer = nullptr;
	if (_lens)
		CardboardLensDistortion_destroy(_lens);
	_lens = nullptr;
	_ready = false;
}

bool CardboardBackend::pollEvents() {
	if (_ready) {
		// The display or the viewer changed, so the lens distortion must be made again
		int w, h;
		Host::getDrawableSize(w, h);
		if (w != _dispW || h != _dispH || CardboardQrCode_getDeviceParamsChangedCount() != _paramsChanges) {
			destroyResources();
			_lastTry = 0;
		}
		return true;
	}

	const uint32 now = g_system->getMillis(true);
	if (_lastTry && now - _lastTry < kParamsRetryMs)
		return true;
	_lastTry = now ? now : 1;

	if (createParams() && createResources())
		_ready = true;
	else if (_lens)
		destroyResources();
	return true;
}

bool CardboardBackend::beginFrame(Frame &frame) {
	float position[3];
	float orientation[4];
	CardboardHeadTracker_getPose(_tracker, bootTimeNs() + kPredictionNs, kLandscapeLeft, position, orientation);

	// The tracker returns the rotation used as the view, the head orientation is its inverse.
	// Only the orientation is used, so the head stays at the origin.
	const Quat view = { orientation[0], orientation[1], orientation[2], orientation[3] };
	const Quat head = conjugate(view);
	frame.shouldRender = true;
	frame.headValid = true;
	frame.head.orientation = head;

	frame.viewsValid = true;
	for (int eye = 0; eye < 2; eye++) {
		frame.eyeOrientation[eye] = head;
		frame.eyeFov[eye] = _fov[eye];
	}

	// The gaze is the pointer
	InputState &input = frame.input;
	input.valid = true;
	input.aimValid[kHandRight] = true;
	forwardVector(head, input.aimDirection[kHandRight][0], input.aimDirection[kHandRight][1], input.aimDirection[kHandRight][2]);

	if (_external) {
		input.select[kHandRight] = _external->select;
		input.menu[kHandRight] = _external->menu;
		input.stick = _external->stick;
		input.inventoryPressed = _external->inventoryPressed;
		input.stickClicked = _external->stickClicked;
		_external->inventoryPressed = false;
		_external->stickClicked = false;
	}
	return true;
}

bool CardboardBackend::endFrame(const Frame &frame, SubmitMode mode, const Theatre &) {
	// Drawing is done in present(), just before the buffer swap
	_mode = mode;
	if (frame.headValid)
		_headOrientation = frame.head.orientation;
	return true;
}

bool CardboardBackend::acquireEye(int eye, GLuint &texture) {
	texture = _tex[eye];
	return texture != 0;
}

void CardboardBackend::drawTheatre(int eye, const Theatre &theatre) {
	GLFuncs &gl = *_gl;

	GLint program, activeTexture, arrayBuffer, attribEnabled;
	gl.getIntegerv(GL_CURRENT_PROGRAM, &program);
	gl.getIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
	gl.getIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
	gl.getVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attribEnabled);
	GLStateSaver saver(gl);

	_target.bind(gl, _tex[eye]);

	float view[16], proj[16];
	viewMatrix(_headOrientation, _eyeFromHead[eye], view);
	projection(_fov[eye], 0.1f, 50.0f, proj);

	const Pose &pose = theatre.pose();
	float right[3] = { theatre.screenWidth() * 0.5f, 0.0f, 0.0f };
	float up[3] = { 0.0f, theatre.screenHeight() * 0.5f, 0.0f };
	rotateVector(pose.orientation, right);
	rotateVector(pose.orientation, up);

	gl.useProgram(_program);
	gl.uniformMatrix4fv(_uView, 1, GL_FALSE, view);
	gl.uniformMatrix4fv(_uProj, 1, GL_FALSE, proj);
	gl.uniform3fv(_uCenter, 1, pose.position);
	gl.uniform3fv(_uRight, 1, right);
	gl.uniform3fv(_uUp, 1, up);
	gl.activeTexture(GL_TEXTURE0);
	gl.bindTexture(GL_TEXTURE_2D, theatre.texture());
	gl.uniform1i(_uTex, 0);
	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	gl.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, kCorners);
	gl.enableVertexAttribArray(0);
	gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);

	_target.unbind(gl);
	if (!attribEnabled)
		gl.disableVertexAttribArray(0);
	gl.bindBuffer(GL_ARRAY_BUFFER, (GLuint)arrayBuffer);
	gl.activeTexture((GLenum)activeTexture);
	gl.useProgram((GLuint)program);
}

void CardboardBackend::present(const Theatre &theatre) {
	if (!_ready || _mode == kSubmitNothing)
		return;
	GLFuncs &gl = *_gl;

	const GLenum toggled[] = { GL_DEPTH_TEST, GL_CULL_FACE, GL_BLEND, GL_SCISSOR_TEST };
	GLboolean wasEnabled[ARRAYSIZE(toggled)];
	for (uint i = 0; i < ARRAYSIZE(toggled); i++) {
		wasEnabled[i] = gl.isEnabled(toggled[i]);
		gl.disable(toggled[i]);
	}

	if (_mode == kSubmitTheatre) {
		for (int eye = 0; eye < 2; eye++)
			drawTheatre(eye, theatre);
	}

	CardboardEyeTextureDescription eyes[2];
	for (int eye = 0; eye < 2; eye++) {
		eyes[eye].texture = _tex[eye];
		eyes[eye].left_u = 0.0f;
		eyes[eye].right_u = 1.0f;
		eyes[eye].top_v = 1.0f;
		eyes[eye].bottom_v = 0.0f;
	}

	gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
	gl.viewport(0, 0, _dispW, _dispH);
	gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	gl.clear(GL_COLOR_BUFFER_BIT);
	CardboardDistortionRenderer_renderEyeToDisplay(_renderer, 0, 0, 0, _dispW, _dispH, &eyes[0], &eyes[1]);

	for (uint i = 0; i < ARRAYSIZE(toggled); i++) {
		if (wasEnabled[i])
			gl.enable(toggled[i]);
	}
}

void CardboardBackend::shutdown() {
	destroyResources();
	if (_gl) {
		_target.destroy(*_gl);
		if (_program)
			_gl->deleteProgram(_program);
	}
	_program = 0;
	if (_tracker) {
		CardboardHeadTracker_pause(_tracker);
		CardboardHeadTracker_destroy(_tracker);
	}
	_tracker = nullptr;
}

Backend *createCardboardBackend() {
	return new CardboardBackend();
}

} // End of namespace XR

#endif // USE_CARDBOARD && ANDROID_BACKEND
