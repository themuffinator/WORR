/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

g_horde.cpp (Game Horde Mode) This file contains the specific gameplay logic for the Horde game
mode. It is responsible for managing the waves of monsters, spawning them into the world, and
checking for the win/loss conditions of each round. Key Responsibilities: - Monster Spawning:
`Horde_RunSpawning` is called periodically to spawn new monsters into the map at valid
deathmatch spawn points. - Weighted Selection: Uses a weighted random selection system
(`Horde_PickMonster`) to choose which type of monster to spawn based on the current wave number,
making later waves more difficult. - Item Drops: Similarly uses a weighted system
(`Horde_PickItem`) to decide which item a monster should drop upon death. - Wave Management:
Tracks the number of monsters left to spawn and the number of monsters currently alive to
determine when a wave has been successfully completed.*/

#include "../g_local.hpp"

static gentity_t* FindClosestPlayerToPoint(const Vector3& point) {
	float best_player_distance = 999999.0f;
	gentity_t* closest = nullptr;

	for (auto ec : active_clients()) {
		if (ec->health <= 0 || ec->client->eliminated) {
			continue;
		}

		const Vector3 v = point - ec->s.origin;
		const float player_distance = v.length();

		if (player_distance < best_player_distance) {
			best_player_distance = player_distance;
			closest = ec;
		}
	}

	return closest;
}

struct weighted_item_t;

using weight_adjust_func_t = void (*)(const weighted_item_t& item, float& weight);

void adjust_weight_health(const weighted_item_t& item, float& weight);
void adjust_weight_weapon(const weighted_item_t& item, float& weight);
void adjust_weight_ammo(const weighted_item_t& item, float& weight);
void adjust_weight_armor(const weighted_item_t& item, float& weight);

constexpr struct weighted_item_t {
	const char* className;
	int32_t min_level = -1;
	int32_t max_level = -1;
	float weight = 1.0f;
	float lvl_w_adjust = 0.0f;
	BitFlags<MonsterFlags> flags = MonsterFlags::None;
	item_id_t item[4] = { IT_NULL };
	weight_adjust_func_t adjust_weight = nullptr;
} items[] = {
	{"item_health_small"},

	{"item_health", -1, -1, 1.0f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_health},
	{"item_health_large", -1, -1, 0.85f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_health},

	{"item_armor_shard"},
	{"item_armor_jacket", -1, 4, 0.65f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_armor},
	{"item_armor_combat", 2, -1, 0.62f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_armor},
	{"item_armor_body", 4, -1, 0.35f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_armor},

	{"weapon_shotgun", -1, -1, 0.98f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_weapon},
	{"weapon_supershotgun", 2, -1, 1.02f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_weapon},
	{"weapon_machinegun", -1, -1, 1.05f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_weapon},
	{"weapon_chaingun", 3, -1, 1.01f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_weapon},
	{"weapon_grenadelauncher", 4, -1, 0.75f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_weapon},

	{"ammo_shells", -1, -1, 1.25f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_ammo},
	{"ammo_bullets", -1, -1, 1.25f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_ammo},
	{"ammo_grenades", 2, -1, 1.25f, 0.0f, MonsterFlags::None, {IT_NULL}, adjust_weight_ammo},
};

void adjust_weight_health(const weighted_item_t& /*item*/, float& /*weight*/) {}

void adjust_weight_weapon(const weighted_item_t& /*item*/, float& /*weight*/) {}

void adjust_weight_ammo(const weighted_item_t& /*item*/, float& /*weight*/) {}

void adjust_weight_armor(const weighted_item_t& /*item*/, float& /*weight*/) {}

//	className,						min_level, max_level, weight, lvl_w_adjust, flags, items
// className, min_level, max_level, weight, lvl_w_adjust, flags, item_drops
constexpr weighted_item_t monsters[] = {
	// =================================================================
	// Quake II Base Monsters
	// =================================================================
	// --- Light Infantry ---
	{ "monster_soldier_light",      -1,  7,    1.50f, -0.45f, MonsterFlags::Ground, {IT_HEALTH_SMALL} },
	{ "monster_soldier",            -1,  7,    0.85f, -0.25f, MonsterFlags::Ground, {IT_AMMO_BULLETS_SMALL, IT_HEALTH_SMALL} },
	{ "monster_soldier_ss",          2,  7,    1.01f, -0.125f,MonsterFlags::Ground, {IT_AMMO_SHELLS_SMALL, IT_HEALTH_SMALL} },
	{ "monster_infantry",            3,  16,   1.05f,  0.125f, MonsterFlags::Ground, {IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS} },
	{ "monster_flipper",             4,  10,   0.85f, -0.15f, MonsterFlags::Water,  {IT_NULL} },
	{ "monster_gunner",              4,  16,   1.08f,  0.5f,  MonsterFlags::Ground, {IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL} },
	{ "monster_berserk",             4,  16,   1.05f,  0.1f,  MonsterFlags::Ground, {IT_ARMOR_SHARD} },
	{ "monster_parasite",            5,  16,   1.04f, -0.08f, MonsterFlags::Ground, {IT_NULL} },
	{ "monster_brain",               6,  16,   0.95f,  0.0f,  MonsterFlags::Ground, {IT_AMMO_CELLS_SMALL} },
	{ "monster_flyer",               6,  16,   0.92f,  0.15f, MonsterFlags::Ground | MonsterFlags::Air, {IT_AMMO_CELLS_SMALL} },
	{ "monster_mutant",              7,  16,   0.85f,  0.0f,  MonsterFlags::Ground, {IT_NULL} },
	{ "monster_floater",             7,  16,   0.9f,   0.0f,  MonsterFlags::Ground | MonsterFlags::Air, {IT_NULL} },
	{ "monster_hover",               8,  16,   0.8f,   0.0f,  MonsterFlags::Ground | MonsterFlags::Air, {IT_NULL} },
	{ "monster_chick",               9,  20,   1.01f, -0.05f, MonsterFlags::Ground, {IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS} },
	{ "monster_medic",              10,  16,   0.95f, -0.05f, MonsterFlags::Ground, {IT_HEALTH_SMALL, IT_HEALTH_MEDIUM} },

	// --- Heavy Infantry ---
	{ "monster_gladiator",           5,  16,   1.07f,  0.3f,  MonsterFlags::Ground | MonsterFlags::Medium, {IT_AMMO_SLUGS} },
	{ "monster_tank",               11,  -1,   0.85f,  0.0f,  MonsterFlags::Ground | MonsterFlags::Medium, {IT_AMMO_ROCKETS} },
	{ "monster_tank_commander",     12,  -1,   0.45f,  0.16f, MonsterFlags::Ground | MonsterFlags::Medium, {IT_AMMO_ROCKETS_SMALL, IT_AMMO_BULLETS_SMALL, IT_AMMO_ROCKETS, IT_AMMO_BULLETS} },

	// --- Bosses (High cost, low weight, appear late) ---
	{ "monster_supertank",          20,  -1,   0.1f,   0.2f,  MonsterFlags::Ground | MonsterFlags::Boss, {IT_HEALTH_MEGA, IT_AMMO_BULLETS_LARGE, IT_AMMO_ROCKETS} },
	{ "monster_hornet",             22,  -1,   0.1f,   0.2f,  MonsterFlags::Ground | MonsterFlags::Boss, {IT_HEALTH_MEGA, IT_AMMO_CELLS_LARGE} }, // boss2
	{ "monster_jorg",               25,  -1,   0.05f,  0.1f,  MonsterFlags::Ground | MonsterFlags::Boss, {IT_HEALTH_MEGA, IT_AMMO_CELLS_LARGE} },
	{ "monster_makron",             30,  -1,   0.05f,  0.1f,  MonsterFlags::Ground | MonsterFlags::Boss, {IT_HEALTH_MEGA, IT_AMMO_CELLS_LARGE} },

	// =================================================================
	// The Reckoning (Mission Pack 1) Monsters
	// =================================================================
	{ "monster_gekk",                6,  16,   0.99f, -0.15f, MonsterFlags::Ground | MonsterFlags::Water, {IT_NULL} },
	{ "monster_fixbot",              8,  16,   0.7f,   0.0f,  MonsterFlags::Air,    {IT_AMMO_BULLETS_SMALL} },
	{ "monster_guardian",           18,  -1,   0.2f,   0.15f, MonsterFlags::Ground | MonsterFlags::Boss, {IT_HEALTH_MEGA} },


	// =================================================================
	// Ground Zero (Mission Pack 2) Monsters
	// =================================================================
	{ "monster_stalker",             2,  10,   1.1f,  -0.2f,  MonsterFlags::Ground, {IT_AMMO_CELLS_SMALL} },
	{ "monster_turret",              4,  -1,   0.5f,   0.05f, MonsterFlags::Ground, {IT_NULL} },
	{ "monster_daedalus",            9,  -1,   0.99f,  0.05f, MonsterFlags::Ground | MonsterFlags::Air, {IT_AMMO_CELLS_SMALL} },
	{ "monster_medic_commander",    13,  -1,   0.4f,   0.15f, MonsterFlags::Ground | MonsterFlags::Medium, {IT_AMMO_CELLS_SMALL, IT_HEALTH_MEDIUM, IT_HEALTH_LARGE} },
	{ "monster_carrier",            16,  -1,   0.2f,   0.15f, MonsterFlags::Ground | MonsterFlags::Boss, {IT_AMMO_BULLETS_LARGE, IT_AMMO_SLUGS, IT_POWERUP_QUAD} },
	{ "monster_widow",              20,  -1,   0.1f,   0.2f,  MonsterFlags::Ground | MonsterFlags::Boss, {IT_HEALTH_MEGA, IT_AMMO_SLUGS, IT_POWERUP_QUAD} },


	// =================================================================
	// Call of the Machine (Remaster Expansion) Monsters
	// =================================================================
	{ "monster_soldier_ripper",      3,  9,    1.25f,  0.25f, MonsterFlags::Ground, {IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL} },
	{ "monster_soldier_hypergun",    2,  9,    1.2f,   0.15f, MonsterFlags::Ground, {IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL} },
	{ "monster_soldier_lasergun",    3,  9,    1.15f,  0.2f,  MonsterFlags::Ground, {IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL} },
	{ "monster_chick_heat",         12,  -1,   0.87f,  0.065f,MonsterFlags::Ground, {IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS} },
	{ "monster_guncmdr",             8,  -1,   0.0f,   0.125f, MonsterFlags::Ground | MonsterFlags::Medium, {IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS, IT_AMMO_CELLS_SMALL} },
	{ "monster_kamikaze",           13,  -1,   0.85f,  0.04f, MonsterFlags::Ground | MonsterFlags::Air, {IT_NULL} },


	// =================================================================
	// Quake 1 (Dimension of the Machine) Monsters
	// =================================================================
	{ "monster_dog",                 1,   8,   1.8f,  -0.5f,  MonsterFlags::Ground, {IT_HEALTH_SMALL} },
	{ "monster_army",                1,   9,   1.6f,  -0.4f,  MonsterFlags::Ground, {IT_AMMO_SHELLS_SMALL} },
	{ "monster_zombie",              2,  10,   0.9f,  -0.1f,  MonsterFlags::Ground, {IT_NULL} },
	{ "monster_ogre",                3,  12,   1.2f,  -0.05f, MonsterFlags::Ground, {IT_AMMO_GRENADES} },
	{ "monster_fish",                4,  10,   0.8f,  -0.15f, MonsterFlags::Water,  {IT_NULL} },
	{ "monster_enforcer",            4,  12,   1.1f,  -0.05f, MonsterFlags::Ground, {IT_AMMO_CELLS_SMALL} },
	{ "monster_knight",              5,  14,   1.0f,   0.0f,  MonsterFlags::Ground, {IT_ARMOR_SHARD} },
	{ "monster_demon1",              6,  15,   0.9f,   0.0f,  MonsterFlags::Ground, {IT_HEALTH_MEDIUM} },
	{ "monster_wizard",              7,  16,   0.8f,   0.1f,  MonsterFlags::Air,    {IT_AMMO_SHELLS} },
	{ "monster_hell_knight",         8,  18,   0.7f,   0.15f, MonsterFlags::Ground | MonsterFlags::Medium, {IT_HEALTH_LARGE} },
	{ "monster_tarbaby",             9,  18,   0.6f,   0.05f, MonsterFlags::Ground, {IT_AMMO_GRENADES} },
	{ "monster_shalrath",           10,  20,   0.5f,   0.2f,  MonsterFlags::Ground | MonsterFlags::Medium, {IT_AMMO_ROCKETS} },
	{ "monster_shambler",           15,  -1,   0.3f,   0.25f, MonsterFlags::Ground | MonsterFlags::Boss, {IT_POWERUP_QUAD} },
};

struct picked_item_t {
	const weighted_item_t* item;
	float weight;
};

static Item* Horde_PickItem() {
	static std::array<picked_item_t, std::size(items)> picked_items;
	size_t num_picked_items = 0;
	float total_weight = 0.0f;

	for (const auto& item : items) {
		if (item.min_level != -1 && level.roundNumber < item.min_level) {
			continue;
		}
		if (item.max_level != -1 && level.roundNumber > item.max_level) {
			continue;
		}

		float weight = item.weight + ((level.roundNumber - item.min_level) * item.lvl_w_adjust);

		if (item.adjust_weight) {
			item.adjust_weight(item, weight);
		}

		if (weight <= 0.0f) {
			continue;
		}

		total_weight += weight;
		picked_items[num_picked_items++] = { &item, total_weight };
	}

	if (total_weight == 0.0f) {
		return nullptr;
	}

	const float r = frandom() * total_weight;

	for (size_t i = 0; i < num_picked_items; i++) {
		if (r < picked_items[i].weight) {
			return FindItemByClassname(picked_items[i].item->className);
		}
	}

	return nullptr;
}

static const char* Horde_PickMonster() {
	static std::array<picked_item_t, std::size(monsters)> picked_monsters;
	size_t num_picked_monsters = 0;
	float total_weight = 0.0f;

	for (const auto& monster : monsters) {
		if (monster.min_level != -1 && level.roundNumber < monster.min_level) {
			continue;
		}
		if (monster.max_level != -1 && level.roundNumber > monster.max_level) {
			continue;
		}

		float weight = monster.weight + ((level.roundNumber - monster.min_level) * monster.lvl_w_adjust);

		if (monster.adjust_weight) {
			monster.adjust_weight(monster, weight);
		}

		if (weight <= 0.0f) {
			continue;
		}

		total_weight += weight;
		picked_monsters[num_picked_monsters++] = { &monster, total_weight };
	}

	if (total_weight == 0.0f) {
		return nullptr;
	}

	const float r = frandom() * total_weight;

	for (size_t i = 0; i < num_picked_monsters; i++) {
		if (r < picked_monsters[i].weight) {
			return picked_monsters[i].item->className;
		}
	}

	return nullptr;
}

void Horde_RunSpawning() {
	if (Game::IsNot(GameType::Horde)) {
		return;
	}

	const bool warmup = level.matchState == MatchState::Warmup_Default || level.matchState == MatchState::Warmup_ReadyUp;

	if (!warmup && level.roundState != RoundState::In_Progress) {
		return;
	}

	if (warmup && (level.campaign.totalMonsters - level.campaign.killedMonsters >= 30)) {
		return;
	}

	if (level.horde_all_spawned) {
		return;
	}

	if (level.horde_monster_spawn_time <= level.time) {
		gentity_t* e = Spawn();
		e->className = Horde_PickMonster();
		const select_spawn_result_t result = SelectDeathmatchSpawnPoint(nullptr, vec3_origin, false, true, false, false);

		if (result.spot) {
			e->s.origin = result.spot->s.origin;
			e->s.angles = result.spot->s.angles;

			e->item = Horde_PickItem();
			ED_CallSpawn(e);
			level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + random_time(0.3_sec, 0.5_sec);

			e->enemy = FindClosestPlayerToPoint(e->s.origin);
			if (e->enemy) {
				FoundTarget(e);
			}

			if (!warmup) {
				level.horde_num_monsters_to_spawn--;

				if (!level.horde_num_monsters_to_spawn) {
					//gi.Broadcast_Print(PRINT_CENTER, "All monsters spawned.\nClean up time!");
					level.horde_all_spawned = true;
				}
			}
		}
		else {
			level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + 1_sec;
		}
	}
}

[[maybe_unused]] static bool Horde_AllMonstersDead() {
	for (size_t i = 0; i < globals.maxEntities; i++) {
		if (!g_entities[i].inUse) {
			continue;
		}
		if (g_entities[i].svFlags & SVF_MONSTER) {
			if (!g_entities[i].deadFlag && g_entities[i].health > 0) {
				return false;
			}
		}
	}

	return true;
}
