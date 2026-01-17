#pragma once

#include <string>

#include "../g_local.hpp"
#include "json/json.h"
#include "../../bgame/version.hpp"

inline constexpr size_t SAVE_FORMAT_VERSION = 1;

/*
=============
WriteSaveMetadata

Populates metadata describing the save format and engine version.
=============
*/
inline void WriteSaveMetadata(Json::Value& json)
{
	json["save_version"] = SAVE_FORMAT_VERSION;
	json["engine_version"] = std::string(worr::version::kGameVersion);
}

/*
=============
ValidateSaveMetadata

Verifies the save uses a supported format and engine version.
=============
*/
inline bool ValidateSaveMetadata(const Json::Value& json, const char* context)
{
	bool valid = true;
	const bool strict = g_strict_saves && g_strict_saves->integer;
	const std::string expectedVersion = std::string(worr::version::kGameVersion);

	auto log_message = [&](const std::string& message) {
		if (strict)
			gi.Com_ErrorFmt("{} save: {}", context, message);
		else
			gi.Com_PrintFmt("{} save: {}\n", context, message);
	};

	const Json::Value& saveVersion = json["save_version"];
	if (!saveVersion.isUInt()) {
		log_message("missing or invalid save_version");
		valid = false;
	}
	else if (saveVersion.asUInt() != SAVE_FORMAT_VERSION) {
		log_message(std::string("expected save version ") + std::to_string(SAVE_FORMAT_VERSION) + " but found " + saveVersion.asString());
		valid = false;
	}

	const Json::Value& engineVersion = json["engine_version"];
	if (!engineVersion.isString()) {
		log_message("missing or invalid engine_version");
		valid = false;
	}
	else if (engineVersion.asString() != expectedVersion) {
		log_message("expected engine version " + expectedVersion + " but found " + engineVersion.asString());
		valid = false;
	}

	return valid;
}
