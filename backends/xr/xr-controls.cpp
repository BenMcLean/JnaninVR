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

#ifdef USE_JNANINVR

#include "common/debug.h"
#include "common/events.h"
#include "common/system.h"

#include "backends/xr/xr-controls.h"
#include "backends/xr/xr-host.h"

namespace XR {

namespace {

// Thumbstick turning
const float kSmoothTurnSpeed = 240.0f; // degrees per second at full deflection
const float kSnapTurnAngle = 45.0f;
const float kTurnDeadZone = 0.15f;
const float kSnapThreshold = 0.7f;
const float kSnapRelease = 0.3f;

} // End of anonymous namespace

InputState::InputState() : valid(false), inventoryPressed(false), stick(0.0f), stickClicked(false) {
	for (int h = 0; h < kHandCount; h++) {
		select[h] = menu[h] = aimValid[h] = false;
		for (int i = 0; i < 3; i++)
			aimOrigin[h][i] = aimDirection[h][i] = 0.0f;
	}
}

Controls::Controls() : _activeHand(kHandRight), _buttonHeld(false), _lastMouseX(0), _lastMouseY(0),
		_aimValid(false), _aimYaw(0.0f), _aimPitch(0.0f), _snapTurn(false), _snapLatched(false),
		_pendingTurn(0.0f), _inventoryPressed(false), _lastTurnTicks(0) {
	for (int h = 0; h < kHandCount; h++)
		_selectDown[h] = _menuDown[h] = false;
	_aimDir[0] = _aimDir[1] = _aimDir[2] = 0.0f;
}

void Controls::update(const InputState &input, const Theatre &theatre, bool stereoActive) {
	for (int h = 0; h < kHandCount; h++) {
		if (input.select[h] && !_selectDown[h] && !_buttonHeld)
			_activeHand = h;
	}

	if (input.inventoryPressed)
		_inventoryPressed = true;

	// Thumbstick turning: smooth, or snapped in steps. A click switches between the two.
	if (input.stickClicked) {
		_snapTurn = !_snapTurn;
		_snapLatched = false;
		debug(1, "XR: %s turning", _snapTurn ? "snap" : "smooth");
	}

	const float stick = input.stick;
	const uint32 now = g_system->getMillis(true);
	const float elapsed = MIN<float>((now - _lastTurnTicks) / 1000.0f, 0.1f);
	_lastTurnTicks = now;
	if (_snapTurn) {
		if (!_snapLatched && fabsf(stick) > kSnapThreshold) {
			_pendingTurn += stick > 0.0f ? kSnapTurnAngle : -kSnapTurnAngle;
			_snapLatched = true;
		} else if (fabsf(stick) < kSnapRelease) {
			_snapLatched = false;
		}
	} else if (fabsf(stick) > kTurnDeadZone) {
		const float scaled = (fabsf(stick) - kTurnDeadZone) / (1.0f - kTurnDeadZone);
		_pendingTurn += (stick > 0.0f ? 1.0f : -1.0f) * scaled * kSmoothTurnSpeed * elapsed;
	}

	// Aim ray of the active hand
	int winW, winH;
	Host::getWindowSize(winW, winH);
	int mouseX = _lastMouseX, mouseY = _lastMouseY;
	bool onScreen = false;

	_aimValid = input.aimValid[_activeHand];
	if (_aimValid) {
		const float *dir = input.aimDirection[_activeHand];
		_aimDir[0] = dir[0];
		_aimDir[1] = dir[1];
		_aimDir[2] = dir[2];
		_aimYaw = radToDeg(atan2f(-dir[0], -dir[2]));
		_aimPitch = radToDeg(asinf(CLIP(dir[1], -1.0f, 1.0f)));

		float u, v;
		if (theatre.intersect(input.aimOrigin[_activeHand], dir, u, v)) {
			mouseX = (int)(u * winW);
			mouseY = (int)(v * winH);
			onScreen = true;
		}
	}

	// In stereo mode the engine maps the aim ray to its own pointer
	if (onScreen && !stereoActive) {
		Host::pushMouseMotion(mouseX, mouseY);
		_lastMouseX = mouseX;
		_lastMouseY = mouseY;
	}

	const bool selectActive = input.select[_activeHand];
	if (selectActive != _buttonHeld && (onScreen || stereoActive || !selectActive)) {
		Host::pushMouseButton(selectActive, mouseX, mouseY);
		_buttonHeld = selectActive;
	}

	for (int h = 0; h < kHandCount; h++) {
		if (input.menu[h] != _menuDown[h])
			Host::pushEscape(input.menu[h]);
		_selectDown[h] = input.select[h];
		_menuDown[h] = input.menu[h];
	}
}

bool handleExternalEvent(const Common::Event &event, ExternalInput &input) {
	switch (event.type) {
	case Common::EVENT_LBUTTONDOWN:
	case Common::EVENT_LBUTTONUP:
		input.select = event.type == Common::EVENT_LBUTTONDOWN;
		return true;
	case Common::EVENT_MOUSEMOVE:
		return true;
	case Common::EVENT_JOYBUTTON_DOWN:
	case Common::EVENT_JOYBUTTON_UP: {
		const bool down = event.type == Common::EVENT_JOYBUTTON_DOWN;
		switch (event.joystick.button) {
		case Common::JOYSTICK_BUTTON_A:
			input.select = down;
			return true;
		case Common::JOYSTICK_BUTTON_B:
		case Common::JOYSTICK_BUTTON_START:
			input.menu = down;
			return true;
		case Common::JOYSTICK_BUTTON_X:
		case Common::JOYSTICK_BUTTON_Y:
			if (down)
				input.inventoryPressed = true;
			return true;
		case Common::JOYSTICK_BUTTON_LEFT_STICK:
		case Common::JOYSTICK_BUTTON_RIGHT_STICK:
			if (down)
				input.stickClicked = true;
			return true;
		default:
			return false;
		}
	}
	case Common::EVENT_JOYAXIS_MOTION:
		if (event.joystick.axis == Common::JOYSTICK_AXIS_LEFT_STICK_X ||
		    event.joystick.axis == Common::JOYSTICK_AXIS_RIGHT_STICK_X ||
		    event.joystick.axis == Common::JOYSTICK_AXIS_HAT_X) {
			input.stick = CLIP<float>(event.joystick.position / (float)Common::JOYAXIS_MAX, -1.0f, 1.0f);
			return true;
		}
		return false;
	default:
		return false;
	}
}

float Controls::consumeTurn() {
	const float turn = _pendingTurn;
	_pendingTurn = 0.0f;
	return turn;
}

bool Controls::consumeInventoryToggle() {
	const bool pressed = _inventoryPressed;
	_inventoryPressed = false;
	return pressed;
}

bool Controls::getAimOrientation(float &yaw, float &pitch) const {
	if (!_aimValid)
		return false;
	yaw = _aimYaw;
	pitch = _aimPitch;
	return true;
}

} // End of namespace XR

#endif // USE_JNANINVR
