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

#if defined(USE_OPENXR) && defined(ANDROID_BACKEND)

#include "common/textconsole.h"

#include <jni.h>
#include <string.h>
#include <EGL/egl.h>

#define XR_USE_PLATFORM_ANDROID
#include "backends/xr/openxr-platform.h"
#include "backends/platform/android/jni-android.h"
#include "backends/xr/xr-android.h"
#include "backends/xr/xr-host.h"

namespace XR {

namespace Platform {

namespace {

jobject g_application;
XrInstanceCreateInfoAndroidKHR g_instanceInfo;

template<class T>
T xrStruct(XrStructureType type) {
	T s;
	memset(&s, 0, sizeof(s));
	s.type = type;
	return s;
}

} // End of anonymous namespace

bool initLoader() {
	g_application = Host::getActivity();
	if (!g_application) {
		warning("OpenXR: could not get the Android activity");
		return false;
	}

	PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
	if (XR_FAILED(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction *)&initializeLoader)) || !initializeLoader)
		return false;

	XrLoaderInitInfoAndroidKHR loaderInfo = xrStruct<XrLoaderInitInfoAndroidKHR>(XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR);
	loaderInfo.applicationVM = JNI::getJavaVM();
	loaderInfo.applicationContext = g_application;
	return XR_SUCCEEDED(initializeLoader((const XrLoaderInitInfoBaseHeaderKHR *)&loaderInfo));
}

bool init() {
	return eglGetCurrentContext() != EGL_NO_CONTEXT;
}

void getExtensions(Common::Array<const char *> &extensions) {
	extensions.push_back(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
	extensions.push_back(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
}

const void *getInstanceNext() {
	g_instanceInfo = xrStruct<XrInstanceCreateInfoAndroidKHR>(XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR);
	g_instanceInfo.applicationVM = JNI::getJavaVM();
	g_instanceInfo.applicationActivity = g_application;
	return &g_instanceInfo;
}

bool createSession(XrInstance instance, XrSystemId systemId, XrSession &session) {
	PFN_xrGetOpenGLESGraphicsRequirementsKHR getRequirements = nullptr;
	if (XR_FAILED(xrGetInstanceProcAddr(instance, "xrGetOpenGLESGraphicsRequirementsKHR", (PFN_xrVoidFunction *)&getRequirements)))
		return false;
	XrGraphicsRequirementsOpenGLESKHR requirements = xrStruct<XrGraphicsRequirementsOpenGLESKHR>(XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR);
	if (XR_FAILED(getRequirements(instance, systemId, &requirements)))
		return false;

	EGLDisplay display = eglGetCurrentDisplay();
	EGLContext context = eglGetCurrentContext();
	EGLint configId = 0;
	if (!eglQueryContext(display, context, EGL_CONFIG_ID, &configId))
		return false;
	const EGLint attribs[] = { EGL_CONFIG_ID, configId, EGL_NONE };
	EGLConfig config;
	EGLint numConfigs = 0;
	if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs) || numConfigs < 1)
		return false;

	XrGraphicsBindingOpenGLESAndroidKHR binding = xrStruct<XrGraphicsBindingOpenGLESAndroidKHR>(XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR);
	binding.display = display;
	binding.config = config;
	binding.context = context;

	XrSessionCreateInfo sessionInfo = xrStruct<XrSessionCreateInfo>(XR_TYPE_SESSION_CREATE_INFO);
	sessionInfo.next = &binding;
	sessionInfo.systemId = systemId;
	return XR_SUCCEEDED(xrCreateSession(instance, &sessionInfo, &session));
}

} // End of namespace Platform

} // End of namespace XR

#endif // USE_OPENXR && ANDROID_BACKEND
