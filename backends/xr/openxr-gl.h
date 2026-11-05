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

#ifndef BACKENDS_XR_OPENXR_GL_H
#define BACKENDS_XR_OPENXR_GL_H

#include "common/scummsys.h"

#ifdef USE_OPENXR

#include "backends/xr/xr-gl.h"

#ifdef XR_GL_ES
#define XR_USE_GRAPHICS_API_OPENGL_ES
#else
#define XR_USE_GRAPHICS_API_OPENGL
#endif

// A XR_USE_PLATFORM_* macro for the window system binding may be defined before including this file
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace XR {

#ifdef XR_GL_ES
typedef XrSwapchainImageOpenGLESKHR SwapchainImage;
const XrStructureType kSwapchainImageType = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
#else
typedef XrSwapchainImageOpenGLKHR SwapchainImage;
const XrStructureType kSwapchainImageType = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
#endif

} // End of namespace XR

#endif // USE_OPENXR

#endif
