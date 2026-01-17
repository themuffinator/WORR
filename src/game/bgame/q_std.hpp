// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

// q_std.hpp (Quake Standard Library Header)
// This header file provides a set of standard library-like functions and
// modern C++ utilities for the game module. It is designed to replace or
// supplement C-style idioms with safer, more expressive C++ alternatives.
//
// Key Responsibilities:
// - String Formatting: Defines the `G_Fmt` and `G_FmtTo` template functions,
//   which are safe, high-performance wrappers around the {fmt} library for
//   type-safe string formatting.
// - Math Utilities: Declares math constants (M_PI) and utility functions like
//   `LerpAngle` and `anglemod`.
// - Vector Math: Includes `q_vec3.hpp` to provide the `Vector3` class.
// - C-Style String Parsing: Declares `COM_Parse` and `COM_ParseEx`, the legacy
//   functions for tokenizing strings.
// - Type-Safe Macros and Templates: Provides templates like `std::clamp` and
//   `bit_v` for safe, compile-time operations.

#pragma once

// q_std.h -- 'standard' library stuff for game module
// not meant to be included by engine, etc

#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cinttypes>
#include <ctime>
#include <type_traits>
#include <algorithm>
#include <array>
#include <string_view>
#include <string>
#include <numeric>
#include <functional>

#include <format>
namespace fmt = std;
#define FMT_STRING(s) s

struct g_fmt_data_t {
	std::array<std::array<char, 4096>, 2> string{};
	int  istr;
};

// static data for fmt; internal, do not touch
extern g_fmt_data_t g_fmt_data;

// like fmt::format_to_n, but automatically null terminates the output;
// returns the length of the string written (up to N)
#define G_FmtTo_ G_FmtTo

/*
=============
G_FmtTo

Formats text into the provided character buffer and guarantees null termination.
=============
*/
template<size_t N, typename... Args>
inline size_t G_FmtTo(char(&buffer)[N], std::format_string<Args...> format_str, Args &&... args)
{
	auto end = fmt::format_to_n(buffer, N - 1, format_str, std::forward<Args>(args)...);

	*(end.out) = '\0';

	return end.out - buffer;
}

/*
=============
G_FmtTo

Formats text into the provided std::array buffer and guarantees null termination.
=============
*/
template<size_t N, typename... Args>
inline size_t G_FmtTo(std::array<char, N>& buffer, std::format_string<Args...> format_str, Args &&... args)
{
	auto end = fmt::format_to_n(buffer.data(), N - 1, format_str, std::forward<Args>(args)...);

	*(end.out) = '\0';

	return static_cast<size_t>(end.out - buffer.data());
}

// format to temp buffers; doesn't use heap allocation
// unlike `fmt::format` does directly
/*
=============
G_Fmt

Formats into an internal rotating buffer and returns a view of the contents.
=============
*/
template<typename... Args>
[[nodiscard]] inline std::string_view G_Fmt(std::format_string<Args...> format_str, Args &&... args)
{
	g_fmt_data.istr ^= 1;

	size_t len = G_FmtTo_(g_fmt_data.string[g_fmt_data.istr], format_str, std::forward<Args>(args)...);

	return std::string_view(g_fmt_data.string[g_fmt_data.istr].data(), len);
}

// fmt::join replacement
template<typename T>
std::string join_strings(const T& cont, const char* separator) {
	if (cont.empty())
		return "";

	auto it = cont.begin();
	std::string result = *it;
	++it;

	for (; it != cont.end(); ++it) {
		result += separator;
		result += *it;
	}

	return result;
}

using byte = uint8_t;

// note: only works on actual arrays
#define q_countof(a) std::extent_v<decltype(a)>

using std::max;
using std::min;
using std::clamp;

template<typename T>
constexpr T lerp(T from, T to, float t) {
	return (to * t) + (from * (1.f - t));
}

// angle indexes
enum { PITCH, YAW, ROLL };

// coordinate indexes
enum { _X, _Y, _Z };

/*
==============================================================

MATHLIB

==============================================================
*/

constexpr double PI = 3.14159265358979323846; // matches value in gcc v2 math.h
constexpr float PIf = static_cast<float>(PI);

[[nodiscard]] constexpr float RAD2DEG(float x) {
	return (x * 180.0f / PIf);
}

[[nodiscard]] constexpr float DEG2RAD(float x) {
	return (x * PIf / 180.0f);
}

/*
=============
G_AddBlend
Adds a new blend color to the existing blend.
=============
*/
inline void G_AddBlend(float r, float g, float b, float a, std::array<float, 4>& v_blend) {
	if (a <= 0.0f || a > 1.0f)
		return;

	const float existingAlpha = v_blend[3];

	if (existingAlpha >= 1.0f)
		return;

	const float combinedAlpha = existingAlpha + (1.0f - existingAlpha) * a;

	// Clamp to avoid divide-by-zero or unstable blend math
	if (combinedAlpha <= 0.0001f)
		return;

	const float blendFactor = existingAlpha / combinedAlpha;

	v_blend[0] = v_blend[0] * blendFactor + r * (1.0f - blendFactor);
	v_blend[1] = v_blend[1] * blendFactor + g * (1.0f - blendFactor);
	v_blend[2] = v_blend[2] * blendFactor + b * (1.0f - blendFactor);
	v_blend[3] = combinedAlpha;
}

//============================================================================

/*
===============
LerpAngle

===============
*/
[[nodiscard]] constexpr float LerpAngle(float a2, float a1, float frac) {
	if (a1 - a2 > 180)
		a1 -= 360;
	if (a1 - a2 < -180)
		a1 += 360;
	return a2 + frac * (a1 - a2);
}

[[nodiscard]] inline float anglemod(float a) {
	float v = fmod(a, 360.0f);

	if (v < 0)
		return 360.f + v;

	return v;
}

#include "q_vec3.hpp"

//=============================================

char* COM_ParseEx(const char** data_p, const char* seps, char* buffer = nullptr, size_t buffer_size = 0, bool* truncated = nullptr);

// data is an in/out parm, returns a parsed out token
inline char* COM_Parse(const char** data_p, char* buffer = nullptr, size_t buffer_size = 0, bool* truncated = nullptr) {
	return COM_ParseEx(data_p, "\r\n\t ", buffer, buffer_size, truncated);
}

//=============================================

// portable case insensitive compare
[[nodiscard]] int Q_strcasecmp(const char* s1, const char* s2);
[[nodiscard]] int Q_strncasecmp(const char* s1, const char* s2, size_t n);

// BSD string utils - haleyjd
size_t Q_strlcpy(char* dst, const char* src, size_t siz);
size_t Q_strlcat(char* dst, const char* src, size_t siz);

// EOF
