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

extern void OpenDmMatchInfoMenu(gentity_t *ent);

void OpenMatchInfoMenu(gentity_t *ent) {
	OpenDmMatchInfoMenu(ent);
}
