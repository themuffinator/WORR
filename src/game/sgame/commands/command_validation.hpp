#pragma once

#include <format>
#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>

namespace Commands {

/*
=============
ValidatePrintableASCII

Ensures user-facing command tokens only contain printable ASCII characters
so vote handlers can emit clear errors before parsing potentially unsafe
inputs.
=============
*/
inline bool ValidatePrintableASCII(std::string_view value, std::string_view fieldLabel, std::string& errorMessage) {
	if (value.empty()) {
	errorMessage = std::format("{} is required.", fieldLabel);
	return false;
}

	const auto isInvalid = [](unsigned char ch) {
		return ch < 0x20 || ch > 0x7E;
};

	if (std::ranges::any_of(value, isInvalid)) {
	errorMessage = std::format("{} must use printable ASCII characters.", fieldLabel);
	return false;
}

	errorMessage.clear();
	return true;
}

} // namespace Commands
