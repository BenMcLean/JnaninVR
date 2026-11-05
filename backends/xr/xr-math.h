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

#ifndef BACKENDS_XR_XR_MATH_H
#define BACKENDS_XR_XR_MATH_H

#include "common/scummsys.h"

#ifdef USE_JNANINVR

#include "common/util.h"

#include <math.h>

namespace XR {

struct Quat {
	float x, y, z, w;
};

/** A position and orientation in the tracking space (right handed, Y up, -Z forward). */
struct Pose {
	Quat orientation;
	float position[3];

	Pose() {
		orientation.x = orientation.y = orientation.z = 0.0f;
		orientation.w = 1.0f;
		position[0] = position[1] = position[2] = 0.0f;
	}
};

inline float radToDeg(float r) {
	return r * (180.0f / (float)M_PI);
}

/** Decompose an orientation into yaw (around Y), pitch (around X) and roll (around Z), applied in that order. */
inline void orientationToEuler(const Quat &q, float &yaw, float &pitch, float &roll) {
	const float m02 = 2.0f * (q.x * q.z + q.w * q.y);
	const float m12 = 2.0f * (q.y * q.z - q.w * q.x);
	const float m22 = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	const float m10 = 2.0f * (q.x * q.y + q.w * q.z);
	const float m11 = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);

	yaw = radToDeg(atan2f(m02, m22));
	pitch = radToDeg(asinf(CLIP(-m12, -1.0f, 1.0f)));
	roll = radToDeg(atan2f(m10, m11));
}

inline float dot3(const float *a, const float *b) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/** Rotate a vector by a unit quaternion, in place. */
inline void rotateVector(const Quat &q, float *v) {
	const float t[3] = {
		2.0f * (q.y * v[2] - q.z * v[1]),
		2.0f * (q.z * v[0] - q.x * v[2]),
		2.0f * (q.x * v[1] - q.y * v[0])
	};
	const float r[3] = {
		v[0] + q.w * t[0] + (q.y * t[2] - q.z * t[1]),
		v[1] + q.w * t[1] + (q.z * t[0] - q.x * t[2]),
		v[2] + q.w * t[2] + (q.x * t[1] - q.y * t[0])
	};
	v[0] = r[0];
	v[1] = r[1];
	v[2] = r[2];
}

inline Quat conjugate(const Quat &q) {
	const Quat r = { -q.x, -q.y, -q.z, q.w };
	return r;
}

inline Quat multiplyQuaternions(const Quat &a, const Quat &b) {
	Quat r;
	r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
	r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
	r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
	r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
	return r;
}

/** An upright orientation looking at the given yaw and pitch, in degrees. */
inline Quat yawPitchQuat(float yawDeg, float pitchDeg) {
	const float halfYaw = yawDeg * (float)M_PI / 360.0f;
	const float halfPitch = pitchDeg * (float)M_PI / 360.0f;
	const Quat yaw = { 0.0f, sinf(halfYaw), 0.0f, cosf(halfYaw) };
	const Quat pitch = { sinf(halfPitch), 0.0f, 0.0f, cosf(halfPitch) };
	return multiplyQuaternions(yaw, pitch);
}

/** The forward vector (0, 0, -1) rotated by q. */
inline void forwardVector(const Quat &q, float &x, float &y, float &z) {
	x = -2.0f * (q.x * q.z + q.w * q.y);
	y = -2.0f * (q.y * q.z - q.w * q.x);
	z = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));
}

} // End of namespace XR

#endif // USE_JNANINVR

#endif
