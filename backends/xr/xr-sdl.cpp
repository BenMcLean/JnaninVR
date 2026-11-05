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

#if defined(USE_JNANINVR) && defined(SDL_BACKEND)

#include <SDL.h>
#include <string.h>
#include <stdlib.h>

#include "backends/xr/xr-host.h"

namespace XR {

namespace Host {

namespace {

SDL_Window *g_window;

} // End of anonymous namespace

bool isRequested() {
	return getenv("SCUMMVM_XR") != nullptr;
}

const char *getRequestedBackend() {
	const char *name = getenv("SCUMMVM_XR_BACKEND");
	return name ? name : "";
}

bool init() {
	g_window = SDL_GL_GetCurrentWindow();
	return g_window != nullptr;
}

void *getProcAddress(const char *name) {
	return SDL_GL_GetProcAddress(name);
}

void getWindowSize(int &width, int &height) {
	SDL_GetWindowSize(g_window, &width, &height);
}

void getDrawableSize(int &width, int &height) {
	SDL_GL_GetDrawableSize(g_window, &width, &height);
}

void pushMouseMotion(int x, int y) {
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_MOUSEMOTION;
	ev.motion.windowID = SDL_GetWindowID(g_window);
	ev.motion.x = x;
	ev.motion.y = y;
	SDL_PushEvent(&ev);
}

void pushMouseButton(bool down, int x, int y) {
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	ev.button.windowID = SDL_GetWindowID(g_window);
	ev.button.button = SDL_BUTTON_LEFT;
	ev.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	ev.button.clicks = 1;
	ev.button.x = x;
	ev.button.y = y;
	SDL_PushEvent(&ev);
}

void pushEscape(bool down) {
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	ev.key.windowID = SDL_GetWindowID(g_window);
	ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	ev.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
	ev.key.keysym.sym = SDLK_ESCAPE;
	SDL_PushEvent(&ev);
}

} // End of namespace Host

} // End of namespace XR

#endif // USE_JNANINVR && SDL_BACKEND
