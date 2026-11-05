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

#ifndef BACKENDS_XR_XR_CONTROLS_H
#define BACKENDS_XR_XR_CONTROLS_H

#include "common/scummsys.h"

#ifdef USE_JNANINVR

#include "backends/xr/xr-theatre.h"

namespace Common {
struct Event;
}

namespace XR {

enum { kHandLeft, kHandRight, kHandCount };

/**
 * One frame of device input, as reported by a backend. Devices without two
 * pointing controllers only fill in the first hand.
 */
struct InputState {
	/** False when the devices could not be read this frame. */
	bool valid;

	bool select[kHandCount];
	bool menu[kHandCount];

	/** Set for the frame in which the inventory button went down. */
	bool inventoryPressed;
	/** Turning axis, -1 (left) to 1 (right). */
	float stick;
	/** Set for the frame in which the turning axis was clicked. */
	bool stickClicked;

	bool aimValid[kHandCount];
	float aimOrigin[kHandCount][3];
	float aimDirection[kHandCount][3];

	InputState();
};

/**
 * Input that does not come from the headset session: a gamepad, the touch
 * screen... Fed from the application events, read by backends that have no
 * controllers of their own.
 */
struct ExternalInput {
	bool select, menu;
	bool inventoryPressed, stickClicked;
	float stick;

	ExternalInput() : select(false), menu(false), inventoryPressed(false), stickClicked(false), stick(0.0f) {}
};

/**
 * Turns device input into what the engine consumes: mouse and escape events
 * while the theatre screen is shown, the aim ray, turning and the inventory button.
 */
class Controls {
public:
	Controls();

	void update(const InputState &input, const Theatre &theatre, bool stereoActive);

	/** The turn requested with the thumbstick since the last call, in degrees, positive to the right. */
	float consumeTurn();
	bool consumeInventoryToggle();

	bool hasAim() const { return _aimValid; }
	const float *aimDirection() const { return _aimDir; }
	/** Yaw and pitch of the aim ray, in degrees. */
	bool getAimOrientation(float &yaw, float &pitch) const;

private:
	int _activeHand;
	bool _selectDown[kHandCount];
	bool _menuDown[kHandCount];
	bool _buttonHeld;
	int _lastMouseX, _lastMouseY;

	bool _aimValid;
	float _aimDir[3];
	float _aimYaw, _aimPitch;

	bool _snapTurn;
	bool _snapLatched;
	float _pendingTurn;
	bool _inventoryPressed;
	uint32 _lastTurnTicks;
};

/** Take the gamepad and touch events. Returns true when the event was used up. */
bool handleExternalEvent(const Common::Event &event, ExternalInput &input);

} // End of namespace XR

#endif // USE_JNANINVR

#endif
