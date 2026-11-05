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

#ifndef BACKENDS_XR_XR_THEATRE_H
#define BACKENDS_XR_XR_THEATRE_H

#include "common/scummsys.h"

#ifdef USE_JNANINVR

#include "backends/xr/xr-gl.h"
#include "backends/xr/xr-math.h"

namespace XR {

/**
 * "Theatre mode": the application window shown on a flat screen floating in
 * front of the user. This holds the copy of the window contents and the
 * placement of the screen, and maps a pointing ray to window coordinates.
 * Showing it is up to the backend.
 */
class Theatre {
public:
	static const float kDistance;
	static const float kWidth;

	Theatre();

	bool init(GLFuncs &gl);
	void shutdown(GLFuncs &gl);

	/** Copy the current window backbuffer. Call right before the buffer swap. */
	void capture(GLFuncs &gl);

	bool hasImage() const { return _valid; }
	int width() const { return _width; }
	int height() const { return _height; }
	/** Framebuffer holding the captured window contents. */
	GLuint framebuffer() const { return _fbo; }
	/** Texture holding the captured window contents. */
	GLuint texture() const { return _tex; }

	/** Place the screen in front of the head, upright. Without a head pose the last placement is kept. */
	void anchor(const Pose *head);
	void unanchor() { _anchored = false; }
	bool isAnchored() const { return _anchored; }

	const Pose &pose() const { return _pose; }
	/** Size of the screen in the tracking space. */
	float screenWidth() const { return kWidth; }
	float screenHeight() const { return _width > 0 ? kWidth * _height / (float)_width : kWidth; }

	/** Where a ray hits the screen, in the range 0..1 from the top left corner. */
	bool intersect(const float *origin, const float *direction, float &u, float &v) const;

private:
	GLuint _tex, _fbo;
	int _width, _height;
	bool _valid;

	Pose _pose;
	bool _anchored;
};

} // End of namespace XR

#endif // USE_JNANINVR

#endif
