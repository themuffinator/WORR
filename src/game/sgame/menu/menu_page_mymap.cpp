/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_mymap.cpp (Menu Page - MyMap) This file implements the UI for selecting a personal
MyMap queue entry along with optional rule override flags.*/

#include "../g_local.hpp"
#include "../commands/command_registration.hpp"
#include <array>
#include <memory>
#include <string>
#include <vector>

extern void OpenJoinMenu(gentity_t* ent);

namespace {

struct MyMapMenuState {
	uint16_t enableFlags = 0;
	uint16_t disableFlags = 0;
};

struct MapFlagEntry {
	uint16_t bit;
	const char* code;
	const char* label;
};

static constexpr std::array<MapFlagEntry, 10> kMyMapFlags = { {
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

void MyMapFlags_Clear(MyMapMenuState& state) {
	state.enableFlags = 0;
	state.disableFlags = 0;
}

void MyMapFlags_ToggleTri(MyMapMenuState& state, uint16_t mask) {
	const bool en = (state.enableFlags & mask) != 0;
	const bool dis = (state.disableFlags & mask) != 0;

	if (!en && !dis) {
		state.enableFlags |= mask;
	}
	else if (en) {
		state.enableFlags &= ~mask;
		state.disableFlags |= mask;
	}
	else {
		state.disableFlags &= ~mask;
	}
}

std::string MyMapFlags_Summary(const MyMapMenuState& state) {
	std::string out;
	for (const auto& f : kMyMapFlags) {
		const bool en = (state.enableFlags & f.bit) != 0;
		const bool dis = (state.disableFlags & f.bit) != 0;
		if (en) { out += "+"; out += f.code; out += " "; }
		if (dis) { out += "-"; out += f.code; out += " "; }
	}
	if (out.empty()) out = "Default";
	else if (out.back() == ' ') out.pop_back();
	return out;
}

std::vector<std::string> MyMapFlagArgs(const MyMapMenuState& state) {
	std::vector<std::string> args;
	for (const auto& f : kMyMapFlags) {
		if (state.enableFlags & f.bit) {
			args.emplace_back(std::string("+") + f.code);
		}
		if (state.disableFlags & f.bit) {
			args.emplace_back(std::string("-") + f.code);
		}
	}
	return args;
}

void OpenMyMapFlagsMenu(gentity_t* ent, const std::shared_ptr<MyMapMenuState>& state);

void OpenMyMapMenuInternal(gentity_t* ent, const std::shared_ptr<MyMapMenuState>& state) {
	if (!ent || !ent->client) {
		return;
	}

	MenuBuilder builder;
	builder.addFixed("MyMap", MenuAlign::Center).addFixed("", MenuAlign::Left);

	builder.addFixed("Flags: " + MyMapFlags_Summary(*state), MenuAlign::Left, [state](gentity_t* e, Menu&) {
		OpenMyMapFlagsMenu(e, state);
		});

	builder.addFixed("Clear Flags", MenuAlign::Left, [state](gentity_t* e, Menu&) {
		MyMapFlags_Clear(*state);
		OpenMyMapMenuInternal(e, state);
		});

	builder.addFixed("");

	if (game.mapSystem.mapPool.empty()) {
		builder.add("No maps available", MenuAlign::Left);
	}
	else {
		for (const auto& entry : game.mapSystem.mapPool) {
			const std::string displayName = entry.longName.empty() ? entry.filename : entry.longName;
			builder.add(displayName, MenuAlign::Left, [state, mapname = entry.filename](gentity_t* e, Menu&) {
				const std::vector<std::string> flags = MyMapFlagArgs(*state);
				if (Commands::CheckMyMapAllowed(e) && Commands::QueueMyMapRequest(e, mapname, flags)) {
					MenuSystem::Close(e);
				}
				});
		}
	}

	builder.addFixed("").addFixed("Return", MenuAlign::Left, [](gentity_t* e, Menu&) {
		OpenJoinMenu(e);
		});

	MenuSystem::Open(ent, builder.build());
}

void OpenMyMapFlagsMenu(gentity_t* ent, const std::shared_ptr<MyMapMenuState>& state) {
	MenuBuilder builder;
	builder.add("MyMap Flags", MenuAlign::Center).spacer();

	for (const auto& f : kMyMapFlags) {
		const bool en = (state->enableFlags & f.bit) != 0;
		const bool dis = (state->disableFlags & f.bit) != 0;
		const char* flagState = (!en && !dis) ? "Default" : (en ? "Enabled" : "Disabled");
		builder.add(std::string(f.label) + " [" + flagState + "]", MenuAlign::Left, [state, mask = f.bit](gentity_t* e, Menu&) {
			MyMapFlags_ToggleTri(*state, mask);
			OpenMyMapFlagsMenu(e, state);
			});
	}

	builder.spacer().add("Back", MenuAlign::Left, [state](gentity_t* e, Menu&) {
		OpenMyMapMenuInternal(e, state);
		});

	MenuSystem::Open(ent, builder.build());
}

} // namespace

void OpenMyMapMenu(gentity_t* ent) {
	if (!ent || !ent->client) {
		return;
	}

	auto state = std::make_shared<MyMapMenuState>();
	OpenMyMapMenuInternal(ent, state);
}
