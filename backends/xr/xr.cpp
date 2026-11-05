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

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "common/scummsys.h"

#ifdef USE_JNANINVR

#include "common/debug.h"
#include "common/events.h"
#include "common/str.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "backends/xr/xr.h"
#include "backends/xr/xr-backend.h"
#include "backends/xr/xr-host.h"

namespace XR {

namespace {

enum State {
	kUninitialized,
	kRunning,
	kFailed
};

// A stereo frame submitted within this time means the engine is driving the headset
const uint32 kStereoTimeoutMs = 150;

struct Context {
	State state;
	Backend *backend;
	GLFuncs gl;

	Theatre theatre;
	Controls controls;
	ExternalInput externalInput;
	EyeTarget eyeTarget;

	Frame frame;
	bool frameOpen;
	EyeView eyeViews[2];
	float headYaw, headPitch;

	bool eyesReady;
	int eyeW, eyeH;
	bool stereoRendered;
	uint32 stereoTicks;
	bool stereoEverSubmitted;
	uint32 lastFrameTicks;

	Context() : state(kUninitialized), backend(nullptr), frameOpen(false), headYaw(0.0f), headPitch(0.0f),
			eyesReady(false), eyeW(0), eyeH(0), stereoRendered(false), stereoTicks(0),
			stereoEverSubmitted(false), lastFrameTicks(0) {
		memset(&gl, 0, sizeof(gl));
		memset(eyeViews, 0, sizeof(eyeViews));
	}
};

Context *g_ctx = nullptr;

void fail(Context &c) {
	c.state = kFailed;
}

/** Make and start the requested backend; without a request, the first that starts. */
bool createBackend(Context &c) {
	const Common::String requested = Host::getRequestedBackend();

	struct Candidate {
		const char *name;
		Backend *(*create)();
	};
	static const Candidate candidates[] = {
#ifdef USE_OPENXR
		{ "openxr", createOpenXRBackend },
#endif
#ifdef USE_CARDBOARD
		{ "cardboard", createCardboardBackend },
#endif
		{ nullptr, nullptr }
	};

	for (const Candidate *cand = candidates; cand->name; cand++) {
		if (!requested.empty() && requested != cand->name)
			continue;

		Backend *backend = cand->create();
		c.gl.es2 = backend->isES2Only();
		if (backend->init(c.gl)) {
			debug(1, "XR: using the %s backend", cand->name);
			backend->setExternalInput(&c.externalInput);
			c.backend = backend;
			return true;
		}
		backend->shutdown();
		delete backend;
	}
	return false;
}

bool init(Context &c) {
	if (!Host::init() || !loadGLFuncs(c.gl)) {
		warning("XR: could not get the window or required GL functions");
		return false;
	}

	if (!createBackend(c))
		return false;

	return c.theatre.init(c.gl) && c.eyeTarget.create(c.gl);
}

Context *getContext() {
	if (g_ctx)
		return g_ctx->state == kRunning ? g_ctx : nullptr;
	if (!Host::isRequested())
		return nullptr;

	g_ctx = new Context();
	if (init(*g_ctx)) {
		g_ctx->state = kRunning;
		debug("XR: initialised");
	} else {
		fail(*g_ctx);
	}
	return g_ctx->state == kRunning ? g_ctx : nullptr;
}

bool stereoRecent(const Context &c) {
	return c.stereoEverSubmitted && g_system->getMillis(true) - c.stereoTicks < kStereoTimeoutMs;
}

void handleEvents(Context &c) {
	if (!c.backend->pollEvents()) {
		fail(c);
		return;
	}
	if (!c.backend->isRunning())
		c.frameOpen = false;
}

bool openFrame(Context &c) {
	c.frame = Frame();
	if (!c.backend->beginFrame(c.frame))
		return false;
	c.frameOpen = true;

	if (c.frame.headValid) {
		float roll;
		orientationToEuler(c.frame.head.orientation, c.headYaw, c.headPitch, roll);
	}
	if (c.frame.viewsValid) {
		for (int e = 0; e < 2; e++) {
			EyeView &v = c.eyeViews[e];
			orientationToEuler(c.frame.eyeOrientation[e], v.yaw, v.pitch, v.roll);
			v.tanLeft = c.frame.eyeFov[e].tanLeft;
			v.tanRight = c.frame.eyeFov[e].tanRight;
			v.tanDown = c.frame.eyeFov[e].tanDown;
			v.tanUp = c.frame.eyeFov[e].tanUp;
		}
	}
	if (c.frame.input.valid)
		c.controls.update(c.frame.input, c.theatre, stereoRecent(c));
	return true;
}

bool closeFrame(Context &c) {
	SubmitMode mode = kSubmitNothing;
	if (c.frame.shouldRender && c.stereoRendered) {
		mode = kSubmitStereo;
		c.theatre.unanchor();
	} else if (c.frame.shouldRender && c.theatre.hasImage()) {
		mode = kSubmitTheatre;
		if (!c.theatre.isAnchored())
			c.theatre.anchor(c.frame.headValid ? &c.frame.head : nullptr);
	}

	c.frameOpen = false;
	c.stereoRendered = false;
	return c.backend->endFrame(c.frame, mode, c.theatre);
}

bool ensureEyeResources(Context &c) {
	if (c.eyesReady)
		return true;
	if (!c.backend->prepareEyes())
		return false;

	c.backend->getEyeSize(c.eyeW, c.eyeH);
	c.eyeTarget.resize(c.gl, c.eyeW, c.eyeH);
	c.eyesReady = true;
	debug(1, "XR: stereo resources created (%dx%d per eye)", c.eyeW, c.eyeH);
	return true;
}

} // End of anonymous namespace

void captureBackbuffer() {
	Context *c = getContext();
	if (!c)
		return;

	// Nobody sees the window while the engine's own views are shown
	if (!stereoRecent(*c))
		c->theatre.capture(c->gl);
	c->backend->present(c->theatre);
}

void setPaused(bool paused) {
	if (g_ctx && g_ctx->state == kRunning)
		g_ctx->backend->setPaused(paused);
}

bool consumeEvent(const Common::Event &event) {
	if (!g_ctx || g_ctx->state != kRunning || !g_ctx->backend->isRunning() || !g_ctx->backend->wantsExternalInput())
		return false;
	return handleExternalEvent(event, g_ctx->externalInput);
}

void pump() {
	Context *c = getContext();
	if (!c)
		return;

	handleEvents(*c);
	if (c->state != kRunning || !c->backend->isRunning())
		return;

	if (!c->frameOpen) {
		// The engine is submitting its own frames
		if (stereoRecent(*c))
			return;

		// Beginning a frame paces us to the display; don't do it more often than needed
		const uint32 now = g_system->getMillis(true);
		if (now - c->lastFrameTicks + 2 < c->backend->framePeriodMs())
			return;

		if (!openFrame(*c)) {
			fail(*c);
			return;
		}
	}

	if (!closeFrame(*c))
		fail(*c);
	c->lastFrameTicks = g_system->getMillis(true);
}

bool beginStereo(int &width, int &height) {
	Context *c = getContext();
	if (!c)
		return false;

	handleEvents(*c);
	if (c->state != kRunning || !c->backend->isRunning())
		return false;

	if (!c->frameOpen && !openFrame(*c)) {
		fail(*c);
		return false;
	}
	if (!c->frame.shouldRender || !c->frame.viewsValid)
		return false;
	if (!ensureEyeResources(*c)) {
		fail(*c);
		return false;
	}

	width = c->eyeW;
	height = c->eyeH;
	return true;
}

const EyeView &getEyeView(int eye) {
	return g_ctx->eyeViews[eye];
}

void bindEye(int eye) {
	Context &c = *g_ctx;

	GLuint texture;
	if (!c.backend->acquireEye(eye, texture))
		return;
	c.eyeTarget.bind(c.gl, texture);
}

void endEye(int eye) {
	Context &c = *g_ctx;
	if (!c.eyeTarget.isBound())
		return;

	c.eyeTarget.unbind(c.gl);
	c.backend->releaseEye(eye);
}

void endStereo() {
	Context &c = *g_ctx;

	c.gl.finish();
	c.stereoRendered = true;
	c.stereoEverSubmitted = true;
	c.stereoTicks = g_system->getMillis(true);
	if (!closeFrame(c))
		fail(c);
	c.lastFrameTicks = g_system->getMillis(true);
}

float consumeTurn() {
	if (!g_ctx || g_ctx->state != kRunning)
		return 0.0f;
	return g_ctx->controls.consumeTurn();
}

bool consumeInventoryToggle() {
	if (!g_ctx || g_ctx->state != kRunning)
		return false;
	return g_ctx->controls.consumeInventoryToggle();
}

void leaveStereo() {
	if (g_ctx)
		g_ctx->stereoEverSubmitted = false;
}

bool isSessionRunning() {
	return g_ctx && g_ctx->state == kRunning && g_ctx->backend->isRunning();
}

bool isStereoActive() {
	return g_ctx && g_ctx->state == kRunning && stereoRecent(*g_ctx);
}

bool getHeadOrientation(float &yaw, float &pitch) {
	if (!g_ctx || g_ctx->state != kRunning || !g_ctx->frame.headValid)
		return false;
	yaw = g_ctx->headYaw;
	pitch = g_ctx->headPitch;
	return true;
}

bool projectAim(int eye, float &x, float &y) {
	if (!g_ctx || g_ctx->state != kRunning || !g_ctx->controls.hasAim() || !g_ctx->frame.viewsValid)
		return false;

	// Rotate the aim direction into the eye space with the inverse eye orientation
	float v[3] = { g_ctx->controls.aimDirection()[0], g_ctx->controls.aimDirection()[1], g_ctx->controls.aimDirection()[2] };
	rotateVector(conjugate(g_ctx->frame.eyeOrientation[eye]), v);
	if (v[2] > -0.001f)
		return false;

	const EyeView &view = g_ctx->eyeViews[eye];
	const float tx = v[0] / -v[2];
	const float ty = v[1] / -v[2];
	x = (tx - view.tanLeft) / (view.tanRight - view.tanLeft);
	y = 1.0f - (ty - view.tanDown) / (view.tanUp - view.tanDown);
	return true;
}

bool getAimOrientation(float &yaw, float &pitch) {
	if (!g_ctx || g_ctx->state != kRunning)
		return false;
	return g_ctx->controls.getAimOrientation(yaw, pitch);
}

void shutdown() {
	if (!g_ctx)
		return;

	Context *c = g_ctx;
	g_ctx = nullptr;

	// The GL context is still current
	if (c->gl.deleteFramebuffers) {
		c->theatre.shutdown(c->gl);
		c->eyeTarget.destroy(c->gl);
	}
	if (c->backend) {
		c->backend->shutdown();
		delete c->backend;
	}
	delete c;
}

} // End of namespace XR

#endif // USE_JNANINVR
