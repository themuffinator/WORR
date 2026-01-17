// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

// q_vec3.hpp (Quake Vector3)
// This header defines the `Vector3` class, a modern C++ replacement for the
// traditional C-style `vec3_t` (float array). It provides a type-safe,
// object-oriented interface for 3D vector mathematics.
//
// Key Responsibilities:
// - `Vector3` Class: Defines the structure with x, y, and z members.
// - Operator Overloading: Overloads standard arithmetic operators (+, -, *, /)
//   and comparison operators (==, !=) for intuitive vector operations.
// - Vector Math Functions: Provides essential vector math methods like `dot`,
//   `length`, `normalize`, and `cross`. All core functions are `constexpr`,
//   allowing for compile-time vector calculations where possible.
// - Utility Functions: Declares helper functions for common game-related vector
//   tasks, such as `VectorToAngles`, `AngleVectors`, and `G_ProjectSource`.

#pragma once
#include <array>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>

using nullptr_t = std::nullptr_t;

struct Vector3 {
	static constexpr float kDivisionEpsilon = 1.0e-6f;

	// NOTE: This type is used in engine/shared structs that are often
	// memset/memcpy'd. Keep it POD-like (no references/pointers to internal
	// storage) so zeroing memory yields a valid vector.
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	/*
	=============
	SafeDivisor

	Clamps divisors away from zero while asserting in debug builds.
	=============
	*/
	[[nodiscard]] static constexpr float SafeDivisor(float divisor) {
		const bool near_zero = divisor > -kDivisionEpsilon && divisor < kDivisionEpsilon;

		#ifndef NDEBUG
		if (!std::is_constant_evaluated()) {
			assert(!near_zero && "Vector3 division by zero or near-zero divisor");
		}
		#endif

		if (near_zero) {
			return divisor >= 0.0f ? kDivisionEpsilon : -kDivisionEpsilon;
		}

		return divisor;
	}

	/*
	=============
	Vector3

	Constructs a zero vector.
	=============
	*/
	constexpr Vector3() = default;

	/*
	=============
	Vector3

	Constructs a vector with explicit components.
	=============
	*/
	constexpr Vector3(float xIn, float yIn, float zIn) : x(xIn), y(yIn), z(zIn) {}

	[[nodiscard]] constexpr float* data() noexcept {
		return &x;
	}
	[[nodiscard]] constexpr const float* data() const noexcept {
		return &x;
	}

	/*
	=============
	operator[]

	Provides checked const access to vector components.
	=============
	*/
	[[nodiscard]] constexpr const float& operator[](size_t i) const {
		if (i >= 3) {
			throw std::out_of_range("Vector3 index out of range");
		}

		return data()[i];
	}

	/*
	=============
	operator[]

	Provides checked mutable access to vector components.
	=============
	*/
	[[nodiscard]] constexpr float& operator[](size_t i) {
		if (i >= 3) {
			throw std::out_of_range("Vector3 index out of range");
		}

		return data()[i];
	}
	// comparison
	[[nodiscard]] constexpr bool equals(const Vector3& v) const {
		return x == v.x && y == v.y && z == v.z;
	}
	[[nodiscard]] inline bool equals(const Vector3& v, const float& epsilon) const {
		return std::fabs(x - v.x) <= epsilon && std::fabs(y - v.y) <= epsilon && std::fabs(z - v.z) <= epsilon;
	}
	[[nodiscard]] constexpr bool operator==(const Vector3& v) const {
		return equals(v);
	}
	[[nodiscard]] constexpr bool operator!=(const Vector3& v) const {
		return !(*this == v);
	}

	/*
	=============
	is_zero

	Returns true if all components are zero.
	=============
	*/
	[[nodiscard]] constexpr bool is_zero() const {
		return x == 0.0f && y == 0.0f && z == 0.0f;
	}
	[[nodiscard]] constexpr explicit operator bool() const {
		return x || y || z;
	}

	// dot
	[[nodiscard]] constexpr float dot(const Vector3& v) const {
		return (x * v.x) + (y * v.y) + (z * v.z);
	}
	[[nodiscard]] constexpr Vector3 scaled(const Vector3& v) const {
		return { x * v.x, y * v.y, z * v.z };
	}
	constexpr Vector3& scale(const Vector3& v) {
		*this = this->scaled(v);
		return *this;
	}

	// basic operators
	[[nodiscard]] constexpr Vector3 operator-(const Vector3& v) const {
		return { x - v.x, y - v.y, z - v.z };
	}
	[[nodiscard]] constexpr Vector3 operator+(const Vector3& v) const {
		return { x + v.x, y + v.y, z + v.z };
	}
	/*
	=============
	operator/

	Divides component-wise by another vector using guarded divisors.
	=============
	*/
	[[nodiscard]] constexpr Vector3 operator/(const Vector3& v) const {
		return { x / SafeDivisor(v.x), y / SafeDivisor(v.y), z / SafeDivisor(v.z) };
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	/*
	=============
	operator/

	Divides each component by a scalar using a guarded divisor.
	=============
	*/
	[[nodiscard]] constexpr Vector3 operator/(const T& v) const {
		const float divisor = SafeDivisor(static_cast<float>(v));
		return { static_cast<float>(x / divisor), static_cast<float>(y / divisor), static_cast<float>(z / divisor) };
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	[[nodiscard]] constexpr Vector3 operator*(const T& v) const {
		return { static_cast<float>(x * v), static_cast<float>(y * v), static_cast<float>(z * v) };
	}
	[[nodiscard]] constexpr Vector3 operator-() const {
		return { -x, -y, -z };
	}

	constexpr Vector3& operator-=(const Vector3& v) {
		*this = *this - v;
		return *this;
	}
	constexpr Vector3& operator+=(const Vector3& v) {
		*this = *this + v;
		return *this;
	}
	constexpr Vector3& operator/=(const Vector3& v) {
		*this = *this / v;
		return *this;
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr Vector3& operator/=(const T& v) {
		*this = *this / v;
		return *this;
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr Vector3& operator*=(const T& v) {
		*this = *this * v;
		return *this;
	}

	// operations
	[[nodiscard]] constexpr float lengthSquared() const {
		return this->dot(*this);
	}
	[[nodiscard]] inline float length() const {
		return sqrtf(lengthSquared());
	}
	[[nodiscard]] inline Vector3 normalized() const {
		float len = length();
		return len ? (*this * (1.f / len)) : *this;
	}
	[[nodiscard]] inline Vector3 normalized(float& len) const {
		len = length();
		return len ? (*this * (1.f / len)) : *this;
	}
	inline float normalize() {
		float len = length();

		if (len)
			*this *= (1.f / len);

		return len;
	}
	[[nodiscard]] constexpr Vector3 cross(const Vector3& v) const {
		return {
			y * v.z - z * v.y,
			z * v.x - x * v.z,
			x * v.y - y * v.x
		};
	}
};

static_assert(sizeof(Vector3) == sizeof(float) * 3);
static_assert(alignof(Vector3) == alignof(float));
static_assert(std::is_standard_layout_v<Vector3>);
static_assert(std::is_trivially_copyable_v<Vector3>);

constexpr Vector3 vec3_origin{};

inline void AngleVectors(const Vector3& angles, Vector3* forward, Vector3* right, Vector3* up) {
float angle = angles[YAW] * (PIf * 2 / 360);
float sy = std::sin(angle);
float cy = cosf(angle);
angle = angles[PITCH] * (PIf * 2 / 360);
float sp = std::sin(angle);
float cp = cosf(angle);
angle = angles[ROLL] * (PIf * 2 / 360);
float sr = std::sin(angle);
float cr = cosf(angle);

	if (forward) {
		forward->x = cp * cy;
		forward->y = cp * sy;
		forward->z = -sp;
	}
	if (right) {
		right->x = (-1 * sr * sp * cy + -1 * cr * -sy);
		right->y = (-1 * sr * sp * sy + -1 * cr * cy);
		right->z = -1 * sr * cp;
	}
	if (up) {
		up->x = (cr * sp * cy + -sr * -sy);
		up->y = (cr * sp * sy + -sr * cy);
		up->z = cr * cp;
	}
}

struct angle_vectors_t {
	Vector3 forward, right, up;
};

// for destructuring
inline angle_vectors_t AngleVectors(const Vector3& angles) {
	angle_vectors_t v{};
	AngleVectors(angles, &v.forward, &v.right, &v.up);
	return v;
}

// silly wrappers to allow old C code to work
inline void AngleVectors(const Vector3& angles, Vector3& forward, Vector3& right, Vector3& up) {
	AngleVectors(angles, &forward, &right, &up);
}
inline void AngleVectors(const Vector3& angles, Vector3& forward, Vector3& right, nullptr_t) {
	AngleVectors(angles, &forward, &right, nullptr);
}
inline void AngleVectors(const Vector3& angles, Vector3& forward, nullptr_t, Vector3& up) {
	AngleVectors(angles, &forward, nullptr, &up);
}
inline void AngleVectors(const Vector3& angles, Vector3& forward, nullptr_t, nullptr_t) {
	AngleVectors(angles, &forward, nullptr, nullptr);
}
inline void AngleVectors(const Vector3& angles, nullptr_t, nullptr_t, Vector3& up) {
	AngleVectors(angles, nullptr, nullptr, &up);
}
inline void AngleVectors(const Vector3& angles, nullptr_t, Vector3& right, nullptr_t) {
	AngleVectors(angles, nullptr, &right, nullptr);
}

inline void ClearBounds(Vector3& mins, Vector3& maxs) {
	mins[0] = mins[1] = mins[2] = std::numeric_limits<float>::infinity();
	maxs[0] = maxs[1] = maxs[2] = -std::numeric_limits<float>::infinity();
}

inline void AddPointToBounds(const Vector3& v, Vector3& mins, Vector3& maxs) {
	for (int i = 0; i < 3; i++) {
		float val = v[i];
		if (val < mins[i])
			mins[i] = val;
		if (val > maxs[i])
			maxs[i] = val;
	}
}

[[nodiscard]] constexpr Vector3 ProjectPointOnPlane(const Vector3& p, const Vector3& normal) {
	float inv_denom = 1.0f / normal.dot(normal);
	float d = normal.dot(p) * inv_denom;
	return p - ((normal * inv_denom) * d);
}

/*
** assumes "src" is normalized
*/
[[nodiscard]] inline Vector3 PerpendicularVector(const Vector3& src) {
	int	   pos;
	int	   i;
	float  minelem = 1.0F;
	Vector3 tempvec{};

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for (pos = 0, i = 0; i < 3; i++) {
		if (std::fabs(src[i]) < minelem) {
			pos = i;
			minelem = std::fabs(src[i]);
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src & normalize the result
	*/
	return ProjectPointOnPlane(tempvec, src).normalized();
}

using mat3_t = std::array<std::array<float, 3>, 3>;

/*
================
R_ConcatRotations
================
*/
[[nodiscard]] constexpr mat3_t R_ConcatRotations(const mat3_t& in1, const mat3_t& in2) {
	return {
		std::array<float, 3> {
			in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0],
			in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1],
			in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2]
		},
		{
			in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0],
			in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1],
			in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2]
		},
		{
			in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0],
			in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1],
			in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2]
		}
	};
}

[[nodiscard]] inline Vector3 RotatePointAroundVector(const Vector3& dir, const Vector3& point, float degrees) {
	mat3_t	m{};
	mat3_t  im;
	mat3_t  zrot;
	mat3_t  rot;
	Vector3 vr, vup, vf;

	vf = dir;

	vr = PerpendicularVector(dir);
	vup = vr.cross(vf);

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	im = m;

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

zrot = {};
zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

zrot[0][0] = cosf(DEG2RAD(degrees));
zrot[0][1] = std::sin(DEG2RAD(degrees));
zrot[1][0] = -std::sin(DEG2RAD(degrees));
zrot[1][1] = cosf(DEG2RAD(degrees));

	rot = R_ConcatRotations(R_ConcatRotations(m, zrot), im);

	return {
		rot[0][0] * point[0] + rot[0][1] * point[1] + rot[0][2] * point[2],
		rot[1][0] * point[0] + rot[1][1] * point[1] + rot[1][2] * point[2],
		rot[2][0] * point[0] + rot[2][1] * point[1] + rot[2][2] * point[2]
	};
}

[[nodiscard]] constexpr Vector3 closest_point_to_box(const Vector3& p, const Vector3& bmin_in, const Vector3& bmax_in) {
	const Vector3 lo{
		std::min(bmin_in.x, bmax_in.x),
		std::min(bmin_in.y, bmax_in.y),
		std::min(bmin_in.z, bmax_in.z)
	};
	const Vector3 hi{
		std::max(bmin_in.x, bmax_in.x),
		std::max(bmin_in.y, bmax_in.y),
		std::max(bmin_in.z, bmax_in.z)
	};

	return Vector3{
		std::clamp(p.x, lo.x, hi.x),
		std::clamp(p.y, lo.y, hi.y),
		std::clamp(p.z, lo.z, hi.z)
	};
}

[[nodiscard]] inline float distance_between_boxes(const Vector3& absminsa, const Vector3& absmaxsa, const Vector3& absminsb, const Vector3& absmaxsb) {
	float len = 0;

	for (size_t i = 0; i < 3; i++) {
		if (absmaxsa[i] < absminsb[i]) {
			float d = absmaxsa[i] - absminsb[i];
			len += d * d;
		}
		else if (absminsa[i] > absmaxsb[i]) {
			float d = absminsa[i] - absmaxsb[i];
			len += d * d;
		}
	}

	return sqrt(len);
}

[[nodiscard]] constexpr bool boxes_intersect(const Vector3& amins, const Vector3& amaxs, const Vector3& bmins, const Vector3& bmaxs) {
	return amins.x <= bmaxs.x &&
		amaxs.x >= bmins.x &&
		amins.y <= bmaxs.y &&
		amaxs.y >= bmins.y &&
		amins.z <= bmaxs.z &&
		amaxs.z >= bmins.z;
}

/*
==================
ClipVelocity

Slide off of the impacting object
==================
*/
constexpr float STOP_EPSILON = 0.1f;

[[nodiscard]] constexpr Vector3 ClipVelocity(const Vector3& in, const Vector3& normal, float overbounce) {
	float dot = in.dot(normal);
	Vector3 out = in + (normal * (-2 * dot));
	out *= overbounce - 1.f;

	if (out.lengthSquared() < STOP_EPSILON * STOP_EPSILON)
		out = {};

	return out;
}

[[nodiscard]] constexpr Vector3 SlideClipVelocity(const Vector3& in, const Vector3& normal, float overbounce) {
	float backoff = in.dot(normal) * overbounce;
	Vector3 out = in - (normal * backoff);

	for (int i = 0; i < 3; i++)
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;

	return out;
}

[[nodiscard]] inline float vectoyaw(const Vector3& vec) {
	// PMM - fixed to correct for pitch of 0
	if (vec[PITCH] == 0) {
		if (vec[YAW] == 0)
			return 0.f;
		else if (vec[YAW] > 0)
			return 90.f;
		else
			return 270.f;
	}

	float yaw = (atan2(vec[YAW], vec[PITCH]) * (180.f / PIf));

	if (yaw < 0)
		yaw += 360;

	return yaw;
}

[[nodiscard]] inline Vector3 VectorToAngles(const Vector3& vec) {
	float forward;
	float yaw, pitch;

	if (vec[1] == 0 && vec[0] == 0) {
		if (vec[2] > 0)
			return { -90.f, 0.f, 0.f };
		else
			return { -270.f, 0.f, 0.f };
	}

	// PMM - fixed to correct for pitch of 0
	if (vec[0])
		yaw = (atan2(vec[1], vec[0]) * (180.f / PIf));
	else if (vec[1] > 0)
		yaw = 90;
	else
		yaw = 270;

	if (yaw < 0)
		yaw += 360;

	forward = sqrt(vec[0] * vec[0] + vec[1] * vec[1]);
	pitch = (atan2(vec[2], forward) * (180.f / PIf));

	if (pitch < 0)
		pitch += 360;

	return { -pitch, yaw, 0 };
}

[[nodiscard]] constexpr Vector3 G_ProjectSource(const Vector3& point, const Vector3& distance, const Vector3& forward, const Vector3& right) {
	return point + (forward * distance[0]) + (right * distance[1]) + Vector3{ 0.f, 0.f, distance[2] };
}

[[nodiscard]] constexpr Vector3 G_ProjectSource2(const Vector3& point, const Vector3& distance, const Vector3& forward, const Vector3& right, const Vector3& up) {
	return point + (forward * distance[0]) + (right * distance[1]) + (up * distance[2]);
}

[[nodiscard]] inline Vector3 slerp(const Vector3& from, const Vector3& to, float t) {
	float dot = from.dot(to);
	float aFactor;
	float bFactor;
	if (std::fabs(dot) > 0.9995f) {
		aFactor = 1.0f - t;
		bFactor = t;
	}
	else {
		float ang = acos(dot);
		float sinOmega = sin(ang);
		float sinAOmega = sin((1.0f - t) * ang);
		float sinBOmega = sin(t * ang);
		aFactor = sinAOmega / sinOmega;
		bFactor = sinBOmega / sinOmega;
	}
	return from * aFactor + to * bFactor;
}

// Fmt support
template<>
struct fmt::formatter<Vector3> : fmt::formatter<float> {
	template<typename FormatContext>
	auto format(const Vector3& p, FormatContext& ctx) const -> decltype(ctx.out()) {
		auto out = fmt::formatter<float>::format(p.x, ctx);
		out = fmt::format_to(out, " ");
		ctx.advance_to(out);
		out = fmt::formatter<float>::format(p.y, ctx);
		out = fmt::format_to(out, " ");
		ctx.advance_to(out);
		return fmt::formatter<float>::format(p.z, ctx);
	}
};
