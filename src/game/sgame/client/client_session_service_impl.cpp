/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

client_session_service_impl.cpp implementation.*/

#include "client_session_service_impl.hpp"

#include "../g_local.hpp"
#include "../gameplay/g_proball.hpp"
#include "../gameplay/client_config.hpp"
#include "client_stats_service.hpp"
#include "../commands/commands.hpp"
#include "../gameplay/g_headhunters.hpp"
#include "../monsters/m_player.hpp"
#include "../bots/bot_includes.hpp"
#include "../player/p_client_shared.hpp"
#include "../../bgame/logger.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

void BroadcastTeamChange(gentity_t* ent, Team old_team, bool inactive, bool silent);

namespace {

/*
=============
G_SpawnHasGravity

Returns true when the entity should be affected by gravity based on its
current flags and gravity scaling.
=============
*/
static bool G_SpawnHasGravity(const gentity_t* ent)
{
return !(ent->flags & FL_FLY) && ent->gravity != 0.0f;
}

/*
=============
ApplyFallingDamage

Matches legacy P_FallingDamage behavior for player movement.
=============
*/
static void ApplyFallingDamage(gentity_t* ent, const PMove& pm) {
	int damage;
	Vector3 dir;

	if (ent->health <= 0 || ent->deadFlag)
		return;

	if (ent->s.modelIndex != MODELINDEX_PLAYER)
		return;

	if (ent->moveType == MoveType::NoClip || ent->moveType == MoveType::FreeCam)
		return;

	if (pm.waterLevel == WATER_UNDER)
		return;

	if (ent->client->grapple.releaseTime >= level.time ||
		(ent->client->grapple.entity &&
			ent->client->grapple.state > GrappleState::Fly))
		return;

	float delta = pm.impactDelta;

	delta = delta * delta * 0.0001f;

	if (pm.waterLevel == WATER_WAIST)
		delta *= 0.25f;
	if (pm.waterLevel == WATER_FEET)
		delta *= 0.5f;

	if (delta < 1.0f)
		return;

	ent->client->feedback.bobTime = 0;

	if (ent->client->landmark_free_fall) {
		delta = min(30.f, delta);
		ent->client->landmark_free_fall = false;
		ent->client->landmark_noise_time = level.time + 100_ms;
	}

	if (delta < 15) {
		if (!(pm.s.pmFlags & PMF_ON_LADDER))
			ent->s.event = EV_FOOTSTEP;
		return;
	}

	ent->client->feedback.fallValue = delta * 0.5f;
	if (ent->client->feedback.fallValue > 40)
		ent->client->feedback.fallValue = 40;
	ent->client->feedback.fallTime = level.time + FALL_TIME();

	int med_min = RS(Quake3Arena) ? 40 : 30;
	int far_min = RS(Quake3Arena) ? 61 : 55;

	if (delta > med_min) {
		if (delta >= far_min)
			ent->s.event = EV_FALL_FAR;
		else
			ent->s.event = EV_FALL_MEDIUM;
		if (g_fallingDamage->integer && !Game::Has(GameFlags::Arena)) {
			const int healthBefore = ent->health;
			const int feedbackBefore = ent->client
				? (ent->client->damage.blood + ent->client->damage.armor + ent->client->damage.powerArmor)
				: 0;

			if (RS(Quake3Arena))
				damage = ent->s.event == EV_FALL_FAR ? 10 : 5;
			else {
				damage = (int)((delta - 30) / 3);
				if (damage < 1)
					damage = 1;
			}
			dir = { 0, 0, 1 };

			Damage(ent, world, world, dir, ent->s.origin, vec3_origin, damage, 0, DamageFlags::Normal, ModID::FallDamage);

			if (ent->client) {
				const int feedbackAfter = ent->client->damage.blood + ent->client->damage.armor + ent->client->damage.powerArmor;
				const int healthDelta = healthBefore - ent->health;
				if (healthDelta > 0 && ent->health > 0 && feedbackAfter == feedbackBefore) {
					// Ensure fall damage generates HUD feedback even if damage tracking misses it.
					ent->client->damage.blood += healthDelta;
					ent->client->damage.origin = ent->s.origin;
					ent->client->last_damage_time = level.time + COOP_DAMAGE_RESPAWN_TIME;
				}
			}
		}
	}
	else {
		ent->s.event = EV_FALL_SHORT;
	}

	if (ent->health)
		G_PlayerNoise(ent, pm.s.origin, PlayerNoise::Self);
}

/*
=============
ClientLogLabel

Build a concise label for client logging, including entity number and display name.
=============
*/
static std::string ClientLogLabel(const gentity_t* ent)
{
	if (!ent || !ent->client)
		return "#-1 (<no client>)";

	const char* name = ent->client->sess.netName[0] ? ent->client->sess.netName : "<unnamed>";
	return std::format("#{} ({})", ent->s.number, name);
}

/*
=============
ClientSkinOverride

Restricts players to the stock Quake 2 skin sets when the server disallows
custom skins, falling back to sensible defaults when needed.
=============
*/
static const char* ClientSkinOverride(const char* s) {
	// 1) If we allow custom skins, just pass it through
	if (g_allowCustomSkins->integer)
		return s;

	static const std::array<std::pair<std::string_view,
		std::vector<std::string_view>>,
		3> stockSkins = { {
	{ "male",   { "grunt",
				  "cipher","claymore","ctf_b","ctf_r","deaddude","disguise",
				  "flak","howitzer","insane1","insane2","insane3","major",
				  "nightops","pointman","psycho","rampage","razor","recon",
				  "rogue_b","rogue_r","scout","sniper","viper" } },
	{ "female", { "athena",
				  "brianna","cobalt","ctf_b","ctf_r","disguise","ensign",
				  "jezebel","jungle","lotus","rogue_b","rogue_r","stiletto",
				  "venus","voodoo" } },
	{ "cyborg", { "oni911",
				  "ctf_b","ctf_r","disguise","ps9000","tyr574" } }
	} };

	std::string_view req{ s };
	// 2) Split "model/skin"
	auto slash = req.find('/');
	std::string_view model = (slash != std::string_view::npos)
		? req.substr(0, slash)
		: std::string_view{};
	std::string_view skin = (slash != std::string_view::npos)
		? req.substr(slash + 1)
		: std::string_view{};

	// 3) Default to "male/grunt" if nothing sensible
	if (model.empty()) {
		model = "male";
		skin = "grunt";
	}

	// 4) Look up in our stock-skins table
	for (auto& [m, skins] : stockSkins) {
		if (m == model) {
			// 4a) If the skin is known, no change
			if (std::find(skins.begin(), skins.end(), skin) != skins.end()) {
				return s;
			}
			// 4b) Otherwise revert to this model's default skin
			auto& defaultSkin = skins.front();
			gi.Com_PrintFmt("{}: reverting to default skin: \"{}\" -> \"{}/{}\"\n", __FUNCTION__, s, m, defaultSkin);
			return G_Fmt("{}/{}", m, defaultSkin).data();
		}
	}

	// 5) Model not found at all -> global default
	gi.Com_PrintFmt("{}: model not recognized, reverting to \"male/grunt\" for \"{}\"\n", __FUNCTION__, s);
	return "male/grunt";}

/*
=================
HandleMenuMovement

Processes menu navigation and activation input for clients currently in a menu.
=================
*/
static bool HandleMenuMovement(gentity_t* ent, usercmd_t* ucmd) {
	if (!ent->client->menu.current)
		return false;

	// [Paril-KEX] handle menu movement
	int32_t menu_sign = ucmd->forwardMove > 0 ? 1 : ucmd->forwardMove < 0 ? -1 : 0;

	if (ent->client->menu_sign != menu_sign) {
		ent->client->menu_sign = menu_sign;

		if (menu_sign > 0) {
			PreviousMenuItem(ent);
			return true;
		}
		else if (menu_sign < 0) {
			NextMenuItem(ent);
			return true;
		}
	}

	if (ent->client->latchedButtons & (BUTTON_ATTACK | BUTTON_JUMP)) {
		ActivateSelectedMenuItem(ent);
		ent->client->latchedButtons &= ~(BUTTON_ATTACK | BUTTON_JUMP);
		return true;
	}

	return false;
}

/*
=============
ClientPMoveClip

Provides the PMove clip callback with world-only collisions so that
spectator and noclip traces stay constrained to BSP geometry.
=============
*/
static trace_t ClientPMoveClip(gvec3_cref_t start, gvec3_cptr_t mins, gvec3_cptr_t maxs, gvec3_cref_t end, contents_t mask) {
	return gi.game_import_t::clip(world, start, mins, maxs, end, mask);
}

/*
=================
ClientInactivityTimer

Returns false if the client is dropped due to inactivity.
=================
*/
static bool ClientInactivityTimer(gentity_t* ent) {
	if (!ent || !ent->client)
		return true;

	auto* cl = ent->client;
	if (Tournament_IsActive()) {
		cl->sess.inactivityTime = level.time + 1_min;
		cl->sess.inactivityWarning = false;
		cl->sess.inactiveStatus = false;
		return true;
	}

	// Check if inactivity is enabled
	GameTime timeout = GameTime::from_sec(g_inactivity->integer);
	if (timeout && timeout < 15_sec)
		timeout = 15_sec;

	// First-time setup
	if (!cl->sess.inactivityTime) {
		cl->sess.inactivityTime = level.time + timeout;
		cl->sess.inactivityWarning = false;
		cl->sess.inactiveStatus = false;
		return true;
	}

	// Reset conditions (ineligible for inactivity logic)
	if (!deathmatch->integer || !timeout || !ClientIsPlaying(cl) || cl->eliminated || cl->sess.is_a_bot || ent->s.number == 0) {
		cl->sess.inactivityTime = level.time + 1_min;
		cl->sess.inactivityWarning = false;
		cl->sess.inactiveStatus = false;
		return true;
	}

	// Input activity detected, reset timer
	if (cl->latchedButtons & BUTTON_ANY) {
		cl->sess.inactivityTime = level.time + timeout;
		cl->sess.inactivityWarning = false;
		cl->sess.inactiveStatus = false;
		return true;
	}

	// Timeout reached, remove player
	if (level.time > cl->sess.inactivityTime) {
		cl->sess.inactiveStatus = true;
		cl->sess.inactivityWarning = false;
		cl->sess.inactivityTime = 0_sec;
		gi.LocClient_Print(ent, PRINT_CENTER, "You have been removed from the match\ndue to inactivity.\n");
		worr::Logf(worr::LogLevel::Warn, "{}: dropping {} for inactivity", __FUNCTION__, ClientLogLabel(ent));
		SetTeam(ent, Team::Spectator, true, true, false);
		return false;
	}

	// Warning 10 seconds before timeout
	if (!cl->sess.inactivityWarning && level.time > cl->sess.inactivityTime - 10_sec) {
		cl->sess.inactivityWarning = true;
		gi.LocClient_Print(ent, PRINT_CENTER, "Ten seconds until inactivity trigger!\n");
		gi.localSound(ent, CHAN_AUTO, gi.soundIndex("world/fish.wav"), 1, ATTN_NONE, 0);
		worr::Logf(worr::LogLevel::Trace, "{}: inactivity warning sent to {}", __FUNCTION__, ClientLogLabel(ent));
	}

	return true;
}

/*
=================
ClientTimerActions_ApplyRegeneration

Applies the regeneration powerup's periodic health ticks.
=================
*/
static void ClientTimerActions_ApplyRegeneration(gentity_t* ent) {
	gclient_t* cl = ent->client;
	if (!cl)
		return;

	if (ent->health <= 0 || ent->client->eliminated)
		return;

	if (cl->PowerupTimer(PowerupTimer::Regeneration) <= level.time)
		return;

	if (g_vampiric_damage->integer || !game.map.spawnHealth)
		return;

	if (CombatIsDisabled())
		return;

	float   volume = cl->PowerupCount(PowerupCount::SilencerShots) ? 0.2f : 1.0;
	int     max = cl->pers.maxHealth;
	int     bonus = 0;

	if (ent->health < max) {
		bonus = 15;
	}
	else if (ent->health < max * 2) {
		bonus = 5;
	}

	if (!bonus)
		return;

	ent->health += bonus;
	if (ent->health > max)
		ent->health = max;
	gi.sound(ent, CHAN_AUX, gi.soundIndex("items/regen.wav"), volume, ATTN_NORM, 0);
	cl->pu_regen_time_blip = level.time + 100_ms;}

/*
==================
ClientTimerActions

Actions that happen once a second for player maintenance tasks.
==================
*/
static void ClientTimerActions(gentity_t* ent) {
	if (ent->client->timeResidual > level.time)
		return;

	if (RS(Quake3Arena)) {
		// count down health when over max
		if (ent->health > ent->client->pers.maxHealth)
			ent->health--;

		// count down armor when over max
		if (ent->client->pers.inventory[IT_ARMOR_COMBAT] > ent->client->pers.maxHealth)
			ent->client->pers.inventory[IT_ARMOR_COMBAT]--;
	}
	else {
		if (ent->client->pers.healthBonus > 0) {
			if (ent->health <= 0 || ent->health <= ent->client->pers.maxHealth) {
				ent->client->pers.healthBonus = 0;
			}
			else {
				ent->health--;
				ent->client->pers.healthBonus--;
			}
		}
	}
	ClientTimerActions_ApplyRegeneration(ent);
	ent->client->timeResidual = level.time + 1_sec;}

/*
=============
CheckBanned

Determines whether the connecting player should be rejected based on a
hard-coded ban list. When tripped, the function plays local feedback and
requests that the server kick the player immediately.
=============
*/
static bool CheckBanned(local_game_import_t& gi, LevelLocals& level, gentity_t* ent, char* userInfo,
		const char* socialID) {
	if (!socialID || !*socialID)
		return false;

	// currently all bans are in Steamworks and Epic, don't bother if not from there
	if (socialID[0] != 'S' && socialID[0] != 'E')
		return false;

	// Israel
	if (!Q_strcasecmp(socialID, "Steamworks-76561198026297488")) {
		gi.Info_SetValueForKey(userInfo, "rejmsg", "Antisemite detected!\n");

		if (host && host->client) {
			if (level.time > host->client->lastBannedMessageTime + 10_sec) {
				char name[MAX_INFO_VALUE] = { 0 };
				gi.Info_ValueForKey(userInfo, "name", name, sizeof(name));

				gi.LocClient_Print(host, PRINT_TTS, "ANTISEMITE DETECTED ({})!\n", name);
				host->client->lastBannedMessageTime = level.time;
				gi.LocBroadcast_Print(PRINT_CHAT, "{}: God Bless Palestine\n", name);
			}
		}

		gi.localSound(ent, CHAN_AUTO, gi.soundIndex("world/klaxon3.wav"), 1, ATTN_NONE, 0);
		gi.AddCommandString(G_Fmt("kick {}\n", ent - g_entities - 1).data());
		return true;
	}

	// Kirlomax
	if (!Q_strcasecmp(socialID, "Steamworks-76561198001774610")) {
		gi.Info_SetValueForKey(userInfo, "rejmsg", "WARNING! KNOWN CHEATER DETECTED\n");

		if (host && host->client) {
			if (level.time > host->client->lastBannedMessageTime + 10_sec) {
				char name[MAX_INFO_VALUE] = { 0 };
				gi.Info_ValueForKey(userInfo, "name", name, sizeof(name));

				gi.LocClient_Print(host, PRINT_TTS, "WARNING! KNOWN CHEATER DETECTED ({})!\n", name);
				host->client->lastBannedMessageTime = level.time;
				gi.LocBroadcast_Print(PRINT_CHAT, "{}: I am a known cheater, banned from all servers.\n", name);
			}
		}

		gi.localSound(ent, CHAN_AUTO, gi.soundIndex("world/klaxon3.wav"), 1, ATTN_NONE, 0);
		gi.AddCommandString(G_Fmt("kick {}\n", ent - g_entities - 1).data());
		return true;
	}

	// Model192
	if (!Q_strcasecmp(socialID, "Steamworks-76561197972296343")) {
		gi.Info_SetValueForKey(userInfo, "rejmsg", "WARNING! MOANERTONE DETECTED\n");

		if (host && host->client) {
			if (level.time > host->client->lastBannedMessageTime + 10_sec) {
				char name[MAX_INFO_VALUE] = { 0 };
				gi.Info_ValueForKey(userInfo, "name", name, sizeof(name));

				gi.LocClient_Print(host, PRINT_TTS, "WARNING! MOANERTONE DETECTED ({})!\n", name);
				host->client->lastBannedMessageTime = level.time;
				gi.LocBroadcast_Print(PRINT_CHAT, "{}: Listen up, I have something to moan about.\n", name);
			}
		}

		gi.localSound(ent, CHAN_AUTO, gi.soundIndex("world/klaxon3.wav"), 1, ATTN_NONE, 0);
		gi.AddCommandString(G_Fmt("kick {}\n", ent - g_entities - 1).data());
		return true;
	}

	// Dalude
	if (!Q_strcasecmp(socialID, "Steamworks-76561199001991246") ||
		!Q_strcasecmp(socialID, "EOS-07e230c273be4248bbf26c89033923c1")) {
		ent->client->sess.is_888 = true;
		gi.Info_SetValueForKey(userInfo, "rejmsg", "Fake 888 Agent detected!\n");
		gi.Info_SetValueForKey(userInfo, "name", "Fake 888 Agent");

		if (host && host->client) {
			if (level.time > host->client->lastBannedMessageTime + 10_sec) {
				char name[MAX_INFO_VALUE] = { 0 };
				gi.Info_ValueForKey(userInfo, "name", name, sizeof(name));

				gi.LocClient_Print(host, PRINT_TTS, "FAKE 888 AGENT DETECTED ({})!\n", name);
				host->client->lastBannedMessageTime = level.time;
				gi.LocBroadcast_Print(PRINT_CHAT, "{}: bejesus, what a lovely lobby! certainly better than 888's!\n", name);
			}
		}
		gi.localSound(ent, CHAN_AUTO, gi.soundIndex("world/klaxon3.wav"), 1, ATTN_NONE, 0);
		gi.AddCommandString(G_Fmt("kick {}\n", ent - g_entities - 1).data());
		return true;
	}

	return false;}

/*
================
ClientCheckPermissions

Updates the client's admin/banned flags based on the configured social ID
lists.
================
*/
static void ClientCheckPermissions(GameLocals& game, gentity_t* ent, const char* socialID) {
	if (!socialID || !*socialID) {
		ent->client->sess.banned = false;
		ent->client->sess.admin = false;
		return;
	}

	std::string id(socialID);

	ent->client->sess.banned = game.bannedIDs.contains(id);
	ent->client->sess.admin = game.adminIDs.contains(id);
}

} // namespace

/*
=============
ClientCheckPermissionsForTesting

Delegates to ClientCheckPermissions so tests can validate permission resets.
=============
*/
void ClientCheckPermissionsForTesting(GameLocals& game, gentity_t* ent, const char* socialID) {
	ClientCheckPermissions(game, ent, socialID);
}

namespace worr::server::client {

static ClientSessionServiceImpl::ClientBeginServerFrameFreezeHook clientBeginServerFrameFreezeHook = nullptr;

/*
=============
ClientSessionServiceImpl::SetClientBeginServerFrameFreezeHookForTests

Registers a test-only hook that can short-circuit freeze-tag processing inside
ClientBeginServerFrame.
=============
*/
void ClientSessionServiceImpl::SetClientBeginServerFrameFreezeHookForTests(ClientBeginServerFrameFreezeHook hook) {
	clientBeginServerFrameFreezeHook = hook;
}

/*
=============
ClientSessionServiceImpl::ClientSessionServiceImpl

Stores references to the game state objects and persistence services that were
previously accessed via globals so the service can eventually operate without
that implicit coupling.
=============
*/
ClientSessionServiceImpl::ClientSessionServiceImpl(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
ClientConfigStore& configStore, ClientStatsService& statsService)
: gi_(gi)
, game_(game)
, level_(level)
, configStore_(configStore)
, statsService_(statsService) {}

/*
=============
ClientSessionServiceImpl::ClientConnect

Implements the legacy ClientConnect logic behind the service seam so future
callers can transition away from the procedural entry point.
=============
*/
bool ClientSessionServiceImpl::ClientConnect(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
gentity_t* ent, char* userInfo, const char* socialID, bool isBot) {
	if (!ent)
		return false;

	if (!g_entities || !game.clients || globals.numEntities <= 0)
		return false;

	const ptrdiff_t entIndex = ent - g_entities;

	if (entIndex < 1 || entIndex >= globals.numEntities)
		return false;

	if (entIndex - 1 >= game.maxClients)
		return false;

	const char* safeSocialID = (socialID && *socialID) ? socialID : "";
	auto& configStore = configStore_;

	if (!userInfo)
		userInfo = const_cast<char*>("");

	// they can connect
	ent->client = game.clients + (ent - g_entities - 1);

	ent->client->sess.is_a_bot = isBot;
	ent->client->sess.consolePlayer = false;
	ent->client->sess.admin = false;
	ent->client->sess.banned = false;
	ent->client->sess.is_888 = false;
	
	if (!isBot) {
		if (CheckBanned(gi, level, ent, userInfo, safeSocialID))
		return false;

		ClientCheckPermissions(game, ent, safeSocialID);
	}

	ent->client->sess.team = deathmatch->integer ? Team::None : Team::Free;
	
	// set up userInfo early
	ClientUserinfoChanged(gi, game, level, ent, userInfo);

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->inUse == false) {
		// clear the respawning variables

		if (!ent->client->sess.initialised && ent->client->sess.team == Team::None) {
			ent->client->pers.introTime = 3_sec;

			// force team join
			ent->client->sess.team = deathmatch->integer ? Team::None : Team::Free;
			ent->client->sess.pc = {};

			worr::server::client::InitClientResp(ent->client);

			ent->client->sess.playStartRealTime = GetCurrentRealTimeMillis();
		}

		if (!game.autoSaved || !ent->client->pers.weapon)
			InitClientPersistant(ent, ent->client);
	}

	// make sure we start with known default(s)
	ent->svFlags = SVF_PLAYER;

	if (isBot) {
		ent->svFlags |= SVF_BOT;

		if (bot_name_prefix->string[0] && *bot_name_prefix->string) {
			std::array<char, MAX_NETNAME> oldName = {};
			std::array<char, MAX_NETNAME> newName = {};

			gi.Info_ValueForKey(userInfo, "name", oldName.data(), oldName.size());
			Q_strlcpy(newName.data(), bot_name_prefix->string, newName.size());
			Q_strlcat(newName.data(), oldName.data(), newName.size());
			gi.Info_SetValueForKey(userInfo, "name", newName.data());
	}
	}

	// set up userInfo early
	ClientUserinfoChanged(gi, game, level, ent, userInfo);

	Q_strlcpy(ent->client->sess.socialID, safeSocialID, sizeof(ent->client->sess.socialID));

	std::array<char, MAX_INFO_VALUE> value = {};
	// [Paril-KEX] fetch name because now netName is kinda unsuitable
	gi.Info_ValueForKey(userInfo, "name", value.data(), value.size());
	Q_strlcpy(ent->client->sess.netName, value.data(), sizeof(ent->client->sess.netName));

	ent->client->sess.skillRating = 0;
	ent->client->sess.skillRatingChange = 0;

	if (!isBot) {
		if (ent->client->sess.socialID[0]) {
			configStore.LoadProfile(ent->client, ent->client->sess.socialID, value.data(),
				Game::GetCurrentInfo().short_name_upper.data());
			PCfg_ClientInitPConfig(ent);
		}
		else {
			ent->client->sess.skillRating = configStore.DefaultSkillRating();
		}

		if (ent->client->sess.banned) {
			gi.LocBroadcast_Print(PRINT_HIGH, "BANNED PLAYER {} connects.\n", value.data());
			gi.AddCommandString(G_Fmt("kick {}\n", ent - g_entities - 1).data());
			return false;
		}

		if (ent->client->sess.skillRating > 0) {
			gi.LocBroadcast_Print(PRINT_HIGH, "{} connects. (SR: {})\n", value.data(),
				ent->client->sess.skillRating);
		}
		else {
			gi.LocBroadcast_Print(PRINT_HIGH, "$g_player_connected", value.data());
		}

		// entity 1 is always server host, so make admin
		if (ent == &g_entities[1])
			ent->client->sess.admin = true;

		// Detect if client is on a console system
		if (*safeSocialID && (
				_strnicmp(safeSocialID, "PSN", 3) == 0 ||
				_strnicmp(safeSocialID, "NX", 2) == 0 ||
				_strnicmp(safeSocialID, "GDK", 3) == 0
			)) {
			ent->client->sess.consolePlayer = true;
		}
		else {
			ent->client->sess.consolePlayer = false;
		}
	}

	Client_RebuildWeaponPreferenceOrder(*ent->client);

	if (level.endmatch_grace)
		level.endmatch_grace = 0_ms;

	// set skin
	std::array<char, MAX_INFO_VALUE> val = {};
	if (!gi.Info_ValueForKey(userInfo, "skin", val.data(), sizeof(val)))
		Q_strlcpy(val.data(), "male/grunt", val.size());
	const char* sanitizedSkin = ClientSkinOverride(val.data());
	if (Q_strcasecmp(ent->client->sess.skinName.c_str(), sanitizedSkin)) {
		ent->client->sess.skinName = sanitizedSkin;
		ent->client->sess.skinIconIndex = gi.imageIndex(G_Fmt("/players/{}_i",
			ent->client->sess.skinName).data());
	}

	// count current clients and rank for scoreboard
	CalculateRanks();
	ent->client->pers.connected = true;
	ent->client->sess.inGame = false;

	// [Paril-KEX] force a state update
	ent->sv.init = false;

	return true;
}

/*=============
ClientSessionServiceImpl::ClientBegin

Fully manages the transition from connection to active play, including
initialization, spawn handling, and intermission placement.
=============
*/
void ClientSessionServiceImpl::ClientBegin(local_game_import_t& gi, GameLocals& game, LevelLocals& level, gentity_t* ent) {
	gclient_t* cl = game.clients + (ent - g_entities - 1);
	cl->awaitingRespawn = false;
	cl->respawn_timeout = 0_ms;
	const bool initialJoin = !cl->sess.inGame;
	worr::Logf(worr::LogLevel::Debug, "{}: begin for {} (initial:{}, deathmatch:{})", __FUNCTION__, ClientLogLabel(ent), initialJoin, !!deathmatch->integer);

	// set inactivity timer
	GameTime cv = GameTime::from_sec(g_inactivity->integer);
	if (cv) {
		if (cv < 15_sec) cv = 15_sec;
		cl->sess.inactivityTime = level.time + cv;
		cl->sess.inactivityWarning = false;
	}

	// [Paril-KEX] we're always connected by this point...
	cl->pers.connected = true;

	if (deathmatch->integer) {
		worr::server::client::ClientBeginDeathmatch(ent);
		worr::Logf(worr::LogLevel::Trace, "{}: deathmatch begin for {}", __FUNCTION__, ClientLogLabel(ent));

		if (initialJoin)
			cl->sess.inGame = true;

		if (game.marathon.active && level.matchState >= MatchState::In_Progress)
			Marathon_RegisterClientBaseline(ent->client);

		// count current clients and rank for scoreboard
		CalculateRanks();
		return;
	}

	// [Paril-KEX] set enter time now, so we can send messages slightly
	// after somebody first joins
	cl->resp.enterTime = level.time;
	cl->pers.spawned = true;

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->inUse) {
		// the client has cleared the client side viewAngles upon
		// connecting to the server, which is different than the
		// state when the game is saved, so we need to compensate
		// with deltaangles
		cl->ps.pmove.deltaAngles = cl->ps.viewAngles;
		worr::Logf(worr::LogLevel::Trace, "{}: reusing persisted entity state for {}", __FUNCTION__, ClientLogLabel(ent));
	}
	else {
		// a spawn point will completely reinitialize the entity
		// except for the persistant data that was initialized at
		// ClientConnect() time
		InitGEntity(ent);
		ent->className = "player";
		worr::server::client::InitClientResp(cl);
		cl->coopRespawn.spawnBegin = true;
		worr::server::client::ClientCompleteSpawn(ent);
		cl->coopRespawn.spawnBegin = false;
		worr::Logf(worr::LogLevel::Debug, "{}: fresh spawn initialization complete for {}", __FUNCTION__, ClientLogLabel(ent));

		if (initialJoin) {
			BroadcastTeamChange(ent, Team::None, false, false);
			gi.Com_PrintFmt("{}: initial join broadcast for client {}\n", __FUNCTION__, ent->s.number);
		}
	}

	// make sure we have a known default
	ent->svFlags |= SVF_PLAYER;

	if (level.intermission.time) {
		worr::Logf(worr::LogLevel::Trace, "{}: moving {} to intermission", __FUNCTION__, ClientLogLabel(ent));
		MoveClientToIntermission(ent);
	}
	else {
		// send effect if in a multiplayer game
		if (game.maxClients > 1 && !(ent->svFlags & SVF_NOCLIENT))
			gi.LocBroadcast_Print(PRINT_HIGH, "$g_entered_game", cl->sess.netName);
		worr::Logf(worr::LogLevel::Debug, "{}: {} entered active play", __FUNCTION__, ClientLogLabel(ent));
	}

	level.campaign.coopScalePlayers++;
	G_Monster_CheckCoopHealthScaling();

	// make sure all view stuff is valid
	ClientEndServerFrame(ent);

	// [Paril-KEX] send them goal, if needed
	G_PlayerNotifyGoal(ent);

	// [Paril-KEX] we're going to set this here just to be certain
	// that the level entry timer only starts when a player is actually
	// *in* the level
	worr::server::client::G_SetLevelEntry();

	cl->sess.inGame = true;
}

/*
=============
ClientSessionServiceImpl::ClientUserinfoChanged

Parses and applies userinfo updates, keeping both gameplay and presentation
state (skins, FOV, handedness) synchronized.
=============
*/
void ClientSessionServiceImpl::ClientUserinfoChanged(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
gentity_t* ent, const char* userInfo) {
	if (!userInfo)
		userInfo = "";

	std::array<char, MAX_INFO_VALUE> value{};
	std::array<char, MAX_INFO_VALUE> nameBuffer{};

	// set name
	if (!gi.Info_ValueForKey(userInfo, "name", nameBuffer.data(), nameBuffer.size()))
		Q_strlcpy(nameBuffer.data(), "badinfo", nameBuffer.size());
	Q_strlcpy(ent->client->sess.netName, nameBuffer.data(), sizeof(ent->client->sess.netName));

	// set skin
	if (!gi.Info_ValueForKey(userInfo, "skin", value.data(), value.size()))
		Q_strlcpy(value.data(), "male/grunt", value.size());

	const char* sanitizedSkin = ClientSkinOverride(value.data());
	std::string_view sanitizedSkinView{ sanitizedSkin };
	if (ent->client->sess.skinName != sanitizedSkinView)
		ent->client->sess.skinName.assign(sanitizedSkinView);

	std::string iconPath = G_Fmt("/players/{}_i", ent->client->sess.skinName).data();
	ent->client->sess.skinIconIndex = gi.imageIndex(iconPath.c_str());

	worr::Logf(worr::LogLevel::Trace, "{}: userinfo updated for {} (name:{} skin:{})", __FUNCTION__, ClientLogLabel(ent), ent->client->sess.netName, ent->client->sess.skinName);

	int playernum = ent - g_entities - 1;

	// combine name and skin into a configstring
	const std::string& sessionSkin = ent->client->sess.skinName;
	if (Teams())
		AssignPlayerSkin(ent, sessionSkin);
	else {
		gi.configString(CS_PLAYERSKINS + playernum, G_Fmt("{}\\{}", ent->client->sess.netName, sessionSkin).data());
	}

	//  set player name field (used in id_state view)
	gi.configString(CONFIG_CHASE_PLAYER_NAME + playernum, ent->client->sess.netName);

	// [Kex] netName is used for a couple of other things, so we update this after those.
	if (!(ent->svFlags & SVF_BOT)) {
		const auto encodedName = G_EncodedPlayerName(ent);
		Q_strlcpy(ent->client->pers.netName, encodedName.c_str(), sizeof(ent->client->pers.netName));
	}

	// fov
	if (!gi.Info_ValueForKey(userInfo, "fov", value.data(), value.size())) {
		std::snprintf(value.data(), value.size(), "%.0f", ent->client->ps.fov);
	}
	ent->client->ps.fov = std::clamp(static_cast<float>(strtoul(value.data(), nullptr, 10)), 1.f, 160.f);

	// handedness
	if (gi.Info_ValueForKey(userInfo, "hand", value.data(), value.size())) {
		ent->client->pers.hand = static_cast<Handedness>(std::clamp(atoi(value.data()), static_cast<int>(Handedness::Right), static_cast<int>(Handedness::Center)));
	}
	else {
		ent->client->pers.hand = Handedness::Right;
	}

	// [Paril-KEX] auto-switch
	if (gi.Info_ValueForKey(userInfo, "autoswitch", value.data(), value.size())) {
		ent->client->pers.autoswitch = static_cast<WeaponAutoSwitch>(std::clamp(atoi(value.data()), static_cast<int>(WeaponAutoSwitch::Smart), static_cast<int>(WeaponAutoSwitch::Never)));
	}
	else {
		ent->client->pers.autoswitch = WeaponAutoSwitch::Smart;
	}

	if (gi.Info_ValueForKey(userInfo, "autoshield", value.data(), value.size())) {
		ent->client->pers.autoshield = atoi(value.data());
	}
	else {
		ent->client->pers.autoshield = -1;
	}

	// [Paril-KEX] wants bob
	if (gi.Info_ValueForKey(userInfo, "bobskip", value.data(), value.size())) {
		ent->client->pers.bob_skip = value[0] == '1';
	}
	else {
		ent->client->pers.bob_skip = false;
	}

	// save off the userInfo in case we want to check something later
	Q_strlcpy(ent->client->pers.userInfo, userInfo, sizeof(ent->client->pers.userInfo));}

/*
=============
ClientSessionServiceImpl::ClientDisconnect

Handles the disconnect workflow previously implemented procedurally, ensuring
the player's state is torn down and other systems are notified appropriately
while reporting status via DisconnectResult.
=============
*/

DisconnectResult ClientSessionServiceImpl::ClientDisconnect(local_game_import_t& gi, GameLocals& game, LevelLocals& level, gentity_t* ent) {
	if (!ent || !ent->client) {
		return DisconnectResult::InvalidEntity;
	}

	(void)game;

	gclient_t* cl = ent->client;
	const int64_t now = GetCurrentRealTimeMillis();
	cl->sess.playEndRealTime = now;
	worr::server::client::P_AccumulateMatchPlayTime(cl, now);

	OnDisconnect(gi, ent);

	if (Tournament_IsActive() && Tournament_IsParticipant(cl) &&
		level.matchState == MatchState::In_Progress &&
		match_timeoutLength && match_timeoutLength->integer > 0 &&
		level.timeoutActive <= 0_ms) {
		level.timeoutOwner = world;
		level.timeoutActive = GameTime::from_sec(match_timeoutLength->integer);
		game.tournament.autoTimeoutActive = true;
		gi.LocBroadcast_Print(PRINT_CENTER,
			".Tournament timeout: {} disconnected.\n{} remaining.",
			cl->sess.netName,
			TimeString(match_timeoutLength->integer * 1000, false, false));
		G_LogEvent("MATCH TIMEOUT STARTED");
	}

	if (cl->trackerPainTime) {
		RemoveAttackingPainDaemons(ent);
	}

	if (cl->ownedSphere) {
		if (cl->ownedSphere->inUse) {
			FreeEntity(cl->ownedSphere);
		}
		cl->ownedSphere = nullptr;
	}

	PlayerTrail_Destroy(ent);
	PlayerShellLight_Destroy(ent);

	ProBall::HandleCarrierDisconnect(ent);
	Harvester_HandlePlayerDisconnect(ent);

	HeadHunters::DropHeads(ent, nullptr);
	HeadHunters::ResetPlayerState(cl);

	if (!(ent->svFlags & SVF_NOCLIENT)) {
		TossClientItems(ent);

		gi.WriteByte(svc_muzzleflash);
		gi.WriteEntity(ent);
		gi.WriteByte(MZ_LOGOUT);
		gi.multicast(ent->s.origin, MULTICAST_PVS, false);
	}

	if (cl->pers.connected && cl->sess.initialised && !cl->sess.is_a_bot) {
		if (cl->sess.netName[0]) {
			gi.LocBroadcast_Print(PRINT_HIGH, "{} disconnected.", cl->sess.netName);
		}
	}

	FreeClientFollowers(ent);

	const int clientIndex = ent->s.number - 1;
	MapSelector_ClearVote(level, clientIndex);
	MapSelector_SyncVotes(level);

	G_RevertVote(cl);

	P_SaveGhostSlot(ent);

	const bool wasSpawned = cl->pers.spawned;
	MatchStatsContext statsContext{};

	if (wasSpawned) {
		statsContext = BuildMatchStatsContext(level);
	}

	gi.unlinkEntity(ent);
	ent->s.modelIndex = 0;
	ent->solid = SOLID_NOT;
	ent->inUse = false;
	ent->sv.init = false;
	ent->className = "disconnected";
	cl->pers.connected = false;
	cl->sess.inGame = false;
	cl->sess.matchWins = 0;
	cl->sess.matchLosses = 0;
	cl->pers.limitedLivesPersist = false;
	cl->pers.limitedLivesStash = 0;
	cl->pers.spawned = false;
	ent->timeStamp = level.time + 1_sec;

	if (wasSpawned) {
		statsService_.SaveStatsForDisconnect(statsContext, ent);
	}

	if (deathmatch->integer) {
		CalculateRanks();

		for (auto ec : active_clients()) {
			if (ec->client->showScores) {
				ec->client->menu.updateTime = level.time;
			}
		}
	}

	return DisconnectResult::Success;
}


/*
=============
ClientSessionServiceImpl::OnDisconnect

Validates that the player's ready state can be cleared and, when appropriate,
broadcasts the change before the rest of the disconnect teardown executes.
=============
*/

void ClientSessionServiceImpl::OnDisconnect(local_game_import_t& gi, gentity_t* ent) {
	if (!ent || !ent->client) {
		return;
	}

	gclient_t* cl = ent->client;

	if (!cl->pers.readyStatus) {
		return;
	}

	const bool canUpdateReady = ReadyConditions(ent, false);
	cl->pers.readyStatus = false;

	if (canUpdateReady && cl->sess.netName[0]) {
		gi.LocBroadcast_Print(PRINT_CENTER,
				"%bind:+wheel2:Use Compass to toggle your ready status.%.MATCH IS IN WARMUP\n{} is NOT ready.",
				cl->sess.netName);
	}
}

/*
=============
ClientSessionServiceImpl::ApplySpawnFlags

Copies the spawn temp flags collected by the map parser onto the entity so
that bots, humans, and arena assignments are honored consistently.
=============
*/
void ClientSessionServiceImpl::ApplySpawnFlags(gentity_t* ent) const {
	if (!ent) {
		return;
	}

	if (st.was_key_specified("noBots")) {
		if (st.noBots) {
			ent->flags |= FL_NO_BOTS;
		}
		else {
			ent->flags &= ~FL_NO_BOTS;
		}
	}

	if (st.was_key_specified("noHumans")) {
		if (st.noHumans) {
			ent->flags |= FL_NO_HUMANS;
		}
		else {
			ent->flags &= ~FL_NO_HUMANS;
		}
	}

	if (st.arena) {
		ent->arena = st.arena;
	}
	else if (!st.was_key_specified("arena")) {
		ent->arena = 0;
	}
}

/*
=============
ClientSessionServiceImpl::PrepareSpawnPoint

Ensures the spawn point is valid by freeing stuck points and optionally
configuring the delayed drop logic used by certain N64 spawn locations.
=============
*/
void ClientSessionServiceImpl::PrepareSpawnPoint(gentity_t* ent, bool allowElevatorDrop,
		void (*dropThink)(gentity_t*)) const {
	if (!ent) {
		return;
	}

	const trace_t tr = gi_.trace(ent->s.origin, PLAYER_MINS, PLAYER_MAXS,
		ent->s.origin, ent, MASK_SOLID);

	if (tr.startSolid) {
		G_FixStuckObject(ent, ent->s.origin);
	}

	if (allowElevatorDrop && level_.isN64 && dropThink) {
		ent->think = dropThink;
		ent->nextThink = level_.time + FRAME_TIME_S;
	}}

/*
=============
ClientSessionServiceImpl::ClientThink

Executes the per-frame simulation for a client, handling input processing,
movement, inactivity timers, and weapon logic.
=============
*/
void ClientSessionServiceImpl::ClientThink(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
gentity_t* ent, usercmd_t* ucmd) {
        gclient_t* cl;
        gentity_t* other;
        PMove           pm{};

	level.currentEntity = ent;
	cl = ent->client;
	bool menuHandled = false;

	//no movement during map or match intermission
	if (level.timeoutActive > 0_ms) {
		cl->resp.cmdAngles[PITCH] = ucmd->angles[PITCH];
		cl->resp.cmdAngles[YAW] = ucmd->angles[YAW];
		cl->resp.cmdAngles[ROLL] = ucmd->angles[ROLL];
		cl->ps.pmove.pmType = PM_FREEZE;
		return;
	}

	// [Paril-KEX] pass buttons through even if we are in intermission or
	// chasing.
	cl->oldButtons = cl->buttons;
	cl->buttons = ucmd->buttons;
	cl->latchedButtons |= cl->buttons & ~cl->oldButtons;
	cl->cmd = *ucmd;

	if (cl->menu.current)
		menuHandled = HandleMenuMovement(ent, ucmd);

	if ((cl->latchedButtons & BUTTON_USE) && FreezeTag_IsActive() && ClientIsPlaying(cl) && !cl->eliminated) {
		if (gentity_t* target = worr::server::client::FreezeTag_FindFrozenTarget(ent)) {
			gclient_t* targetCl = target->client;

			if (targetCl && !targetCl->resp.thawer && worr::server::client::FreezeTag_IsValidThawHelper(ent, target)) {
				worr::server::client::FreezeTag_StartThawHold(ent, target);
			}
		}

		cl->latchedButtons &= ~BUTTON_USE;
	}

	const bool initialMenuReady = (cl->initialMenu.delay && level.time > cl->initialMenu.delay);
	if (cl->initialMenu.frozen && ent == host && g_autoScreenshotTool->integer) {
		cl->initialMenu.frozen = false;
		cl->initialMenu.shown = true;
		cl->initialMenu.delay = 0_sec;
		cl->initialMenu.hostSetupDone = true;
	}

	auto showInitialMenu = [&](gentity_t* player) {
		if (!player || !player->client)
			return;

		if ((player->svFlags & SVF_BOT) || player->client->sess.is_a_bot) {
			player->client->initialMenu.frozen = false;
			player->client->initialMenu.hostSetupDone = true;
			return;
		}

		if (player == host) {
			if (g_autoScreenshotTool->integer)
				return;

			if (player->client->initialMenu.frozen && !player->client->initialMenu.hostSetupDone) {
				OpenSetupWelcomeMenu(player);
				player->client->initialMenu.hostSetupDone = true;
				return;
			}

			if (!player->client->initialMenu.frozen && g_owner_push_scores->integer) {
				Commands::Score(player, CommandArgs{});
				return;
			}
		}

		OpenJoinMenu(player);
	};

	if (cl->initialMenu.frozen) {
		if (!ClientIsPlaying(cl)) {
			const bool needsOpen = (!cl->initialMenu.shown && initialMenuReady) || (cl->initialMenu.shown && !cl->menu.current);
			if (needsOpen) {
				showInitialMenu(ent);
				cl->initialMenu.delay = 0_sec;
				cl->initialMenu.shown = true;
			}
		}
	}
	else if (!cl->initialMenu.shown && initialMenuReady) {
		if (!ClientIsPlaying(cl) && (!cl->sess.initialised || cl->sess.inactiveStatus)) {
			showInitialMenu(ent);
			//if (!cl->initialMenu.shown)
			//      gi.LocClient_Print(ent, PRINT_CHAT, "Welcome to {} v{}.\n", worr::version::kGameTitle, worr::version::kGameVersion);
			cl->initialMenu.delay = 0_sec;
			cl->initialMenu.shown = true;
		}
	}

	// check for queued follow targets
	if (!ClientIsPlaying(cl)) {
		if (cl->follow.queuedTarget && level.time > cl->follow.queuedTime + 500_ms) {
			cl->follow.target = cl->follow.queuedTarget;
			cl->follow.update = true;
			cl->follow.queuedTarget = nullptr;
			cl->follow.queuedTime = 0_sec;
			ClientUpdateFollowers(ent);
		}
	}

	// check for inactivity timer
	if (!ClientInactivityTimer(ent))
		return;

	if (g_quadhog->integer)
		if (cl->PowerupTimer(PowerupTimer::QuadDamage) > 0_sec && level.time >= cl->PowerupTimer(PowerupTimer::QuadDamage))
			QuadHog_SetupSpawn(0_ms);

	if (cl->sess.teamJoinTime) {
		GameTime delay = 5_sec;
		if (cl->sess.motdModificationCount != game.motdModificationCount) {
			if (level.time >= cl->sess.teamJoinTime + delay) {
				if (g_showmotd->integer && game.motd.size()) {
					gi.LocCenter_Print(ent, "{}", game.motd.c_str());
					delay += 5_sec;
					cl->sess.motdModificationCount = game.motdModificationCount;
				}
			}
		}
		if (!cl->sess.showed_help && g_showhelp->integer) {
			if (level.time >= cl->sess.teamJoinTime + delay) {
				worr::server::client::PrintModifierIntro(ent);

				cl->sess.showed_help = true;
			}
		}
	}

	if ((ucmd->buttons & BUTTON_CROUCH) && pm_config.n64Physics) {
		if (cl->pers.n64_crouch_warn_times < 12 &&
			cl->pers.n64_crouch_warning < level.time &&
			(++cl->pers.n64_crouch_warn_times % 3) == 0) {
			cl->pers.n64_crouch_warning = level.time + 10_sec;
			gi.LocClient_Print(ent, PRINT_CENTER, "$g_n64_crouching");
		}
	}

	if (level.intermission.time || cl->awaitingRespawn) {
		// [Paril-KEX] Auto-retry delayed spawn
		if (cl->awaitingRespawn && level.time > cl->respawnMinTime) {
			ClientRespawn(ent);
			if (!cl->awaitingRespawn) return;
		}
		cl->ps.pmove.pmType = PM_FREEZE;

		bool n64_sp = false;
		if (cl->menu.current && !menuHandled)
			HandleMenuMovement(ent, ucmd);

		if (level.intermission.time) {
			n64_sp = !deathmatch->integer && level.isN64;

			// can exit intermission after five seconds
			// Paril: except in N64. the camera handles it.
			// Paril again: except on unit exits, we can leave immediately after camera finishes
			if (!level.changeMap.empty() && (!n64_sp || level.intermission.set) && level.time > level.intermission.time + 5_sec && (ucmd->buttons & BUTTON_ANY))
				level.intermission.postIntermission = true;
		}

		if (!n64_sp)
			cl->ps.pmove.viewHeight = ent->viewHeight = DEFAULT_VIEWHEIGHT;
		else
			cl->ps.pmove.viewHeight = ent->viewHeight = 0;
		ent->moveType = MoveType::FreeCam;
		return;
	}

	if (cl->follow.target) {
		cl->resp.cmdAngles = ucmd->angles;
		ent->moveType = MoveType::FreeCam;
	}
	else {


		// set up for pmove

		pmove_state_t& pmState = cl->ps.pmove;

		if (ent->moveType == MoveType::FreeCam) {
			if (cl->menu.current) {
				pmState.pmType = PM_FREEZE;

				// [Paril-KEX] handle menu movement
				if (!menuHandled)
					HandleMenuMovement(ent, ucmd);
			}
			else if (cl->awaitingRespawn)
				pmState.pmType = PM_FREEZE;
			else if (!ClientIsPlaying(ent->client) || cl->eliminated)
				pmState.pmType = PM_SPECTATOR;
			else
				pmState.pmType = PM_NOCLIP;
		}
		else if (ent->moveType == MoveType::NoClip) {
			pmState.pmType = PM_NOCLIP;
		}
		else if (ent->s.modelIndex != MODELINDEX_PLAYER)
			pmState.pmType = PM_GIB;
		else if (ent->deadFlag)
			pmState.pmType = PM_DEAD;
		else if (cl->grapple.state >= GrappleState::Pull)
			pmState.pmType = PM_GRAPPLE;
		else
			pmState.pmType = PM_NORMAL;

		bool ignorePlayers = !G_ShouldPlayersCollide(false) ||
			(CooperativeModeOn() && !(ent->clipMask & CONTENTS_PLAYER));
		if (cl->PowerupTimer(PowerupTimer::Invisibility) > level.time)
			ignorePlayers = true;
		if (ignorePlayers)
			pmState.pmFlags |= PMF_IGNORE_PLAYER_COLLISION;
		else
			pmState.pmFlags &= ~PMF_IGNORE_PLAYER_COLLISION;

		pmState.haste = cl->PowerupTimer(PowerupTimer::Haste) > level.time;
		if (pmState.haste)
			pmState.pmFlags |= PMF_HASTE;
		else
			pmState.pmFlags &= ~PMF_HASTE;

		if ((game.cheatsFlag & GameCheatFlags::Fly) != GameCheatFlags::None)
			pmState.pmFlags |= (PMF_NO_POSITIONAL_PREDICTION | PMF_NO_ANGULAR_PREDICTION);

		if (G_SpawnHasGravity(ent)) {
			pmState.gravity = static_cast<int16_t>(level.gravity * ent->gravity);
			if (!pmState.gravity)
				pmState.gravity = 1;
		}
		else
			pmState.gravity = 0;

		pmState.viewHeight = ent->viewHeight;

		if (cl->resp.cmdAngles[YAW] < -180.f)
			cl->resp.cmdAngles[YAW] += 360.f;
		else if (cl->resp.cmdAngles[YAW] > 180.f)
			cl->resp.cmdAngles[YAW] -= 360.f;

		const pmflags_t previousFlags = pmState.pmFlags;

		pm.s = pmState;
		pm.s.origin = ent->s.origin;
		pm.s.velocity = ent->velocity;
		if (std::memcmp(&cl->old_pmove, &pm.s, sizeof(pm.s)) != 0)
			pm.snapInitial = true;
		pm.cmd = *ucmd;
		if (cl->menu.current && !ClientIsPlaying(cl)) {
			pm.cmd.angles = cl->ps.viewAngles - pm.s.deltaAngles;
		}
		pm.player = ent;
		pm.trace = gi.game_import_t::trace;
		pm.clip = ClientPMoveClip;
		pm.pointContents = gi.game_import_t::pointContents;
		pm.viewOffset = cl->ps.viewOffset;

		const Vector3 oldOrigin = ent->s.origin;
		const Vector3 savedViewAngles = cl->ps.viewAngles;
		const Vector3 savedVAngle = cl->vAngle;

		Pmove(&pm);

		if (pmState.haste)
			pm.s.pmFlags |= PMF_HASTE;
		else
			pm.s.pmFlags &= ~PMF_HASTE;
		pm.s.haste = pmState.haste;

		cl->ps.rdFlags = pm.rdFlags;

		const bool wasOnLadder = (previousFlags & PMF_ON_LADDER) != PMF_NONE;
		const bool onLadder = (pm.s.pmFlags & PMF_ON_LADDER) != PMF_NONE;

		ent->s.origin = pm.s.origin;
		ent->velocity = pm.s.velocity;
		ent->s.event = EV_NONE;
		ent->s.renderFX &= ~RF_BEAM;

		MoveType newMoveType = MoveType::Walk;
		switch (pm.s.pmType) {
		case PM_SPECTATOR:
			newMoveType = MoveType::FreeCam;
			break;
		case PM_FREEZE:
			newMoveType = ent->moveType;
			break;
		case PM_NOCLIP:
			newMoveType = MoveType::NoClip;
			break;
		case PM_DEAD:
		case PM_GIB:
			newMoveType = MoveType::Toss;
			break;
		case PM_GRAPPLE:
			newMoveType = MoveType::Fly;
			break;
		default:
			newMoveType = MoveType::Walk;
			break;
		}

		if (onLadder)
			newMoveType = MoveType::Walk;

		contents_t clipMask = MASK_PLAYERSOLID;
		if (newMoveType == MoveType::FreeCam || newMoveType == MoveType::NoClip)
			clipMask &= ~(CONTENTS_SOLID | CONTENTS_PLAYER);

		ent->clipMask = clipMask;
		ent->moveType = newMoveType;

		ApplyFallingDamage(ent, pm);

		if ((onLadder != wasOnLadder)) {
			cl->last_ladder_pos = ent->s.origin;

			if (onLadder) {
				if (!deathmatch->integer && cl->last_ladder_sound < level.time) {
					ent->s.event = EV_LADDER_STEP;
					cl->last_ladder_sound = level.time + LADDER_SOUND_TIME;
				}
			}
		}

		cl->ps.pmove = pm.s;
		cl->old_pmove = pm.s;
		ent->mins = pm.mins;
		ent->maxs = pm.maxs;

		if (!cl->menu.current)
			cl->resp.cmdAngles = ucmd->angles;

		if (pm.jumpSound && !onLadder) {
			gi.sound(ent, CHAN_VOICE, gi.soundIndex("*jump1.wav"), 1, ATTN_NORM, 0);
		}

		ent->s.angles = pm.viewAngles;
		ent->s.angles[PITCH] = 0;
		ent->s.angles[ROLL] = 0;
		ent->s.angles[YAW] = pm.viewAngles[YAW];
		cl->ps.viewAngles = ent->s.angles;

		if (ent->flags & FL_SAM_RAIMI)
			ent->viewHeight = 8;
		else
			ent->viewHeight = (int)pm.s.viewHeight;

		ent->waterLevel = pm.waterLevel;
		ent->waterType = pm.waterType;
		ent->groundEntity = pm.groundEntity;
		if (pm.groundEntity)
			ent->groundEntity_linkCount = pm.groundEntity->linkCount;

		if (ent->deadFlag) {
			cl->ps.viewAngles[ROLL] = 40;
			cl->ps.viewAngles[PITCH] = -15;
			cl->ps.viewAngles[YAW] = cl->killerYaw;
		}
		else if (!cl->menu.current) {
			cl->vAngle = pm.viewAngles;
			cl->ps.viewAngles = pm.viewAngles;
			AngleVectors(cl->vAngle, cl->vForward, nullptr, nullptr);
		}
		else if (ent->moveType == MoveType::FreeCam || !ClientIsPlaying(cl)) {
			cl->ps.viewAngles = savedViewAngles;
			cl->vAngle = savedVAngle;
			ent->s.angles[PITCH] = 0;
			ent->s.angles[ROLL] = 0;
			ent->s.angles[YAW] = savedViewAngles[YAW];
			AngleVectors(cl->vAngle, cl->vForward, nullptr, nullptr);
		}

		if (cl->grapple.entity)
			Weapon_Grapple_Pull(cl->grapple.entity);

		gi.linkEntity(ent);

		ent->gravity = 1.0f;

		if (ent->moveType != MoveType::NoClip) {
			TouchTriggers(ent);
			if (ent->moveType != MoveType::FreeCam)
				G_TouchProjectiles(ent, oldOrigin);
		}

		// touch other objects
		for (size_t i = 0; i < pm.touch.num; i++) {
			trace_t& tr = pm.touch.traces[i];
			other = tr.ent;

			if (!other || !other->inUse) {
				continue;
			}

			if (other->touch)
				other->touch(other, ent, tr, true);
		}
	}

	// fire weapon from final position if needed
	if (!cl->menu.current && (cl->latchedButtons & BUTTON_ATTACK)) {
		if (!ClientIsPlaying(cl) || (cl->eliminated && !cl->sess.is_a_bot)) {
			cl->latchedButtons = BUTTON_NONE;

			if (cl->follow.target) {
				FreeFollower(ent);
			}
			else
				GetFollowTarget(ent);
		}
		else if (!cl->weapon.thunk) {
			// we can only do this during a ready state and
			// if enough time has passed from last fire
			if (cl->weaponState == WeaponState::Ready && !CombatIsDisabled()) {
				cl->weapon.fireBuffered = true;

				if (cl->weapon.fireFinished <= level.time) {
					cl->weapon.thunk = true;
					Think_Weapon(ent);
				}
			}
		}
	}

	if (!ClientIsPlaying(cl) || (cl->eliminated && !cl->sess.is_a_bot)) {
		if (!menuHandled && !HandleMenuMovement(ent, ucmd)) {
			if (ucmd->buttons & BUTTON_JUMP) {
				if (!(cl->ps.pmove.pmFlags & PMF_JUMP_HELD)) {
					cl->ps.pmove.pmFlags |= PMF_JUMP_HELD;
					if (cl->follow.target)
						FollowNext(ent);
					else
						GetFollowTarget(ent);
				}
			}
			else
				cl->ps.pmove.pmFlags &= ~PMF_JUMP_HELD;
		}
	}

	// update followers if being followed
	for (auto ec : active_clients())
		if (ec->client->follow.target == ent)
			ClientUpdateFollowers(ec);

	// perform once-a-second actions
	ClientTimerActions(ent);}

/*
=============
ClientSessionServiceImpl::ClientBeginServerFrame

Runs pre-entity server frame logic for a client, including respawn checks,
weapon think, and bot updates.
=============
*/
void ClientSessionServiceImpl::ClientBeginServerFrame(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
gentity_t* ent) {
	gclient_t* client = ent->client;

	if (gi.ServerFrame() != client->stepFrame)
		ent->s.renderFX &= ~RF_STAIR_STEP;

	if (level.intermission.time) {
		client->latchedButtons = BUTTON_NONE;
		return;
	}

	if (FreezeTag_IsActive() && client->eliminated) {
		if (client->freeze.thawTime && level.time >= client->freeze.thawTime) {
			client->latchedButtons = BUTTON_NONE;
			if (clientBeginServerFrameFreezeHook && clientBeginServerFrameFreezeHook(ent))
				return;
			worr::server::client::FreezeTag_ThawPlayer(nullptr, ent, false, true);
			return;
		}

		if (worr::server::client::FreezeTag_UpdateThawHold(ent)) {
			client->latchedButtons = BUTTON_NONE;
			if (clientBeginServerFrameFreezeHook && clientBeginServerFrameFreezeHook(ent))
				return;
			return;
		}
	}

	if (client->awaitingRespawn) {
		client->latchedButtons = BUTTON_NONE;
		if ((level.time.milliseconds() % 500) == 0)
			ClientSpawn(ent);
		return;
	}
	if ((ent->svFlags & SVF_BOT) != 0) {
		Bot_BeginFrame(ent);
	}

	// run weapon animations if it hasn't been done by a ucmd_t
	if (!client->weapon.thunk && ClientIsPlaying(client) && !client->eliminated)
		Think_Weapon(ent);
	else
		client->weapon.thunk = false;

	if (ent->client->menu.current) {
		client->latchedButtons = BUTTON_NONE;
		return;
	}
	else if (ent->deadFlag) {
		const button_t latchedButtons = client->latchedButtons;
		client->latchedButtons = BUTTON_NONE;

		//wor: add minimum delay in dm
		if (deathmatch->integer && client->respawnMinTime && level.time > client->respawnMinTime && level.time <= client->respawnMaxTime && !level.intermission.queued) {
			if ((latchedButtons & BUTTON_ATTACK)) {
				ClientRespawn(ent);
				client->latchedButtons = BUTTON_NONE;
			}
		}
		else if (level.time > client->respawnMaxTime && !level.campaign.coopLevelRestartTime) {
			// don't respawn if level is waiting to restart
			// check for coop handling
			if (!G_LimitedLivesRespawn(ent)) {
				// in deathmatch, only wait for attack button
				if ((latchedButtons & (deathmatch->integer ? BUTTON_ATTACK : -1)) ||
					(deathmatch->integer && match_doForceRespawn->integer)) {
					ClientRespawn(ent);
					client->latchedButtons = BUTTON_NONE;
				}
			}
		}
		return;
	}

	// add player trail so monsters can follow
	if (!deathmatch->integer)
		PlayerTrail_Add(ent);

	client->latchedButtons = BUTTON_NONE;}

/*
=============
ClientSessionServiceImpl::OnReadyToggled

Manages the ready-state toggle workflow, including precondition checks,
messaging, and broadcasting.
=============
*/
ReadyResult ClientSessionServiceImpl::OnReadyToggled(gentity_t* ent, bool state, bool toggle) {
	if (!ent || !ent->client) {
		return ReadyResult::NoConditions;
	}
	if (!ReadyConditions(ent, false)) {
		return ReadyResult::NoConditions;
	}
	client_persistant_t* pers = &ent->client->pers;

		if (toggle) {
			pers->readyStatus = !pers->readyStatus;
		}
		else {
			if (pers->readyStatus == state) {
				return ReadyResult::AlreadySet;
			}

			pers->readyStatus = state;
		}

		gi_.LocBroadcast_Print(PRINT_CENTER,
			"%bind:+wheel2:Use Compass to toggle your ready status.%.MATCH IS IN WARMUP\n{} is {}ready.",
			ent->client->sess.netName, ent->client->pers.readyStatus ? "" : "NOT " );

		return ReadyResult::Success;}

} // namespace worr::server::client
