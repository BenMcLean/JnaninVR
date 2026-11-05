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

#ifndef BACKENDS_XR_XR_H
#define BACKENDS_XR_XR_H

#include "common/scummsys.h"

#ifdef USE_JNANINVR

namespace Common {
struct Event;
}

/**
 * Head mounted display support for backends with an OpenGL context.
 *
 * By default ("theatre mode") the ScummVM window contents are shown on a
 * floating screen, and a controller ray is turned into mouse events.
 * Engines can additionally render stereo views themselves (see beginStereo()),
 * in which case the screen is replaced by the engine's views.
 *
 * Enabled at runtime by setting the SCUMMVM_XR environment variable (SDL)
 * or the "xr" configuration key (Android).
 * The way frames reach the user is a XR::Backend (backends/xr/xr-backend.h),
 * the application side is XR::Host (backends/xr/xr-host.h).
 */
namespace XR {

/** Copy the current window backbuffer. Call right before the buffer swap. */
void captureBackbuffer();

/** Run one step of the XR frame loop. Call often, with the GL context current. */
void pump();

/** Release all XR resources. */
void shutdown();

/**
 * Orientation and field of view of one eye. Angles are in degrees, in the
 * XR local space: yaw is counter-clockwise around the up axis, pitch is
 * positive looking up. Only the orientation is tracked, not the position.
 */
struct EyeView {
	float yaw, pitch, roll;
	float tanLeft, tanRight, tanDown, tanUp;
};

/**
 * Start a stereo frame. When it returns true the engine must render both
 * eyes with bindEye() / endEye() and then call endStereo().
 * The eye render target size is returned in width and height.
 */
bool beginStereo(int &width, int &height);

/** The view of an eye (0 or 1) for the current stereo frame. */
const EyeView &getEyeView(int eye);

/** Bind (and clear) the render target of an eye. */
void bindEye(int eye);

/** Finish rendering an eye, and restore the previous render target. */
void endEye(int eye);

/** Submit the stereo frame to the display. */
void endStereo();

/**
 * The turn requested with the thumbstick since the last call, in degrees,
 * positive to the right.
 */
float consumeTurn();

/** Whether the inventory button was pressed since the last call. */
bool consumeInventoryToggle();

/**
 * Offer an application event to the XR code. Returns true if it was used up
 * (gamepad and touch input while the active backend has no controllers).
 */
bool consumeEvent(const Common::Event &event);

/** Stop considering the engine to be rendering stereo frames. The window is shown on the theatre screen. */
void leaveStereo();

/** Whether a headset session is running. */
bool isSessionRunning();

/** Whether a stereo frame was submitted recently. */
bool isStereoActive();

/** Head orientation in local space, as of the current frame. */
bool getHeadOrientation(float &yaw, float &pitch);

/** Orientation of the pointing controller in local space. */
bool getAimOrientation(float &yaw, float &pitch);

/**
 * Where the pointing controller aims in the image of an eye, in the range
 * 0..1 from the top left corner. Returns false when it is not in front of the eye.
 */
bool projectAim(int eye, float &x, float &y);

} // End of namespace XR

#endif // USE_JNANINVR

#endif
