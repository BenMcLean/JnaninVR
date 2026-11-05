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

#ifndef BACKENDS_XR_XR_HOST_H
#define BACKENDS_XR_XR_HOST_H

#include "common/scummsys.h"

#ifdef USE_JNANINVR

namespace XR {

/**
 * What the XR code needs from the application: the GL context, the window
 * size and a way to feed input events to the engine.
 * One implementation is linked per backend (xr-sdl.cpp for SDL).
 */
namespace Host {

/** Whether the user asked for the headset to be used. */
bool isRequested();

/** Name of the backend the user asked for ("openxr", "cardboard"), or an empty string. */
const char *getRequestedBackend();

/** Find the window of the current GL context. */
bool init();

/** Address of a GL function, or nullptr. */
void *getProcAddress(const char *name);

/** Size of the window in input coordinates. */
void getWindowSize(int &width, int &height);

/** Size of the GL drawable in pixels. */
void getDrawableSize(int &width, int &height);

/** Input coordinates are in window coordinates. */
void pushMouseMotion(int x, int y);
void pushMouseButton(bool down, int x, int y);
void pushEscape(bool down);

} // End of namespace Host

} // End of namespace XR

#endif // USE_JNANINVR

#endif
