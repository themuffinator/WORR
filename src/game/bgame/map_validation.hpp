#pragma once

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

/*
=============
G_TrimNonEmpty

Returns a trimmed view of the input when non-empty after whitespace removal.
=============
*/
inline std::optional<std::string_view> G_TrimNonEmpty(std::string_view raw)
{
	const size_t start = raw.find_first_not_of(" \t\r\n");
	if (start == std::string_view::npos)
		return std::nullopt;

	const size_t end = raw.find_last_not_of(" \t\r\n");
	return raw.substr(start, end - start + 1);
}

/*
=============
G_ContainsTraversalTokens

Detects any relative path traversal patterns in the provided value.
=============
*/
inline bool G_ContainsTraversalTokens(std::string_view value)
{
	return value == "." || value == ".." || value.find("..") != std::string_view::npos;
}

/*
=============
G_ContainsPathSeparators

Identifies the presence of forward or backward path separators.
=============
*/
inline bool G_ContainsPathSeparators(std::string_view value)
{
	return value.find('/') != std::string_view::npos || value.find('\\') != std::string_view::npos;
}

/*
=============
G_IsAbsolutePathCandidate

Checks if the value appears to be an absolute path.
=============
*/
inline bool G_IsAbsolutePathCandidate(std::string_view value)
{
	return !value.empty() && (value.front() == '/' || value.front() == '\\');
}

/*
=============
G_ContainsDeviceSpecifier

Detects device specifier characters within the value.
=============
*/
inline bool G_ContainsDeviceSpecifier(std::string_view value)
{
	return value.find(':') != std::string_view::npos;
}

/*
=============
G_HasOnlyPoolCharacters

Ensures the value only contains allowed pool characters.
=============
*/
inline bool G_HasOnlyPoolCharacters(std::string_view value)
{
	for (char ch : value) {
		const unsigned char uch = static_cast<unsigned char>(ch);
		if (std::isalnum(uch))
			continue;
		switch (ch) {
		case '_':
		case '-':
			continue;
		default:
			return false;
}
}
	return true;
}

/*
=============
G_HasOnlyConfigCharacters

Ensures the value only contains allowed configuration filename characters.
=============
*/
inline bool G_HasOnlyConfigCharacters(std::string_view value)
{
	for (char ch : value) {
		const unsigned char uch = static_cast<unsigned char>(ch);
		if (std::isalnum(uch))
			continue;
		switch (ch) {
		case '_':
		case '-':
		case '.':
			continue;
		default:
			return false;
}
}
	return true;
}

/*
=============
G_IsValidMapIdentifier

Returns true when the provided map identifier only contains expected
characters and lacks any traversal tokens.
=============
*/
inline bool G_IsValidMapIdentifier(std::string_view mapName)
{
	if (mapName.empty())
			return false;

	if (mapName == "." || mapName == "..")
			return false;

	if (mapName.find("..") != std::string_view::npos)
			return false;

	for (char ch : mapName) {
		const unsigned char uch = static_cast<unsigned char>(ch);
		if (std::isalnum(uch))
			continue;
		switch (ch) {
		case '_':
		case '-':
			continue;
		default:
			return false;
		}
	}
	return true;
}

/*
=============
G_SanitizeMapPoolFilename

Trims whitespace and rejects any map entry filenames containing
path separators, traversal tokens, or other illegal characters.
=============
*/
inline bool G_SanitizeMapPoolFilename(std::string_view rawName, std::string& sanitized, std::string& rejectReason)
{
	sanitized.clear();

	const std::optional<std::string_view> trimmed = G_TrimNonEmpty(rawName);
	if (!trimmed.has_value()) {
		rejectReason = "is empty";
			return false;
}

	if (G_ContainsPathSeparators(*trimmed)) {
		rejectReason = "contains path separators";
			return false;
}

	if (G_ContainsTraversalTokens(*trimmed)) {
		rejectReason = "contains traversal tokens";
			return false;
}

	if (!G_HasOnlyPoolCharacters(*trimmed)) {
		rejectReason = "contains illegal characters";
			return false;
}

	sanitized.assign(trimmed->begin(), trimmed->end());
	rejectReason.clear();
	return true;
}
/*
=============
G_SanitizeMapConfigFilename

Trims whitespace and rejects map configuration filenames containing
absolute paths, traversal tokens, or disallowed characters. Permits
periods for extensions.
=============
*/
inline bool G_SanitizeMapConfigFilename(std::string_view rawName, std::string& sanitized, std::string& rejectReason)
{
	sanitized.clear();

	const std::optional<std::string_view> trimmed = G_TrimNonEmpty(rawName);
	if (!trimmed.has_value()) {
		rejectReason = "is empty";
			return false;
}

	if (G_IsAbsolutePathCandidate(*trimmed)) {
		rejectReason = "is an absolute path";
			return false;
}

	if (G_ContainsTraversalTokens(*trimmed)) {
		rejectReason = "contains traversal tokens";
			return false;
}

	if (G_ContainsPathSeparators(*trimmed)) {
		rejectReason = "contains path separators";
			return false;
}

	if (G_ContainsDeviceSpecifier(*trimmed)) {
		rejectReason = "contains a device specifier";
			return false;
}

	if (!G_HasOnlyConfigCharacters(*trimmed)) {
		rejectReason = "contains illegal characters";
			return false;
}

	sanitized.assign(trimmed->begin(), trimmed->end());
	rejectReason.clear();
	return true;
}

/*
=============
G_IsValidOverrideDirectory

Validates the override directory string to prevent traversal and ensure only
expected characters are present.
=============
*/
inline bool G_IsValidOverrideDirectory(std::string_view directory)
{
	if (directory.empty())
			return false;

	if (directory.front() == '/' || directory.front() == '\\')
			return false;

	if (directory.find("..") != std::string_view::npos)
			return false;

	if (directory.find('\\') != std::string_view::npos)
			return false;

	if (directory.find(':') != std::string_view::npos)
			return false;

	size_t pos = 0;
	while (pos < directory.size()) {
		const size_t next = directory.find('/', pos);
		std::string_view segment = directory.substr(pos, next - pos);

		if (segment.empty())
			return false;

		if (segment == "." || segment == "..")
			return false;

		for (char ch : segment) {
		const unsigned char uch = static_cast<unsigned char>(ch);
		if (std::isalnum(uch))
			continue;
		switch (ch) {
			case '_':
			case '-':
			continue;
		default:
			return false;
			}
		}

		if (next == std::string_view::npos)
			break;

		pos = next + 1;
	}
	return true;
}
