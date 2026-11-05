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

#include "backends/xr/xr-host.h"
#include "backends/xr/xr-theatre.h"

namespace XR {

const float Theatre::kDistance = 2.0f;
const float Theatre::kWidth = 3.2f;

Theatre::Theatre() : _tex(0), _fbo(0), _width(0), _height(0), _valid(false), _anchored(false) {
	_pose.position[2] = -kDistance;
}

bool Theatre::init(GLFuncs &gl) {
	gl.genFramebuffers(1, &_fbo);
	gl.genTextures(1, &_tex);
	return _fbo != 0 && _tex != 0;
}

void Theatre::shutdown(GLFuncs &gl) {
	if (_fbo)
		gl.deleteFramebuffers(1, &_fbo);
	if (_tex)
		gl.deleteTextures(1, &_tex);
	_fbo = _tex = 0;
	_valid = false;
}

void Theatre::capture(GLFuncs &gl) {
	int w, h;
	Host::getDrawableSize(w, h);
	if (w <= 0 || h <= 0)
		return;

	GLStateSaver saver(gl);
	if (w != _width || h != _height) {
		gl.bindTexture(GL_TEXTURE_2D, _tex);
		if (gl.es2) {
			// The window may have no alpha, and a copy cannot make up components it lacks
			gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
		} else {
			gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		}
		// Without these, a texture of any size is incomplete in ES 2
		gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (!gl.es2) {
			gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
			gl.framebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _tex, 0);
		}
		_width = w;
		_height = h;
	}

	if (gl.es2) {
		gl.bindTexture(GL_TEXTURE_2D, _tex);
		gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
		gl.copyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
	} else {
		gl.bindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
		gl.blitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	_valid = true;
}

void Theatre::anchor(const Pose *head) {
	_anchored = true;
	if (!head)
		return;

	float headYaw, headPitch, roll;
	orientationToEuler(head->orientation, headYaw, headPitch, roll);
	_pose.orientation = yawPitchQuat(headYaw, headPitch);

	float forward[3] = { 0.0f, 0.0f, -1.0f };
	rotateVector(_pose.orientation, forward);
	for (int i = 0; i < 3; i++)
		_pose.position[i] = head->position[i] + forward[i] * kDistance;
}

bool Theatre::intersect(const float *origin, const float *direction, float &u, float &v) const {
	if (!_anchored || _width <= 0)
		return false;

	float normal[3] = { 0.0f, 0.0f, 1.0f };
	float right[3] = { 1.0f, 0.0f, 0.0f };
	float up[3] = { 0.0f, 1.0f, 0.0f };
	rotateVector(_pose.orientation, normal);
	rotateVector(_pose.orientation, right);
	rotateVector(_pose.orientation, up);

	const float denominator = dot3(direction, normal);
	if (fabsf(denominator) <= 0.001f)
		return false;

	const float toScreen[3] = {
		_pose.position[0] - origin[0],
		_pose.position[1] - origin[1],
		_pose.position[2] - origin[2]
	};
	const float t = dot3(toScreen, normal) / denominator;
	const float hit[3] = {
		origin[0] + t * direction[0] - _pose.position[0],
		origin[1] + t * direction[1] - _pose.position[1],
		origin[2] + t * direction[2] - _pose.position[2]
	};
	u = dot3(hit, right) / kWidth + 0.5f;
	v = 0.5f - dot3(hit, up) / screenHeight();
	return t > 0.0f && u >= 0.0f && u < 1.0f && v >= 0.0f && v < 1.0f;
}

} // End of namespace XR

#endif // USE_JNANINVR
