/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_admin.cpp (Menu Page - Admin) now acts as a navigation hub for
administrative tooling. It reminds admins that match tuning is handled through
the match setup wizard, offers an explicit reset back into that wizard, and
provides access to the new command reference page.
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
