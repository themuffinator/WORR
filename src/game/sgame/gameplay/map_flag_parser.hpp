#pragma once

#include "../../bgame/string_compat.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace detail {
	struct MapFlagDefinition {
		const char* code;
		uint16_t bit;
	};

	static constexpr std::array<MapFlagDefinition, 10> kMapFlagDefinitions = { {
		{ "pu", static_cast<uint16_t>(1u << 0) },
		{ "pa", static_cast<uint16_t>(1u << 1) },
		{ "ar", static_cast<uint16_t>(1u << 2) },
		{ "am", static_cast<uint16_t>(1u << 3) },
		{ "ht", static_cast<uint16_t>(1u << 4) },
		{ "bfg", static_cast<uint16_t>(1u << 5) },
		{ "pb", static_cast<uint16_t>(1u << 6) },
		{ "fd", static_cast<uint16_t>(1u << 7) },
		{ "sd", static_cast<uint16_t>(1u << 8) },
		{ "ws", static_cast<uint16_t>(1u << 9) },
	} };
}

/*
=========================
ParseMyMapFlags

Parses MyMap override arguments (e.g. +pu, -fd) and fills the
corresponding enable/disable bitmasks. Returns false if an unknown
flag or malformed token is encountered.
=========================
*/
inline bool ParseMyMapFlags(const std::vector<std::string>& args, uint16_t& enableFlags, uint16_t& disableFlags)
{
	enableFlags = 0;
	disableFlags = 0;

	for (const std::string& arg : args) {
		if (arg.length() < 2 || (arg[0] != '+' && arg[0] != '-'))
			return false;

		const bool enable = (arg[0] == '+');
		const char* flag = arg.c_str() + 1;

		uint16_t bit = 0;
		for (const auto& def : detail::kMapFlagDefinitions) {
			if (_stricmp(flag, def.code) == 0) {
				bit = def.bit;
				break;
			}
		}
		if (bit == 0)
			return false;

		if (enable)
			enableFlags |= bit;
		else
			disableFlags |= bit;
	}

	return true;
}
