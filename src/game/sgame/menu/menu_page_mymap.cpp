/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_mymap.cpp (Menu Page - MyMap) migrated to cgame JSON menus.*/

#include "../g_local.hpp"
#include "menu_ui_helpers.hpp"
#include "menu_ui_list.hpp"

void OpenMyMapMenu(gentity_t *ent)
{
  if (!ent || !ent->client)
    return;

  ent->client->ui.mymap.enableFlags = 0;
  ent->client->ui.mymap.disableFlags = 0;
  UiList_Open(ent, UiListKind::MyMap);
}

namespace {

void UpdateMyMapFlagsMenu(gentity_t *ent, bool openMenu)
{
  if (!ent || !ent->client)
    return;

  const auto &state = ent->client->ui.mymap;
  MenuUi::UiCommandBuilder cmd(ent);
  for (const auto &flag : MapFlagEntries()) {
    cmd.AppendCvar(fmt::format("ui_mymap_flag_{}", flag.code).c_str(),
                   MapFlagStateLabel(state, flag));
  }
  if (openMenu)
    cmd.AppendCommand("pushmenu mymap_flags");
  cmd.Flush();
}

} // namespace

void OpenMyMapFlagsMenu(gentity_t *ent)
{
  UpdateMyMapFlagsMenu(ent, true);
}

void RefreshMyMapFlagsMenu(gentity_t *ent)
{
  UpdateMyMapFlagsMenu(ent, false);
}
