/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_voting.cpp (Menu Page - Voting) This file implements the UI for participating in a
vote that has been called by another player. It displays the vote question and provides the
"Yes" and "No" options. Key Responsibilities: - Vote Display: The `onUpdate` function populates
the menu with the details of the active vote, including who called it and what the proposal is.
- Vote Actions: The `onSelect` callbacks for the "Yes" and "No" options update the server-side
vote counts (`level.vote.countYes`, `level.vote.countNo`). - Countdown Timer: Displays the time
remaining for the vote. - Automatic Closing: The menu automatically closes itself if the vote
ends (either by passing, failing, or timing out) while it is open.*/

#include "../g_local.hpp"
#include "../commands/commands.hpp"

void OpenVoteMenu(gentity_t* ent) {
	if (!Vote_Menu_Active(ent)) return;

	auto menu = std::make_unique<Menu>();
	for (size_t i = 0; i < 18; ++i)
		menu->entries.emplace_back("", MenuAlign::Center);

	menu->onUpdate = [](gentity_t* ent, const Menu& m) {
		if (!Vote_Menu_Active(ent)) {
			MenuSystem::Close(ent);
			return;
		}

		int timeout = 30 - (level.time - level.vote.time).seconds<int>();
		if (timeout <= 0) {
			MenuSystem::Close(ent);
			return;
		}

		auto& menu = const_cast<Menu&>(m);
		int i = 2;
		menu.entries[i++].text = fmt::format("{} called a vote:", level.vote.client->sess.netName);
		i = 4;
		menu.entries[i++].text = fmt::format("{} {}", level.vote.cmd->name, level.vote.arg.c_str());

		if (level.vote.time + 3_sec > level.time) {
			i = 7;
			menu.entries[i++].text = "GET READY TO VOTE!";
			i = 8;
			menu.entries[i++].text = fmt::format("{}...", 3 - (level.time - level.vote.time).seconds<int>());
		}
		else {
			i = 7;
			menu.entries[i++].text = "[ YES ]";
			if (i > 0 && i <= menu.entries.size())
				menu.entries[static_cast<int>(i) - 1].onSelect = [](gentity_t* e, Menu&) {
				level.vote.countYes++;
				e->client->pers.voted = 1;
				gi.Client_Print(e, PRINT_HIGH, "Vote cast.\n");
				MenuSystem::Close(e);
				};

			i = 8;
			menu.entries[i++].text = "[ NO ]";
			if (i > 0 && i <= menu.entries.size())
				menu.entries[static_cast<int>(i) - 1].onSelect = [](gentity_t* e, Menu&) {
				level.vote.countNo++;
				e->client->pers.voted = -1;
				gi.Client_Print(e, PRINT_HIGH, "Vote cast.\n");
				MenuSystem::Close(e);
				};
		}

		i = 16;
		menu.entries[i].text = fmt::format("{}", timeout);
		};

	MenuSystem::Open(ent, std::move(menu));
}
