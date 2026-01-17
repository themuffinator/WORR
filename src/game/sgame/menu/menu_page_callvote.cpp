/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_callvote.cpp (Menu Page - Call Vote) This file contains the UI logic for the "Call a
Vote" menu. It provides a structured way for players to initiate votes for various game actions,
such as changing the map or shuffling teams. Key Responsibilities: - Vote Menu Construction:
Builds the main vote menu, dynamically showing only the vote options that are currently enabled
by the server's `g_vote_flags` cvar. - Sub-Menus for Options: Implements sub-menus for votes
that require additional parameters, such as the map selection list (`OpenCallvoteMapMenu`) or
the timelimit chooser. - Parameter Handling: Manages the state for complex votes, like storing
the selected map and custom map flags before initiating the vote. - Integration with Vote
System: The `onSelect` callbacks for each menu item call the core `Commands::TryLaunchVote`
helper to actually begin the voting process.*/

#include "../commands/commands.hpp"
#include "../g_local.hpp"
#include <string>
#include <string_view>
#include <array>
#include <vector>

extern void OpenJoinMenu(gentity_t* ent);

/*
===============
Helpers
===============
*/

static inline bool VoteEnabled(std::string_view name) {
	if (!g_allowVoting || !g_allowVoting->integer) {
		return false;
	}
	for (const auto& def : Commands::GetRegisteredVoteDefinitions()) {
		if (def.name == name) {
			if (!def.visibleInMenu) {
				return false;
			}
			return (g_vote_flags->integer & def.flag) != 0;
		}
	}
	return false;
}

static void NotifyVoteLaunch(gentity_t* ent, const Commands::VoteLaunchResult& result) {
	if (!result.success && !result.message.empty()) {
		std::string message = result.message;
		if (message.back() != '\n') {
			message.append("\n");
		}
		gi.Client_Print(ent, PRINT_HIGH, message.c_str());
	}
}

static bool TryLaunchMenuVote(gentity_t* ent, std::string_view voteName, std::string_view voteArg) {
	Commands::VoteLaunchResult outcome = Commands::TryLaunchVote(ent, voteName, voteArg);
	if (outcome.success) {
		MenuSystem::Close(ent);
		return true;
	}

	NotifyVoteLaunch(ent, outcome);
	return false;
}

/*
===============
Map flags state
===============
*/

struct MapVoteState {
	uint16_t enableFlags = 0; // bitset matching MyMapOverride mini-flags
	uint16_t disableFlags = 0;
};
static MapVoteState g_mapVote; // maintained while inside callvote menu

struct MapFlagEntry {
	uint16_t bit;
	const char* code;      // short token used by callvote map parser (+code / -code)
	const char* label;     // readable label in menu
};

static constexpr std::array<MapFlagEntry, 10> kMapFlags = { {
	{ MAPFLAG_PU, "pu", "Powerups" },
	{ MAPFLAG_PA, "pa", "Power Armor" },
	{ MAPFLAG_AR, "ar", "Armor" },
	{ MAPFLAG_AM, "am", "Ammo" },
	{ MAPFLAG_HT, "ht", "Health" },
	{ MAPFLAG_BFG, "bfg", "BFG10K" },
	{ MAPFLAG_PB, "pb", "Plasma Beam" },
	{ MAPFLAG_FD, "fd", "Falling Damage" },
	{ MAPFLAG_SD, "sd", "Self Damage" },
	{ MAPFLAG_WS, "ws", "Weapons Stay" },
} };

static inline void MapFlags_Clear() {
	g_mapVote.enableFlags = 0;
	g_mapVote.disableFlags = 0;
}

static inline void MapFlags_ToggleTri(uint16_t mask) {
	// default -> enable -> disable -> default
	const bool en = (g_mapVote.enableFlags & mask) != 0;
	const bool dis = (g_mapVote.disableFlags & mask) != 0;

	if (!en && !dis) {
		g_mapVote.enableFlags |= mask;
	}
	else if (en) {
		g_mapVote.enableFlags &= ~mask;
		g_mapVote.disableFlags |= mask;
	}
	else {
		g_mapVote.disableFlags &= ~mask;
	}
}

static std::string MapFlags_Summary() {
	std::string out;
	for (const auto& f : kMapFlags) {
		const bool en = (g_mapVote.enableFlags & f.bit) != 0;
		const bool dis = (g_mapVote.disableFlags & f.bit) != 0;
		if (en) { out += "+"; out += f.code; out += " "; }
		if (dis) { out += "-"; out += f.code; out += " "; }
	}
	if (out.empty()) out = "Default";
	else if (out.back() == ' ') out.pop_back();
	return out;
}

static std::string BuildMapVoteArg(const std::string& mapname) {
	std::string arg = mapname;
	for (const auto& f : kMapFlags) {
		const bool en = (g_mapVote.enableFlags & f.bit) != 0;
		const bool dis = (g_mapVote.disableFlags & f.bit) != 0;
		if (en) { arg += " +"; arg += f.code; }
		if (dis) { arg += " -"; arg += f.code; }
	}
	return arg;
}

/*
===============
Return helper
===============
*/
static inline void AddReturnToCallvoteMenu(MenuBuilder& builder) {
	builder.addFixed("", MenuAlign::Left).addFixed("Return", MenuAlign::Left, [](gentity_t* e, Menu&) {
		OpenCallvoteMenu(e);
	});
}

/*
===============
Flags menu
===============
*/
static void OpenCallvoteMapFlags(gentity_t* ent);

/*
===============
OpenCallvoteMap
===============
*/
static void OpenCallvoteMap(gentity_t* ent) {
	MenuBuilder builder;
	builder.addFixed("Callvote: Map", MenuAlign::Center).addFixed("", MenuAlign::Left);

	// Flags summary + editor
	builder.addFixed("Flags: " + MapFlags_Summary(), MenuAlign::Left, [](gentity_t* e, Menu&) {
		OpenCallvoteMapFlags(e);
	});

	builder.addFixed("Clear Flags", MenuAlign::Left, [](gentity_t* e, Menu&) {
		MapFlags_Clear();
		OpenCallvoteMap(e);
	});

	builder.addFixed("");

	for (const auto& entry : game.mapSystem.mapPool) {
		const std::string& displayName = entry.longName.empty() ? entry.filename : entry.longName;

		builder.add(displayName, MenuAlign::Left, [mapname = entry.filename](gentity_t* e, Menu&) {
			const std::string fullArg = BuildMapVoteArg(mapname);
			TryLaunchMenuVote(e, "map", fullArg);
		});
	}

	AddReturnToCallvoteMenu(builder);
	MenuSystem::Open(ent, builder.build());
}
/*
===============
OpenCallvoteMapFlags
===============
*/
static void OpenCallvoteMapFlags(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("Map Flags", MenuAlign::Center).spacer();

	for (const auto& f : kMapFlags) {
		const bool en = (g_mapVote.enableFlags & f.bit) != 0;
		const bool dis = (g_mapVote.disableFlags & f.bit) != 0;
		const char* state = (!en && !dis) ? "Default" : (en ? "Enabled" : "Disabled");
		builder.add(std::string(f.label) + " [" + state + "]", MenuAlign::Left, [mask = f.bit](gentity_t* e, Menu&) {
			MapFlags_ToggleTri(mask);
			OpenCallvoteMapFlags(e);
			});
	}

	builder.spacer().add("Back", MenuAlign::Left, [](gentity_t* e, Menu&) {
		OpenCallvoteMap(e);
		});

	MenuSystem::Open(ent, builder.build());
}

/*
===============
OpenCallvoteGametype
===============
*/
static void OpenCallvoteGametype(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("Callvote: Gametype", MenuAlign::Center).spacer();

	// Iterate through the modern GAME_MODES array, which is more maintainable.
	for (const auto& mode : GAME_MODES) {
		if (mode.type == GameType::None)
			continue;
		// Capture the short name by value to ensure its lifetime for the lambda.
		// We convert the string_view to a string for safe capture.
		std::string shortName = std::string(mode.short_name);

		// Add an entry with the gametype's full name. The action calls the vote.
		builder.add(std::string(mode.long_name), MenuAlign::Left,
			[shortName](gentity_t* e, Menu&) {
				TryLaunchMenuVote(e, "gametype", shortName);
			});
	}

	AddReturnToCallvoteMenu(builder);
	MenuSystem::Open(ent, builder.build());
}

/*
=================
OpenCallvoteRuleset
=================
*/
static void OpenCallvoteRuleset(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("Callvote: Ruleset", MenuAlign::Center).spacer();

	for (size_t i = 1; i < Ruleset::Count(); ++i) {
		std::string_view shortName = rs_short_name[i][0];
		const char* longName = rs_long_name[i];

		builder.add(longName, MenuAlign::Left, [shortName](gentity_t* e, Menu&) {
			TryLaunchMenuVote(e, "ruleset", shortName);
			});
	}

	AddReturnToCallvoteMenu(builder);
	MenuSystem::Open(ent, builder.build());
}

/*
===============
OpenCallvoteTimelimit
===============
*/
static void OpenCallvoteTimelimit(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("Callvote: Timelimit", MenuAlign::Center).spacer();

	// Show current
	const int cur = timeLimit ? timeLimit->integer : 0;
	builder.add(G_Fmt("Current: {}", cur ? TimeString(cur * 60000, false, false) : "Disabled").data(), MenuAlign::Left);

	// Disable
	builder.add("Disable", MenuAlign::Left, [](gentity_t* e, Menu&) {
		TryLaunchMenuVote(e, "timelimit", "0");
		});

	// Common presets (minutes)
	static const int kTimes[] = { 5, 10, 15, 20, 30, 45, 60, 90, 120 };
	for (int m : kTimes) {
		builder.add(G_Fmt("Set {} {}", m, m == 1 ? "minute" : "minutes").data(), MenuAlign::Left, [m](gentity_t* e, Menu&) {
			TryLaunchMenuVote(e, "timelimit", std::to_string(m));
			});
	}

	AddReturnToCallvoteMenu(builder);
	MenuSystem::Open(ent, builder.build());
}

/*
===============
OpenCallvoteScorelimit
===============
*/
static void OpenCallvoteScorelimit(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("Callvote: Scorelimit", MenuAlign::Center).spacer();

	// Label depends on gametype. Use helpers already present server-side.
	const char* metric = GT_ScoreLimitString(); // e.g. "frag" or "capture"
	const int cur = GT_ScoreLimit();

	builder.add(G_Fmt("Current: {} {}", cur ? cur : 0, cur ? metric : "(Disabled)").data(), MenuAlign::Left);

	// Disable
	builder.add("Disable", MenuAlign::Left, [metric](gentity_t* e, Menu&) {
		(void)metric;
		TryLaunchMenuVote(e, "scorelimit", "0");
		});

	// Presets
	static const int kScores[] = { 5, 10, 15, 20, 25, 30, 50, 100 };
	for (int s : kScores) {
		builder.add(G_Fmt("Set {} {}", s, metric).data(), MenuAlign::Left, [s](gentity_t* e, Menu&) {
			TryLaunchMenuVote(e, "scorelimit", std::to_string(s));
			});
	}

	AddReturnToCallvoteMenu(builder);
	MenuSystem::Open(ent, builder.build());
}

/*
===============
OpenCallvoteUnlagged
===============
*/
static void OpenCallvoteUnlagged(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("Callvote: Unlagged", MenuAlign::Center).spacer();

	const bool cur = g_lagCompensation && g_lagCompensation->integer;
	builder.add(G_Fmt("Current: {}", cur ? "ENABLED" : "DISABLED").data(), MenuAlign::Left);

	builder.add("Enable", MenuAlign::Left, [](gentity_t* e, Menu&) {
		TryLaunchMenuVote(e, "unlagged", "1");
		});

	builder.add("Disable", MenuAlign::Left, [](gentity_t* e, Menu&) {
		TryLaunchMenuVote(e, "unlagged", "0");
		});

	AddReturnToCallvoteMenu(builder);
	MenuSystem::Open(ent, builder.build());
}

/*
===============
OpenCallvoteRandom
===============
*/
static void OpenCallvoteRandom(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("Callvote: Random", MenuAlign::Center).spacer();

	constexpr int kMin = 2;
	constexpr int kMax = 100;

	for (int i = kMin; i <= kMax; i += 5) {
		builder.add(G_Fmt("1-{}", i).data(), MenuAlign::Left, [i](gentity_t* e, Menu&) {
			TryLaunchMenuVote(e, "random", std::to_string(i));
			});
	}

	AddReturnToCallvoteMenu(builder);
	MenuSystem::Open(ent, builder.build());
}

/*
===============
OpenCallvoteArena
===============
*/
static void OpenCallvoteArena(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("Callvote: Arena", MenuAlign::Center).spacer();

	int optionsAdded = 0;
	for (int i = 0; i < level.arenaTotal; ++i) {
		int arenaNum = i + 1;
		if (arenaNum == level.arenaActive)
			continue;

		builder.add(G_Fmt("Arena {}", arenaNum).data(), MenuAlign::Left, [arenaNum](gentity_t* e, Menu&) {
			TryLaunchMenuVote(e, "arena", std::to_string(arenaNum));
			});
		++optionsAdded;
	}

	if (optionsAdded == 0)
		builder.add("No other arenas available", MenuAlign::Left);

	AddReturnToCallvoteMenu(builder);
	MenuSystem::Open(ent, builder.build());
}

/*
===============
OpenSimpleCallvote
===============
*/
static void OpenSimpleCallvote(const std::string& voteName, gentity_t* ent) {
	TryLaunchMenuVote(ent, voteName, "");
}

/*
===============
OpenCallvoteMenu
===============
*/
void OpenCallvoteMenu(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("Call a Vote", MenuAlign::Center).spacer();

	// Reset map flags when first entering this root
	MapFlags_Clear();

	// Map (with flags)
	if (VoteEnabled("map")) {
		builder.add("Map", MenuAlign::Left, [](gentity_t* e, Menu&) {
			OpenCallvoteMap(e);
			});
	}

	// Next Map
	if (VoteEnabled("nextmap")) {
		builder.add("Next Map", MenuAlign::Left, [](gentity_t* e, Menu&) {
			OpenSimpleCallvote("nextmap", e);
			});
	}

	// Restart
	if (VoteEnabled("restart")) {
		builder.add("Restart Match", MenuAlign::Left, [](gentity_t* e, Menu&) {
			OpenSimpleCallvote("restart", e);
			});
	}

	// Gametype
	if (VoteEnabled("gametype")) {
		builder.add("Gametype", MenuAlign::Left, [](gentity_t* e, Menu&) {
			OpenCallvoteGametype(e);
			});
	}

	// Ruleset
	if (VoteEnabled("ruleset")) {
		builder.add("Ruleset", MenuAlign::Left, [](gentity_t* e, Menu&) {
			OpenCallvoteRuleset(e);
			});
	}

	// Timelimit
	if (VoteEnabled("timelimit")) {
		builder.add("Timelimit", MenuAlign::Left, [](gentity_t* e, Menu&) {
			OpenCallvoteTimelimit(e);
			});
	}

	// Scorelimit
	if (VoteEnabled("scorelimit")) {
		builder.add("Scorelimit", MenuAlign::Left, [](gentity_t* e, Menu&) {
			OpenCallvoteScorelimit(e);
			});
	}

	// Team things only if teams exist
	if (Teams()) {
		if (VoteEnabled("shuffle")) {
			builder.add("Shuffle Teams", MenuAlign::Left, [](gentity_t* e, Menu&) {
				OpenSimpleCallvote("shuffle", e);
				});
		}
		if (VoteEnabled("balance")) {
			builder.add("Balance Teams", MenuAlign::Left, [](gentity_t* e, Menu&) {
				OpenSimpleCallvote("balance", e);
				});
		}
	}

	// Unlagged
	if (VoteEnabled("unlagged")) {
		builder.add("Unlagged", MenuAlign::Left, [](gentity_t* e, Menu&) {
			OpenCallvoteUnlagged(e);
			});
	}

	// Cointoss
	if (VoteEnabled("cointoss")) {
		builder.add("Cointoss", MenuAlign::Left, [](gentity_t* e, Menu&) {
			OpenSimpleCallvote("cointoss", e);
			});
	}

	// Random
	if (VoteEnabled("random")) {
		builder.add("Random Number", MenuAlign::Left, [](gentity_t* e, Menu&) {
			OpenCallvoteRandom(e);
			});
	}

	// Arena page (only if RA2 and vote enabled)
	if (level.arenaTotal && VoteEnabled("arena")) {
		builder.add("Arena", MenuAlign::Left, [](gentity_t* e, Menu&) {
			OpenCallvoteArena(e);
			});
	}

	builder.spacer().add("Return", MenuAlign::Left, [](gentity_t* e, Menu&) {
		OpenJoinMenu(e);
		});

	MenuSystem::Open(ent, builder.build());
}
