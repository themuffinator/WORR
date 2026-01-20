/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_forfeit.cpp (Menu Page - Forfeit)
*/

#include "../g_local.hpp"
#include "menu_ui_helpers.hpp"

void OpenForfeitMenu(gentity_t *ent) {
  if (!ent || !ent->client)
    return;

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCommand("pushmenu forfeit_confirm");
  cmd.Flush();
}
