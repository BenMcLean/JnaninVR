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

#if defined(USE_JNANINVR) && defined(ANDROID_BACKEND)

#include "common/config-manager.h"
#include "common/events.h"
#include "common/system.h"

#include <EGL/egl.h>

#include "backends/platform/android/android.h"
#include "backends/platform/android/jni-android.h"
#include "backends/xr/xr.h"
#include "backends/xr/xr-android.h"
#include "backends/xr/xr-host.h"

namespace XR {

namespace Host {

namespace {

// Set while the XR code queues its own events
bool g_injecting;

jobject g_context;

OSystem_Android *getSystem() {
	return dynamic_cast<OSystem_Android *>(g_system);
}

void push(const Common::Event &ev) {
	g_injecting = true;
	getSystem()->pushEvent(ev);
	g_injecting = false;
}

void pushMouse(Common::EventType type, int x, int y) {
	Common::Event ev;
	ev.type = type;
	ev.mouse.x = x;
	ev.mouse.y = y;
	push(ev);
}

void pushKey(Common::EventType type, Common::KeyCode code) {
	Common::Event ev;
	ev.type = type;
	ev.kbd.keycode = code;
	push(ev);
}

} // End of anonymous namespace

bool isRequested() {
	return ConfMan.hasKey("xr") && ConfMan.getBool("xr");
}

const char *getRequestedBackend() {
	static Common::String name;
	name = ConfMan.hasKey("xr_backend") ? ConfMan.get("xr_backend") : Common::String();
	return name.c_str();
}

jobject getActivity() {
	if (!g_context) {
		jobject activity = JNI::getXRContext();
		if (activity)
			g_context = JNI::getEnv()->NewGlobalRef(activity);
	}
	return g_context;
}

bool filterEvent(const Common::Event &event) {
	return !g_injecting && consumeEvent(event);
}

bool init() {
	return eglGetCurrentContext() != EGL_NO_CONTEXT;
}

void *getProcAddress(const char *name) {
	return (void *)eglGetProcAddress(name);
}

void getWindowSize(int &width, int &height) {
	width = JNI::egl_surface_width;
	height = JNI::egl_surface_height;
}

void getDrawableSize(int &width, int &height) {
	getWindowSize(width, height);
}

void pushMouseMotion(int x, int y) {
	pushMouse(Common::EVENT_MOUSEMOVE, x, y);
}

void pushMouseButton(bool down, int x, int y) {
	pushMouse(down ? Common::EVENT_LBUTTONDOWN : Common::EVENT_LBUTTONUP, x, y);
}

void pushEscape(bool down) {
	pushKey(down ? Common::EVENT_KEYDOWN : Common::EVENT_KEYUP, Common::KEYCODE_ESCAPE);
}

} // End of namespace Host

} // End of namespace XR

#endif // USE_JNANINVR && ANDROID_BACKEND
