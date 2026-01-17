/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_admin_commands.cpp (Menu Page - Admin Commands) exposes a curated
reference for every admin-only console verb. It keeps the game settings work
inside the match setup wizard, but surfaces concise usages so server operators
have the full command catalog at their fingertips.
*/

#include <array>

#include "../g_local.hpp"

namespace {

struct AdminCommandInfo {
	const char* name;
	const char* summary;
	const char* usageLine1;
	const char* usageLine2;
};

constexpr std::array<AdminCommandInfo, 28> kAdminCommands = {{
	{"add_admin", "Adds player to admin.txt", "usage: add_admin <client>", nullptr},
	{"add_ban", "Bans player by social ID", "usage: add_ban <client>", nullptr},
	{"arena", "Force specific arena", "usage: arena <num>", nullptr},
	{"balance", "Force teams to balance", "usage: balance", nullptr},
	{"boot", "Kick player; host/admin safe", "usage: boot <client>", nullptr},
	{"end_match", "End current match now", "usage: end_match", nullptr},
	{"force_vote", "Force pending vote result", "usage: force_vote <y|n>", nullptr},
	{"gametype", "Set current gametype", "usage: gametype <name>", nullptr},
	{"load_admins", "Reload admin.txt", "usage: load_admins", nullptr},
	{"load_bans", "Reload ban list", "usage: load_bans", nullptr},
	{"load_motd", "Reload MOTD file", "usage: load_motd", nullptr},
	{"load_mappool", "Reload map pool + cycle", "usage: load_mappool", nullptr},
	{"load_mapcycle", "Reload map cycle", "usage: load_mapcycle", nullptr},
	{"lock_team", "Lock red or blue team", "usage: lock_team <red|blue>", nullptr},
	{"map_restart", "Restart the current map", "usage: map_restart", nullptr},
	{"next_map", "Skip to the next map", "usage: next_map", nullptr},
	{"ready_all", "Force all players ready", "usage: ready_all", nullptr},
	{"remove_admin", "Remove admin entry", "usage: remove_admin <id>", nullptr},
	{"remove_ban", "Unban from ban.txt", "usage: remove_ban <id>", nullptr},
	{"reset_match", "Reset match in progress", "usage: reset_match", nullptr},
	{"replay", "Replay a tournament game", "usage: replay <game#> [confirm]", nullptr},
	{"ruleset", "Select ruleset (q1/2/3a)", "usage: ruleset <type>", nullptr},
	{"set_map", "Change to another pool map", "usage: set_map <map>", nullptr},
	{"set_team", "Force a client onto team", "usage: set_team <client>", "<team>"},
	{"shuffle", "Shuffle current teams", "usage: shuffle", nullptr},
	{"start_match", "Force match start", "usage: start_match", nullptr},
	{"unlock_team", "Unlock red or blue team", "usage: unlock_team <team>", nullptr},
	{"unready_all", "Force all players unready", "usage: unready_all", nullptr},
}};

} // namespace

void OpenAdminCommandsMenu(gentity_t* ent) {
	MenuBuilder builder;
	builder.add("*Admin Commands*", MenuAlign::Center);
	builder.spacer();
	builder.add("Commands need /admin login.", MenuAlign::Left);
	builder.add("Use console for args.", MenuAlign::Left);
	builder.spacer();

	for (const auto& entry : kAdminCommands) {
		builder.add(entry.name, MenuAlign::Left);
		builder.add(entry.summary, MenuAlign::Left);
		if (entry.usageLine1 && *entry.usageLine1)
			builder.add(entry.usageLine1, MenuAlign::Left);
		if (entry.usageLine2 && *entry.usageLine2)
			builder.add(entry.usageLine2, MenuAlign::Left);
		builder.spacer();
	}

	builder.add("Back", MenuAlign::Left,
			 [](gentity_t* e, Menu&) { OpenAdminSettingsMenu(e); });
	MenuSystem::Open(ent, builder.build());
}
