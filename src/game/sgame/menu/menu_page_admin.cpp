/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_admin.cpp (Menu Page - Admin) now acts as a navigation hub for
administrative tooling. It reminds admins that match tuning is handled through
the match setup wizard, offers an explicit reset back into that wizard, and
provides access to the new command reference page.
*/

#include "../g_local.hpp"

void OpenAdminSettingsMenu(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("*Admin Menu*", MenuAlign::Center);
	builder.spacer();
	builder.add("Match Setup owns game settings.", MenuAlign::Left);
	builder.add("Use Reset State to rerun.", MenuAlign::Left);
	builder.spacer();
	builder.add("Reset State", MenuAlign::Left,
			 [](gentity_t* e, Menu&) { OpenSetupWelcomeMenu(e); });
	if (Tournament_IsActive()) {
		builder.add("Replay Game", MenuAlign::Left,
				 [](gentity_t* e, Menu&) { OpenTournamentReplayMenu(e); });
	}
	builder.add("Admin Commands", MenuAlign::Left,
			 [](gentity_t* e, Menu&) { OpenAdminCommandsMenu(e); });
	builder.spacer().spacer();
	builder.add("Return", MenuAlign::Left, [](gentity_t* e, Menu&) {
		OpenJoinMenu(e);
	});
	MenuSystem::Open(ent, builder.build());
}
