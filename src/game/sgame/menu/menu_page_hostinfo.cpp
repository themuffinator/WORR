/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_hostinfo.cpp (Menu Page - Host Info) This file implements the "Host Info" menu page,
which displays server-specific information to the player, such as the server name, the host's
name, and the Message of the Day (MOTD). Key Responsibilities: - Information Display: The
`OpenHostInfoMenu` function constructs a simple, read-only menu. - Data Fetching: It retrieves
data directly from relevant cvars (like `hostname`) and global game state (like `game.motd`) to
populate the menu entries. - User Navigation: Provides a "Return" option to navigate back to the
main join menu.*/

#include "../g_local.hpp"

extern void OpenJoinMenu(gentity_t* ent);

void OpenHostInfoMenu(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("Host Info", MenuAlign::Center)
		.spacer()
		.add("Server Name:", MenuAlign::Left)
		.add(hostname->string, MenuAlign::Left)
		.spacer();

	if (g_entities[1].client) {
		char value[MAX_INFO_VALUE] = { 0 };
		gi.Info_ValueForKey(g_entities[1].client->pers.userInfo, "name", value, sizeof(value));
		if (value[0]) {
			builder.add("Host:", MenuAlign::Left);
			builder.add(value, MenuAlign::Left);
		}
	}

	if (!game.motd.empty()) {
		builder.spacer().add("Message of the Day:", MenuAlign::Left);
		builder.add(game.motd, MenuAlign::Left);
	}

	builder.spacer().add("Return", MenuAlign::Left, [](gentity_t* ent, Menu&) {
		OpenJoinMenu(ent);
		});

	MenuSystem::Open(ent, builder.build());
}
