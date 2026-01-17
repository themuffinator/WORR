/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

bot_utils.cpp implementation.*/

#include "../g_local.hpp"
#include "../gameplay/g_capture.hpp"
#include "../monsters/m_player.hpp"
#include "bot_utils.hpp"

#include <algorithm>

	constexpr int Team_Coop_Monster = 0;

/*
================
Player_UpdateState
================
*/
static void Player_UpdateState(gentity_t *player) {
	const client_persistant_t &persistant = player->client->pers;

	player->sv.entFlags = SVFL_NONE;
	if (player->groundEntity != nullptr || (player->flags & FL_PARTIALGROUND) != 0) {
		player->sv.entFlags |= SVFL_ONGROUND;
	} else {
		if (player->client->ps.pmove.pmFlags & PMF_JUMP_HELD) {
			player->sv.entFlags |= SVFL_IS_JUMPING;
		}
	}

	if (player->client->ps.pmove.pmFlags & PMF_ON_LADDER) {
		player->sv.entFlags |= SVFL_ON_LADDER;
	}

	if ((player->client->ps.pmove.pmFlags & PMF_DUCKED) != 0) {
		player->sv.entFlags |= SVFL_IS_CROUCHING;
	}

	const bool hasDamageBoost = std::any_of(
		DamageBoostTimers.begin(),
		DamageBoostTimers.end(),
		[player](const PowerupTimer timer) {
			return player->client->PowerupTimer(timer) > level.time;
		});

	if (hasDamageBoost) {
		player->sv.entFlags |= SVFL_HAS_DMG_BOOST;
	}

        if (player->client->PowerupTimer(PowerupTimer::BattleSuit) > level.time) {
                player->sv.entFlags |= SVFL_HAS_PROTECTION;
        }

        if (player->client->PowerupTimer(PowerupTimer::Invisibility) > level.time) {
                player->sv.entFlags |= SVFL_HAS_INVISIBILITY;
        }

	if ((player->client->ps.pmove.pmFlags & PMF_TIME_KNOCKBACK) != 0) {
		player->sv.entFlags |= SVFL_HAS_TELEPORTED;
	}

	if (player->takeDamage) {
		player->sv.entFlags |= SVFL_TAKES_DAMAGE;
	}

	if (player->solid == SOLID_NOT) {
		player->sv.entFlags |= SVFL_IS_HIDDEN;
	}

	if ((player->flags & FL_INWATER) != 0) {
		if (player->waterLevel >= WATER_WAIST) {
			player->sv.entFlags |= SVFL_IN_WATER;
		}
	}

	if ((player->flags & FL_NOTARGET) != 0) {
		player->sv.entFlags |= SVFL_NO_TARGET;
	}

	if ((player->flags & FL_GODMODE) != 0) {
		player->sv.entFlags |= SVFL_GOD_MODE;
	}

	if (player->moveType == MoveType::NoClip) {
		player->sv.entFlags |= SVFL_IS_NOCLIP;
	}

	if (player->client->anim.end == FRAME_flip12) {
		player->sv.entFlags |= SVFL_IS_FLIPPING_OFF;
	}

	if (player->client->anim.end == FRAME_salute11) {
		player->sv.entFlags |= SVFL_IS_SALUTING;
	}

	if (player->client->anim.end == FRAME_taunt17) {
		player->sv.entFlags |= SVFL_IS_TAUNTING;
	}

	if (player->client->anim.end == FRAME_wave11) {
		player->sv.entFlags |= SVFL_IS_WAVING;
	}

	if (player->client->anim.end == FRAME_point12) {
		player->sv.entFlags |= SVFL_IS_POINTING;
	}

	if ((player->client->ps.pmove.pmFlags & PMF_DUCKED) == 0 && player->client->anim.priority <= ANIM_WAVE) {
		player->sv.entFlags |= SVFL_CAN_GESTURE;
	}

	if (player->lastMOD.id == ModID::Telefragged || player->lastMOD.id == ModID::Telefrag_Spawn) {
		player->sv.entFlags |= SVFL_WAS_TELEFRAGGED;
	}

	if (!ClientIsPlaying(player->client) || player->client->eliminated) {
		player->sv.entFlags |= SVFL_IS_SPECTATOR;
	}

	player_skinnum_t pl_skinnum{};
	pl_skinnum.skinNum = player->s.skinNum;
	player->sv.team = pl_skinnum.teamIndex;

	player->sv.buttons = player->client->buttons;

	const item_id_t armorType = ArmorIndex(player);
	player->sv.armorType = armorType;
	player->sv.armorValue = persistant.inventory[armorType];

	player->sv.health = (player->deadFlag != true) ? player->health : -1;
	player->sv.weapon = (persistant.weapon != nullptr) ? persistant.weapon->id : IT_NULL;

	player->sv.lastAttackerTime = static_cast<int32_t>(player->client->last_attacker_time.milliseconds());
	player->sv.respawnTime = static_cast<int32_t>(player->client->respawnMaxTime.milliseconds());
	player->sv.waterLevel = player->waterLevel;
	player->sv.viewHeight = player->viewHeight;

	player->sv.viewAngles = player->client->vAngle;
	player->sv.viewForward = player->client->vForward;
	player->sv.velocity = player->velocity;

	player->sv.groundEntity = player->groundEntity;
	player->sv.enemy = player->enemy;

	static_assert(sizeof(persistant.inventory) <= sizeof(player->sv.inventory));
	memcpy(&player->sv.inventory, &persistant.inventory, sizeof(persistant.inventory));

	if (!player->sv.init) {
		player->sv.init = true;
		player->sv.className = player->className;
		player->sv.targetName = player->targetName;
		player->sv.lobbyUserNum = P_GetLobbyUserNum(player);
		player->sv.startingHealth = player->health;
		player->sv.maxHealth = player->maxHealth;

		// NOTE: entries are assumed to be ranked with the first armor assumed
		// NOTE: to be the "best", and last the "worst". You don't need to add
		// NOTE: entries for things like armor shards, only actual armors.
		// NOTE: Check "MAX_ARMOR_TYPES" to raise/lower the armor count.
		armorInfo_t *armorInfo = player->sv.armorInfo;
		armorInfo[0].itemID = IT_ARMOR_BODY;
		armorInfo[0].max_count = armor_stats[game.ruleset][Armor::Body].max_count;
		armorInfo[1].itemID = IT_ARMOR_COMBAT;
		armorInfo[1].max_count = armor_stats[game.ruleset][Armor::Combat].max_count;
		armorInfo[2].itemID = IT_ARMOR_JACKET;
		armorInfo[2].max_count = armor_stats[game.ruleset][Armor::Jacket].max_count;

                gi.Info_ValueForKey(player->client->pers.userInfo, "name", player->sv.netName, sizeof(player->sv.netName));

		gi.Bot_RegisterEntity(player);
	}
}

/*
================
Monster_UpdateState
================
*/
static void Monster_UpdateState(gentity_t *monster) {
	monster->sv.entFlags = SVFL_NONE;
	if (monster->groundEntity != nullptr) {
		monster->sv.entFlags |= SVFL_ONGROUND;
	}

	if (monster->takeDamage) {
		monster->sv.entFlags |= SVFL_TAKES_DAMAGE;
	}

	if (monster->solid == SOLID_NOT || monster->moveType == MoveType::None) {
		monster->sv.entFlags |= SVFL_IS_HIDDEN;
	}

	if ((monster->flags & FL_INWATER) != 0) {
		monster->sv.entFlags |= SVFL_IN_WATER;
	}

	if (CooperativeModeOn()) {
		monster->sv.team = Team_Coop_Monster;
	} else {
		monster->sv.team = Team_None; // TODO: CTF/TDM/etc...
	}

	monster->sv.health = (monster->deadFlag != true) ? monster->health : -1;
	monster->sv.waterLevel = monster->waterLevel;
	monster->sv.enemy = monster->enemy;
	monster->sv.groundEntity = monster->groundEntity;

	int32_t viewHeight = monster->viewHeight;
	if ((monster->monsterInfo.aiFlags & AI_DUCKED) != 0) {
		viewHeight = int32_t(monster->maxs[2] - 4.0f);
	}
	monster->sv.viewHeight = viewHeight;

	monster->sv.viewAngles = monster->s.angles;

	AngleVectors(monster->s.angles, monster->sv.viewForward, nullptr, nullptr);

	monster->sv.velocity = monster->velocity;

	if (!monster->sv.init) {
		monster->sv.init = true;
		monster->sv.className = monster->className;
		monster->sv.targetName = monster->targetName;
		monster->sv.startingHealth = monster->health;
		monster->sv.maxHealth = monster->maxHealth;

		gi.Bot_RegisterEntity(monster);
	}
}

/*
================
Item_UpdateState
================
*/
static void Item_UpdateState(gentity_t *item) {
	item->sv.entFlags = SVFL_IS_ITEM;
	item->sv.respawnTime = 0;

	if (item->team != nullptr) {
		item->sv.entFlags |= SVFL_IN_TEAM;
	} // some DM maps have items chained together in teams...

	if (item->solid == SOLID_NOT) {
		item->sv.entFlags |= SVFL_IS_HIDDEN;

		if (item->nextThink.milliseconds() > 0) {
			if ((item->svFlags & SVF_RESPAWNING) != 0) {
				const GameTime pendingRespawnTime = (item->nextThink - level.time);
				item->sv.respawnTime = static_cast<int32_t>(pendingRespawnTime.milliseconds());
			} else {
				// item will respawn at some unknown time in the future...
				item->sv.respawnTime = Item_UnknownRespawnTime;
			}
		}
	}

	const item_id_t itemID = item->item->id;
	if (itemID == IT_FLAG_RED || itemID == IT_FLAG_BLUE) {
		item->sv.entFlags |= SVFL_IS_OBJECTIVE;

		const Team flagTeam = (itemID == IT_FLAG_RED) ? Team::Red : Team::Blue;
		const FlagStatus flagStatus = GetFlagStatus(flagTeam);

		item->sv.entFlags &= ~(SVFL_OBJECTIVE_AT_BASE | SVFL_OBJECTIVE_TAKEN | SVFL_OBJECTIVE_DROPPED);
		switch (flagStatus) {
		case FlagStatus::AtBase:
			item->sv.entFlags |= SVFL_OBJECTIVE_AT_BASE;
			break;
		case FlagStatus::Dropped:
			item->sv.entFlags |= SVFL_OBJECTIVE_DROPPED;
			break;
		case FlagStatus::Taken:
		case FlagStatus::TakenRed:
		case FlagStatus::TakenBlue:
			item->sv.entFlags |= SVFL_OBJECTIVE_TAKEN;
			break;
		default:
			break;
		}
	}

	// always need to update these for items, since random item spawning
	// could change them at any time...
	item->sv.className = item->className;
	item->sv.itemID = item->item->id;

	if (!item->sv.init) {
		item->sv.init = true;
		item->sv.targetName = item->targetName;

		gi.Bot_RegisterEntity(item);
	}
}

/*
================
Trap_UpdateState
================
*/
static void Trap_UpdateState(gentity_t *danger) {
	danger->sv.entFlags = SVFL_TRAP_DANGER;
	danger->sv.velocity = danger->velocity;

	if (danger->owner != nullptr && danger->owner->client != nullptr) {
		player_skinnum_t pl_skinnum{};
		pl_skinnum.skinNum = danger->owner->s.skinNum;
		danger->sv.team = pl_skinnum.teamIndex;
	}

	if (danger->groundEntity != nullptr) {
		danger->sv.entFlags |= SVFL_ONGROUND;
	}

	if ((danger->flags & FL_TRAP_LASER_FIELD) == 0) {
		danger->sv.entFlags |= SVFL_ACTIVE; // non-lasers are always active
	} else {
		danger->sv.startOrigin = danger->s.origin;
		danger->sv.endOrigin = danger->s.oldOrigin;
		if ((danger->svFlags & SVF_NOCLIENT) == 0) {
			if ((danger->s.renderFX & RF_BEAM)) {
				danger->sv.entFlags |= SVFL_ACTIVE; // lasers are active!!
			}
		}
	}

	if (!danger->sv.init) {
		danger->sv.init = true;
		danger->sv.className = danger->className;

		gi.Bot_RegisterEntity(danger);
	}
}

/*
================
Mover_UpdateState
================
*/
static void Mover_UpdateState(gentity_t *entity) {
	entity->sv.entFlags = SVFL_NONE;
	entity->sv.health = entity->health;

	if (entity->takeDamage) {
		entity->sv.entFlags |= SVFL_TAKES_DAMAGE;
	}

	// plats, movers, and doors use this to determine move state.
	const bool isDoor = ((entity->svFlags & SVF_DOOR) != 0);
	const bool isReversedDoor = (isDoor && entity->spawnFlags.has(SPAWNFLAG_DOOR_REVERSE));

	// doors have their top/bottom states reversed from plats
	// ( unless "reverse" spawnflag is set! )
	if (isDoor && !isReversedDoor) {
		if (entity->moveInfo.state == MoveState::Top) {
			entity->sv.entFlags |= SVFL_MOVESTATE_BOTTOM;
		} else if (entity->moveInfo.state == MoveState::Bottom) {
			entity->sv.entFlags |= SVFL_MOVESTATE_TOP;
		}
	} else {
		if (entity->moveInfo.state == MoveState::Top) {
			entity->sv.entFlags |= SVFL_MOVESTATE_TOP;
		} else if (entity->moveInfo.state == MoveState::Bottom) {
			entity->sv.entFlags |= SVFL_MOVESTATE_BOTTOM;
		}
	}

	if (entity->moveInfo.state == MoveState::Up || entity->moveInfo.state == MoveState::Down) {
		entity->sv.entFlags |= SVFL_MOVESTATE_MOVING;
	}

	entity->sv.startOrigin = entity->moveInfo.startOrigin;
	entity->sv.endOrigin = entity->moveInfo.endOrigin;

	if (entity->svFlags & SVF_DOOR) {
		if (entity->flags & FL_LOCKED) {
			entity->sv.entFlags |= SVFL_IS_LOCKED_DOOR;
		}
	}

	if (!entity->sv.init) {
		entity->sv.init = true;
		entity->sv.className = entity->className;
		entity->sv.targetName = entity->targetName;
		entity->sv.spawnFlags = entity->spawnFlags.value;
	}
}

/*
================
Entity_UpdateState
================
*/
void Entity_UpdateState(gentity_t *ent) {
	if (ent->svFlags & SVF_MONSTER) {
		Monster_UpdateState(ent);
	} else if (ent->flags & FL_TRAP || ent->flags & FL_TRAP_LASER_FIELD) {
		Trap_UpdateState(ent);
	} else if (ent->item != nullptr) {
		Item_UpdateState(ent);
	} else if (ent->client != nullptr) {
		Player_UpdateState(ent);
	} else {
		Mover_UpdateState(ent);
	}
}

static USE(info_nav_lock_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	gentity_t *n = nullptr;

	while ((n = G_FindByString<&gentity_t::targetName>(n, self->target))) {
		if (!(n->svFlags & SVF_DOOR)) {
			gi.Com_PrintFmt("{} tried targeting {}, a non-SVF_DOOR\n", *self, *n);
			continue;
		}

		n->flags ^= FL_LOCKED;
	}
}

/*QUAKED info_nav_lock (1.0 1.0 0.0) (-16 -16 0) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Toggles locked state on linked entity.
*/
void SP_info_nav_lock(gentity_t *self) {
	if (!self->targetName) {
		gi.Com_PrintFmt("{} missing targetName\n", *self);
		FreeEntity(self);
		return;
	}

	if (!self->target) {
		gi.Com_PrintFmt("{} missing target\n", *self);
		FreeEntity(self);
		return;
	}

	self->svFlags |= SVF_NOCLIENT;
	self->use = info_nav_lock_use;
}

/*
================
FindLocalPlayer
================
*/
	const gentity_t *FindLocalPlayer() {
	const gentity_t *localPlayer = nullptr;

	const gentity_t *ent = &g_entities[0];
	for (size_t i = 0; i < globals.numEntities; i++, ent++) {
		if (!ent->inUse || !(ent->svFlags & SVF_PLAYER)) {
			continue;
		}

		if (ent->health <= 0) {
			continue;
		}

		localPlayer = ent;
		break;
	}

	return localPlayer;
}

/*
================
FindFirstBot
================
*/
	const gentity_t *FindFirstBot() {
	const gentity_t *firstBot = nullptr;

	const gentity_t *ent = &g_entities[0];
	for (size_t i = 0; i < globals.numEntities; i++, ent++) {
		if (!ent->inUse || !(ent->svFlags & SVF_PLAYER)) {
			continue;
		}

		if (ent->health <= 0) {
			continue;
		}

		if (!(ent->svFlags & SVF_BOT)) {
			continue;
		}

		firstBot = ent;
		break;
	}

	return firstBot;
}

/*
================
FindFirstMonster
================
*/
	const gentity_t *FindFirstMonster() {
	const gentity_t *firstMonster = nullptr;

	const gentity_t *ent = &g_entities[0];
	for (size_t i = 0; i < globals.numEntities; i++, ent++) {
		if (!ent->inUse || !(ent->svFlags & SVF_MONSTER)) {
			continue;
		}

		if (ent->health <= 0) {
			continue;
		}

		firstMonster = ent;
		break;
	}

	return firstMonster;
}

/*
================
FindFirstMonster

"Actors" are either players or monsters - i.e. something alive and thinking.
================
*/
	const gentity_t *FindActorUnderCrosshair(const gentity_t *player) {
	if (player == nullptr || !player->inUse) {
		return nullptr;
	}

	Vector3 forward, right, up;
	AngleVectors(player->client->vAngle, forward, right, up);

	const Vector3 eye_position = (player->s.origin + Vector3{ 0.0f, 0.0f, (float)player->viewHeight });
	const Vector3 end = (eye_position + (forward * 8192.0f));
	const contents_t mask = (MASK_PROJECTILE & ~CONTENTS_DEADMONSTER);

	trace_t tr = gi.traceLine(eye_position, end, player, mask);

	const gentity_t *traceEnt = tr.ent;
	if (traceEnt == nullptr || !tr.ent->inUse) {
		return nullptr;
	}

	if (!(traceEnt->svFlags & SVF_PLAYER) && !(traceEnt->svFlags & SVF_MONSTER)) {
		return nullptr;
	}

	if (traceEnt->health <= 0) {
		return nullptr;
	}

	return traceEnt;
}
