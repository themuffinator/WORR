/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_matchstats.cpp (Menu Page - Match Stats) This file implements the in-game menu page
for players to view their own performance statistics for the current match. Key
Responsibilities: - Stats Display: The `onUpdate` callback dynamically populates the menu with
the player's current stats. - Data Source: It reads data directly from the
`client_match_stats_t` struct associated with the player's client entity. - Real-Time Updates:
Because it uses the `onUpdate` callback, the stats displayed in the menu are updated live as the
match progresses whenever the menu is open.*/

#include "../g_local.hpp"

void OpenPlayerMatchStatsMenu(gentity_t* ent) {
	auto menu = std::make_unique<Menu>();

	for (int i = 0; i < 16; ++i)
		menu->entries.emplace_back("", MenuAlign::Left);

	menu->onUpdate = [](gentity_t* ent, const Menu& m) {
		if (!ent || !ent->client || !g_matchstats->integer) return;

		auto& menu = const_cast<Menu&>(m);
		auto& st = ent->client->pers.match;
		int i = 0;

		menu.entries[i++].text = "Player Stats for Match";

		char value[MAX_INFO_VALUE] = {};
		gi.Info_ValueForKey(g_entities[1].client->pers.userInfo, "name", value, sizeof(value));
		if (value[0]) menu.entries[i++].text = value;

		menu.entries[i++].text = "--------------------------";
		menu.entries[i++].text = fmt::format("kills: {}", st.totalKills);
		menu.entries[i++].text = fmt::format("deaths: {}", st.totalDeaths);
		if (st.totalDeaths > 0)
			menu.entries[i++].text = fmt::format("k/d ratio: {:.2f}", (float)st.totalKills / st.totalDeaths);
		else i++;
		menu.entries[i++].text = fmt::format("dmg dealt: {}", st.totalDmgDealt);
		menu.entries[i++].text = fmt::format("dmg received: {}", st.totalDmgReceived);
		if (st.totalDmgReceived > 0)
			menu.entries[i++].text = fmt::format("dmg ratio: {:.2f}", (float)st.totalDmgDealt / st.totalDmgReceived);
		else i++;
		menu.entries[i++].text = fmt::format("shots fired: {}", st.totalShots);
		menu.entries[i++].text = fmt::format("shots on target: {}", st.totalHits);
		if (st.totalShots > 0)
			menu.entries[i++].text = fmt::format("total accuracy: {}%", (int)((float)st.totalHits / st.totalShots * 100));
		};

	MenuSystem::Open(ent, std::move(menu));
}
