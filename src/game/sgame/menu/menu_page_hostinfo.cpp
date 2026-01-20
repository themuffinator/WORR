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

extern void OpenDmHostInfoMenu(gentity_t *ent);

void OpenHostInfoMenu(gentity_t *ent) {
	OpenDmHostInfoMenu(ent);
}
