// Copyright (c) DarkMatter Projects 2023-2025
// Licensed under the GNU General Public License 2.0.
//
// command_registration.hpp - Shared header for command modules.
// Declares the global RegisterCommand function, allowing individual
// command modules to register their functions without needing direct
// access to the central command map.

#pragma once

#include "command_system.hpp"

// Declare the global registration function that all modules will use.
void RegisterCommand(
	std::string_view name,
	void (*function)(gentity_t*, const CommandArgs&),
	BitFlags<CommandFlag> flags = CommandFlag::None,
	bool floodExempt = false
);

// Declare common helper functions shared across command modules.
namespace Commands {
	void PrintUsage(gentity_t* ent, const CommandArgs& args, std::string_view required_params, std::string_view optional_params, std::string_view help_text);
	const char* ResolveSocialID(const char* rawArg, gentity_t*& foundClient);
	bool TeamSkillShuffle();

	// Helper exposed for UI/menu systems to check vote availability.
	bool IsVoteCommandEnabled(std::string_view name);

	// Helpers exposed for gameplay systems to trigger core client commands.
	void Help(gentity_t* ent, const CommandArgs& args);
	void Score(gentity_t* ent, const CommandArgs& args);
	bool CheckMyMapAllowed(gentity_t* ent);
	bool QueueMyMapRequest(gentity_t* ent, std::string_view mapName, const std::vector<std::string>& flagArgs);
}
