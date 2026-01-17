/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_matchinfo.cpp (Menu Page - Match Info) This file implements the "Match Info" menu
page, which provides players with a summary of the current match's settings and rules. Key
Responsibilities: - Information Display: `OpenMatchInfoMenu` builds a read-only menu that
displays key details about the ongoing match. - Data Fetching: It gathers information from
various sources, including the level locals (map name, author), game state (gametype, ruleset),
and cvars (timelimit, scorelimit) to populate the menu. - Rule Summary: It can be extended to
show a detailed summary of active game mutators and server settings (e.g., "InstaGib", "Weapons
Stay").*/

#include "../g_local.hpp"

extern void OpenJoinMenu(gentity_t* ent);

void OpenMatchInfoMenu(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("Match Info", MenuAlign::Center)
		.spacer()
		.add(level.gametype_name.data(), MenuAlign::Left)
		.add(fmt::format("map: {}", level.longName.data()), MenuAlign::Left)
		.add(fmt::format("mapname: {}", level.mapName.data()), MenuAlign::Left);

	if (level.author[0])
		builder.add(fmt::format("author: {}", level.author), MenuAlign::Left);
	if (level.author2[0])
		builder.add(fmt::format("      {}", level.author2), MenuAlign::Left);

	builder.add(fmt::format("ruleset: {}", rs_long_name[(int)game.ruleset]), MenuAlign::Left);

	if (GT_ScoreLimit())
		builder.add(fmt::format("{} limit: {}", GT_ScoreLimitString(), GT_ScoreLimit()), MenuAlign::Left);

	if (timeLimit->value > 0)
		builder.add(fmt::format("time limit: {}", TimeString(timeLimit->value * 60000, false, false)), MenuAlign::Left);

	builder.spacer().add("Return", MenuAlign::Left, [](gentity_t* ent, Menu&) {
		OpenJoinMenu(ent);
		});

	MenuSystem::Open(ent, builder.build());
}
