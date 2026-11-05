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

#ifndef BACKENDS_XR_XR_ANDROID_H
#define BACKENDS_XR_XR_ANDROID_H

#include "common/scummsys.h"

#if defined(USE_JNANINVR) && defined(ANDROID_BACKEND)

#include <jni.h>

namespace Common {
struct Event;
}

namespace XR {

namespace Host {

/** The activity of the application, as a global reference, or nullptr. */
jobject getActivity();

/**
 * For the Android event queue: offer an event the application is about to
 * queue. Returns true if the XR code used it up. Events made by the XR code
 * itself are never used up.
 */
bool filterEvent(const Common::Event &event);

} // End of namespace Host

} // End of namespace XR

#endif // USE_JNANINVR && ANDROID_BACKEND

#endif
