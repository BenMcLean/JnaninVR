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

#ifndef BACKENDS_XR_XR_BACKEND_H
#define BACKENDS_XR_XR_BACKEND_H

#include "common/scummsys.h"

#ifdef USE_JNANINVR

#include "backends/xr/xr-controls.h"
#include "backends/xr/xr-gl.h"
#include "backends/xr/xr-math.h"
#include "backends/xr/xr-theatre.h"

namespace XR {

/** Field of view of one eye as the tangents of the angles from the view direction. */
struct EyeFov {
	float tanLeft, tanRight, tanDown, tanUp;
};

/** What a backend learns when it starts a frame. */
struct Frame {
	bool shouldRender;

	bool headValid;
	Pose head;

	bool viewsValid;
	Quat eyeOrientation[2];
	EyeFov eyeFov[2];

	InputState input;

	Frame() : shouldRender(false), headValid(false), viewsValid(false) {
		for (int e = 0; e < 2; e++) {
			eyeOrientation[e].x = eyeOrientation[e].y = eyeOrientation[e].z = 0.0f;
			eyeOrientation[e].w = 1.0f;
			eyeFov[e].tanLeft = eyeFov[e].tanDown = -1.0f;
			eyeFov[e].tanRight = eyeFov[e].tanUp = 1.0f;
		}
	}
};

enum SubmitMode {
	kSubmitNothing,
	/** The engine rendered both eyes. */
	kSubmitStereo,
	/** Show the captured window on the theatre screen. */
	kSubmitTheatre
};

/**
 * A way to put frames in front of the user and read the head and controllers:
 * OpenXR, Cardboard... The core in xr.cpp drives a backend and owns everything
 * that does not depend on it (see Theatre and Controls).
 * All methods are called with the GL context current.
 */
class Backend {
public:
	virtual ~Backend() {}

	/** Whether the GL context may be limited to OpenGL ES 2.0 (see GLFuncs::es2). */
	virtual bool isES2Only() const { return false; }

	/** Create the session. The GL functions outlive the backend. */
	virtual bool init(GLFuncs &gl) = 0;
	/** Release everything. Must cope with a failed init(). */
	virtual void shutdown() = 0;

	/** Handle runtime events. Returns false when the session cannot continue. */
	virtual bool pollEvents() = 0;
	/** Whether frames can be submitted right now. */
	virtual bool isRunning() const = 0;
	/** Time between frames, in milliseconds. */
	virtual uint32 framePeriodMs() const = 0;

	/** Pace to the display and start a frame, reading the poses and input. Returns false on a fatal error. */
	virtual bool beginFrame(Frame &frame) = 0;
	/** Finish the frame started by beginFrame(). */
	virtual bool endFrame(const Frame &frame, SubmitMode mode, const Theatre &theatre) = 0;

	/**
	 * Called with the window contents captured, right before the buffer swap.
	 * Backends that draw to the window themselves do it here.
	 */
	virtual void present(const Theatre &theatre) { (void)theatre; }

	/** The application went to the background (true) or came back. Called from any thread. */
	virtual void setPaused(bool paused) { (void)paused; }

	/** Whether the gamepad and touch events should drive this backend (see getExternalInput()). */
	virtual bool wantsExternalInput() const { return false; }
	void setExternalInput(ExternalInput *input) { _external = input; }

	/** Create the per eye render targets. */
	virtual bool prepareEyes() = 0;
	virtual void getEyeSize(int &width, int &height) const = 0;
	/** Get the texture to render an eye into. */
	virtual bool acquireEye(int eye, GLuint &texture) = 0;
	virtual bool releaseEye(int eye) = 0;

protected:
	/** Input from events, or nullptr. Read and cleared by the backend. */
	ExternalInput *_external = nullptr;
};

#ifdef USE_OPENXR
Backend *createOpenXRBackend();
#endif
#ifdef USE_CARDBOARD
Backend *createCardboardBackend();
#endif

} // End of namespace XR

#endif // USE_JNANINVR

#endif
