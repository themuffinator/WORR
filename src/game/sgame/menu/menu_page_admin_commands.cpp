/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_admin_commands.cpp (Menu Page - Admin Commands) exposes a curated
reference for every admin-only console verb. It keeps the game settings work
inside the match setup wizard, but surfaces concise usages so server operators
have the full command catalog at their fingertips.
*/

#include "../g_local.hpp"
#include "menu_ui_helpers.hpp"

void OpenAdminCommandsMenu(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	MenuUi::UiCommandBuilder cmd(ent);
	cmd.AppendCommand("pushmenu admin_commands");
	cmd.Flush();
}
