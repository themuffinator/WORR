/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_forfeit.cpp (Menu Page - Forfeit)
*/

#include "../commands/command_voting.hpp"
#include "../g_local.hpp"

void OpenForfeitMenu(gentity_t *ent) {
  auto menu = std::make_unique<Menu>();

  menu->entries.emplace_back("CONFIRM FORFEIT", MenuAlign::Center);
  menu->entries.emplace_back("", MenuAlign::Center);
  menu->entries.emplace_back("Are you sure you want to forfeit?",
                             MenuAlign::Center);
  menu->entries.emplace_back("", MenuAlign::Center);

  // YES
  MenuEntry yesOpt("YES", MenuAlign::Center);
  yesOpt.onSelect = [](gentity_t *e, Menu &) {
    // Execute forfeit command
    // We use TryLaunchVote directly or CallVote?
    // TryLaunchVote returns a result, CallVote prints.
    // Since we want to emulate "callvote forfeit", we can use CallVote or
    // construct args. Or just run the command string.
    // gi.AddCommandString("callvote forfeit"); -- but checking if we can do
    // typically C++ call. Commands::TryLaunchVote is available.

    auto result = Commands::TryLaunchVote(e, "forfeit", "");
    if (result.success) {
      gi.Client_Print(e, PRINT_HIGH, "Forfeit vote called.\n");
    } else {
      gi.Client_Print(
          e, PRINT_HIGH,
          fmt::format("Failed to call forfeit: {}\n", result.message).c_str());
    }
    MenuSystem::Close(e);
  };
  menu->entries.push_back(std::move(yesOpt));

  // NO
  MenuEntry noOpt("NO", MenuAlign::Center);
  noOpt.onSelect = [](gentity_t *e, Menu &) { MenuSystem::Close(e); };
  menu->entries.push_back(std::move(noOpt));

  MenuSystem::Open(ent, std::move(menu));
}
