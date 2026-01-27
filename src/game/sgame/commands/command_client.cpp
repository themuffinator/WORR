/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

commands_client.cpp - Implements all general client-side commands.*/

#include "command_system.hpp"
#include "command_registration.hpp"
#include "command_voting.hpp"
#include "../g_local.hpp"
#include "../client/client_session_service_impl.hpp"
#include "../gameplay/map_flag_parser.hpp"
#include "../gameplay/client_config.hpp"
#include "../monsters/m_player.hpp"
#include "../menu/menu_ui_list.hpp"
#include "../../bgame/weapon_pref_utils.hpp"
#include <string>
#include <format>
#include <vector>
#include <array>
#include <algorithm>
#include <cctype>
#include <ranges>
#include <sstream>

void MapSelector_CastVote(gentity_t* ent, int voteIndex);

namespace Commands {

	// --- Forward Declarations for Client Functions ---
	void Admin(gentity_t* ent, const CommandArgs& args);
	void ClientList(gentity_t* ent, const CommandArgs& args);
	void EyeCam(gentity_t* ent, const CommandArgs& args);
	void FragMessages(gentity_t* ent, const CommandArgs& args);
	void Forfeit(gentity_t* ent, const CommandArgs& args);
	void Help(gentity_t* ent, const CommandArgs& args);
	void Hook(gentity_t* ent, const CommandArgs& args);
	void CrosshairID(gentity_t* ent, const CommandArgs& args);
	void KillBeep(gentity_t* ent, const CommandArgs& args);
	void Kill(gentity_t* ent, const CommandArgs& args);
	void MapInfo(gentity_t* ent, const CommandArgs& args);
	void MapPool(gentity_t* ent, const CommandArgs& args);
	void MapCycle(gentity_t* ent, const CommandArgs& args);
	void Motd(gentity_t* ent, const CommandArgs& args);
	void MyMap(gentity_t* ent, const CommandArgs& args);
	void MySkill(gentity_t* ent, const CommandArgs& args);
	void Vote(gentity_t* ent, const CommandArgs& args);
	void Score(gentity_t* ent, const CommandArgs& args);
	void Stats(gentity_t* ent, const CommandArgs& args);
	void JoinTeam(gentity_t* ent, const CommandArgs& args);
	void TimeIn(gentity_t* ent, const CommandArgs& args);
	void TimeOut(gentity_t* ent, const CommandArgs& args);
	void Timer(gentity_t* ent, const CommandArgs& args);
	void UnHook(gentity_t* ent, const CommandArgs& args);
	void DmJoinClose(gentity_t* ent, const CommandArgs& args);
	void DmHostInfo(gentity_t* ent, const CommandArgs& args);
	void DmMatchInfo(gentity_t* ent, const CommandArgs& args);
	void WelcomeContinue(gentity_t* ent, const CommandArgs& args);
	void WorrUiListPrev(gentity_t* ent, const CommandArgs& args);
	void WorrUiListNext(gentity_t* ent, const CommandArgs& args);
	void WorrUiListClose(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteMenu(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteMap(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteMapFlags(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteMapFlag(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteMapClear(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteGametype(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteRuleset(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteTimelimit(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteScorelimit(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteUnlagged(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteRandom(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteArena(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteNextMap(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteRestart(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteShuffle(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteBalance(gentity_t* ent, const CommandArgs& args);
	void WorrCallvoteCointoss(gentity_t* ent, const CommandArgs& args);
	void WorrMyMapMenu(gentity_t* ent, const CommandArgs& args);
	void WorrMyMapFlags(gentity_t* ent, const CommandArgs& args);
	void WorrMyMapFlag(gentity_t* ent, const CommandArgs& args);
	void WorrMyMapClear(gentity_t* ent, const CommandArgs& args);
	void WorrMyMapQueue(gentity_t* ent, const CommandArgs& args);
	void WorrTourneyInfo(gentity_t* ent, const CommandArgs& args);
	void WorrTourneyMaps(gentity_t* ent, const CommandArgs& args);
	void WorrTourneyVeto(gentity_t* ent, const CommandArgs& args);
	void WorrTourneyPick(gentity_t* ent, const CommandArgs& args);
	void WorrTourneyBan(gentity_t* ent, const CommandArgs& args);
	void WorrTourneyReplayMenu(gentity_t* ent, const CommandArgs& args);
	void WorrTourneyReplayConfirm(gentity_t* ent, const CommandArgs& args);
	void WorrTourneyReplay(gentity_t* ent, const CommandArgs& args);
	void WorrVoteYes(gentity_t* ent, const CommandArgs& args);
	void WorrVoteNo(gentity_t* ent, const CommandArgs& args);
	void WorrVoteClose(gentity_t* ent, const CommandArgs& args);
	void WorrMapSelectorVote(gentity_t* ent, const CommandArgs& args);
	void WorrMapSelectorClose(gentity_t* ent, const CommandArgs& args);
	void WorrMatchStatsMenu(gentity_t* ent, const CommandArgs& args);
	void WorrMatchStatsClose(gentity_t* ent, const CommandArgs& args);
	void WorrForfeitMenu(gentity_t* ent, const CommandArgs& args);
	void WorrForfeitYes(gentity_t* ent, const CommandArgs& args);
	void WorrAdminMenu(gentity_t* ent, const CommandArgs& args);
	void WorrAdminCommands(gentity_t* ent, const CommandArgs& args);
	void Wave(gentity_t* ent, const CommandArgs& args);
	void Say(gentity_t* ent, const CommandArgs& args);
	void SayTeam(gentity_t* ent, const CommandArgs& args);
	void Where(gentity_t* ent, const CommandArgs& args);

namespace inventory {
	void WeapNext(gentity_t* ent, const CommandArgs& args);
void WeapPrev(gentity_t* ent, const CommandArgs& args);
}

	namespace follow {

		/*
		=============
		Follow
		=============
		*/
		void Follow(gentity_t* ent, const CommandArgs& args) {
			if (ClientIsPlaying(ent->client)) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_4805c1b66629");
				return;
			}
			if (args.count() < 2) {
				PrintUsage(ent, args, "<client_name|number>", "", "Follows the specified player.");
				return;
			}

			gentity_t* follow_ent = ClientEntFromString(args.getString(1).data());
			if (!follow_ent || !follow_ent->inUse || !ClientIsPlaying(follow_ent->client)) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_8f54bfa61bb5");
				return;
			}

			ent->client->follow.target = follow_ent;
			ent->client->follow.update = true;
			ClientUpdateFollowers(ent);
		}

		/*
		=============
		FollowKiller
		=============
		*/
		void FollowKiller(gentity_t* ent, const CommandArgs& args) {
			(void)args;
			ent->client->sess.pc.follow_killer = !ent->client->sess.pc.follow_killer;
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_ee1f4535e75b", ent->client->sess.pc.follow_killer ? "ON" : "OFF");
		}

		/*
		=============
		FollowLeader
		=============
		*/
		void FollowLeader(gentity_t* ent, const CommandArgs& args) {
			(void)args;
			ent->client->sess.pc.follow_leader = !ent->client->sess.pc.follow_leader;
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_d3b4121bc093", ent->client->sess.pc.follow_leader ? "ON" : "OFF");
		}

		/*
		=============
		FollowPowerup
		=============
		*/
		void FollowPowerup(gentity_t* ent, const CommandArgs& args) {
			(void)args;
			ent->client->sess.pc.follow_powerup = !ent->client->sess.pc.follow_powerup;
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_6bed12fd4a3c", ent->client->sess.pc.follow_powerup ? "ON" : "OFF");
		}

	} // namespace follow

/*
=============
Forfeit

Allows the losing player in a duel to forfeit the match.
=============
*/
	void Forfeit(gentity_t* ent, const CommandArgs& args) {
		if (!Game::Has(GameFlags::OneVOne)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_0d92bc808334");
			return;
		}
		if (level.matchState < MatchState::In_Progress) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_756744b8e4c8");
			return;
		}
		const int runnerUpIndex = level.sortedClients[1];
		if (runnerUpIndex < 0 || runnerUpIndex >= static_cast<int>(game.maxClients)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_959b3e505382");
			return;
		}
		gclient_t* runnerUp = &game.clients[runnerUpIndex];
		if (!runnerUp->pers.connected) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_959b3e505382");
			return;
		}
		if (ent->client != runnerUp) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_c4325eaf6a89");
			return;
		}
		if (!g_allowForfeit->integer) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_e49affcf0b2e");
			return;
		}
		std::string msg = std::format("{} forfeits the match.", ent->client->sess.netName);
		QueueIntermission(msg.c_str(), true, false);
	}

	void Help(gentity_t* ent, const CommandArgs& args) {
		if (deathmatch->integer) {
			Score(ent, args);
			return;
		}
		if (level.intermission.time || ent->health <= 0) return;

		ent->client->showInventory = false;
		ent->client->showScores = false;

		if (ent->client->showHelp &&
		ent->client->pers.game_help1changed == game.help[0].modificationCount &&
		ent->client->pers.game_help2changed == game.help[1].modificationCount) {
			ent->client->showHelp = false;
			globals.serverFlags &= ~SERVER_FLAG_SLOW_TIME;
			return;
		}

		ent->client->showHelp = true;
		ent->client->pers.helpChanged = 0;
		globals.serverFlags |= SERVER_FLAG_SLOW_TIME;
		DrawHelpComputer(ent);
	}

	void Hook(gentity_t* ent, const CommandArgs& args) {
		if (!g_allow_grapple->integer || !g_grapple_offhand->integer) return;
		Weapon_Hook(ent);
	}

	namespace chat {
		constexpr size_t kChatMaxLen = 256;

		Weapon WeaponFromItemId(item_id_t id) {
			switch (id) {
			case IT_WEAPON_GRAPPLE: return Weapon::GrapplingHook;
			case IT_WEAPON_BLASTER: return Weapon::Blaster;
			case IT_WEAPON_CHAINFIST: return Weapon::Chainfist;
			case IT_WEAPON_SHOTGUN: return Weapon::Shotgun;
			case IT_WEAPON_SSHOTGUN: return Weapon::SuperShotgun;
			case IT_WEAPON_MACHINEGUN: return Weapon::Machinegun;
			case IT_WEAPON_ETF_RIFLE: return Weapon::ETFRifle;
			case IT_WEAPON_CHAINGUN: return Weapon::Chaingun;
			case IT_AMMO_GRENADES: return Weapon::HandGrenades;
			case IT_AMMO_TRAP: return Weapon::Trap;
			case IT_AMMO_TESLA: return Weapon::TeslaMine;
			case IT_WEAPON_GLAUNCHER: return Weapon::GrenadeLauncher;
			case IT_WEAPON_PROXLAUNCHER: return Weapon::ProxLauncher;
			case IT_WEAPON_RLAUNCHER: return Weapon::RocketLauncher;
			case IT_WEAPON_HYPERBLASTER: return Weapon::HyperBlaster;
			case IT_WEAPON_IONRIPPER: return Weapon::IonRipper;
			case IT_WEAPON_PLASMAGUN: return Weapon::PlasmaGun;
			case IT_WEAPON_PLASMABEAM: return Weapon::PlasmaBeam;
			case IT_WEAPON_THUNDERBOLT: return Weapon::Thunderbolt;
			case IT_WEAPON_RAILGUN: return Weapon::Railgun;
			case IT_WEAPON_PHALANX: return Weapon::Phalanx;
			case IT_WEAPON_BFG: return Weapon::BFG10K;
			case IT_WEAPON_DISRUPTOR: return Weapon::Disruptor;
			default: return Weapon::None;
			}
		}

		std::string ShortWeaponName(const Item* weapon) {
			if (!weapon)
				return {};

			const Weapon weapon_id = WeaponFromItemId(weapon->id);
			if (weapon_id != Weapon::None) {
				const std::string_view abbr = WeaponToAbbreviation(weapon_id);
				if (!abbr.empty())
					return std::string(abbr);
			}

			return weapon->useName ? weapon->useName : "";
		}

		std::string LongWeaponName(const Item* weapon) {
			if (!weapon)
				return {};

			return weapon->useName ? weapon->useName : "";
		}

		int PowerArmorCells(const gentity_t* ent) {
			if (!ent || !ent->client)
				return -1;

			if (ent->client->pers.inventory[IT_POWER_SCREEN] > 0 ||
				ent->client->pers.inventory[IT_POWER_SHIELD] > 0)
				return ent->client->pers.inventory[IT_AMMO_CELLS];

			return -1;
		}

		std::string Macro_Health(gentity_t* ent) {
			return std::format("H:{}", ent->health);
		}

		std::string Macro_RawHealth(gentity_t* ent) {
			return std::format("{}", ent->health);
		}

		std::string Macro_ShortArmor(gentity_t* ent) {
			const item_id_t armor_index = ArmorIndex(ent);
			const int count = (armor_index == IT_NULL) ? 0 : ent->client->pers.inventory[armor_index];
			const int power = PowerArmorCells(ent);
			if (power >= 0)
				return std::format("A:{} P:{}", count, power);
			return std::format("A:{}", count);
		}

		std::string Macro_RawArmor(gentity_t* ent) {
			const item_id_t armor_index = ArmorIndex(ent);
			if (armor_index == IT_NULL)
				return "0";
			return std::format("{}", ent->client->pers.inventory[armor_index]);
		}

		std::string Macro_LongArmor(gentity_t* ent) {
			const item_id_t armor_index = ArmorIndex(ent);
			const int count = (armor_index == IT_NULL) ? 0 : ent->client->pers.inventory[armor_index];
			const char* armor_name = (armor_index == IT_NULL) ? nullptr : GetItemByIndex(armor_index)->useName;
			const int power = PowerArmorCells(ent);

			std::string result;
			if (armor_name && *armor_name)
				result = std::format("A:{} {}", count, armor_name);
			else
				result = std::format("A:{}", count);

			if (power >= 0)
				result += std::format(" P:{}", power);

			return result;
		}

		std::string Macro_LongWeapon(gentity_t* ent) {
			const Item* weapon = ent->client->pers.weapon;
			if (!weapon)
				return {};

			std::string name = LongWeaponName(weapon);
			if (name.empty())
				return {};

			if (weapon->ammo == IT_NULL)
				return name;

			const int ammo = ent->client->pers.inventory[weapon->ammo];
			return std::format("{}:{}", name, ammo);
		}

		std::string Macro_ShortWeapon(gentity_t* ent) {
			const Item* weapon = ent->client->pers.weapon;
			if (!weapon)
				return {};

			std::string name = ShortWeaponName(weapon);
			if (name.empty())
				return {};

			if (weapon->ammo == IT_NULL)
				return name;

			const int ammo = ent->client->pers.inventory[weapon->ammo];
			return std::format("{}:{}", name, ammo);
		}

		std::string Macro_Location(gentity_t* ent) {
			return Location_NameFor(ent);
		}

		std::string NearbyPlayers(gentity_t* ent, bool team_only) {
			if (!ent || ent->health < 0)
				return {};

			std::string result;
			for (auto player : active_players()) {
				if (player == ent)
					continue;
				if (player->health <= 0)
					continue;
				if (team_only && !OnSameTeam(ent, player))
					continue;
				if (!visible(ent, player))
					continue;

				if (!result.empty())
					result.append(", ");
				result.append(player->client->sess.netName);

				if (result.size() > 200)
					break;
			}

			return result;
		}

		std::string Macro_NearByTeam(gentity_t* ent) {
			return NearbyPlayers(ent, true);
		}

		std::string Macro_NearByAll(gentity_t* ent) {
			return NearbyPlayers(ent, false);
		}

		struct chat_macro_t {
			std::string_view symbol;
			std::string (*replacement)(gentity_t* ent);
		};

		const std::array<chat_macro_t, 11> kChatMacros = {
			chat_macro_t{ "%h", Macro_Health },
			chat_macro_t{ "%H", Macro_Health },
			chat_macro_t{ "%A", Macro_LongArmor },
			chat_macro_t{ "%a", Macro_ShortArmor },
			chat_macro_t{ "%W", Macro_LongWeapon },
			chat_macro_t{ "%w", Macro_ShortWeapon },
			chat_macro_t{ "%l", Macro_Location },
			chat_macro_t{ "%n", Macro_NearByTeam },
			chat_macro_t{ "%N", Macro_NearByAll },
			chat_macro_t{ "#h", Macro_RawHealth },
			chat_macro_t{ "#a", Macro_RawArmor }
		};

		void ExpandMacros(gentity_t* ent, std::string& text, size_t max_len) {
			for (const auto& macro : kChatMacros) {
				size_t pos = 0;
				while ((pos = text.find(macro.symbol, pos)) != std::string::npos) {
					std::string replacement = macro.replacement(ent);
					if (replacement.empty()) {
						text.erase(pos, macro.symbol.size());
						continue;
					}

					if (text.size() - macro.symbol.size() + replacement.size() > max_len) {
						if (g_verbose->integer)
							gi.Com_PrintFmt("Chat macro overflow from {}\n", ent->client->sess.netName);
						return;
					}

					text.replace(pos, macro.symbol.size(), replacement);
					pos += replacement.size();
				}
			}
		}

		void SanitizeMessage(std::string& text) {
			for (char& ch : text) {
				const unsigned char uch = static_cast<unsigned char>(ch);
				if (ch == '\r' || ch == '\n' || uch == 127)
					ch = '.';
			}
		}

		void SayInternal(gentity_t* ent, const CommandArgs& args, bool team) {
			if (!ent || !ent->client)
				return;
			if (args.count() < 2)
				return;

			std::string message = args.joinFrom(1);
			if (message.empty())
				return;

			const std::string prefix = team
				? std::format("({}): ", ent->client->sess.netName)
				: std::format("{}: ", ent->client->sess.netName);

			if (prefix.size() >= kChatMaxLen)
				return;

			const size_t max_payload = kChatMaxLen - prefix.size();
			if (message.size() > max_payload)
				message.resize(max_payload);
			if (message.empty())
				return;

			ExpandMacros(ent, message, max_payload);
			if (message.empty())
				return;

			SanitizeMessage(message);

			const std::string text = prefix + message + "\n";
			if (CheckFlood(ent))
				return;

			if (g_dedicated->integer)
				gi.Com_Print(text.c_str());

			for (auto other : active_clients()) {
				if (team && other != ent && !OnSameTeam(ent, other))
					continue;
				gi.LocClient_Print(other, PRINT_CHAT, text.c_str());
			}
		}
	} // namespace chat

	void Say(gentity_t* ent, const CommandArgs& args) {
		chat::SayInternal(ent, args, false);
	}

	void SayTeam(gentity_t* ent, const CommandArgs& args) {
		chat::SayInternal(ent, args, true);
	}

/*
=============
Admin

Enables administrative permissions when the correct password is supplied.
=============
*/
	void Admin(gentity_t* ent, const CommandArgs& args) {
		if (!g_allowAdmin->integer) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_dcaf4bdac459");
			return;
		}

		if (args.count() < 2) {
			PrintUsage(ent, args, "<password>", "", "Enables administrative access with the correct password.");
			return;
		}

		if (ent->client->sess.admin) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_139b351ebd95");
			return;
		}

		std::string password(args.getString(1));
		if (admin_password->string && *admin_password->string && Q_strcasecmp(admin_password->string, password.c_str()) == 0) {
			ent->client->sess.admin = true;
			gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_aa6c0d95b39c", ent->client->sess.netName);
			return;
		}

		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_9c8fe37f5f58");
	}

	void CrosshairID(gentity_t* ent, const CommandArgs& args) {
		ent->client->sess.pc.show_id = !ent->client->sess.pc.show_id;
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_7b782003ecf3", ent->client->sess.pc.show_id ? "ON" : "OFF");
	}

	/*
	===============
	Impulse
	Quake 1-style one-shot impulse handler.
	Implements:
	1..8  = weapon selects (Q1 mapping -> nearest Q2 weapon)
	9     = give all (cheat; SP/sv_cheats only)
	10    = next weapon
	12    = previous weapon
	21    = drop current weapon (if droppable)
	255   = give+activate Quad (cheat; SP/sv_cheats only)
	===============
	*/
	static void Impulse(gentity_t* ent, const CommandArgs& args) {
		if (!ent || !ent->client)
		return;

		gclient_t* cl = ent->client;
		if (!ClientIsPlaying(cl) || level.intermission.time)
		return;

		// parse impulse number
		std::optional<int> opt = args.getInt(1);
		if (!opt) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_0fe4295ce9e6");
			return;
		}
		const int n = *opt;
		if (n < 0 || n > 255) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_b2e39f7bc1a2");
			return;
		}

		// Helpers
		auto has_item = [&](item_id_t id) -> bool {
			return id > IT_NULL && id < IT_TOTAL && cl->pers.inventory[id] > 0;
		};
		auto use_item = [&](item_id_t id) -> bool {
			if (!has_item(id)) return false;
			Item* it = &itemList[id];
			if (!it->use) return false;
			it->use(ent, it);
			return cl->weapon.pending == it || !(it->flags & IF_WEAPON);
		};
		auto try_use_first_owned = [&](std::initializer_list<item_id_t> ids) -> bool {
			for (auto id : ids) if (use_item(id)) return true;
			return false;
		};
		auto cheats_allowed = [&]() -> bool {
			// Mirror your projects cheat gates here.
			// Common pattern: sv_cheats or singleplayer-only.
			return (CheatsOk(ent));
		};

		// Q1 -> Q2 weapon mapping by item_id_t (adjust IDs to your enum)
		// Replace the placeholder IDs below with your actual item_id_t constants.
		const item_id_t ID_BLASTER = IT_WEAPON_BLASTER;
		const item_id_t ID_SHOTGUN = IT_WEAPON_SHOTGUN;
		const item_id_t ID_SUPERSHOTGUN = IT_WEAPON_SSHOTGUN;
		const item_id_t ID_MACHINEGUN = IT_WEAPON_MACHINEGUN;
		const item_id_t ID_CHAINGUN = IT_WEAPON_CHAINGUN;
		const item_id_t ID_HYPERBLASTER = IT_WEAPON_HYPERBLASTER;
		const item_id_t ID_GRENADELAUNCHER = IT_WEAPON_GLAUNCHER;
		const item_id_t ID_ROCKETLAUNCHER = IT_WEAPON_RLAUNCHER;
		const item_id_t ID_RAILGUN = IT_WEAPON_RAILGUN;

		const item_id_t ID_QUAD = IT_POWERUP_QUAD; // your Quad item id

		bool handled = false;

		switch (n) {
			// 1..8: weapon selection (with sensible fallbacks where Q1/Q2 differ)
			case 1: handled = try_use_first_owned({ ID_BLASTER }); break;                       // Axe -> Blaster
			case 2: handled = try_use_first_owned({ ID_SHOTGUN }); break;                       // SG
			case 3: handled = try_use_first_owned({ ID_SUPERSHOTGUN }); break;                  // SSG
			case 4: handled = try_use_first_owned({ ID_MACHINEGUN }); break;                    // NG  -> MG
			case 5: handled = try_use_first_owned({ ID_CHAINGUN, ID_HYPERBLASTER }); break;     // SNG -> CG, fallback HB
			case 6: handled = try_use_first_owned({ ID_GRENADELAUNCHER }); break;               // GL
			case 7: handled = try_use_first_owned({ ID_ROCKETLAUNCHER }); break;                // RL
			case 8: handled = try_use_first_owned({ ID_HYPERBLASTER, ID_RAILGUN }); break;      // LG  -> HB, fallback RG

			// 9: give all (cheat)
			case 9:
			if (!cheats_allowed()) return;
			for (int id = IT_NULL + 1; id < IT_TOTAL; ++id) {
				const Item* it = &itemList[id];
				if (it->flags & IF_WEAPON) {
					cl->pers.inventory[id] = std::max(1, cl->pers.inventory[id]);
				}
				// Optional: fill ammo, keys, and armor to sane amounts if you want classic "give all"
			}
			gi.LocClient_Print(ent, PRINT_LOW, "$g_sgame_auto_8f2d7f078423");
			handled = true;
			break;
			// 10: next weapon
			case 10:
				Commands::inventory::WeapNext(ent, args);
				handled = true;
				break;

			// 12: previous weapon
			case 12:
				Commands::inventory::WeapPrev(ent, args);
				handled = true;
				break;

			// 255: give + activate Quad (cheat)
			case 255:
			if (!cheats_allowed()) return;
			if (ID_QUAD > IT_NULL && ID_QUAD < IT_TOTAL) {
				cl->pers.inventory[ID_QUAD] += 1;
				use_item(ID_QUAD); // trigger it immediately
				gi.LocClient_Print(ent, PRINT_LOW, "$g_sgame_auto_4501a0248675");
				handled = true;
			}
			break;

			default:
			// Unknown impulses silently ignored (classic Q1 feel), but a hint helps once.
			gi.LocClient_Print(ent, PRINT_LOW, "$g_sgame_auto_0047bb964b5c", n);
			return;
		}

		if (!handled && n >= 1 && n <= 8) {
			gi.LocClient_Print(ent, PRINT_LOW, "$g_sgame_auto_43bb937adbc9", n);
		}
	}

	namespace inventory {

		/*
		=============
		Drop

		Handles dropping items, weapons, or tech, broadcasting POI data to teammates when
		allowed.
		=============
		*/
		void Drop(gentity_t* ent, const CommandArgs& args) {
			if (CombatIsDisabled()) {
				return;
			}

			std::string itemQuery = args.joinFrom(1);
			std::string_view arg1 = args.getString(1);

			if (itemQuery.empty()) {
				PrintUsage(ent, args, "<item_name|tech|weapon>", "", "Drops an item, your current tech, or your current weapon.");
				return;
			}

			Item* it = nullptr;

			if (arg1 == "tech") {
				it = Tech_Held(ent);
				if (it) {
					it->drop(ent, it);
					ValidateSelectedItem(ent);
				}
				return;
			}
			if (arg1 == "weapon") {
				it = ent->client->pers.weapon;
				if (it && it->drop) {
					it->drop(ent, it);
					ValidateSelectedItem(ent);
				}
				return;
			}

			if (args.getString(0) == "drop_index") {
				if (auto itemIndex = args.getInt(1)) {
					if (*itemIndex > IT_NULL && *itemIndex < IT_TOTAL) {
						it = GetItemByIndex(static_cast<item_id_t>(*itemIndex));
					}
				}
			}
			else {
				it = FindItem(arg1.data());

				if (!it) {
					if (auto parsedIndex = CommandArgs::ParseInt(arg1)) {
						if (*parsedIndex > IT_NULL && *parsedIndex < IT_TOTAL) {
							it = GetItemByIndex(static_cast<item_id_t>(*parsedIndex));
						}
					}
				}
			}

			if (!it) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_unknown_item_name", arg1.data());
				return;
			}

			if (!it->drop) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_item_not_droppable");
				return;
			}

			if (it->id == IT_FLAG_RED || it->id == IT_FLAG_BLUE) {
				if (!(match_dropCmdFlags->integer & 1)) {
					gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_891c05d361d0");
					return;
				}
			}
			else if (it->flags & IF_POWERUP) {
				if (!(match_dropCmdFlags->integer & 2)) {
					gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_45ed49e15a7b");
					return;
				}
			}
			else if (it->flags & (IF_WEAPON | IF_AMMO)) {
				if (!(match_dropCmdFlags->integer & 4)) {
					gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_f61733e6f1a0");
					return;
				}
				if (!ItemSpawnsEnabled()) {
					gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_3770d5da103b");
					return;
				}
			}

			if (it->flags & IF_WEAPON && deathmatch->integer && match_weaponsStay->integer) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_bf247978f8d5");
				return;
			}

			if (!ent->client->pers.inventory[it->id]) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_out_of_item", it->pickupName);
				return;
			}

			it->drop(ent, it);

			if (Teams() && g_teamplay_item_drop_notice->integer) {
				uint32_t key = GetUnicastKey();
				std::string message = std::format("[TEAM]: {} drops {}\n", ent->client->sess.netName, it->useName);

				for (auto ec : active_clients()) {
					if (ent == ec) continue;

					bool isTeammate = OnSameTeam(ent, ec);
					bool isFollowingTeammate = !ClientIsPlaying(ec->client) && ec->client->follow.target && OnSameTeam(ent, ec->client->follow.target);

					if (isTeammate || isFollowingTeammate) {
						gi.WriteByte(svc_poi);
						gi.WriteShort(POI_PING + (ent->s.number - 1));
						gi.WriteShort(5000);
						gi.WritePosition(ent->s.origin);
						gi.WriteShort(gi.imageIndex(it->icon));
						gi.WriteByte(215);
						gi.WriteByte(POI_FLAG_NONE);
						gi.unicast(ec, false);
						gi.localSound(ec, CHAN_AUTO, gi.soundIndex("misc/help_marker.wav"), 1.0f, ATTN_NONE, 0.0f, key);
						gi.LocClient_Print(ec, PRINT_TTS, message.c_str(), ent->client->sess.netName);
					}
				}
			}

			ValidateSelectedItem(ent);
		}

		/*
		=============
		InvDrop
		=============
		*/
		void InvDrop(gentity_t* ent, const CommandArgs& args) {
			ValidateSelectedItem(ent);
			if (ent->client->pers.selectedItem == IT_NULL) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_no_item_to_drop");
				return;
			}
			Item* it = &itemList[ent->client->pers.selectedItem];
			if (!it->drop) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_item_not_droppable");
				return;
			}
			it->drop(ent, it);
			ValidateSelectedItem(ent);
		}

		/*
		=============
		Inven
		=============
		*/
		void Inven(gentity_t* ent, const CommandArgs& args) {
			gclient_t* cl = ent->client;
			cl->showScores = false;
			cl->showHelp = false;
			globals.serverFlags &= ~SERVER_FLAG_SLOW_TIME;

			if (deathmatch->integer) {
				if (Vote_Menu_Active(ent)) return;
				if (IsUiMenuOpen(cl)) {
					CloseActiveMenu(ent);
					return;
				}
				if (cl->initialMenu.frozen) {
					if (cl->initialMenu.dmWelcomeActive)
						return;
					if (!cl->initialMenu.dmJoinActive)
						OpenJoinMenu(ent);
					return;
				}
				if (cl->initialMenu.dmJoinActive)
					ForceCloseDmJoinMenu(ent);
				else
					OpenJoinMenu(ent);
				return;
			}

			if (cl->showInventory) {
				cl->showInventory = false;
				return;
			}

			globals.serverFlags |= SERVER_FLAG_SLOW_TIME;
			cl->showInventory = true;
			gi.WriteByte(svc_inventory);
			int i = 0;
			for (; i < IT_TOTAL; ++i) {
				gi.WriteShort(cl->pers.inventory[i]);
			}
			for (; i < MAX_ITEMS; ++i) {
				gi.WriteShort(0);
			}
			gi.unicast(ent, true);
		}

		/*
		=============
		InvNext
		=============
		*/
		void InvNext(gentity_t* ent, const CommandArgs& args) { SelectNextItem(ent, IF_ANY); }

		/*
		=============
		InvNextP
		=============
		*/
		void InvNextP(gentity_t* ent, const CommandArgs& args) { SelectNextItem(ent, IF_TIMED | IF_POWERUP | IF_SPHERE); }

		/*
		=============
		InvNextW
		=============
		*/
		void InvNextW(gentity_t* ent, const CommandArgs& args) { SelectNextItem(ent, IF_WEAPON); }

		/*
		=============
		InvPrev
		=============
		*/
		void InvPrev(gentity_t* ent, const CommandArgs& args) { SelectPrevItem(ent, IF_ANY); }

		/*
		=============
		InvPrevP
		=============
		*/
		void InvPrevP(gentity_t* ent, const CommandArgs& args) { SelectPrevItem(ent, IF_TIMED | IF_POWERUP | IF_SPHERE); }

		/*
		=============
		InvPrevW
		=============
		*/
		void InvPrevW(gentity_t* ent, const CommandArgs& args) { SelectPrevItem(ent, IF_WEAPON); }

		/*
		=============
		InvUse
		=============
		*/
		void InvUse(gentity_t* ent, const CommandArgs& args) {
			if (deathmatch->integer && IsBlockingUiMenuOpen(ent->client))
				return;
			if (level.intermission.time || !ClientIsPlaying(ent->client) || ent->health <= 0) return;

			ValidateSelectedItem(ent);
			if (ent->client->pers.selectedItem == IT_NULL) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_no_item_to_use");
				return;
			}
			Item* it = &itemList[ent->client->pers.selectedItem];
			if (!it->use) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_item_not_usable");
				return;
			}

			ent->client->noWeaponChains = true;
			it->use(ent, it);
			ValidateSelectedItem(ent);
		}

	} // namespace inventory

/*
=============
ClientList

Displays connected clients, optionally sorted by score or time played.
=============
*/
	void ClientList(gentity_t* ent, const CommandArgs& args) {
		enum class ClientListSortMode {
			None,
			Score,
			Time,
		};

		ClientListSortMode sortMode = ClientListSortMode::None;

		if (args.count() > 1) {
			std::string arg = std::string(args.getString(1));
			std::ranges::transform(arg, arg.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			if (arg == "score") {
				sortMode = ClientListSortMode::Score;
			}
			else if (arg == "time") {
				sortMode = ClientListSortMode::Time;
			}
		}

		std::vector<int> clientIndices;
		clientIndices.reserve(game.maxClients);

		for (auto* clientEnt : active_clients()) {
			if (!clientEnt || !clientEnt->client || !clientEnt->client->pers.connected) {
				continue;
			}

			const int index = static_cast<int>((clientEnt - g_entities) - 1);
			clientIndices.push_back(index);
		}

		auto sortBy = [&sortMode](int lhs, int rhs) {
			gclient_t* left = &game.clients[lhs];
			gclient_t* right = &game.clients[rhs];

			switch (sortMode) {
			case ClientListSortMode::Score:
				return ClientScoreForStandings(left) >
					   ClientScoreForStandings(right);
			case ClientListSortMode::Time:
				return (level.time - left->resp.enterTime) > (level.time - right->resp.enterTime);
			default:
				return lhs < rhs;
			}
		};

		std::ranges::stable_sort(clientIndices, sortBy);

		gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "$g_sgame_auto_93bfb36db106", clientIndices.size());

		for (int clientIndex : clientIndices) {
			gclient_t* cl = &game.clients[clientIndex];
			const auto timePlayed = (level.time - cl->resp.enterTime).seconds<int32_t>();

			gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY,
				"$g_sgame_auto_16e0f4a83957",
				clientIndex, cl->sess.netName,
				ClientScoreForStandings(cl), timePlayed);
		}

		gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "$format_blank_line");
	}

/*
=============
FragMessages

Toggles display of frag message popups for the client.
=============
*/
	void FragMessages(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		ent->client->sess.pc.show_fragmessages = !ent->client->sess.pc.show_fragmessages;
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_1e4996f3f794", ent->client->sess.pc.show_fragmessages ? "Activating" : "Disabling");
	}

/*
=============
EyeCam

Toggles the eyecam view when following other players.
=============
*/
	void EyeCam(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		ent->client->sess.pc.use_eyecam = !ent->client->sess.pc.use_eyecam;
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_b54334bfc9f9", ent->client->sess.pc.use_eyecam ? "ON" : "OFF");
	}

	void KillBeep(gentity_t* ent, const CommandArgs& args) {
		int num = 0;
		if (auto val = args.getInt(1)) {
			num = std::clamp(*val, 0, 4);
		}
		else {
			num = (ent->client->sess.pc.killbeep_num + 1) % 5;
		}
		const char* sb[5] = { "off", "clang", "beep-boop", "insane", "tang-tang" };
		ent->client->sess.pc.killbeep_num = num;
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_8e50c9c3f759", sb[num]);
	}

	void Kill(gentity_t* ent, const CommandArgs& args) {
		if (level.intermission.time) return;
		if (deathmatch->integer && (level.time - ent->client->respawnMaxTime) < 5_sec) return;
		if (CombatIsDisabled()) return;

		ent->flags &= ~FL_GODMODE;
		ent->health = 0;
		player_die(ent, ent, ent, 100000, vec3_origin, { ModID::Suicide, Game::Is(GameType::TeamDeathmatch) });
	}

	void MapInfo(gentity_t* ent, const CommandArgs& args) {
		if (level.mapName[0]) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_ed3b6b55b6b1", level.mapName.data());
		}
		else {
			return;
		}
		if (level.longName[0]) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_75688e14b4eb", level.longName.data());
		}
		if (level.author[0]) {
			std::string authors = level.author;
			if (level.author2[0]) {
				authors += ", ";
				authors += level.author2;
			}
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_979ac75bb95e", level.author2[0] ? "s" : "", authors.c_str());
		}
	}

	/*
	=============
	MapPool

	Lists maps in the pool, optionally filtered by name.
	=============
	*/
	void MapPool(gentity_t* ent, const CommandArgs& args) {
		std::string filter = args.joinFrom(1);
		if (filter.empty()) {
			filter = std::string(args.getString(1));
		}

		int count = PrintMapListFiltered(ent, false, filter);
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_dbd17340d1e5", count);
	}

	/*
	=============
	MapCycle

	Lists maps eligible for the cycle, optionally filtered by name.
	=============
	*/
	void MapCycle(gentity_t* ent, const CommandArgs& args) {
		std::string filter = args.joinFrom(1);
		if (filter.empty()) {
			filter = std::string(args.getString(1));
		}

		int count = PrintMapListFiltered(ent, true, filter);
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_c06989704e08", count);
	}

	void Motd(gentity_t* ent, const CommandArgs& args) {
		if (!game.motd.empty()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_cfceb5bdde07", game.motd.c_str());
		}
		else {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_862a9a492d00");
		}
	}

	bool CheckMyMapAllowed(gentity_t* ent) {
		if (!ent || !ent->client) {
			return false;
		}
		if (Tournament_IsActive()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_fdae15df893a");
			return false;
		}
		if (!g_maps_mymap || !g_maps_mymap->integer || (g_allowMymap && !g_allowMymap->integer)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_f682e4dca47f");
			return false;
		}
		if (!ent->client->sess.socialID[0]) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_9bd212761302");
			return false;
		}
		return true;
	}

	bool QueueMyMapRequest(gentity_t* ent, std::string_view mapName, const std::vector<std::string>& flagArgs) {
		if (!ent || !ent->client) {
			return false;
		}

		const MapEntry* map = game.mapSystem.GetMapEntry(std::string(mapName));
		if (!map) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_356fb3f821c4", mapName.data());
			return false;
		}
		if (map->filename.empty()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_1c95ca8d760c");
			return false;
		}
		if (game.mapSystem.IsMapInQueue(map->filename)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_9049813cef3c", map->filename.c_str());
			return false;
		}
		if (game.mapSystem.IsClientInQueue(ent->client->sess.socialID)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_0821b9c2179c");
			return false;
		}

		uint16_t enableFlags = 0;
		uint16_t disableFlags = 0;
		if (!ParseMyMapFlags(flagArgs, enableFlags, disableFlags)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_fb851cb3819e");
			return false;
		}

		std::string_view socialID = ent->client->sess.socialID;
		const auto enqueueResult = game.mapSystem.EnqueueMyMapRequest(*map, socialID, enableFlags, disableFlags, level.time);
		if (!enqueueResult.accepted) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_b8defe3277d3");
			return false;
		}

		std::string display = map->filename;
		for (const auto& flag : flagArgs) {
			display.push_back(' ');
			display.append(flag);
		}

		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_365594b83c4e", display.c_str());
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_e95aa4fc7359",
			ent->client->sess.netName,
			display.c_str());
		if (enqueueResult.evictedOldest) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_fc37447ee950");
		}
		return true;
	}

	void TourneyPick(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			PrintUsage(ent, args, "<mapname>", "", "Selects a map for the tournament series.");
			return;
		}

		std::string message;
		if (!Tournament_HandleVetoAction(ent, TournamentVetoAction::Pick, args.getString(1), message)) {
			if (!message.empty())
				gi.LocClient_Print(ent, PRINT_HIGH, G_Fmt("{}\n", message).data());
			return;
		}

		if (!message.empty())
			gi.LocClient_Print(ent, PRINT_HIGH, G_Fmt("{}\n", message).data());
	}

	void TourneyBan(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			PrintUsage(ent, args, "<mapname>", "", "Bans a map from the tournament series.");
			return;
		}

		std::string message;
		if (!Tournament_HandleVetoAction(ent, TournamentVetoAction::Ban, args.getString(1), message)) {
			if (!message.empty())
				gi.LocClient_Print(ent, PRINT_HIGH, G_Fmt("{}\n", message).data());
			return;
		}

		if (!message.empty())
			gi.LocClient_Print(ent, PRINT_HIGH, G_Fmt("{}\n", message).data());
	}

	void TourneyStatus(gentity_t* ent, const CommandArgs& args) {
		const std::string status = Tournament_GetVetoStatus();
		gi.LocClient_Print(ent, PRINT_HIGH, G_Fmt("{}\n", status).data());
	}

	/*
	=============
	MyMap

	Queues a requested map for play, applying optional MyMap override flags.
	=============
	*/
	void MyMap(gentity_t* ent, const CommandArgs& args) {
		if (!CheckMyMapAllowed(ent)) {
			return;
		}
		if (args.count() < 2 || args.getString(1) == "?") {
			PrintUsage(ent, args, "<mapname>", "[+flag] [-flag] ...", "Queues a map to be played next with optional rule modifiers.");
			return;
		}

		std::string_view mapName = args.getString(1);

		std::vector<std::string> flagArgs;
		flagArgs.reserve(args.count() > 2 ? args.count() - 2 : 0);
		for (int i = 2; i < args.count(); ++i) {
			std::string_view flag = args.getString(i);
			if (!flag.empty()) {
				flagArgs.emplace_back(flag);
			}
		}

		QueueMyMapRequest(ent, mapName, flagArgs);
	}

	void MySkill(gentity_t* ent, const CommandArgs& args) {
		int totalSkill = 0, numPlayers = 0;
		for (auto ec : active_players()) {
			totalSkill += ec->client->sess.skillRating;
			numPlayers++;
		}
		int averageSkill = (numPlayers > 0) ? (totalSkill / numPlayers) : 0;
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_5db842818f0e", level.gametype_name.data(), ent->client->sess.skillRating, averageSkill);
	}

	namespace readiness {

		namespace {

			inline worr::server::client::ClientSessionServiceImpl& SessionService() {
				return worr::server::client::GetClientSessionService();
			}

			/*
			=============
			HandleReadyResult

			Provides user feedback for ready-state changes routed through the
			ClientSessionServiceImpl.
			=============
			*/
			void HandleReadyResult(gentity_t* ent, worr::server::client::ReadyResult result, bool state, bool toggle) {
				const char* readyState = state ? "ready" : "not ready";
				switch (result) {
				case worr::server::client::ReadyResult::Success:
					return;
				case worr::server::client::ReadyResult::AlreadySet: {
					const std::string message = std::string(G_Fmt("You are already {}.\n", readyState));
					gi.LocClient_Print(ent, PRINT_HIGH, message.data());
					return;
				}
				case worr::server::client::ReadyResult::NoConditions:
					gi.LocClient_Print(ent, PRINT_HIGH, toggle ? "You cannot toggle readiness right now.\n" : "You cannot change readiness right now.\n");
					return;
}
			}
		}

		/*
		=============
		NotReady
		=============
		*/
		void NotReady(gentity_t* ent, const CommandArgs& args) {
			(void)args;
			const auto result = SessionService().OnReadyToggled(ent, false, false);
			HandleReadyResult(ent, result, false, false);
		}

		/*
		=============
		Ready
		=============
		*/
		void Ready(gentity_t* ent, const CommandArgs& args) {
			(void)args;
			const auto result = SessionService().OnReadyToggled(ent, true, false);
			HandleReadyResult(ent, result, true, false);
		}

		/*
		=============
		ReadyUp
		=============
		*/
		void ReadyUp(gentity_t* ent, const CommandArgs& args) {
			(void)args;
			const auto result = SessionService().OnReadyToggled(ent, false, true);
			HandleReadyResult(ent, result, false, true);
		}

	} // namespace readiness

	namespace inventory {

		/*
		=============
		PutAway
		=============
		*/
		void PutAway(gentity_t* ent, const CommandArgs& args) {
			(void)args;
			if (ent->client->initialMenu.frozen)
				return;
			ent->client->showScores = false;
			ent->client->showHelp = false;
			ent->client->showInventory = false;
			globals.serverFlags &= ~SERVER_FLAG_SLOW_TIME;
			if (deathmatch->integer && IsUiMenuOpen(ent->client)) {
				if (Vote_Menu_Active(ent)) return;
				CloseActiveMenu(ent);
			}
		}

	} // namespace inventory

	void Score(gentity_t* ent, const CommandArgs& args) {
		if (level.intermission.time) return;
		if (!deathmatch->integer && !coop->integer) return;
		if (ent->client->initialMenu.frozen) return;

		if (Vote_Menu_Active(ent)) {
			ent->client->ps.stats[STAT_SHOW_STATUSBAR] = ClientIsPlaying(ent->client) ? 1 : 0;
			return;
		}

		ent->client->showInventory = false;
		ent->client->showHelp = false;
		globals.serverFlags &= ~SERVER_FLAG_SLOW_TIME;

		if (IsUiMenuOpen(ent->client)) CloseActiveMenu(ent);

		if (ent->client->showScores) {
			ent->client->showScores = false;
		}
		else {
			ent->client->showScores = true;
			MultiplayerScoreboard(ent);
		}
	}

	namespace inventory {

		/*
		=============
		SetWeaponPref

		Parses weapon preference tokens before persisting them via ClientConfigStore.
		=============
		*/
		void SetWeaponPref(gentity_t* ent, const CommandArgs& args) {
			if (!ent || !ent->client)
			return;

			auto* cl = ent->client;
			std::array<bool, static_cast<size_t>(Weapon::Total)> seen{};
			std::vector<Weapon> parsed;
			parsed.reserve(static_cast<size_t>(std::max(args.count() - 1, 0)));
			std::vector<std::string> invalidTokens;
			bool capacityExceeded = false;

			for (int i = 1; i < args.count(); ++i) {
				std::string_view token = args.getString(i);
				if (token.empty())
				continue;

				std::string normalized;
				switch (TryAppendWeaponPreference(token, parsed, seen, &normalized)) {
					case WeaponPrefAppendResult::Added:
					break;
					case WeaponPrefAppendResult::Duplicate:
					break;
					case WeaponPrefAppendResult::Invalid:
					if (!normalized.empty())
					invalidTokens.emplace_back(std::move(normalized));
					break;
					case WeaponPrefAppendResult::CapacityExceeded:
					capacityExceeded = true;
					break;
				}
			}

			cl->sess.weaponPrefs.swap(parsed);
			Client_RebuildWeaponPreferenceOrder(*cl);
			GetClientConfigStore().SaveWeaponPreferences(cl);

			if (!invalidTokens.empty()) {
				std::ostringstream joined;
				for (size_t i = 0; i < invalidTokens.size(); ++i) {
					if (i)
					joined << ", ";
					joined << invalidTokens[i];
				}
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_669f70b66b88", joined.str().c_str());
			}

			if (capacityExceeded) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_7d3f24d82dd1",
				static_cast<int>(WeaponPreferenceCapacity));
			}

			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_af29d2944da5");
		}

	} // namespace inventory

	void Stats(gentity_t* ent, const CommandArgs& args) {
		if (!Game::Has(GameFlags::CTF)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_ef742dd283be");
			return;
		}
		// Further implementation for displaying CTF stats would go here.
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_d43d5d6d27e2");
	}

	void JoinTeam(gentity_t* ent, const CommandArgs& args) {
		if (Tournament_IsActive()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_f5023bb3f0ec");
			return;
		}
		if (args.count() < 2) {
			const char* teamName = "spectating";
			if (ClientIsPlaying(ent->client)) {
				teamName = Teams_TeamName(ent->client->sess.team);
			}
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_you_are_on_team", teamName);
			return;
		}

		const bool wasSpectator = !ClientIsPlaying(ent->client);
		Team team = StringToTeamNum(args.getString(1).data());
		if (team != Team::None) {
			const bool isBot = (ent->svFlags & SVF_BOT) || ent->client->sess.is_a_bot;
			if (!isBot && FreezeTag_IsFrozen(ent) && team != ent->client->sess.team) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_cant_change_teams");
				return;
			}

			const bool changed = ::SetTeam(ent, team, false, false, false);
			if (ent->client->initialMenu.frozen &&
				(changed || (team == Team::Spectator && wasSpectator))) {
				ent->client->initialMenu.frozen = false;
				ent->client->initialMenu.shown = true;
				ent->client->initialMenu.delay = 0_sec;
				ent->client->initialMenu.hostSetupDone = true;
				ent->client->initialMenu.dmWelcomeActive = false;
				ent->client->initialMenu.dmJoinActive = false;
			}
		}
	}

	void DmJoinClose(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;

		CloseDmJoinMenu(ent);
	}

	void DmHostInfo(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;

		OpenDmHostInfoMenu(ent);
	}

	void DmMatchInfo(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;

		OpenDmMatchInfoMenu(ent);
	}

	void WelcomeContinue(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;

		auto* cl = ent->client;
		if (!cl->initialMenu.frozen && !cl->initialMenu.dmWelcomeActive)
			return;

		cl->initialMenu.frozen = false;
		cl->initialMenu.shown = true;
		cl->initialMenu.delay = 0_sec;
		cl->initialMenu.hostSetupDone = true;
		cl->initialMenu.dmWelcomeActive = false;
		cl->initialMenu.dmJoinActive = false;

		if (match_autoJoin && match_autoJoin->integer) {
			Team targetTeam = Teams() ? PickTeam(-1) : Team::Free;
			::SetTeam(ent, targetTeam, false, false, false);
		}
		else {
			OpenJoinMenu(ent);
		}
	}

	static bool TryLaunchVoteWithFeedback(gentity_t* ent, std::string_view voteName, std::string_view voteArg) {
		auto result = TryLaunchVote(ent, voteName, voteArg);
		if (!result.success) {
			if (!result.message.empty()) {
				gi.LocClient_Print(ent, PRINT_HIGH, std::format("{}\n", result.message).c_str());
			}
			return false;
		}
		return true;
	}

	void WorrUiListPrev(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		UiList_Prev(ent);
	}

	void WorrUiListNext(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		UiList_Next(ent);
	}

	void WorrUiListClose(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;
		ent->client->ui.list.kind = UiListKind::None;
		ent->client->ui.list.page = 0;
	}

	void WorrCallvoteMenu(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		OpenCallvoteMenu(ent);
	}

	void WorrCallvoteMap(gentity_t* ent, const CommandArgs& args) {
		if (!ent || !ent->client)
			return;
		if (args.count() < 2) {
			OpenCallvoteMapMenu(ent);
			return;
		}
		const std::string_view mapName = args.getString(1);
		const std::string voteArg = BuildMapFlagVoteArg(ent->client->ui.callvoteMap, mapName);
		TryLaunchVoteWithFeedback(ent, "map", voteArg);
	}

	void WorrCallvoteMapFlags(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		OpenCallvoteMapFlagsMenu(ent);
	}

	void WorrCallvoteMapFlag(gentity_t* ent, const CommandArgs& args) {
		if (!ent || !ent->client)
			return;
		if (args.count() < 2)
			return;
		if (!ToggleMapFlagByCode(ent->client->ui.callvoteMap, args.getString(1))) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_861cb1288a5f");
			return;
		}
		RefreshCallvoteMapFlagsMenu(ent);
		UiList_Refresh(ent, false);
	}

	void WorrCallvoteMapClear(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;
		ent->client->ui.callvoteMap.enableFlags = 0;
		ent->client->ui.callvoteMap.disableFlags = 0;
		RefreshCallvoteMapFlagsMenu(ent);
		UiList_Refresh(ent, false);
	}

	void WorrCallvoteGametype(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			OpenCallvoteGametypeMenu(ent);
			return;
		}
		TryLaunchVoteWithFeedback(ent, "gametype", args.getString(1));
	}

	void WorrCallvoteRuleset(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			OpenCallvoteRulesetMenu(ent);
			return;
		}
		TryLaunchVoteWithFeedback(ent, "ruleset", args.getString(1));
	}

	void WorrCallvoteTimelimit(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			OpenCallvoteTimelimitMenu(ent);
			return;
		}
		TryLaunchVoteWithFeedback(ent, "timelimit", args.getString(1));
	}

	void WorrCallvoteScorelimit(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			OpenCallvoteScorelimitMenu(ent);
			return;
		}
		TryLaunchVoteWithFeedback(ent, "scorelimit", args.getString(1));
	}

	void WorrCallvoteUnlagged(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			OpenCallvoteUnlaggedMenu(ent);
			return;
		}
		TryLaunchVoteWithFeedback(ent, "unlagged", args.getString(1));
	}

	void WorrCallvoteRandom(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			OpenCallvoteRandomMenu(ent);
			return;
		}
		TryLaunchVoteWithFeedback(ent, "random", args.getString(1));
	}

	void WorrCallvoteArena(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			OpenCallvoteArenaMenu(ent);
			return;
		}
		TryLaunchVoteWithFeedback(ent, "arena", args.getString(1));
	}

	void WorrCallvoteNextMap(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		TryLaunchVoteWithFeedback(ent, "nextmap", "");
	}

	void WorrCallvoteRestart(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		TryLaunchVoteWithFeedback(ent, "restart", "");
	}

	void WorrCallvoteShuffle(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		TryLaunchVoteWithFeedback(ent, "shuffle", "");
	}

	void WorrCallvoteBalance(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		TryLaunchVoteWithFeedback(ent, "balance", "");
	}

	void WorrCallvoteCointoss(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		TryLaunchVoteWithFeedback(ent, "cointoss", "");
	}

	void WorrMyMapMenu(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		OpenMyMapMenu(ent);
	}

	void WorrMyMapFlags(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		OpenMyMapFlagsMenu(ent);
	}

	void WorrMyMapFlag(gentity_t* ent, const CommandArgs& args) {
		if (!ent || !ent->client)
			return;
		if (args.count() < 2)
			return;
		if (!ToggleMapFlagByCode(ent->client->ui.mymap, args.getString(1))) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_861cb1288a5f");
			return;
		}
		RefreshMyMapFlagsMenu(ent);
		UiList_Refresh(ent, false);
	}

	void WorrMyMapClear(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;
		ent->client->ui.mymap.enableFlags = 0;
		ent->client->ui.mymap.disableFlags = 0;
		RefreshMyMapFlagsMenu(ent);
		UiList_Refresh(ent, false);
	}

	void WorrMyMapQueue(gentity_t* ent, const CommandArgs& args) {
		if (!ent || !ent->client)
			return;
		if (args.count() < 2)
			return;
		if (!CheckMyMapAllowed(ent))
			return;
		std::vector<std::string> flagArgs = BuildMapFlagArgs(ent->client->ui.mymap);
		QueueMyMapRequest(ent, args.getString(1), flagArgs);
	}

	void WorrTourneyInfo(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		OpenTournamentInfoMenu(ent);
	}

	void WorrTourneyMaps(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		OpenTournamentMapChoicesMenu(ent);
	}

	void WorrTourneyVeto(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		OpenTournamentVetoMenu(ent);
	}

	void WorrTourneyPick(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			UiList_Open(ent, UiListKind::TournamentPick);
			return;
		}
		TourneyPick(ent, CommandArgs{ "tourney_pick", args.getString(1) });
	}

	void WorrTourneyBan(gentity_t* ent, const CommandArgs& args) {
		if (args.count() < 2) {
			UiList_Open(ent, UiListKind::TournamentBan);
			return;
		}
		TourneyBan(ent, CommandArgs{ "tourney_ban", args.getString(1) });
	}

	void WorrTourneyReplayMenu(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		OpenTournamentReplayMenu(ent);
	}

	void WorrTourneyReplayConfirm(gentity_t* ent, const CommandArgs& args) {
		if (!ent || !ent->client)
			return;
		if (args.count() < 2)
			return;
		auto parsed = CommandArgs::ParseInt(args.getString(1));
		if (!parsed)
			return;
		OpenTournamentReplayConfirmMenu(ent, *parsed);
	}

	void WorrTourneyReplay(gentity_t* ent, const CommandArgs& args) {
		if (!ent || !ent->client)
			return;
		if (args.count() < 2)
			return;
		auto parsed = CommandArgs::ParseInt(args.getString(1));
		if (!parsed)
			return;

		std::string message;
		if (!Tournament_ReplayGame(*parsed, message)) {
			if (!message.empty())
				gi.LocClient_Print(ent, PRINT_HIGH, std::format("{}\n", message).c_str());
		}
		CloseActiveMenu(ent);
	}

	void WorrVoteYes(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		Vote(ent, CommandArgs{ "vote", "yes" });
		CloseActiveMenu(ent);
	}

	void WorrVoteNo(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		Vote(ent, CommandArgs{ "vote", "no" });
		CloseActiveMenu(ent);
	}

	void WorrVoteClose(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;
		ent->client->ui.voteActive = false;
	}

	void WorrMapSelectorVote(gentity_t* ent, const CommandArgs& args) {
		if (!ent || !ent->client)
			return;
		if (args.count() < 2)
			return;
		auto parsed = CommandArgs::ParseInt(args.getString(1));
		if (!parsed)
			return;
		MapSelector_CastVote(ent, *parsed);
		ent->client->ui.mapSelectorNextUpdate = level.time;
	}

	void WorrMapSelectorClose(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;
		ent->client->ui.mapSelectorActive = false;
	}

	void WorrMatchStatsMenu(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		OpenPlayerMatchStatsMenu(ent);
	}

	void WorrMatchStatsClose(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;
		ent->client->ui.matchStatsActive = false;
	}

	void WorrForfeitMenu(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		OpenForfeitMenu(ent);
	}

	void WorrForfeitYes(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;
		if (TryLaunchVoteWithFeedback(ent, "forfeit", "")) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_18fcb09b23e4");
		}
		CloseActiveMenu(ent);
	}

	void WorrAdminMenu(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;
		if (!ent->client->sess.admin) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_bc5186a8887c");
			return;
		}
		OpenAdminSettingsMenu(ent);
	}

	void WorrAdminCommands(gentity_t* ent, const CommandArgs& args) {
		(void)args;
		if (!ent || !ent->client)
			return;
		if (!ent->client->sess.admin) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_bc5186a8887c");
			return;
		}
		OpenAdminCommandsMenu(ent);
	}

	void TimeIn(gentity_t* ent, const CommandArgs& args) {
		if (!level.timeoutActive) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_09bc6a880057");
			return;
		}
		if (!ent->client->sess.admin && level.timeoutOwner != ent) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_6421882515c4");
			return;
		}
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_sgame_auto_6171c4a72be2", ent->client->sess.netName);
		level.timeoutActive = 3_sec;
	}

	void TimeOut(gentity_t* ent, const CommandArgs& args) {
		if (match_timeoutLength->integer <= 0) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_7e04f97c50b2");
			return;
		}
		if (level.matchState != MatchState::In_Progress) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_a10866f58699");
			return;
		}
		if (ent->client->pers.timeout_used && !ent->client->sess.admin) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_694f6046128f");
			return;
		}
		if (level.timeoutActive > 0_ms) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_f92678f588fa");
			return;
		}
		level.timeoutOwner = ent;
		level.timeoutActive = GameTime::from_sec(match_timeoutLength->integer);
		gi.LocBroadcast_Print(PRINT_CENTER, "$g_sgame_auto_d55060a5237d", ent->client->sess.netName, TimeString(match_timeoutLength->integer * 1000, false, false));
		ent->client->pers.timeout_used = true;
		G_LogEvent("MATCH TIMEOUT STARTED");
	}

	void Timer(gentity_t* ent, const CommandArgs& args) {
		ent->client->sess.pc.show_timer = !ent->client->sess.pc.show_timer;
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_755738c9099d", ent->client->sess.pc.show_timer ? "ON" : "OFF");
	}

	void UnHook(gentity_t* ent, const CommandArgs& args) {
		Weapon_Grapple_DoReset(ent->client);
	}

	namespace inventory {

		/*
		=============
		Use

		Activates an inventory item, supporting holdable shortcuts and ensuring the
		client state remains consistent.
		=============
		*/
		void Use(gentity_t* ent, const CommandArgs& args) {
			const std::string_view cmd = args.getString(0);
			std::string itemQuery = args.joinFrom(1);
			std::string_view itemName = args.getString(1);
			if (itemQuery.empty()) {
				if (cmd == "use_index" || cmd == "use_index_only")
					PrintUsage(ent, args, "<item_index>", "", "Uses an item from your inventory by index.");
				else
					PrintUsage(ent, args, "<item_name>", "", "Uses an item from your inventory.");
				return;
			}

			Item* it = nullptr;
			if (cmd == "use_index" || cmd == "use_index_only") {
				std::optional<int> itemIndex = args.getInt(1);
				if (!itemIndex && itemQuery == "${inventory index}")
					itemIndex = static_cast<int>(ent->client->pers.selectedItem);

				if (itemIndex && *itemIndex > IT_NULL && *itemIndex < IT_TOTAL)
					it = GetItemByIndex(static_cast<item_id_t>(*itemIndex));
			}
			else if (itemName == "holdable") {
				if (ent->client->pers.inventory[IT_TELEPORTER])
					it = GetItemByIndex(IT_TELEPORTER);
				else if (ent->client->pers.inventory[IT_ADRENALINE])
					it = GetItemByIndex(IT_ADRENALINE);
			}
			else {
				it = FindItem(itemQuery.c_str());
				if (!it) {
					if (auto parsedIndex = CommandArgs::ParseInt(itemName)) {
						if (*parsedIndex > IT_NULL && *parsedIndex < IT_TOTAL)
							it = GetItemByIndex(static_cast<item_id_t>(*parsedIndex));
					}
				}
			}

			if (!it) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_unknown_item_name", itemQuery.c_str());
				return;
			}
			if (!it->use) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_item_not_usable");
				return;
			}
			if (!ent->client->pers.inventory[it->id]) {
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_out_of_item", it->pickupName);
				return;
			}

			ent->client->noWeaponChains = (cmd != "use" && cmd != "use_index");

			it->use(ent, it);
			ValidateSelectedItem(ent);
		}

	} // namespace inventory

	void Wave(gentity_t* ent, const CommandArgs& args) {
		if (ent->deadFlag || ent->moveType == MoveType::NoClip) return;

		const int gesture = args.getInt(1).value_or(GESTURE_FLIP_OFF);

		const bool doAnimate = ent->client->anim.priority <= ANIM_WAVE && !(ent->client->ps.pmove.pmFlags & PMF_DUCKED);

		if (doAnimate)
		ent->client->anim.priority = ANIM_WAVE;

		const char* otherNotifyMsg = nullptr;
		const char* otherNotifyNoneMsg = nullptr;

		Vector3 start{};
		Vector3 dir{};
		P_ProjectSource(ent, ent->client->vAngle, vec3_origin, start, dir);

		gentity_t* aimingAt = nullptr;
		float bestDist = -9999.0f;

		for (auto player : active_players()) {
			if (player == ent)
			continue;

			Vector3 cdir = player->s.origin - start;
			float dist = cdir.normalize();

			float dot = ent->client->vForward.dot(cdir);

			if (dot < 0.97f)
			continue;
			else if (dist < bestDist)
			continue;

			bestDist = dist;
			aimingAt = player;
		}

		trace_t pointTrace{};
		gentity_t* tracedEnt = nullptr;
		const Item* pointingItem = nullptr;

		if (gesture == GESTURE_POINT) {
			pointTrace = gi.traceLine(start, start + (ent->client->vForward * 2048.0f), ent, static_cast<contents_t>(MASK_SHOT & ~CONTENTS_WINDOW));

			if (pointTrace.fraction != 1.0f)
			tracedEnt = pointTrace.ent;

			if (tracedEnt && tracedEnt->item) {
				const Item* candidate = tracedEnt->item;

				if (candidate && ((candidate->flags & IF_WEAPON) || candidate->highValue != HighValueItems::None))
				pointingItem = candidate;
			}
		}

		const char* pointingItemName = nullptr;

		if (pointingItem) {
			pointingItemName = pointingItem->pickupName;

			if ((!pointingItemName || !pointingItemName[0]) && pointingItem->pickupNameDefinitive)
			pointingItemName = pointingItem->pickupNameDefinitive;
		}

		switch (gesture) {
			case GESTURE_FLIP_OFF:
			otherNotifyMsg = "$g_flipoff_other";
			otherNotifyNoneMsg = "$g_flipoff_none";
			if (doAnimate) {
				ent->s.frame = FRAME_flip01 - 1;
				ent->client->anim.end = FRAME_flip12;
			}
			break;
			case GESTURE_SALUTE:
			otherNotifyMsg = "$g_salute_other";
			otherNotifyNoneMsg = "$g_salute_none";
			if (doAnimate) {
				ent->s.frame = FRAME_salute01 - 1;
				ent->client->anim.end = FRAME_salute11;
			}
			break;
			case GESTURE_TAUNT:
			otherNotifyMsg = "$g_taunt_other";
			otherNotifyNoneMsg = "$g_taunt_none";
			if (doAnimate) {
				ent->s.frame = FRAME_taunt01 - 1;
				ent->client->anim.end = FRAME_taunt17;
			}
			break;
			case GESTURE_WAVE:
			otherNotifyMsg = "$g_wave_other";
			otherNotifyNoneMsg = "$g_wave_none";
			if (doAnimate) {
				ent->s.frame = FRAME_wave01 - 1;
				ent->client->anim.end = FRAME_wave11;
			}
			break;
			case GESTURE_POINT:
			default:
			otherNotifyMsg = "$g_point_other";
			otherNotifyNoneMsg = "$g_point_none";
			if (doAnimate) {
				ent->s.frame = FRAME_point01 - 1;
				ent->client->anim.end = FRAME_point12;
			}
			break;
		}

		bool hasTarget = false;

		if (gesture == GESTURE_POINT) {
			for (auto player : active_players()) {
				if (player == ent)
				continue;
				else if (!OnSameTeam(ent, player))
				continue;

				hasTarget = true;
				break;
			}
		}

		const char* pointTargetName = nullptr;

		if (aimingAt)
		pointTargetName = aimingAt->client->sess.netName;
		else if (pointingItemName)
		pointTargetName = pointingItemName;

		if (gesture == GESTURE_POINT && hasTarget) {
			if (CheckFlood(ent))
			return;

			const char* pingNotifyMsg = pointTargetName ? "$g_point_other" : "$g_point_other_ping";

			const uint32_t key = GetUnicastKey();

			if (pointTrace.fraction != 1.0f) {
				for (auto player : active_players()) {
					if (player != ent && !OnSameTeam(ent, player))
					continue;

					gi.WriteByte(svc_poi);
					gi.WriteShort(POI_PING + (ent->s.number - 1));
					gi.WriteShort(5000);
					gi.WritePosition(pointTrace.endPos);
					gi.WriteShort(level.picPing);
					gi.WriteByte(208);
					gi.WriteByte(POI_FLAG_NONE);
					gi.unicast(player, false);

					gi.localSound(player, CHAN_AUTO, gi.soundIndex("misc/help_marker.wav"), 1.0f, ATTN_NONE, 0.0f, key);
					if (pointTargetName)
					gi.LocClient_Print(player, PRINT_TTS, pingNotifyMsg, ent->client->sess.netName, pointTargetName);
					else
					gi.LocClient_Print(player, PRINT_TTS, pingNotifyMsg, ent->client->sess.netName);
				}
			}
		}
		else {
			if (CheckFlood(ent))
			return;

			gentity_t* targ = nullptr;
			while ((targ = FindRadius(targ, ent->s.origin, 1024.0f)) != nullptr) {
				if (ent == targ)
				continue;
				if (!targ->client)
				continue;
				if (!gi.inPVS(ent->s.origin, targ->s.origin, false))
				continue;

				if (pointTargetName && otherNotifyMsg)
				gi.LocClient_Print(targ, PRINT_TTS, otherNotifyMsg, ent->client->sess.netName, pointTargetName);
				else if (otherNotifyNoneMsg)
				gi.LocClient_Print(targ, PRINT_TTS, otherNotifyNoneMsg, ent->client->sess.netName);
			}

			if (pointTargetName && otherNotifyMsg)
			gi.LocClient_Print(ent, PRINT_TTS, otherNotifyMsg, ent->client->sess.netName, pointTargetName);
			else if (otherNotifyNoneMsg)
			gi.LocClient_Print(ent, PRINT_TTS, otherNotifyNoneMsg, ent->client->sess.netName);
		}

		ent->client->anim.time = 0_ms;
	}

	namespace inventory {

		/*
		=============
		WeapLast
		=============
		*/
		void WeapLast(gentity_t* ent, const CommandArgs& args) {
			(void)args;
			gclient_t* cl = ent->client;
			if (!cl->pers.weapon || !cl->pers.lastWeapon) return;

			cl->noWeaponChains = true;
			Item* it = cl->pers.lastWeapon;
			if (!cl->pers.inventory[it->id]) return;

			it->use(ent, it);
		}

		/*
		=============
		WeapNext
		=============
		*/
		void WeapNext(gentity_t* ent, const CommandArgs& args) {
			(void)args;
			gclient_t* cl = ent->client;
			if (!cl->pers.weapon) return;

			cl->noWeaponChains = true;
			item_id_t selected_weapon = cl->pers.weapon->id;

			for (int i = 1; i <= IT_TOTAL; i++) {
				item_id_t index = static_cast<item_id_t>((static_cast<int>(selected_weapon) + i) % IT_TOTAL);
				if (index > IT_NULL && cl->pers.inventory[index]) {
					Item* it = &itemList[index];
					if (it->use && (it->flags & IF_WEAPON)) {
						it->use(ent, it);
						return;
					}
				}
			}
		}

		/*
		=============
		WeapPrev
		=============
		*/
		void WeapPrev(gentity_t* ent, const CommandArgs& args) {
			(void)args;
			gclient_t* cl = ent->client;
			if (!cl->pers.weapon) return;

			cl->noWeaponChains = true;
			item_id_t selected_weapon = cl->pers.weapon->id;

			for (int i = 1; i <= IT_TOTAL; i++) {
				item_id_t index = static_cast<item_id_t>((static_cast<int>(selected_weapon) + IT_TOTAL - i) % IT_TOTAL);
				if (index > IT_NULL && cl->pers.inventory[index]) {
					Item* it = &itemList[index];
					if (it->use && (it->flags & IF_WEAPON)) {
						it->use(ent, it);
						return;
					}
				}
			}
		}

	} // namespace inventory

	void Where(gentity_t* ent, const CommandArgs& args) {
		if (!ent || !ent->client) return;
		const Vector3& origin = ent->s.origin;
		const auto& angles = ent->client->ps.viewAngles;
		std::string location = std::format("{:.1f} {:.1f} {:.1f} {:.1f} {:.1f} {:.1f}",
		origin[_X], origin[_Y], origin[_Z], angles[PITCH], angles[YAW], angles[ROLL]);
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_sgame_auto_ec08ac9fcd95", location.c_str());
		gi.SendToClipBoard(location.c_str());
}

	class ClientCommandController {
		public:
		static void RegisterAll();

		private:
		static void RegisterCoreCommands();
		static void RegisterFollowCommands();
		static void RegisterInventoryCommands();
		static void RegisterReadinessCommands();
	};

	/*
	=============
	ClientCommandController::RegisterAll
	=============
	*/
	void ClientCommandController::RegisterAll() {
		RegisterCoreCommands();
		RegisterFollowCommands();
		RegisterInventoryCommands();
		RegisterReadinessCommands();
	}

	/*
	=============
	ClientCommandController::RegisterCoreCommands
	=============
	*/
	void ClientCommandController::RegisterCoreCommands() {
		using enum CommandFlag;
		RegisterCommand("admin", &Admin, AllowIntermission | AllowSpectator);
		RegisterCommand("clientlist", &ClientList, AllowDead | AllowIntermission | AllowSpectator);
		RegisterCommand("eyecam", &EyeCam, AllowSpectator);
		RegisterCommand("fm", &FragMessages, AllowSpectator | AllowDead);
		RegisterCommand("forfeit", &Forfeit, AllowDead, true);
		RegisterCommand("help", &Help, AllowDead | AllowSpectator, true);
		RegisterCommand("hook", &Hook, {}, true);
		RegisterCommand("id", &CrosshairID, AllowSpectator | AllowDead);
		RegisterCommand("impulse", &Impulse);
		RegisterCommand("kb", &KillBeep, AllowSpectator | AllowDead);
		RegisterCommand("kill", &Kill);
		RegisterCommand("mapcycle", &MapCycle, AllowDead | AllowSpectator);
		RegisterCommand("mapinfo", &MapInfo, AllowDead | AllowSpectator);
		RegisterCommand("mappool", &MapPool, AllowDead | AllowSpectator);
		RegisterCommand("motd", &Motd, AllowSpectator | AllowIntermission);
		RegisterCommand("mymap", &MyMap, AllowDead | AllowSpectator);
		RegisterCommand("score", &Score, AllowDead | AllowIntermission | AllowSpectator, true);
		RegisterCommand("say", &Say, AllowDead | AllowIntermission | AllowSpectator, true);
		RegisterCommand("say_team", &SayTeam, AllowDead | AllowIntermission | AllowSpectator, true);
		RegisterCommand("steam", &SayTeam, AllowDead | AllowIntermission | AllowSpectator, true);
		RegisterCommand("sr", &MySkill, AllowDead | AllowSpectator);
		RegisterCommand("stats", &Stats, AllowIntermission | AllowSpectator);
		RegisterCommand("team", &JoinTeam, AllowDead | AllowSpectator);
		RegisterCommand("timein", &TimeIn, AllowDead | AllowSpectator);
		RegisterCommand("timeout", &TimeOut, AllowDead | AllowSpectator);
		RegisterCommand("tourney_ban", &TourneyBan, AllowDead | AllowSpectator);
		RegisterCommand("tourney_pick", &TourneyPick, AllowDead | AllowSpectator);
		RegisterCommand("tourney_status", &TourneyStatus, AllowDead | AllowSpectator);
		RegisterCommand("timer", &Timer, AllowSpectator | AllowDead);
		RegisterCommand("unhook", &UnHook, {}, true);
		RegisterCommand("worr_dm_join_close", &DmJoinClose, AllowDead | AllowSpectator);
		RegisterCommand("worr_dm_hostinfo", &DmHostInfo, AllowDead | AllowSpectator);
		RegisterCommand("worr_dm_matchinfo", &DmMatchInfo, AllowDead | AllowSpectator);
		RegisterCommand("worr_welcome_continue", &WelcomeContinue, AllowDead | AllowSpectator);
		RegisterCommand("worr_ui_list_prev", &WorrUiListPrev, AllowDead | AllowSpectator);
		RegisterCommand("worr_ui_list_next", &WorrUiListNext, AllowDead | AllowSpectator);
		RegisterCommand("worr_ui_list_close", &WorrUiListClose, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_menu", &WorrCallvoteMenu, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_map", &WorrCallvoteMap, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_map_flags", &WorrCallvoteMapFlags, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_map_flag", &WorrCallvoteMapFlag, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_map_clear", &WorrCallvoteMapClear, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_gametype", &WorrCallvoteGametype, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_ruleset", &WorrCallvoteRuleset, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_timelimit", &WorrCallvoteTimelimit, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_scorelimit", &WorrCallvoteScorelimit, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_unlagged", &WorrCallvoteUnlagged, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_random", &WorrCallvoteRandom, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_arena", &WorrCallvoteArena, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_nextmap", &WorrCallvoteNextMap, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_restart", &WorrCallvoteRestart, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_shuffle", &WorrCallvoteShuffle, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_balance", &WorrCallvoteBalance, AllowDead | AllowSpectator);
		RegisterCommand("worr_callvote_cointoss", &WorrCallvoteCointoss, AllowDead | AllowSpectator);
		RegisterCommand("worr_mymap_menu", &WorrMyMapMenu, AllowDead | AllowSpectator);
		RegisterCommand("worr_mymap_flags", &WorrMyMapFlags, AllowDead | AllowSpectator);
		RegisterCommand("worr_mymap_flag", &WorrMyMapFlag, AllowDead | AllowSpectator);
		RegisterCommand("worr_mymap_clear", &WorrMyMapClear, AllowDead | AllowSpectator);
		RegisterCommand("worr_mymap_queue", &WorrMyMapQueue, AllowDead | AllowSpectator);
		RegisterCommand("worr_tourney_info", &WorrTourneyInfo, AllowDead | AllowSpectator);
		RegisterCommand("worr_tourney_maps", &WorrTourneyMaps, AllowDead | AllowSpectator);
		RegisterCommand("worr_tourney_veto", &WorrTourneyVeto, AllowDead | AllowSpectator);
		RegisterCommand("worr_tourney_pick", &WorrTourneyPick, AllowDead | AllowSpectator);
		RegisterCommand("worr_tourney_ban", &WorrTourneyBan, AllowDead | AllowSpectator);
		RegisterCommand("worr_tourney_replay_menu", &WorrTourneyReplayMenu, AllowDead | AllowSpectator | AdminOnly);
		RegisterCommand("worr_tourney_replay_confirm", &WorrTourneyReplayConfirm, AllowDead | AllowSpectator | AdminOnly);
		RegisterCommand("worr_tourney_replay", &WorrTourneyReplay, AllowDead | AllowSpectator | AdminOnly);
		RegisterCommand("worr_vote_yes", &WorrVoteYes, AllowDead | AllowSpectator);
		RegisterCommand("worr_vote_no", &WorrVoteNo, AllowDead | AllowSpectator);
		RegisterCommand("worr_vote_close", &WorrVoteClose, AllowDead | AllowSpectator);
		RegisterCommand("worr_mapselector_vote", &WorrMapSelectorVote, AllowDead | AllowSpectator | AllowIntermission);
		RegisterCommand("worr_mapselector_close", &WorrMapSelectorClose, AllowDead | AllowSpectator | AllowIntermission);
		RegisterCommand("worr_matchstats_menu", &WorrMatchStatsMenu, AllowDead | AllowSpectator);
		RegisterCommand("worr_matchstats_close", &WorrMatchStatsClose, AllowDead | AllowSpectator);
		RegisterCommand("worr_forfeit_menu", &WorrForfeitMenu, AllowDead | AllowSpectator);
		RegisterCommand("worr_forfeit_yes", &WorrForfeitYes, AllowDead | AllowSpectator);
		RegisterCommand("worr_admin_menu", &WorrAdminMenu, AllowDead | AllowSpectator | AdminOnly);
		RegisterCommand("worr_admin_commands", &WorrAdminCommands, AllowDead | AllowSpectator | AdminOnly);
		RegisterCommand("wave", &Wave, {}, true);
		RegisterCommand("where", &Where, AllowSpectator);
	}

	/*
	=============
	ClientCommandController::RegisterFollowCommands
	=============
	*/
	void ClientCommandController::RegisterFollowCommands() {
		using enum CommandFlag;
		RegisterCommand("follow", &follow::Follow, AllowSpectator | AllowDead, true);
		RegisterCommand("followkiller", &follow::FollowKiller, AllowSpectator | AllowDead, true);
		RegisterCommand("followleader", &follow::FollowLeader, AllowSpectator | AllowDead, true);
		RegisterCommand("followpowerup", &follow::FollowPowerup, AllowSpectator | AllowDead, true);
	}

	/*
	=============
	ClientCommandController::RegisterInventoryCommands
	=============
	*/
	void ClientCommandController::RegisterInventoryCommands() {
		using enum CommandFlag;
		RegisterCommand("drop", &inventory::Drop);
		RegisterCommand("dropindex", &inventory::Drop);
		RegisterCommand("invdrop", &inventory::InvDrop);
		RegisterCommand("inven", &inventory::Inven, AllowDead | AllowSpectator, true);
		RegisterCommand("invnext", &inventory::InvNext, AllowSpectator | AllowIntermission, true);
		RegisterCommand("invnextp", &inventory::InvNextP, {}, true);
		RegisterCommand("invnextw", &inventory::InvNextW, {}, true);
		RegisterCommand("invprev", &inventory::InvPrev, AllowSpectator | AllowIntermission, true);
		RegisterCommand("invprevp", &inventory::InvPrevP, {}, true);
		RegisterCommand("invprevw", &inventory::InvPrevW, {}, true);
		RegisterCommand("invuse", &inventory::InvUse, AllowSpectator | AllowIntermission, true);
		RegisterCommand("putaway", &inventory::PutAway, AllowSpectator);
		RegisterCommand("setweaponpref", &inventory::SetWeaponPref, AllowDead | AllowIntermission | AllowSpectator);
		RegisterCommand("use", &inventory::Use, {}, true);
		RegisterCommand("use_index", &inventory::Use, {}, true);
		RegisterCommand("use_index_only", &inventory::Use, {}, true);
		RegisterCommand("use_only", &inventory::Use, {}, true);
		RegisterCommand("weaplast", &inventory::WeapLast, {}, true);
		RegisterCommand("weapnext", &inventory::WeapNext, {}, true);
			RegisterCommand("weapprev", &inventory::WeapPrev, {}, true);
	}

	/*
	=============
	ClientCommandController::RegisterReadinessCommands
	=============
	*/
	void ClientCommandController::RegisterReadinessCommands() {
		using enum CommandFlag;
		RegisterCommand("notready", &readiness::NotReady, AllowDead);
		RegisterCommand("ready", &readiness::Ready, AllowDead);
		RegisterCommand("ready_up", &readiness::ReadyUp, AllowDead);
		RegisterCommand("readyup", &readiness::ReadyUp, AllowDead);
	}

	/*
	=============
	RegisterClientCommands
	=============
	*/
	void RegisterClientCommands() {
		ClientCommandController::RegisterAll();
	}

} // namespace Commands



