/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

m_actor.cpp (AI Companion Actor) This file has been corrected to restore the functionality from
the original QuakeActor mod by Shaun 'Cyberslash' Wilson. The AI logic, weapon handling,*/

#include "../g_local.hpp"
#include "m_actor.hpp"

[[maybe_unused]] constexpr SpawnFlags SPAWNFLAG_TARGET_ACTOR_JUMP = 1_spawnflag;
[[maybe_unused]] constexpr SpawnFlags SPAWNFLAG_TARGET_ACTOR_SHOOT = 2_spawnflag;
[[maybe_unused]] constexpr SpawnFlags SPAWNFLAG_TARGET_ACTOR_ATTACK = 4_spawnflag;
[[maybe_unused]] constexpr SpawnFlags SPAWNFLAG_TARGET_ACTOR_DONT_ATTACK = 8_spawnflag;
[[maybe_unused]] constexpr SpawnFlags SPAWNFLAG_TARGET_ACTOR_HOLD = 32_spawnflag;
[[maybe_unused]] constexpr SpawnFlags SPAWNFLAG_TARGET_ACTOR_BRUTAL = 64_spawnflag;

// Forward declarations for AI state functions
void actor_stand(gentity_t* self);
void actor_run(gentity_t* self);
void actor_walk(gentity_t* self);
void actor_attack(gentity_t* self);
void actor_dead(gentity_t* self);
void actor_use(gentity_t* self, gentity_t* other, gentity_t* activator);
void actor_pain(gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod);
void actor_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod);
void actor_fire(gentity_t* self);
void actor_attacktarget(gentity_t* self, gentity_t* targ);

// Global sound indices for actor weapons
static int actor_chaingun_loop;
static int actor_chaingun_winddown;
static int actor_hyperb_loop;

// Actor names for random assignment if not specified in the editor
constexpr std::array<const char*, 4> actor_names_male = { "Bitterman", "Howitzer", "Rambear", "Disruptor" };
constexpr std::array<const char*, 4> actor_names_female = { "Lotus", "Athena", "Voodoo", "Jezebel" };
constexpr std::array<const char*, 3> actor_names_cyborg = { "ONI911", "PS9000", "TYR574" };
constexpr std::array<const char*, 4> actor_pain_messages = { "Watch it", "#$@*& you", "Idiot", "Check your targets" };


//============================================================================
// ACTOR UTILITY FUNCTIONS (Ported from C)
//============================================================================

/*
================
CheckSeenKiller

This checks to see if any other actors saw a client
killing the actor killed, and if so they attack the client.
================
*/
static void CheckSeenKiller(gentity_t* victim, gentity_t* killer)
{
	for (uint32_t j = 1; j < game.maxEntities; j++) {
		gentity_t* other = &g_entities[j];
		if (!other->inUse)
			continue;
		if (!(other->monsterInfo.aiFlags & AI_GOOD_GUY))
			continue;
		if (other->enemy)
			continue;
		// Check visibility and add a random chance to prevent all actors from attacking at once
		if (visible(other, killer, false) && (rand() < 0.5f)) {
			other->enemy = killer;
			FoundTarget(other);
		}
	}
}

//============================================================================
// ACTOR WEAPONRY (Ported from C)
//============================================================================

// Use original MZ_ muzzleflash instead of MZ2_
static void actorMuzzleflash(gentity_t* self, const Vector3& start, int flashType)
{
	gi.WriteByte(svc_muzzleflash);
	gi.WriteShort(self - g_entities);
	gi.WriteByte(static_cast<byte>(flashType));
	gi.multicast(start, MULTICAST_PVS, false);
}

static void actorBlaster(gentity_t* self, const Vector3& start, const Vector3& forward, bool hyper)
{
	if (hyper) {
		self->s.sound = actor_hyperb_loop;
		actorMuzzleflash(self, start, MZ_HYPERBLASTER);
		fire_blaster(self, start, forward, 15, 1000, EF_HYPERBLASTER, ModID::Blaster, false);
	}
	else {
		actorMuzzleflash(self, start, MZ_BLASTER);
		fire_blaster(self, start, forward, 10, 1000, EF_BLASTER, ModID::Blaster, false);
	}
}

static void actorShotgun(gentity_t* self, const Vector3& start, const Vector3& forward, bool super_shotgun)
{
	if (super_shotgun) {
		actorMuzzleflash(self, start, MZ_SSHOTGUN);
		// Fire two slightly offset shotgun blasts to simulate a super shotgun
		Vector3 v_l = self->s.angles;
		v_l[YAW] -= 5;
		Vector3 forward_l = AngleVectors(v_l).forward;
		fire_shotgun(self, start, forward_l, 6, 12, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SHOTGUN_COUNT, ModID::Unknown);

		Vector3 v_r = self->s.angles;
		v_r[YAW] += 5;
		Vector3 forward_r = AngleVectors(v_r).forward;
		fire_shotgun(self, start, forward_r, 6, 12, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SHOTGUN_COUNT, ModID::Unknown);
	}
	else {
		actorMuzzleflash(self, start, MZ_SHOTGUN);
		fire_shotgun(self, start, forward, 4, 8, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SHOTGUN_COUNT, ModID::Unknown);
	}
}

static void actorMachinegun(gentity_t* self, const Vector3& start, const Vector3& forward, bool chaingun)
{
	int shots = 1;
	if (chaingun) {
		switch (self->s.frame) {
		default:
		case FRAME_attack1:
			self->s.sound = actor_chaingun_loop;
			shots = 3;
			break;
		case FRAME_attack2:
			self->s.sound = actor_chaingun_loop;
			shots = 2;
			break;
		case FRAME_attack3:
			gi.sound(self, CHAN_AUTO, actor_chaingun_winddown, 1, ATTN_NORM, 0);
			shots = 1;
			break;
		}
		for (int i = 0; i < shots; i++) {
			actorMuzzleflash(self, start, static_cast<int>(MZ_CHAINGUN1 + (self->s.frame - FRAME_attack1)));
			fire_bullet(self, start, forward, 5, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, ModID::Unknown);
		}
	}
	else {
		actorMuzzleflash(self, start, MZ_MACHINEGUN);
		fire_bullet(self, start, forward, 3, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, ModID::Machinegun);
	}
}

static void actorRailgun(gentity_t* self, const Vector3& start, const Vector3& forward)
{
	actorMuzzleflash(self, start, MZ_RAILGUN);
	fire_rail(self, start, forward, 50, 100);
}

// Main fire function, called from attack animation frames
void actor_attacktarget(gentity_t* self, gentity_t* targ)
{
	self->s.sound = 0;

	// Project the shot from the weapon model
	Vector3 forward, right;
	AngleVectors(self->s.angles, forward, right, nullptr);
	Vector3 start = G_ProjectSource(self->s.origin, { 24, 8, static_cast<float>(self->viewHeight) - 8 }, forward, right);

	Vector3 aimDir;
	if (targ && targ->inUse) {
		Vector3 targetPos = targ->s.origin;
		if (targ->health > 0) {
			targetPos[2] += targ->viewHeight;
			targetPos -= targ->velocity * 0.2f;
		}
		else {
			targetPos = targ->absMin;
			targetPos[2] += targ->size[2] / 2;
		}
		aimDir = (targetPos - start).normalized();
	}
	else {
		aimDir = forward;
	}

	switch (self->style) {
	case 1: actorBlaster(self, start, aimDir, false); break;
	case 2: actorShotgun(self, start, aimDir, false); break;
	case 3: actorShotgun(self, start, aimDir, true); break;
	case 4:
		actorMachinegun(self, start, aimDir, false);
		if (level.time >= self->monsterInfo.pauseTime) self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;
		else self->monsterInfo.aiFlags |= AI_HOLD_FRAME;
		break;
	case 5:
		actorMachinegun(self, start, aimDir, true);
		if (level.time >= self->monsterInfo.pauseTime) self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;
		else self->monsterInfo.aiFlags |= AI_HOLD_FRAME;
		break;
	case 8:
		actorBlaster(self, start, aimDir, true);
		if (level.time >= self->monsterInfo.pauseTime) self->monsterInfo.aiFlags &= ~AI_HOLD_FRAME;
		else self->monsterInfo.aiFlags |= AI_HOLD_FRAME;
		break;
	case 9: actorRailgun(self, start, aimDir); break;
	}
}

void actor_fire(gentity_t* self)
{
	// Only fire on the first attack frame for most weapons
	if (self->style != 5 && self->s.frame != FRAME_attack1) {
		return;
	}

	actor_attacktarget(self, self->enemy);
}

static void actor_reload(gentity_t* self)
{
	// Jump to the end of the attack animation for faster-reloading weapons
	if (self->style <= 4) {
		self->s.frame = FRAME_attack8;
	}
}

//============================================================================
// ACTOR MOVEMENT AND ANIMATION FRAMES
//============================================================================

MonsterFrame actor_frames_stand[] = {
	{ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand},
	{ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand},
	{ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand},
	{ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}, {ai_stand}
};
MMOVE_T(actor_move_stand) = { FRAME_stand01, FRAME_stand40, actor_frames_stand, nullptr };

MonsterFrame actor_frames_walk[] = { {ai_walk, 30}, {ai_walk, 30}, {ai_walk, 30}, {ai_walk, 30}, {ai_walk, 30}, {ai_walk, 30} };
MMOVE_T(actor_move_walk) = { FRAME_run1, FRAME_run6, actor_frames_walk, nullptr };

MonsterFrame actor_frames_run[] = { {ai_run, 35}, {ai_run, 35}, {ai_run, 35}, {ai_run, 35}, {ai_run, 35}, {ai_run, 35} };
MMOVE_T(actor_move_run) = { FRAME_run1, FRAME_run6, actor_frames_run, nullptr };

MonsterFrame actor_frames_pain1[] = { {ai_move, -5}, {ai_move, 4}, {ai_move, 1}, {ai_move, 1} };
MMOVE_T(actor_move_pain1) = { FRAME_pain101, FRAME_pain104, actor_frames_pain1, actor_run };

MonsterFrame actor_frames_pain2[] = { {ai_move, -4}, {ai_move, 4}, {ai_move, 0}, {ai_move, 0} };
MMOVE_T(actor_move_pain2) = { FRAME_pain201, FRAME_pain204, actor_frames_pain2, actor_run };

MonsterFrame actor_frames_pain3[] = { {ai_move, -1}, {ai_move, 1}, {ai_move, 0}, {ai_move, 0} };
MMOVE_T(actor_move_pain3) = { FRAME_pain301, FRAME_pain304, actor_frames_pain3, actor_run };

MonsterFrame actor_frames_death1[] = { {ai_move}, {ai_move}, {ai_move, -13}, {ai_move, 14}, {ai_move, 3}, {ai_move, -2} };
MMOVE_T(actor_move_death1) = { FRAME_death101, FRAME_death106, actor_frames_death1, actor_dead };

MonsterFrame actor_frames_death2[] = { {ai_move}, {ai_move, 7}, {ai_move, -6}, {ai_move, -5}, {ai_move, 1}, {ai_move} };
MMOVE_T(actor_move_death2) = { FRAME_death201, FRAME_death206, actor_frames_death2, actor_dead };

MonsterFrame actor_frames_flipoff[] = {
	{ai_turn}, {ai_turn}, {ai_turn}, {ai_turn}, {ai_turn}, {ai_turn},
	{ai_turn}, {ai_turn}, {ai_turn}, {ai_turn}, {ai_turn}, {ai_turn}
};
MMOVE_T(actor_move_flipoff) = { FRAME_flip01, FRAME_flip12, actor_frames_flipoff, actor_run };

MonsterFrame actor_frames_taunt[] = {
	{ai_turn}, {ai_turn}, {ai_turn}, {ai_turn}, {ai_turn}, {ai_turn}, {ai_turn},
	{ai_turn}, {ai_turn}, {ai_turn}, {ai_turn}, {ai_turn}, {ai_turn}, {ai_turn},
	{ai_turn}, {ai_turn}, {ai_turn}
};
MMOVE_T(actor_move_taunt) = { FRAME_taunt01, FRAME_taunt17, actor_frames_taunt, actor_run };

MonsterFrame actor_frames_attack[] = {
	{ai_charge, -2, actor_fire},
	{ai_charge, -2, actor_fire}, // For chaingun
	{ai_charge, 3,  actor_fire}, // For chaingun
	{ai_charge, 2,  nullptr},
	{ai_charge, 1,  actor_reload},
	{ai_charge, 0,  nullptr},
	{ai_charge, 0,  nullptr},
	{ai_charge, 0,  nullptr}
};
MMOVE_T(actor_move_attack) = { FRAME_attack1, FRAME_attack8, actor_frames_attack, actor_run };

//============================================================================
// ACTOR AI BEHAVIOR
//============================================================================

MONSTERINFO_STAND(actor_stand)(gentity_t* self) -> void {
	M_SetAnimation(self, &actor_move_stand);

	// Randomize start frame
	if (level.time < 1_sec) {
		self->s.frame = self->monsterInfo.active_move->firstFrame + (rand() % (self->monsterInfo.active_move->lastFrame - self->monsterInfo.active_move->firstFrame + 1));
	}
}

MONSTERINFO_WALK(actor_walk)(gentity_t* self) -> void { M_SetAnimation(self, &actor_move_walk); }

MONSTERINFO_RUN(actor_run)(gentity_t* self) -> void
{
	if ((level.time < self->pain_debounce_time) && (!self->enemy)) {
		if (self->moveTarget) self->monsterInfo.walk(self);
		else self->monsterInfo.stand(self);
		return;
	}

	if (self->monsterInfo.aiFlags & AI_STAND_GROUND) {
		self->monsterInfo.stand(self);
		return;
	}
	M_SetAnimation(self, &actor_move_run);
}

MONSTERINFO_ATTACK(actor_attack)(gentity_t* self) -> void
{
	M_SetAnimation(self, &actor_move_attack);
	// Set pause time for machinegun/chaingun bursts
	self->monsterInfo.pauseTime = level.time + GameTime::from_sec(rand() % 15 + self->style) * gi.frameTimeMs;
}

PAIN(actor_pain)(gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void
{
	if (level.time < self->pain_debounce_time) return;
	self->pain_debounce_time = level.time + 3_sec;

	// If shot by a player, taunt them
	if (other->client && (rand() < 0.4f)) {
		self->ideal_yaw = vectoyaw(other->s.origin - self->s.origin);
		if (rand() < 0.5f) M_SetAnimation(self, &actor_move_flipoff);
		else M_SetAnimation(self, &actor_move_taunt);
                gi.LocClient_Print(other, PRINT_CHAT, "$g_sgame_auto_e7c7ff2067be", self->message, actor_pain_messages[rand() % 4], other->client->pers.netName);
		return;
	}

	// Standard pain animation
	int n = irandom(3);
	if (n == 0) {
		M_SetAnimation(self, &actor_move_pain1);
		gi.sound(self, CHAN_VOICE, gi.soundIndex(std::format("../players/{}/pain100_1.wav", self->model).data()), 1, ATTN_NORM, 0);
	}
	else if (n == 1) {
		M_SetAnimation(self, &actor_move_pain2);
		gi.sound(self, CHAN_VOICE, gi.soundIndex(std::format("../players/{}/pain100_2.wav", self->model).data()), 1, ATTN_NORM, 0);
	}
	else {
		M_SetAnimation(self, &actor_move_pain3);
		gi.sound(self, CHAN_VOICE, gi.soundIndex(std::format("../players/{}/pain75_1.wav", self->model).data()), 1, ATTN_NORM, 0);
	}
}

void actor_dead(gentity_t* self)
{
	self->mins = { -16, -16, -24 };
	self->maxs = { 16, 16, -8 };
	self->moveType = MoveType::Toss;
	self->svFlags |= SVF_DEADMONSTER;
	self->nextThink = 0_ms;
	gi.linkEntity(self);
}

DIE(actor_die)(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void
{
	// Remove weapon model
	self->s.modelIndex2 = 0;

	// Let other actors know who the killer is
	if (attacker) {
		CheckSeenKiller(self, attacker);
	}

	// Check for gib
	if (self->health <= -80) {
		ThrowGibs(self, damage, {
			{2, "models/objects/gibs/bone/tris.md2"},
			{4, "models/objects/gibs/sm_meat/tris.md2"},
			{"models/objects/gibs/head2/tris.md2", GIB_HEAD}
			});
		self->deadFlag = true;
		return;
	}

	if (self->deadFlag) return;

	self->deadFlag = true;
	self->takeDamage = true;

	int n = irandom(2);
	if (n == 0) {
		M_SetAnimation(self, &actor_move_death1);
		gi.sound(self, CHAN_VOICE, gi.soundIndex(std::format("../players/{}/death1.wav", self->model).data()), 1, ATTN_NORM, 0);
	}
	else {
		M_SetAnimation(self, &actor_move_death2);
		gi.sound(self, CHAN_VOICE, gi.soundIndex(std::format("../players/{}/death2.wav", self->model).data()), 1, ATTN_NORM, 0);
	}
}

//============================================================================
// ACTOR SPAWNING
//============================================================================

void actor_use(gentity_t* self, gentity_t* other, gentity_t* activator)
{
	if (!self->target) {
		return;
	}
	
	self->moveTarget = self->goalEntity = PickTarget(self->target);
	if (!self->moveTarget || strcmp(self->moveTarget->className, "target_actor") != 0) {
		gi.Com_PrintFmt("{}: has bad target {}\n", *self, self->target);
		self->target = nullptr;
		self->monsterInfo.stand(self);
		return;
	}
	self->ideal_yaw = self->s.angles[YAW] = vectoyaw(self->goalEntity->s.origin - self->s.origin);
	self->monsterInfo.walk(self);
	self->target = nullptr;
}

/*QUAKED misc_actor (1 .5 0) (-16 -16 -24) (16 16 32) x TRIGGER_SPAWN x x NO_VWEP DONT_ATTACK
Friendly AI actor that follows the player and assists in combat.

Properties:
"style"   - Weapon to use (1:Blaster, 2:Shotgun, 3:SSG, 4:MG, 5:CG, 8:HB, 9:RG)
"model"   - Model to use ("male", "female", "cyborg")
"count"   - Skin number to use
"message" - Actor's name (for chat messages)

Spawnflags:
NO_VWEP (8)       - Will use plain weapon.md2 instead of a weapon-specific VWEP model.
DONT_ATTACK (16)  - Will not attack enemies until it reaches its first target_actor.
*/
void SP_misc_actor(gentity_t* self)
{
	if (deathmatch->integer) {
		FreeEntity(self);
		return;
	}

	// Precache sounds
	actor_chaingun_loop = gi.soundIndex("weapons/chngnl1a.wav");
	actor_chaingun_winddown = gi.soundIndex("weapons/chngnd1a.wav");
	actor_hyperb_loop = gi.soundIndex("weapons/hyperbl1a.wav");

	self->moveType = MoveType::Step;
	self->solid = SOLID_BBOX;
	self->mins = { -16, -16, -24 };
	self->maxs = { 16, 16, 32 };

	// Set model, skin, and name with defaults if not provided
	if (!self->model) {
		int model_choice = irandom(3);
		if (model_choice == 0) {
			// FIX: Use ED_NewString for all string field assignments
			self->model = ED_NewString("male");
			self->count = irandom(actor_names_male.size());
			if (!self->message) self->message = ED_NewString(actor_names_male[self->count]);
		}
		else if (model_choice == 1) {
			// FIX: Use ED_NewString
			self->model = ED_NewString("female");
			self->count = irandom(actor_names_female.size());
			if (!self->message) self->message = ED_NewString(actor_names_female[self->count]);
		}
		else {
			// FIX: Use ED_NewString
			self->model = ED_NewString("cyborg");
			self->count = irandom(actor_names_cyborg.size());
			if (!self->message) self->message = ED_NewString(actor_names_cyborg[self->count]);
		}
	}
	self->s.modelIndex = gi.modelIndex(std::format("models/actor/{}.md2", self->model).data());
	self->s.skinNum = self->count;

	// Set weapon style with a random default
	if (!self->style) {
		self->style = irandom(1, 7);
		if (self->style > 5) self->style += 2; // Skip grenade launcher and rocket launcher
	}
#if 0
	// Set weapon model based on style
	const char* wep_model_name = "weapon";
	if (!self->spawnFlags.has(8_spawnflag)) { // NO_VWEP
		switch (self->style) {
		case 1: wep_model_name = "w_blaster"; break;
		case 2: wep_model_name = "w_shotgun"; break;
		case 3: wep_model_name = "w_sshotgun"; break;
		case 4: wep_model_name = "w_machinegun"; break;
		case 5: wep_model_name = "w_chaingun"; break;
		case 8: wep_model_name = "w_hyperblaster"; break;
		case 9: wep_model_name = "w_railgun"; break;
		}
	}
	self->s.modelIndex2 = gi.modelIndex(std::format("players/{}/{}.md2", self->model, wep_model_name).data());
#endif
	self->health = self->maxHealth = 100;
	self->gibHealth = -50;
	self->mass = 200;
	
	self->pain = actor_pain;
	self->die = actor_die;
	//self->use = actor_use;
	self->monsterInfo.stand = actor_stand;
	self->monsterInfo.walk = actor_walk;
	self->monsterInfo.run = actor_run;
	self->monsterInfo.attack = actor_attack;
	self->monsterInfo.sight = nullptr; // Uses modified FindTarget logic
	self->monsterInfo.aiFlags |= AI_GOOD_GUY;

	gi.linkEntity(self);
	
	self->monsterInfo.active_move = &actor_move_stand;
	self->monsterInfo.scale = MODEL_SCALE;
	walkmonster_start(self);

	// monster_start resets skin, so we set it again
	self->s.skinNum = self->count;
}

//============================================================================
// SCRIPTING ENTITY: target_actor
//============================================================================

/*QUAKED target_actor (.5 .3 0) (-8 -8 -8) (8 8 8) JUMP SHOOT ATTACK DONT_ATTACK HOLD BRUTAL
Scripts the behavior of a misc_actor.

Spawnflags:
JUMP (1)        - jump in set direction upon reaching this target
SHOOT (2)       - take a single shot at the pathTarget
ATTACK (4)      - attack pathTarget until it or actor is dead
DONT_ATTACK (8) - actor will not attack enemies until it reaches its NEXT target_actor
HOLD (32)       - hold position while attacking
BRUTAL (64)     - be brutal in combat (more aggressive)

"target"     - next target_actor in the path
"pathTarget" - target of any action (shoot/attack)
"wait"       - amount of time actor should pause at this point
"message"    - actor will "say" this to all players
"speed"      - (for JUMP) speed thrown forward (default 200)
"height"     - (for JUMP) speed thrown upwards (default 200)
*/
static TOUCH(target_actor_touch)(gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void
{
	if (other->moveTarget != self) return;
	if (other->enemy) return;

	other->goalEntity = other->moveTarget = nullptr;

	if (self->message) {
		gi.Com_PrintFmt("{}: {}\n", other->message, self->message);
	}

	if (self->wait) {
		//other->monsterInfo.wait = level.time + GameTime::from_sec(self->wait);
	}

	// JUMP
	if (self->spawnFlags.has(1_spawnflag)) {
		other->velocity = self->moveDir * self->speed;
		if (other->groundEntity) {
			other->groundEntity = nullptr;
			other->velocity[_Z] = self->moveDir[2];
			gi.sound(other, CHAN_VOICE, gi.soundIndex("player/male/jump1.wav"), 1, ATTN_NORM, 0);
		}
	}
	// SHOOT
	else if (self->spawnFlags.has(2_spawnflag)) {
		if (self->pathTarget) {
			gentity_t* targ = PickTarget(self->pathTarget);
			other->monsterInfo.pauseTime = level.time;
			actor_attacktarget(other, targ);
			if (targ && targ->use) targ->use(targ, other, other);
		}
	}
	// ATTACK
	else if (self->spawnFlags.has(4_spawnflag)) {
		if (self->pathTarget) {
			other->enemy = PickTarget(self->pathTarget);
			if (other->enemy) {
				other->goalEntity = other->enemy;
				if (self->spawnFlags.has(64_spawnflag)) other->monsterInfo.aiFlags |= AI_BRUTAL;
				if (self->spawnFlags.has(32_spawnflag)) {
					other->monsterInfo.aiFlags |= AI_STAND_GROUND;
					actor_stand(other);
				}
				else {
					actor_run(other);
				}
			}
		}
	}

	// Update DONT_ATTACK status
	if (self->spawnFlags.has(8_spawnflag) && !other->spawnFlags.has(16_spawnflag)) {
		other->spawnFlags |= 16_spawnflag;
	}
	else if (!self->spawnFlags.has(8_spawnflag) && other->spawnFlags.has(16_spawnflag)) {
		other->spawnFlags &= ~16_spawnflag;
	}


	// Find next target in path
	if (self->target) {
		other->moveTarget = PickTarget(self->target);
	}
	else {
		other->monsterInfo.pauseTime = HOLD_FOREVER;
		other->monsterInfo.stand(other);
		return;
	}

	if (!other->goalEntity) {
		other->goalEntity = other->moveTarget;
	}

	if (!other->moveTarget && !other->enemy) {
		other->monsterInfo.pauseTime = HOLD_FOREVER;
		other->monsterInfo.stand(other);
	}
	else if (other->moveTarget == other->goalEntity) {
		other->ideal_yaw = vectoyaw(other->moveTarget->s.origin - other->s.origin);
	}
}

void SP_target_actor(gentity_t* self)
{
	if (!self->targetName) {
		gi.Com_PrintFmt("{} with no targetname.\n", *self);
	}

	self->solid = SOLID_TRIGGER;
	self->touch = target_actor_touch;
	self->mins = { -8, -8, -8 };
	self->maxs = { 8, 8, 8 };
	self->svFlags = SVF_NOCLIENT;

	if (self->spawnFlags.has(1_spawnflag)) { // JUMP
		if (!self->speed) self->speed = 200;
		if (!st.height) st.height = 200;
		if (self->s.angles[YAW] == 0) self->s.angles[YAW] = 360;
		SetMoveDir(self->s.angles, self->moveDir);
		self->moveDir[2] = (float)st.height;
	}

	gi.linkEntity(self);
}
