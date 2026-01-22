// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

// bg_local.hpp (Both Games)
// This header contains definitions that are shared between both the client-side
// (cgame) and server-side (game) modules. It is the foundation for any code
// that needs to be commonly understood by both, preventing code duplication.
//
// Key Responsibilities:
// - Defines the player_state_t stats enumeration (STAT_*), which dictates the
//   layout of the data array sent from the server to the client for HUD rendering.
// - Declares enumerations for ammo (AmmoID) and powerups (powerup_t).
// - Provides structures and functions for compressing and decompressing HUD data
//   (e.g., ammo and powerup counts) to save network bandwidth.
// - Defines the shared physics configuration (pm_config_t) and declares the
//   core player movement function (Pmove).

#pragma once
#include "q_std.hpp"

// define GAME_INCLUDE so that game.h does not define the
// short, server-visible gclient_t and gentity_t structures,
// because we define the full size ones in this file
#define GAME_INCLUDE
#include "game.hpp"

//
// p_move.c
//
struct pm_config_t {
	int32_t		airAccel = 0;
	bool		n64Physics = false;
	bool		q3Overbounce = false;
};

extern pm_config_t pm_config;

void Pmove(PMove* pmove);
using pm_trace_func_t = trace_t(const Vector3& start, const Vector3& mins, const Vector3& maxs, const Vector3& end);
using pm_trace_t = std::function<pm_trace_func_t>;
void PM_StepSlideMove_Generic(Vector3& origin, Vector3& velocity, float frametime, const Vector3& mins, const Vector3& maxs, touch_list_t& touch, bool has_time, pm_trace_t trace);

enum class StuckResult {
	GoodPosition,
	Fixed,
	NoGoodPosition
};

using stuck_object_trace_fn_t = trace_t(const Vector3&, const Vector3&, const Vector3&, const Vector3&);

StuckResult G_FixStuckObject_Generic(Vector3& origin, const Vector3& own_mins, const Vector3& own_maxs, std::function<stuck_object_trace_fn_t> trace);

// state for coop respawning; used to select which
// message to print for the player this is set on.
enum class CoopRespawn {
	None, // no messagee
	InCombat, // player is in combat
	BadArea, // player not in a good spot
	Blocked, // spawning was blocked by something
	Waiting, // for players that are waiting to respawn
	NoLives, // out of lives, so need to wait until level switch
	Total
};

// reserved general CS ranges
constexpr int32_t HUD_BLOB_SEGMENTS = 12;
constexpr size_t HUD_BLOB_SEGMENT_SIZE = CS_MAX_STRING_LENGTH - 1;
constexpr size_t HUD_BLOB_MAX_SIZE = HUD_BLOB_SEGMENTS * HUD_BLOB_SEGMENT_SIZE;

enum hud_blob_flag_t : uint32_t {
	HUD_FLAG_MINHUD = 1u << 0
};

enum scoreboard_mode_t : int32_t {
	SB_MODE_FFA = 0,
	SB_MODE_TEAM = 1,
	SB_MODE_DUEL = 2
};

enum scoreboard_flag_t : uint32_t {
	SB_FLAG_TEAMS = 1u << 0,
	SB_FLAG_DUEL = 1u << 1,
	SB_FLAG_CTF = 1u << 2,
	SB_FLAG_ROUNDS = 1u << 3,
	SB_FLAG_DOMINATION = 1u << 4,
	SB_FLAG_PROBALL = 1u << 5,
	SB_FLAG_INTERMISSION = 1u << 6
};

enum scoreboard_row_flag_t : uint32_t {
	SB_ROW_READY = 1u << 0,
	SB_ROW_ELIMINATED = 1u << 1,
	SB_ROW_FLAG_RED = 1u << 2,
	SB_ROW_FLAG_BLUE = 1u << 3
};

enum {
	CONFIG_MATCH_STATE = CS_GENERAL,
	CONFIG_MATCH_STATE2,
	CONFIG_CHASE_PLAYER_NAME,
	CONFIG_CHASE_PLAYER_NAME_END = CONFIG_CHASE_PLAYER_NAME + MAX_CLIENTS,

	// nb: offset by 1 since NONE is zero
	CONFIG_COOP_RESPAWN_STRING,
	CONFIG_COOP_RESPAWN_STRING_END = CONFIG_COOP_RESPAWN_STRING + (static_cast<int>(CoopRespawn::Total) - 1),

	// [Paril-KEX] if 1, n64 player physics apply
	CONFIG_N64_PHYSICS,
	CONFIG_HEALTH_BAR_NAME, // active health bar name

	CONFIG_STORY,
	// [Paril-KEX] if 1, Quake 3 overbounce physics apply
	CONFIG_Q3_OVERBOUNCE,

	// [WORR] HUD blob for cgame-side statusbar/scoreboard/EOU data
	CONFIG_HUD_BLOB,
	CONFIG_HUD_BLOB_END = CONFIG_HUD_BLOB + HUD_BLOB_SEGMENTS - 1,

	CONFIG_LAST
};

static_assert(CONFIG_LAST <= CS_GENERAL + MAX_GENERAL);

// ammo IDs
enum class AmmoID : uint8_t {
	Bullets,
	Shells,
	Rockets,
	Grenades,
	Cells,
	Slugs,
	MagSlugs,
	Traps,
	Flechettes,
	TeslaMines,
	Rounds,
	ProxMines,

	_Total
};

// powerup IDs
enum powerup_t : uint8_t {
	POWERUP_NONE,
	POWERUP_SCREEN,
	POWERUP_SHIELD,

	POWERUP_AM_BOMB,

	POWERUP_QUAD,
	POWERUP_HASTE,
	POWERUP_BATTLESUIT,
	POWERUP_INVISIBILITY,
	POWERUP_SILENCER,
	POWERUP_REBREATHER,
	POWERUP_ENVIROSUIT,
	POWERUP_ADRENALINE,
	POWERUP_IR_GOGGLES,
	POWERUP_DOUBLE,
	POWERUP_SPHERE_VENGEANCE,
	POWERUP_SPHERE_HUNTER,
	POWERUP_SPHERE_DEFENDER,
	POWERUP_DOPPELGANGER,

	POWERUP_FLASHLIGHT,
	POWERUP_COMPASS,

	POWERUP_TECH_DISRUPTOR_SHIELD,
	POWERUP_TECH_POWER_AMP,
	POWERUP_TECH_TIME_ACCEL,
	POWERUP_TECH_AUTODOC,

	POWERUP_REGENERATION,
	POWERUP_EMPATHY_SHIELD,
	POWERUP_ANTIGRAV_BELT,
	POWERUP_SPAWN_PROTECTION,

	POWERUP_BALL,

	POWERUP_MAX
};

// ammo stats compressed in 9 bits per entry
// since the range is 0-300
constexpr size_t BITS_PER_AMMO = 9;

template<typename TI>
constexpr size_t num_of_type_for_bits(size_t num_bits) {
	return (num_bits + (sizeof(TI) * 8) - 1) / ((sizeof(TI) * 8) + 1);
}

template<size_t bits_per_value>
constexpr void set_compressed_integer(uint16_t* start, uint8_t id, uint16_t count) {
	uint16_t bit_offset = bits_per_value * id;
	uint16_t byte = bit_offset / 8;
	uint16_t bit_shift = bit_offset % 8;
	uint16_t mask = (bit_v<bits_per_value> -1) << bit_shift;
	uint16_t* base = (uint16_t*)((uint8_t*)start + byte);
	*base = (*base & ~mask) | ((count << bit_shift) & mask);
}

template<size_t bits_per_value>
constexpr uint16_t get_compressed_integer(uint16_t* start, uint8_t id) {
	uint16_t bit_offset = bits_per_value * id;
	uint16_t byte = bit_offset / 8;
	uint16_t bit_shift = bit_offset % 8;
	uint16_t mask = (bit_v<bits_per_value> -1) << bit_shift;
	uint16_t* base = (uint16_t*)((uint8_t*)start + byte);
	return (*base & mask) >> bit_shift;
}

constexpr size_t NUM_BITS_FOR_AMMO = 9;
constexpr size_t NUM_AMMO_STATS = num_of_type_for_bits<uint16_t>(NUM_BITS_FOR_AMMO * static_cast<int>(AmmoID::_Total));
// if this value is set on an STAT_AMMO_INFO_xxx, don't render ammo
constexpr uint16_t AMMO_VALUE_INFINITE = bit_v<NUM_BITS_FOR_AMMO> -1;

constexpr void SetAmmoStat(uint16_t* start, uint8_t ammoID, uint16_t count) {
	set_compressed_integer<NUM_BITS_FOR_AMMO>(start, ammoID, count);
}

constexpr uint16_t GetAmmoStat(uint16_t* start, uint8_t ammoID) {
	return get_compressed_integer<NUM_BITS_FOR_AMMO>(start, ammoID);
}

// powerup stats compressed in 2 bits per entry;
// 3 is the max you'll ever hold, and for some
// (flashlight) it's to indicate on/off state
constexpr size_t NUM_BITS_PER_POWERUP = 2;
constexpr size_t NUM_POWERUP_STATS = num_of_type_for_bits<uint16_t>(NUM_BITS_PER_POWERUP * POWERUP_MAX);

constexpr void SetPowerupStat(uint16_t* start, uint8_t powerup_id, uint16_t count) {
	set_compressed_integer<NUM_BITS_PER_POWERUP>(start, powerup_id, count);
}

constexpr uint16_t GetPowerupStat(uint16_t* start, uint8_t powerup_id) {
	return get_compressed_integer<NUM_BITS_PER_POWERUP>(start, powerup_id);
}

// player_state->stats[] indexes
enum player_stat_t {
	STAT_HEALTH_ICON = 0,
	STAT_HEALTH = 1,
	STAT_AMMO_ICON = 2,
	STAT_AMMO = 3,
	STAT_ARMOR_ICON = 4,
	STAT_ARMOR = 5,
	STAT_SELECTED_ICON = 6,
	STAT_PICKUP_ICON = 7,
	STAT_PICKUP_STRING = 8,
	STAT_POWERUP_ICON = 9,
	STAT_POWERUP_TIME = 10,
	STAT_HELPICON = 11,
	STAT_SELECTED_ITEM = 12,
	STAT_LAYOUTS = 13,
	STAT_SCORE = 14,
	STAT_FLASHES = 15, // cleared each frame, 1 = health, 2 = armor
	STAT_FOLLOWING = 16,
	STAT_SPECTATOR = 17,

	STAT_MINISCORE_FIRST_PIC = 18,
	STAT_MINISCORE_FIRST_SCORE = 19,
	STAT_MINISCORE_SECOND_PIC = 20,
	STAT_MINISCORE_SECOND_SCORE = 21,
	STAT_CTF_FLAG_PIC = 22,
	STAT_MINISCORE_FIRST_POS = 23,
	STAT_MINISCORE_SECOND_POS = 24,
	STAT_TEAM_RED_HEADER = 25,
	STAT_TEAM_BLUE_HEADER = 26,
	STAT_TECH = 27,
	STAT_CROSSHAIR_ID_VIEW = 28,
	STAT_MATCH_STATE = 29,
	STAT_CROSSHAIR_ID_VIEW_COLOR = 30,
	STAT_TEAMPLAY_INFO = 31,

	// [Kex] More stats for weapon wheel
	STAT_WEAPONS_OWNED_1 = 32,
	STAT_WEAPONS_OWNED_2 = 33,
	STAT_AMMO_INFO_START = 34,
	STAT_AMMO_INFO_END = STAT_AMMO_INFO_START + NUM_AMMO_STATS - 1,
	STAT_POWERUP_INFO_START,
	STAT_POWERUP_INFO_END = STAT_POWERUP_INFO_START + NUM_POWERUP_STATS - 1,

	// [Paril-KEX] Key display
	STAT_KEY_A,
	STAT_KEY_B,
	STAT_KEY_C,

	// [Paril-KEX] currently active wheel weapon (or one we're switching to)
	STAT_ACTIVE_WHEEL_WEAPON,
	// [Paril-KEX] top of screen coop respawn state
	STAT_COOP_RESPAWN,
	// [Paril-KEX] respawns remaining
	STAT_LIVES,
	// [Paril-KEX] hit marker; # of damage we successfully landed
	STAT_HIT_MARKER,
	// [Paril-KEX]
	STAT_SELECTED_ITEM_NAME,
	// [Paril-KEX]
	STAT_HEALTH_BARS, // two health bar values; 7 bits for value, 1 bit for active
	// [Paril-KEX]
	STAT_ACTIVE_WEAPON,

	STAT_SCORELIMIT,
	STAT_DUEL_HEADER,

	STAT_SHOW_STATUSBAR,

	STAT_COUNTDOWN,

	STAT_MINISCORE_FIRST_VAL,
	STAT_MINISCORE_SECOND_VAL,

	STAT_MONSTER_COUNT,
	STAT_ROUND_NUMBER,

	STAT_GAMEPLAY_CARRIED,

	// don't use; just for verification
	STAT_LAST
};

static_assert(STAT_LAST <= MAX_STATS, "stats list overflow");
