/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_admin.cpp (Menu Page - Admin) acts as a navigation hub for
administrative tooling. It exposes tournament replay controls when available
and provides access to the command reference page.
*/

#include "../g_local.hpp"
#include "menu_ui_helpers.hpp"

void OpenAdminSettingsMenu(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	MenuUi::UiCommandBuilder cmd(ent);
	cmd.AppendCvar("ui_admin_show_replay", Tournament_IsActive() ? "1" : "0");
	cmd.AppendCommand("pushmenu admin_menu");
	cmd.Flush();
}
