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

#ifndef BACKENDS_XR_OPENXR_PLATFORM_H
#define BACKENDS_XR_OPENXR_PLATFORM_H

#include "backends/xr/openxr-gl.h"

#include "common/array.h"

#ifdef USE_OPENXR

namespace XR {

/**
 * What the OpenXR backend needs from the host: the native window system
 * binding of the OpenGL context.
 * One implementation is linked per backend (openxr-sdl.cpp for SDL).
 */
namespace Platform {

/** Prepare the OpenXR loader, before an instance is created. */
bool initLoader();

/** Look up the native window and GL context handles. The GL context must be current. */
bool init();

/** The OpenXR extensions needed to bind the GL context. */
void getExtensions(Common::Array<const char *> &extensions);

/** Structure to chain to the instance creation, or nullptr. */
const void *getInstanceNext();

/** Create a session bound to the current GL context. */
bool createSession(XrInstance instance, XrSystemId systemId, XrSession &session);

} // End of namespace Platform

} // End of namespace XR

#endif // USE_OPENXR

#endif
