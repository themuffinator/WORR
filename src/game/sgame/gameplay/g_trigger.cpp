/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_trigger.cpp (Game Triggers) This file implements the behavior of various "trigger_*" entities.
Triggers are invisible, solid (or non-solid) volumes in the world that activate their targets
when touched by other entities (usually players or monsters). They are a fundamental part of
level scripting. Key Responsibilities: - Basic Triggers: Implements standard triggers like
`trigger_once` and `trigger_multiple`. - Specialized Triggers: Contains logic for more complex
triggers, such as `trigger_hurt` (which applies damage), `trigger_push` (which applies a
velocity change), and `trigger_teleport`. - Conditional Triggers: Implements triggers that
require specific conditions to be met, like `trigger_key` (requires a key item) or
`trigger_counter`. - Initialization: The `InitTrigger` function sets up the common properties
for all trigger entities.*/

#include "../g_local.hpp"
#include "../../bgame/char_array_utils.hpp"

constexpr SpawnFlags SPAWNFLAG_TRIGGER_MONSTER = 0x01_spawnflag;
constexpr SpawnFlags SPAWNFLAG_TRIGGER_NOT_PLAYER = 0x02_spawnflag;
constexpr SpawnFlags SPAWNFLAG_TRIGGER_TRIGGERED = 0x04_spawnflag;
constexpr SpawnFlags SPAWNFLAG_TRIGGER_TOGGLE = 0x08_spawnflag;
constexpr SpawnFlags SPAWNFLAG_TRIGGER_LATCHED = 0x10_spawnflag;
constexpr SpawnFlags SPAWNFLAG_TRIGGER_CLIP = 0x20_spawnflag;

static void InitTrigger(gentity_t* self) {
	if (st.was_key_specified("angle") || st.was_key_specified("angles") || self->s.angles)
		SetMoveDir(self->s.angles, self->moveDir);

	self->solid = SOLID_TRIGGER;
	self->moveType = MoveType::None;
	// [Paril-KEX] adjusted to allow mins/maxs to be defined
	// by hand instead
	if (self->model)
		gi.setModel(self, self->model);
	self->svFlags = SVF_NOCLIENT;
}

// the wait time has passed, so set back up for another activation
static THINK(multi_wait) (gentity_t* ent) -> void {
	ent->nextThink = 0_ms;
}

// the trigger was just activated
// ent->activator should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
static void multi_trigger(gentity_t* ent) {
	if (ent->nextThink)
		return; // already been triggered

	UseTargets(ent, ent->activator);

	if (ent->wait > 0) {
		ent->think = multi_wait;
		ent->nextThink = level.time + GameTime::from_sec(ent->wait + ent->random * crandom());
	}
	else { // we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		ent->touch = nullptr;
		ent->nextThink = level.time + FRAME_TIME_S;
		ent->think = FreeEntity;
	}
}

static USE(Use_Multi) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	// PGM
	if (ent->spawnFlags.has(SPAWNFLAG_TRIGGER_TOGGLE)) {
		if (ent->solid == SOLID_TRIGGER)
			ent->solid = SOLID_NOT;
		else
			ent->solid = SOLID_TRIGGER;
		gi.linkEntity(ent);
	}
	else {
		ent->activator = activator;
		multi_trigger(ent);
	}
	// PGM
}

static TOUCH(Touch_Multi) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (other->client) {
		if (self->spawnFlags.has(SPAWNFLAG_TRIGGER_NOT_PLAYER))
			return;
	}
	else if (other->svFlags & SVF_MONSTER) {
		if (!self->spawnFlags.has(SPAWNFLAG_TRIGGER_MONSTER))
			return;
	}
	else
		return;

	if (CombatIsDisabled())
		return;

	if (self->spawnFlags.has(SPAWNFLAG_TRIGGER_CLIP)) {
		trace_t clip = gi.clip(self, other->s.origin, other->mins, other->maxs, other->s.origin, G_GetClipMask(other));

		if (clip.fraction == 1.0f)
			return;
	}

	if (self->moveDir) {
		Vector3 forward;

		AngleVectors(other->s.angles, forward, nullptr, nullptr);
		if (forward.dot(self->moveDir) < 0)
			return;
	}

	self->activator = other;
	multi_trigger(self);
}

/*QUAKED trigger_multiple (.5 .5 .5) ? MONSTER NOT_PLAYER TRIGGERED TOGGLE LATCHED x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Variable sized repeatable trigger.  Must be targeted at one or more entities.
If "delay" is set, the trigger waits some time after activating before firing.
"wait" : Seconds between triggerings. (.2 default)
"random": Wait variance, default is 0.

TOGGLE - using this trigger will activate/deactivate it. trigger will begin inactive.

sounds
1)	secret
2)	beep beep
3)	large switch
4)
set "message" to text string
*/
static USE(trigger_enable) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->solid = SOLID_TRIGGER;
	self->use = Use_Multi;
	gi.linkEntity(self);
}

static BoxEntitiesResult_t latched_trigger_filter(gentity_t* other, void* data) {
	gentity_t* self = (gentity_t*)data;

	if (other->client) {
		if (self->spawnFlags.has(SPAWNFLAG_TRIGGER_NOT_PLAYER))
			return BoxEntitiesResult_t::Skip;
	}
	else if (other->svFlags & SVF_MONSTER) {
		if (!self->spawnFlags.has(SPAWNFLAG_TRIGGER_MONSTER))
			return BoxEntitiesResult_t::Skip;
	}
	else
		return BoxEntitiesResult_t::Skip;

	if (self->moveDir) {
		Vector3 forward;

		AngleVectors(other->s.angles, forward, nullptr, nullptr);
		if (forward.dot(self->moveDir) < 0)
			return BoxEntitiesResult_t::Skip;
	}

	self->activator = other;
	return BoxEntitiesResult_t::Keep | BoxEntitiesResult_t::End;
}

static THINK(latched_trigger_think) (gentity_t* self) -> void {
	self->nextThink = level.time + 1_ms;

	bool any_inside = !!gi.BoxEntities(self->absMin, self->absMax, nullptr, 0, AREA_SOLID, latched_trigger_filter, self);

	if (!!self->count != any_inside) {
		UseTargets(self, self->activator);
		self->count = any_inside ? 1 : 0;
	}
}

void SP_trigger_multiple(gentity_t* ent) {
	if (ent->sounds == 1)
		ent->noiseIndex = gi.soundIndex("misc/secret.wav");
	else if (ent->sounds == 2)
		ent->noiseIndex = gi.soundIndex("misc/talk.wav");
	else if (ent->sounds == 3)
		ent->noiseIndex = gi.soundIndex("misc/trigger1.wav");

	if (!ent->wait)
		ent->wait = 0.2f;

	InitTrigger(ent);

	if (ent->spawnFlags.has(SPAWNFLAG_TRIGGER_LATCHED)) {
		if (ent->spawnFlags.has(SPAWNFLAG_TRIGGER_TRIGGERED | SPAWNFLAG_TRIGGER_TOGGLE))
			gi.Com_PrintFmt("{}: latched and triggered/toggle are not supported\n", *ent);

		ent->think = latched_trigger_think;
		ent->nextThink = level.time + 1_ms;
		ent->use = Use_Multi;
		return;
	}
	else
		ent->touch = Touch_Multi;

	if (ent->spawnFlags.has(SPAWNFLAG_TRIGGER_TRIGGERED | SPAWNFLAG_TRIGGER_TOGGLE)) {
		ent->solid = SOLID_NOT;
		ent->use = trigger_enable;
	}
	else {
		ent->solid = SOLID_TRIGGER;
		ent->use = Use_Multi;
	}

	gi.linkEntity(ent);

	if (ent->spawnFlags.has(SPAWNFLAG_TRIGGER_CLIP))
		ent->svFlags |= SVF_HULL;
}

/*QUAKED trigger_once (.5 .5 .5) ? x x TRIGGERED x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Triggers once, then removes itself.
You must set the key "target" to the name of another object in the level that has a matching "targetName".

If TRIGGERED, this trigger must be triggered before it is live.

sounds
 1)	secret
 2)	beep beep
 3)	large switch
 4)

"message"	string to be displayed when triggered
*/

void SP_trigger_once(gentity_t* ent) {
	// make old maps work because I messed up on flag assignments here
	// triggered was on bit 1 when it should have been on bit 4
	if (ent->spawnFlags.has(SPAWNFLAG_TRIGGER_MONSTER)) {
		ent->spawnFlags &= ~SPAWNFLAG_TRIGGER_MONSTER;
		ent->spawnFlags |= SPAWNFLAG_TRIGGER_TRIGGERED;
		gi.Com_PrintFmt("{}: fixed TRIGGERED flag\n", *ent);
	}

	ent->wait = -1;
	SP_trigger_multiple(ent);
}

/*QUAKED trigger_relay (.5 .5 .5) (-8 -8 -8) (8 8 8) RED_ONLY BLUE_ONLY RANDOM x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This fixed size trigger cannot be touched, it can only be fired by other events.
The activator can be forced to be from a certain team.
if RANDOM is checked, only one of the targets will be fired, not all of them
*/
constexpr SpawnFlags SPAWNFLAGS_TRIGGER_RELAY_NO_SOUND = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAGS_TRIGGER_RELAY_RED_ONLY = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAGS_TRIGGER_RELAY_BLUE_ONLY = 2_spawnflag;
constexpr SpawnFlags SPAWNFLAGS_TRIGGER_RELAY_RANDOM = 4_spawnflag;

static USE(trigger_relay_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (self->crosslevel_flags && !(self->crosslevel_flags == (game.crossLevelFlags & SFL_CROSS_TRIGGER_MASK & self->crosslevel_flags)))
		return;

	if (deathmatch->integer) {
		if (self->spawnFlags.has(SPAWNFLAGS_TRIGGER_RELAY_RED_ONLY) && activator->client && activator->client->sess.team != Team::Red)
			return;

		if (self->spawnFlags.has(SPAWNFLAGS_TRIGGER_RELAY_BLUE_ONLY) && activator->client && activator->client->sess.team != Team::Blue)
			return;
	}

	if (self->spawnFlags.has(SPAWNFLAGS_TRIGGER_RELAY_RANDOM)) {
		gentity_t* ent;

		ent = PickTarget(self->target);
		if (ent && ent->use) {
			ent->use(ent, self, activator);
		}
		return;
	}

	UseTargets(self, activator);
}

void SP_trigger_relay(gentity_t* self) {
	self->use = trigger_relay_use;

	if ((!deathmatch->integer && self->spawnFlags.has(SPAWNFLAGS_TRIGGER_RELAY_NO_SOUND)) || deathmatch->integer)
		self->noiseIndex = -1;
}

/*
==============================================================================

trigger_key

==============================================================================
*/

/*QUAKED trigger_key (.5 .5 .5) (-8 -8 -8) (8 8 8) MULTI x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A relay trigger that only fires it's targets if player has the proper key.
Use "item" to specify the required key, for example "key_data_cd"

MULTI : allow multiple uses
*/
static USE(trigger_key_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	item_id_t index;

	if (!self->item)
		return;
	if (!activator->client)
		return;

	index = self->item->id;
	if (!activator->client->pers.inventory[index]) {
		if (level.time < self->touch_debounce_time)
			return;
		self->touch_debounce_time = level.time + 5_sec;
		gi.LocCenter_Print(activator, "$g_you_need", self->item->pickupNameDefinitive);
		gi.sound(activator, CHAN_AUTO, gi.soundIndex("misc/keytry.wav"), 1, ATTN_NORM, 0);
		return;
	}

	gi.sound(activator, CHAN_AUTO, gi.soundIndex("misc/keyuse.wav"), 1, ATTN_NORM, 0);
	if (coop->integer) {
		if (self->item->id == IT_KEY_POWER_CUBE || self->item->id == IT_KEY_EXPLOSIVE_CHARGES) {
			int cube;

			for (cube = 0; cube < 8; cube++)
				if (activator->client->pers.powerCubes & (1 << cube))
					break;

			for (auto ce : active_clients()) {
				if (ce->client->pers.powerCubes & (1 << cube)) {
					ce->client->pers.inventory[index]--;
					ce->client->pers.powerCubes &= ~(1 << cube);

					// [Paril-KEX] don't allow respawning players to keep
					// used keys
					if (!P_UseCoopInstancedItems()) {
						ce->client->resp.coopRespawn.inventory[index] = 0;
						ce->client->resp.coopRespawn.powerCubes &= ~(1 << cube);
					}
				}
			}
		}
		else {
			for (auto ce : active_clients()) {
				ce->client->pers.inventory[index] = 0;

				// [Paril-KEX] don't allow respawning players to keep
				// used keys
				if (!P_UseCoopInstancedItems())
					ce->client->resp.coopRespawn.inventory[index] = 0;
			}
		}
	}
	else {
		// don't remove keys in DM
		if (!deathmatch->integer)
			activator->client->pers.inventory[index]--;
	}

	UseTargets(self, activator);

	// allow multi use
	if (deathmatch->integer || !self->spawnFlags.has(1_spawnflag))
		self->use = nullptr;
}

void SP_trigger_key(gentity_t* self) {
	if (!st.item) {
		gi.Com_PrintFmt("{}: no key item\n", *self);
		return;
	}
	self->item = FindItemByClassname(st.item);

	if (!self->item) {
		gi.Com_PrintFmt("{}: item {} not found\n", *self, st.item);
		return;
	}

	if (!self->target) {
		gi.Com_PrintFmt("{}: no target\n", *self);
		return;
	}

	gi.soundIndex("misc/keytry.wav");
	gi.soundIndex("misc/keyuse.wav");

	self->use = trigger_key_use;
}

/*
==============================================================================

trigger_counter

==============================================================================
*/

/*QUAKED trigger_counter (.5 .5 .5) ? NOMESSAGE x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Acts as an intermediary for an action that takes multiple inputs.

If NOMESSAGE is not set, it will print "1 more.. " etc when triggered and "sequence complete" when finished.

After the counter has been triggered "count" times (default 2), it will fire all of it's targets and remove itself.
*/

constexpr SpawnFlags SPAWNFLAG_COUNTER_NOMESSAGE = 1_spawnflag;

static USE(trigger_counter_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (self->count == 0)
		return;

	self->count--;

	if (self->count) {
		if (!(self->spawnFlags & SPAWNFLAG_COUNTER_NOMESSAGE)) {
			gi.LocCenter_Print(activator, "$g_more_to_go", self->count);
			gi.sound(activator, CHAN_AUTO, gi.soundIndex("misc/talk1.wav"), 1, ATTN_NORM, 0);
		}
		return;
	}

	if (!(self->spawnFlags & SPAWNFLAG_COUNTER_NOMESSAGE)) {
		gi.LocCenter_Print(activator, "$g_sequence_completed");
		gi.sound(activator, CHAN_AUTO, gi.soundIndex("misc/talk1.wav"), 1, ATTN_NORM, 0);
	}
	self->activator = activator;
	multi_trigger(self);
}

void SP_trigger_counter(gentity_t* self) {
	self->wait = -1;
	if (!self->count)
		self->count = 2;

	self->use = trigger_counter_use;
}

/*
==============================================================================

trigger_always

==============================================================================
*/

/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This trigger will always fire.  It is activated by the world.
*/
void SP_trigger_always(gentity_t* ent) {
	// we must have some delay to make sure our use targets are present
	if (!ent->delay)
		ent->delay = 0.2f;
	UseTargets(ent, ent);
}

//==========================================================

/*QUAKED trigger_deathcount (1 0 0) (-8 -8 -8) (8 8 8) REPEAT
Fires targets only if minimum death count has been achieved in the level.
Deaths considered are monsters during campaigns and players during deathmatch.

"count"	minimum number of deaths required (default 10)

REPEAT : repeats per every 'count' deaths
*/
void SP_trigger_deathcount(gentity_t* ent) {
	if (!ent->count) {
		gi.Com_PrintFmt("{}: No count key set, setting to 10.\n", *ent);
		ent->count = 10;
	}

	int kills = deathmatch->integer ? level.match.totalDeaths : level.campaign.killedMonsters;

	if (!kills)
		return;

	if (ent->spawnFlags.has(1_spawnflag)) {	// only once
		if (kills == ent->count) {
			UseTargets(ent, ent);
			FreeEntity(ent);
			return;
		}
	}
	else {	// every 'count' deaths
		if (!(kills % ent->count)) {
			UseTargets(ent, ent);
		}
	}
}

//==========================================================

/*QUAKED trigger_no_monsters (1 0 0) (-8 -8 -8) (8 8 8) ONCE
Fires targets only if all monsters have been killed or none are present.
Auto-removed in deathmatch (except horde mode).

ONCE : will be removed after firing once
*/
void SP_trigger_no_monsters(gentity_t* ent) {
	if (deathmatch->integer && Game::IsNot(GameType::Horde)) {
		FreeEntity(ent);
		return;
	}

	if (level.campaign.killedMonsters < level.campaign.totalMonsters)
		return;

	UseTargets(ent, ent);

	if (ent->spawnFlags.has(1_spawnflag))
		FreeEntity(ent);
}

//==========================================================

/*QUAKED trigger_monsters (1 0 0) (-8 -8 -8) (8 8 8) ONCE
Fires targets only if monsters are present in the level.
Auto-removed in deathmatch (except horde mode).

ONCE : will be removed after firing once
*/
void SP_trigger_monsters(gentity_t* ent) {
	if (deathmatch->integer && Game::IsNot(GameType::Horde)) {
		FreeEntity(ent);
		return;
	}

	if (level.campaign.killedMonsters >= level.campaign.totalMonsters)
		return;

	UseTargets(ent, ent);

	if (ent->spawnFlags.has(1_spawnflag))
		FreeEntity(ent);
}

/*
==============================================================================

trigger_push

==============================================================================
*/

/*
=================
AimAtTarget

Calculate origin2 so the target apogee will be hit
=================
*/
static THINK(AimAtTarget) (gentity_t* self) -> void {
	gentity_t* ent;
	Vector3	origin;
	float	height, gravity, time, forward;
	float	dist;
	const bool gravityChanged = self->lastGravityModCount != game.gravity_modCount;

	ent = PickTarget(self->target);
	if (!ent) {
		FreeEntity(self);
		return;
	}

	if (!self->targetEnt)
		self->targetEnt = ent;

	if (!gravityChanged && self->origin2 != vec3_origin) {
		self->nextThink = level.time + 100_ms;
		return;
	}

	origin = self->absMin + self->absMax;
	origin *= 0.5;

	height = ent->s.origin[_Z] - origin[_Z];
	gravity = level.gravity;
	if (gravity <= 0.0f) {
		self->nextThink = level.time + 100_ms;
		return;
	}

	time = sqrt(height / (0.5 * gravity));
	if (!time) {
		FreeEntity(self);
		return;
	}

	// set origin2 to the push velocity
	self->origin2 = ent->s.origin - origin;
	self->origin2[2] = 0;
	dist = self->origin2.normalize();

	forward = dist / time;
	self->origin2 *= forward;
	self->origin2[2] = time * gravity;
	self->lastGravityModCount = game.gravity_modCount;
	self->nextThink = level.time + 100_ms;

	//gi.Com_PrintFmt("{}: origin2={}\n", __FUNCTION__, self->origin2);
}

constexpr SpawnFlags SPAWNFLAG_PUSH_ONCE = 0x01_spawnflag;
constexpr SpawnFlags SPAWNFLAG_PUSH_PLUS = 0x02_spawnflag;
constexpr SpawnFlags SPAWNFLAG_PUSH_SILENT = 0x04_spawnflag;
constexpr SpawnFlags SPAWNFLAG_PUSH_START_OFF = 0x08_spawnflag;
constexpr SpawnFlags SPAWNFLAG_PUSH_CLIP = 0x10_spawnflag;

static cached_soundIndex windsound;

static TOUCH(trigger_push_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (self->spawnFlags.has(SPAWNFLAG_PUSH_CLIP)) {
		trace_t clip = gi.clip(self, other->s.origin, other->mins, other->maxs, other->s.origin, G_GetClipMask(other));

		if (clip.fraction == 1.0f)
			return;
	}

	Vector3	velocity = vec3_origin;

	if (self->target)
		velocity = self->origin2 ? self->origin2 : self->moveDir * (self->speed * 10);

	if (strcmp(other->className, "grenade") == 0) {
		other->velocity = velocity ? velocity : self->moveDir * (self->speed * 10);
	}
	else if (other->health > 0 || (other->client && other->client->eliminated)) {
		other->velocity = velocity ? velocity : self->moveDir * (self->speed * 10);

		if (other->client) {
			// don't take falling damage immediately from this
			other->client->oldVelocity = other->velocity;
			other->client->oldGroundEntity = other->groundEntity;
			if (!(self->spawnFlags & SPAWNFLAG_PUSH_SILENT) && (other->fly_sound_debounce_time < level.time)) {
				other->fly_sound_debounce_time = level.time + 1.5_sec;
				gi.sound(other, CHAN_AUTO, windsound, 1, ATTN_NORM, 0);
			}
		}
	}

	if (self->spawnFlags.has(SPAWNFLAG_PUSH_ONCE))
		FreeEntity(self);
}

static USE(trigger_push_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (self->solid == SOLID_NOT)
		self->solid = SOLID_TRIGGER;
	else
		self->solid = SOLID_NOT;
	gi.linkEntity(self);
}

void trigger_push_active(gentity_t* self);

static void trigger_effect(gentity_t* self) {
	Vector3 origin;
	int	   i;

	origin = (self->absMin + self->absMax) * 0.5f;

	for (i = 0; i < 10; i++) {
		origin[_Z] += (self->speed * 0.01f) * (i + frandom());
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_TUNNEL_SPARKS);
		gi.WriteByte(1);
		gi.WritePosition(origin);
		gi.WriteDir(vec3_origin);
		gi.WriteByte(irandom(0x74, 0x7C));
		gi.multicast(self->s.origin, MULTICAST_PVS, false);
	}
}

static THINK(trigger_push_inactive) (gentity_t* self) -> void {
	if (self->delay > level.time.seconds()) {
		self->nextThink = level.time + 100_ms;
	}
	else {
		self->touch = trigger_push_touch;
		self->think = trigger_push_active;
		self->nextThink = level.time + 100_ms;
		self->delay = (self->nextThink + GameTime::from_sec(self->wait)).seconds();
	}
}

THINK(trigger_push_active) (gentity_t* self) -> void {
	if (self->delay > level.time.seconds()) {
		self->nextThink = level.time + 100_ms;
		trigger_effect(self);
	}
	else {
		self->touch = nullptr;
		self->think = trigger_push_inactive;
		self->nextThink = level.time + 100_ms;
		self->delay = (self->nextThink + GameTime::from_sec(self->wait)).seconds();
	}
}

/*QUAKED trigger_push (.5 .5 .5) ? PUSH_ONCE PUSH_PLUS PUSH_SILENT START_OFF CLIP x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Pushes the player
"speed"	defaults to 1000
"wait"  defaults to 10, must use PUSH_PLUS

If targeted, it will toggle on and off when used.
If it has a target, it will set an apogee to that target and modify velocity and direction accordingly (ala-Q3)

START_OFF - toggled trigger_push begins in off setting
SILENT - doesn't make wind noise
*/
void SP_trigger_push(gentity_t* self) {
	InitTrigger(self);

	self->lastGravityModCount = game.gravity_modCount;

	if (self->target) {
		self->think = AimAtTarget;
		self->nextThink = level.time + 100_ms;
	}

	if (!(self->spawnFlags & SPAWNFLAG_PUSH_SILENT))
		windsound.assign("world/jumppad.wav");
	self->touch = trigger_push_touch;

	if (self->spawnFlags.has(SPAWNFLAG_PUSH_PLUS)) {
		if (!self->wait)
			self->wait = 10;

		self->think = trigger_push_active;
		self->nextThink = level.time + 100_ms;
		self->delay = (self->nextThink + GameTime::from_sec(self->wait)).seconds();
	}

	if (!self->speed)
		self->speed = 1000;

	if (self->targetName) { // toggleable
		self->use = trigger_push_use;
		if (self->spawnFlags.has(SPAWNFLAG_PUSH_START_OFF))
			self->solid = SOLID_NOT;
	}
	else if (self->spawnFlags.has(SPAWNFLAG_PUSH_START_OFF)) {
		gi.Com_PrintFmt("{}: START_OFF but not targeted.\n", *self);
		self->svFlags = SVF_NONE;
		self->touch = nullptr;
		self->solid = SOLID_BSP;
		self->moveType = MoveType::Push;
	}

	if (self->spawnFlags.has(SPAWNFLAG_PUSH_CLIP))
		self->svFlags |= SVF_HULL;

	gi.linkEntity(self);
}


static USE(target_push_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (!activator->client || !ClientIsPlaying(activator->client))
		return;

	activator->velocity = self->origin2;
}

/*QUAKED target_push (.5 .5 .5) (-8 -8 -8) (8 8 8) BOUNCEPAD
Pushes the activator in the direction of angle, or towards a target apex.
"speed"		defaults to 1000

BOUNCEPAD: if set, will play a bouncepad sound instead of a wind sound.
*/
void SP_target_push(gentity_t* self) {
	if (!self->speed)
		self->speed = 1000;

	self->lastGravityModCount = game.gravity_modCount;
	self->origin2 = self->origin2 * self->speed;

	if (self->spawnFlags.has(1_spawnflag)) {
		windsound.assign("world/jumppad.wav");
	}
	else {
		windsound.assign("misc/windfly.wav");
	}

	if (self->target) {
		self->absMin = self->s.origin;
		self->absMax = self->s.origin;
		self->think = AimAtTarget;
		self->nextThink = level.time + FRAME_TIME_MS;
	}
	self->use = target_push_use;
}

/*
==============================================================================

trigger_hurt

==============================================================================
*/

/*QUAKED trigger_hurt (.5 .5 .5) ? START_OFF TOGGLE SILENT NO_PROTECTION SLOW NO_PLAYERS NO_MONSTERS x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Any entity that touches this will be hurt.

It does dmg points of damage each server frame

SILENT			supresses playing the sound
SLOW			changes the damage rate to once per second
NO_PROTECTION	*nothing* stops the damage

"dmg"			default 5 (whole numbers only)

*/

constexpr SpawnFlags SPAWNFLAG_HURT_START_OFF = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_HURT_TOGGLE = 2_spawnflag;
constexpr SpawnFlags SPAWNFLAG_HURT_SILENT = 4_spawnflag;
constexpr SpawnFlags SPAWNFLAG_HURT_NO_PROTECTION = 8_spawnflag;
constexpr SpawnFlags SPAWNFLAG_HURT_SLOW = 16_spawnflag;
constexpr SpawnFlags SPAWNFLAG_HURT_NO_PLAYERS = 32_spawnflag;
constexpr SpawnFlags SPAWNFLAG_HURT_NO_MONSTERS = 64_spawnflag;
constexpr SpawnFlags SPAWNFLAG_HURT_CLIPPED = 128_spawnflag;

static USE(hurt_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (self->solid == SOLID_NOT)
		self->solid = SOLID_TRIGGER;
	else
		self->solid = SOLID_NOT;
	gi.linkEntity(self);

	if (!(self->spawnFlags & SPAWNFLAG_HURT_TOGGLE))
		self->use = nullptr;
}

static TOUCH(hurt_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	DamageFlags damageFlags;

	if (!other->takeDamage)
		return;
	else if (!(other->svFlags & SVF_MONSTER) && !(other->flags & FL_DAMAGEABLE) && (!other->client) && (strcmp(other->className, "misc_explobox") != 0))
		return;
	else if (self->spawnFlags.has(SPAWNFLAG_HURT_NO_MONSTERS) && (other->svFlags & SVF_MONSTER))
		return;
	else if (self->spawnFlags.has(SPAWNFLAG_HURT_NO_PLAYERS) && (other->client))
		return;

	if (self->timeStamp > level.time)
		return;

	if (self->spawnFlags.has(SPAWNFLAG_HURT_CLIPPED)) {
		trace_t clip = gi.clip(self, other->s.origin, other->mins, other->maxs, other->s.origin, G_GetClipMask(other));

		if (clip.fraction == 1.0f)
			return;
	}

	if (self->spawnFlags.has(SPAWNFLAG_HURT_SLOW))
		self->timeStamp = level.time + 1_sec;
	else
		self->timeStamp = level.time + 10_hz;

	if (!(self->spawnFlags & SPAWNFLAG_HURT_SILENT)) {
		if (self->fly_sound_debounce_time < level.time) {
			gi.sound(other, CHAN_AUTO, self->noiseIndex, 1, ATTN_NORM, 0);
			self->fly_sound_debounce_time = level.time + 1_sec;
		}
	}

	if (self->spawnFlags.has(SPAWNFLAG_HURT_NO_PROTECTION))
		damageFlags = DamageFlags::NoProtection;
	else
		damageFlags = DamageFlags::Normal;

	Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, self->dmg, damageFlags, ModID::Hurt);
}

void SP_trigger_hurt(gentity_t* self) {
	InitTrigger(self);

	self->noiseIndex = gi.soundIndex("world/electro.wav");
	self->touch = hurt_touch;

	if (!self->dmg)
		self->dmg = 5;

	if (self->spawnFlags.has(SPAWNFLAG_HURT_START_OFF))
		self->solid = SOLID_NOT;
	else
		self->solid = SOLID_TRIGGER;

	if (self->spawnFlags.has(SPAWNFLAG_HURT_TOGGLE))
		self->use = hurt_use;

	gi.linkEntity(self);

	if (self->spawnFlags.has(SPAWNFLAG_HURT_CLIPPED))
		self->svFlags |= SVF_HULL;
}

/*
==============================================================================

trigger_gravity

==============================================================================
*/

/*QUAKED trigger_gravity (.5 .5 .5) ? TOGGLE START_OFF x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Changes the touching entites gravity to the value of "gravity".
1.0 is standard gravity for the level.

TOGGLE - trigger_gravity can be turned on and off
START_OFF - trigger_gravity starts turned off (implies TOGGLE)
*/

constexpr SpawnFlags SPAWNFLAG_GRAVITY_TOGGLE = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_GRAVITY_START_OFF = 2_spawnflag;
constexpr SpawnFlags SPAWNFLAG_GRAVITY_CLIPPED = 4_spawnflag;

static USE(trigger_gravity_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (self->solid == SOLID_NOT)
		self->solid = SOLID_TRIGGER;
	else
		self->solid = SOLID_NOT;
	gi.linkEntity(self);
}

static TOUCH(trigger_gravity_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {

	if (self->spawnFlags.has(SPAWNFLAG_GRAVITY_CLIPPED)) {
		trace_t clip = gi.clip(self, other->s.origin, other->mins, other->maxs, other->s.origin, G_GetClipMask(other));

		if (clip.fraction == 1.0f)
			return;
	}

	other->gravity = self->gravity;
}

void SP_trigger_gravity(gentity_t* self) {
	if (!st.gravity || !*st.gravity) {
		gi.Com_PrintFmt("{}: no gravity set\n", *self);
		FreeEntity(self);
		return;
	}

	InitTrigger(self);

	self->gravity = (float)atof(st.gravity);

	if (self->spawnFlags.has(SPAWNFLAG_GRAVITY_TOGGLE))
		self->use = trigger_gravity_use;

	if (self->spawnFlags.has(SPAWNFLAG_GRAVITY_START_OFF)) {
		self->use = trigger_gravity_use;
		self->solid = SOLID_NOT;
	}

	self->touch = trigger_gravity_touch;

	gi.linkEntity(self);

	if (self->spawnFlags.has(SPAWNFLAG_GRAVITY_CLIPPED))
		self->svFlags |= SVF_HULL;
}

/*
==============================================================================

trigger_monsterjump

==============================================================================
*/

/*QUAKED trigger_monsterjump (.5 .5 .5) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Walking monsters that touch this will jump in the direction of the trigger's angle
"speed" default to 200, the speed thrown forward
"height" default to 200, the speed thrown upwards

TOGGLE - trigger_monsterjump can be turned on and off
START_OFF - trigger_monsterjump starts turned off (implies TOGGLE)
*/

constexpr SpawnFlags SPAWNFLAG_MONSTERJUMP_TOGGLE = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_MONSTERJUMP_START_OFF = 2_spawnflag;
constexpr SpawnFlags SPAWNFLAG_MONSTERJUMP_CLIPPED = 4_spawnflag;

static USE(trigger_monsterjump_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (self->solid == SOLID_NOT)
		self->solid = SOLID_TRIGGER;
	else
		self->solid = SOLID_NOT;
	gi.linkEntity(self);
}

static TOUCH(trigger_monsterjump_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (other->flags & (FL_FLY | FL_SWIM))
		return;
	if (other->svFlags & SVF_DEADMONSTER)
		return;
	if (!(other->svFlags & SVF_MONSTER))
		return;

	if (self->spawnFlags.has(SPAWNFLAG_MONSTERJUMP_CLIPPED)) {
		trace_t clip = gi.clip(self, other->s.origin, other->mins, other->maxs, other->s.origin, G_GetClipMask(other));

		if (clip.fraction == 1.0f)
			return;
	}

	// set XY even if not on ground, so the jump will clear lips
	other->velocity[_X] = self->moveDir[0] * self->speed;
	other->velocity[_Y] = self->moveDir[1] * self->speed;

	if (!other->groundEntity)
		return;

	other->groundEntity = nullptr;
	other->velocity[_Z] = self->moveDir[2];
}

void SP_trigger_monsterjump(gentity_t* self) {
	if (!self->speed)
		self->speed = 200;
	if (!st.height)
		st.height = 200;
	if (self->s.angles[YAW] == 0)
		self->s.angles[YAW] = 360;
	InitTrigger(self);
	self->touch = trigger_monsterjump_touch;
	self->moveDir[2] = (float)st.height;

	if (self->spawnFlags.has(SPAWNFLAG_MONSTERJUMP_TOGGLE))
		self->use = trigger_monsterjump_use;

	if (self->spawnFlags.has(SPAWNFLAG_MONSTERJUMP_START_OFF)) {
		self->use = trigger_monsterjump_use;
		self->solid = SOLID_NOT;
	}

	gi.linkEntity(self);

	if (self->spawnFlags.has(SPAWNFLAG_MONSTERJUMP_CLIPPED))
		self->svFlags |= SVF_HULL;
}

/*
==============================================================================

trigger_flashlight

==============================================================================
*/

/*QUAKED trigger_flashlight (.5 .5 .5) ? CLIPPED x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Players moving against this trigger will have their flashlight turned on or off.
"style" default to 0, set to 1 to always turn flashlight on, 2 to always turn off,
		otherwise "angles" are used to control on/off state
*/

constexpr SpawnFlags SPAWNFLAG_FLASHLIGHT_CLIPPED = 1_spawnflag;

static TOUCH(trigger_flashlight_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (!other->client)
		return;

	if (self->spawnFlags.has(SPAWNFLAG_FLASHLIGHT_CLIPPED)) {
		trace_t clip = gi.clip(self, other->s.origin, other->mins, other->maxs, other->s.origin, G_GetClipMask(other));

		if (clip.fraction == 1.0f)
			return;
	}

	if (self->style == 1) {
		P_ToggleFlashlight(other, true);
	}
	else if (self->style == 2) {
		P_ToggleFlashlight(other, false);
	}
	else if (other->velocity.lengthSquared() > 32.f) {
		Vector3 forward = other->velocity.normalized();
		P_ToggleFlashlight(other, forward.dot(self->moveDir) > 0);
	}
}

void SP_trigger_flashlight(gentity_t* self) {
	if (self->s.angles[YAW] == 0)
		self->s.angles[YAW] = 360;
	InitTrigger(self);
	self->touch = trigger_flashlight_touch;
	self->moveDir[2] = (float)st.height;

	if (self->spawnFlags.has(SPAWNFLAG_FLASHLIGHT_CLIPPED))
		self->svFlags |= SVF_HULL;
	gi.linkEntity(self);
}


/*
==============================================================================

trigger_fog

==============================================================================
*/

/*QUAKED trigger_fog (.5 .5 .5) ? AFFECT_FOG AFFECT_HEIGHTFOG INSTANTANEOUS FORCE BLEND x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Players moving against this trigger will have their fog settings changed.
Fog/heightfog will be adjusted if the spawnFlags are set. Instantaneous
ignores any delays. Force causes it to ignore movement dir and always use
the "on" values. Blend causes it to change towards how far you are into the trigger
with respect to angles.
"target" can target an info_notnull to pull the keys below from.
"delay" default to 0.5; time in seconds a change in fog will occur over
"wait" default to 0.0; time in seconds before a re-trigger can be executed

"fog_density"; density value of fog, 0-1
"fog_color"; color value of fog, 3d vector with values between 0-1 (r g b)
"fog_density_off"; transition density value of fog, 0-1
"fog_color_off"; transition color value of fog, 3d vector with values between 0-1 (r g b)
"fog_sky_factor"; sky factor value of fog, 0-1
"fog_sky_factor_off"; transition sky factor value of fog, 0-1

"heightfog_falloff"; falloff value of heightfog, 0-1
"heightfog_density"; density value of heightfog, 0-1
"heightfog_start_color"; the start color for the fog (r g b, 0-1)
"heightfog_start_dist"; the start distance for the fog (units)
"heightfog_end_color"; the start color for the fog (r g b, 0-1)
"heightfog_end_dist"; the end distance for the fog (units)

"heightfog_falloff_off"; transition falloff value of heightfog, 0-1
"heightfog_density_off"; transition density value of heightfog, 0-1
"heightfog_start_color_off"; transition the start color for the fog (r g b, 0-1)
"heightfog_start_dist_off"; transition the start distance for the fog (units)
"heightfog_end_color_off"; transition the start color for the fog (r g b, 0-1)
"heightfog_end_dist_off"; transition the end distance for the fog (units)
*/

constexpr SpawnFlags SPAWNFLAG_FOG_AFFECT_FOG = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_FOG_AFFECT_HEIGHTFOG = 2_spawnflag;
constexpr SpawnFlags SPAWNFLAG_FOG_INSTANTANEOUS = 4_spawnflag;
constexpr SpawnFlags SPAWNFLAG_FOG_FORCE = 8_spawnflag;
constexpr SpawnFlags SPAWNFLAG_FOG_BLEND = 16_spawnflag;

static TOUCH(trigger_fog_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (!other->client)
		return;

	if (self->timeStamp > level.time)
		return;

	self->timeStamp = level.time + GameTime::from_sec(self->wait);

	gentity_t* fog_value_storage = self;

	if (self->moveTarget)
		fog_value_storage = self->moveTarget;

	if (self->spawnFlags.has(SPAWNFLAG_FOG_INSTANTANEOUS))
		other->client->pers.fog_transition_time = 0_ms;
	else
		other->client->pers.fog_transition_time = GameTime::from_sec(fog_value_storage->delay);

	if (self->spawnFlags.has(SPAWNFLAG_FOG_BLEND)) {
		Vector3 center = (self->absMin + self->absMax) * 0.5f;
		Vector3 half_size = (self->size * 0.5f) + (other->size * 0.5f);
		Vector3 start = (-self->moveDir).scaled(half_size);
		Vector3 end = (self->moveDir).scaled(half_size);
		Vector3 player_dist = (other->s.origin - center).scaled(Vector3{ fabs(self->moveDir[0]),fabs(self->moveDir[1]),fabs(self->moveDir[2]) });

		float dist = (player_dist - start).length();
		dist /= (start - end).length();
		dist = std::clamp(dist, 0.f, 1.f);

		if (self->spawnFlags.has(SPAWNFLAG_FOG_AFFECT_FOG)) {
			other->client->pers.wanted_fog = {
				lerp(fog_value_storage->fog.density_off, fog_value_storage->fog.density, dist),
				lerp(fog_value_storage->fog.color_off[0], fog_value_storage->fog.color[0], dist),
				lerp(fog_value_storage->fog.color_off[1], fog_value_storage->fog.color[1], dist),
				lerp(fog_value_storage->fog.color_off[2], fog_value_storage->fog.color[2], dist),
				lerp(fog_value_storage->fog.sky_factor_off, fog_value_storage->fog.sky_factor, dist)
			};
		}

		if (self->spawnFlags.has(SPAWNFLAG_FOG_AFFECT_HEIGHTFOG)) {
			other->client->pers.wanted_heightfog = {
				{
					lerp(fog_value_storage->heightfog.start_color_off[0], fog_value_storage->heightfog.start_color[0], dist),
					lerp(fog_value_storage->heightfog.start_color_off[1], fog_value_storage->heightfog.start_color[1], dist),
					lerp(fog_value_storage->heightfog.start_color_off[2], fog_value_storage->heightfog.start_color[2], dist),
					lerp(fog_value_storage->heightfog.start_dist_off, fog_value_storage->heightfog.start_dist, dist)
				},
			{
				lerp(fog_value_storage->heightfog.end_color_off[0], fog_value_storage->heightfog.end_color[0], dist),
				lerp(fog_value_storage->heightfog.end_color_off[1], fog_value_storage->heightfog.end_color[1], dist),
				lerp(fog_value_storage->heightfog.end_color_off[2], fog_value_storage->heightfog.end_color[2], dist),
				lerp(fog_value_storage->heightfog.end_dist_off, fog_value_storage->heightfog.end_dist, dist)
			},
				lerp(fog_value_storage->heightfog.falloff_off, fog_value_storage->heightfog.falloff, dist),
				lerp(fog_value_storage->heightfog.density_off, fog_value_storage->heightfog.density, dist)
			};
		}

		return;
	}

	bool use_on = true;

	if (!self->spawnFlags.has(SPAWNFLAG_FOG_FORCE)) {
		float len;
		Vector3 forward = other->velocity.normalized(len);

		// not moving enough to trip; this is so we don't trip
		// the wrong direction when on an elevator, etc.
		if (len <= 0.0001f)
			return;

		use_on = forward.dot(self->moveDir) > 0;
	}

	if (self->spawnFlags.has(SPAWNFLAG_FOG_AFFECT_FOG)) {
		if (use_on) {
			other->client->pers.wanted_fog = {
				fog_value_storage->fog.density,
				fog_value_storage->fog.color[0],
				fog_value_storage->fog.color[1],
				fog_value_storage->fog.color[2],
				fog_value_storage->fog.sky_factor
			};
		}
		else {
			other->client->pers.wanted_fog = {
				fog_value_storage->fog.density_off,
				fog_value_storage->fog.color_off[0],
				fog_value_storage->fog.color_off[1],
				fog_value_storage->fog.color_off[2],
				fog_value_storage->fog.sky_factor_off
			};
		}
	}

	if (self->spawnFlags.has(SPAWNFLAG_FOG_AFFECT_HEIGHTFOG)) {
		if (use_on) {
			other->client->pers.wanted_heightfog = {
				{
					fog_value_storage->heightfog.start_color[0],
					fog_value_storage->heightfog.start_color[1],
					fog_value_storage->heightfog.start_color[2],
					fog_value_storage->heightfog.start_dist
				},
				{
					fog_value_storage->heightfog.end_color[0],
					fog_value_storage->heightfog.end_color[1],
					fog_value_storage->heightfog.end_color[2],
					fog_value_storage->heightfog.end_dist
				},
				fog_value_storage->heightfog.falloff,
				fog_value_storage->heightfog.density
			};
		}
		else {
			other->client->pers.wanted_heightfog = {
				{
					fog_value_storage->heightfog.start_color_off[0],
					fog_value_storage->heightfog.start_color_off[1],
					fog_value_storage->heightfog.start_color_off[2],
					fog_value_storage->heightfog.start_dist_off
				},
				{
					fog_value_storage->heightfog.end_color_off[0],
					fog_value_storage->heightfog.end_color_off[1],
					fog_value_storage->heightfog.end_color_off[2],
					fog_value_storage->heightfog.end_dist_off
				},
				fog_value_storage->heightfog.falloff_off,
				fog_value_storage->heightfog.density_off
			};
		}
	}
}

void SP_trigger_fog(gentity_t* self) {
	if (self->s.angles[YAW] == 0)
		self->s.angles[YAW] = 360;

	InitTrigger(self);

	if (!(self->spawnFlags & (SPAWNFLAG_FOG_AFFECT_FOG | SPAWNFLAG_FOG_AFFECT_HEIGHTFOG)))
		gi.Com_PrintFmt("WARNING: {} with no fog spawnFlags set\n", *self);

	if (self->target) {
		self->moveTarget = PickTarget(self->target);

		if (self->moveTarget) {
			if (!self->moveTarget->delay)
				self->moveTarget->delay = 0.5f;
		}
	}

	if (!self->delay)
		self->delay = 0.5f;

	self->touch = trigger_fog_touch;
}

/*QUAKED trigger_coop_relay (.5 .5 .5) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The same as a trigger_relay.
*/

constexpr SpawnFlags SPAWNFLAG_COOP_RELAY_AUTO_FIRE = 1_spawnflag;

static inline bool trigger_coop_relay_filter(gentity_t* player) {
	return (player->health <= 0 || player->deadFlag || player->moveType == MoveType::NoClip || player->moveType == MoveType::FreeCam ||
		!ClientIsPlaying(player->client) || player->s.modelIndex != MODELINDEX_PLAYER);
}

static bool trigger_coop_relay_can_use(gentity_t* self, gentity_t* activator) {
	//wor: this is a hinderance, remove this
	return true;
}

static USE(trigger_coop_relay_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (!trigger_coop_relay_can_use(self, activator)) {
		if (self->timeStamp < level.time)
			gi.LocCenter_Print(activator, self->message);

		self->timeStamp = level.time + 5_sec;
		return;
	}

	const char* msg = self->message;
	self->message = nullptr;
	UseTargets(self, activator);
	self->message = msg;
}

static BoxEntitiesResult_t trigger_coop_relay_player_filter(gentity_t* ent, void* data) {
	if (!ent->client)
		return BoxEntitiesResult_t::Skip;
	else if (trigger_coop_relay_filter(ent))
		return BoxEntitiesResult_t::Skip;

	return BoxEntitiesResult_t::Keep;
}

static THINK(trigger_coop_relay_think) (gentity_t* self) -> void {
	std::array<gentity_t*, MAX_SPLIT_PLAYERS> players{};
	size_t num_active = 0;

	for (auto player : active_clients())
		if (!trigger_coop_relay_filter(player))
			num_active++;

	size_t n = gi.BoxEntities(self->absMin, self->absMax, players.data(), num_active, AREA_SOLID, trigger_coop_relay_player_filter, nullptr);

	if (n == num_active) {
		const char* msg = self->message;
		self->message = nullptr;
		UseTargets(self, &globals.gentities[1]);
		self->message = msg;

		FreeEntity(self);
		return;
	}
	else if (n && self->timeStamp < level.time) {
		for (size_t i = 0; i < n; i++)
			gi.LocCenter_Print(players[i], self->message);

		for (auto player : active_clients())
			if (std::find(players.begin(), players.end(), player) == players.end())
				gi.LocCenter_Print(player, self->map.data());

		self->timeStamp = level.time + 5_sec;
	}

	self->nextThink = level.time + GameTime::from_sec(self->wait);
}

/*
=============
SP_trigger_coop_relay

Initializes the cooperative relay trigger entity, configuring defaults for
messaging, timing, and activation behavior.
=============
*/
void SP_trigger_coop_relay(gentity_t* self) {
	if (self->targetName && self->spawnFlags.has(SPAWNFLAG_COOP_RELAY_AUTO_FIRE))
		gi.Com_PrintFmt("{}: targetName and auto-fire are mutually exclusive\n", *self);

	InitTrigger(self);

	if (!self->message)
		self->message = "$g_coop_wait_for_players";

	if (CharArrayIsBlank(self->map))
		Q_strlcpy(self->map.data(), "$g_coop_players_waiting_for_you", self->map.size());

	if (!self->wait)
		self->wait = 1;

	if (self->spawnFlags.has(SPAWNFLAG_COOP_RELAY_AUTO_FIRE)) {
		self->think = trigger_coop_relay_think;
		self->nextThink = level.time + GameTime::from_sec(self->wait);
	}
	else
		self->use = trigger_coop_relay_use;
	self->svFlags |= SVF_NOCLIENT;
	gi.linkEntity(self);
}


/*QUAKED info_teleport_destination (.5 .5 .5) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Destination marker for a teleporter.
*/
void SP_info_teleport_destination(gentity_t* self) {}

constexpr SpawnFlags SPAWNFLAG_TELEPORT_SPECTATORS_ONLY = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_TELEPORT_NO_FX = 2_spawnflag;
constexpr SpawnFlags SPAWNFLAG_TELEPORT_CTF_ONLY = 4_spawnflag;
constexpr SpawnFlags SPAWNFLAG_TELEPORT_START_ON = 8_spawnflag;
constexpr SpawnFlags SPAWNFLAG_TELEPORT_PLAYERS_ONLY = 16_spawnflag;

/*QUAKED trigger_teleport (.5 .5 .5) ? SPECTATORS_ONLY NO_FX CTF_ONLY START_ON PLAYERS_ONLY x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Any object touching this will be transported to the corresponding
info_teleport_destination entity. You must set the "target" field,
and create an object with a "targetName" field that matches.

If the trigger_teleport has a targetName, it will only teleport
entities when it has been fired.

SPECTATORS_ONLY: only spectators are teleported (no players, etc.)
PLAYERS_ONLY: only players are teleported (no spectators, etc.)
NO_FX: no teleport effect is played, and the player does not get a teleport event
CTF_ONLY: only in CTF mode
START_ON: when trigger has targetName, start active, deactivate when used.
*/
static TOUCH(trigger_teleport_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	gentity_t* dest;

	if (!other->client)
		return;

	if (self->delay)
		return;

	if (self->spawnFlags.has(SPAWNFLAG_TELEPORT_CTF_ONLY) && Game::IsNot(GameType::CaptureTheFlag))
		return;

	if (self->spawnFlags.has(SPAWNFLAG_TELEPORT_PLAYERS_ONLY) && !ClientIsPlaying(other->client))
		return;

	if (self->spawnFlags.has(SPAWNFLAG_TELEPORT_SPECTATORS_ONLY) && ClientIsPlaying(other->client))
		return;

	dest = PickTarget(self->target);
	if (!dest) {
		gi.Com_Print("Teleport Destination not found!\n");
		return;
	}

	if (other->moveType != MoveType::FreeCam) {
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_TELEPORT_EFFECT);
		gi.WritePosition(other->s.origin);
		gi.multicast(other->s.origin, MULTICAST_PVS, false);
	}

	G_ExplodeNearbyMinesSafe(dest->s.origin, 202.0f, other);

	other->s.origin = dest->s.origin;
	other->s.oldOrigin = dest->s.origin;
	other->s.origin[_Z] += 10;

	if (other->client) {
		TeleporterVelocity(other, dest->s.angles);

		// draw the teleport splash at source and on the player
		if (ClientIsPlaying(other->client) && !self->spawnFlags.has(SPAWNFLAG_TELEPORT_NO_FX)) {
			other->s.event = EV_PLAYER_TELEPORT;
			self->s.event = EV_PLAYER_TELEPORT;
		}

		// set angles
		other->client->ps.pmove.deltaAngles = dest->s.angles - other->client->resp.cmdAngles;

		other->client->ps.viewAngles = {};
		other->client->vAngle = {};
	}

	other->s.angles = {};

	gi.linkEntity(other);

	// kill anything at the destination
	KillBox(other, !!other->client);

	// [Paril-KEX] move sphere, if we own it
	if (other->client && other->client->ownedSphere) {
		gentity_t* sphere = other->client->ownedSphere;
		sphere->s.origin = other->s.origin;
		sphere->s.origin[_Z] = other->absMax[2];
		sphere->s.angles[YAW] = other->s.angles[YAW];
		gi.linkEntity(sphere);
	}
}

static USE(trigger_teleport_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->delay = self->delay ? 0 : 1;
}

void SP_trigger_teleport(gentity_t* self) {

	InitTrigger(self);

	if (!self->wait)
		self->wait = 0.2f;

	self->delay = 0;

	if (self->targetName) {
		self->use = trigger_teleport_use;
		if (!self->spawnFlags.has(SPAWNFLAG_TELEPORT_START_ON))
			self->delay = 1;
	}

	self->touch = trigger_teleport_touch;

	self->solid = SOLID_TRIGGER;
	self->moveType = MoveType::None;

	if (self->s.angles)
		SetMoveDir(self->s.angles, self->moveDir);

	gi.setModel(self, self->model);
	gi.linkEntity(self);
}

/*QUAKED trigger_ctf_teleport (0.5 0.5 0.5) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Players touching this will be teleported
*/

//just here to help old map conversions
static TOUCH(old_teleporter_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	gentity_t* dest;
	Vector3	 forward;

	if (!other->client)
		return;
	dest = PickTarget(self->target);
	if (!dest) {
		gi.Com_Print("Couldn't find destination\n");
		return;
	}

	Weapon_Grapple_DoReset(other->client);

	// unlink to make sure it can't possibly interfere with KillBox
	gi.unlinkEntity(other);

	other->s.origin = dest->s.origin;
	other->s.oldOrigin = dest->s.origin;
	other->s.origin[_Z] += 10;

	TeleporterVelocity(other, dest->s.angles);

	// draw the teleport splash at source and on the player
	if (ClientIsPlaying(other->client)) {
		self->enemy->s.event = EV_PLAYER_TELEPORT;
		other->s.event = EV_PLAYER_TELEPORT;
	}

	// set angles
	other->client->ps.pmove.deltaAngles = dest->s.angles - other->client->resp.cmdAngles;

	other->s.angles[PITCH] = 0;
	other->s.angles[YAW] = dest->s.angles[YAW];
	other->s.angles[ROLL] = 0;
	other->client->ps.viewAngles = dest->s.angles;
	other->client->vAngle = dest->s.angles;

	// give a little forward velocity
	AngleVectors(other->client->vAngle, forward, nullptr, nullptr);
	other->velocity = forward * 200;

	gi.linkEntity(other);

	// kill anything at the destination
	if (!KillBox(other, true)) {
	}

	// [Paril-KEX] move sphere, if we own it
	if (other->client->ownedSphere) {
		gentity_t* sphere = other->client->ownedSphere;
		sphere->s.origin = other->s.origin;
		sphere->s.origin[_Z] = other->absMax[2];
		sphere->s.angles[YAW] = other->s.angles[YAW];
		gi.linkEntity(sphere);
	}
}

void SP_trigger_ctf_teleport(gentity_t* ent) {
	gentity_t* s;
	int		 i;

	if (!ent->target) {
		gi.Com_PrintFmt("{} without a target.\n", *ent);
		FreeEntity(ent);
		return;
	}

	ent->svFlags |= SVF_NOCLIENT;
	ent->solid = SOLID_TRIGGER;
	ent->touch = old_teleporter_touch;
	gi.setModel(ent, ent->model);
	gi.linkEntity(ent);

	// noise maker and splash effect dude
	s = Spawn();
	ent->enemy = s;
	for (i = 0; i < 3; i++)
		s->s.origin[i] = ent->mins[i] + (ent->maxs[i] - ent->mins[i]) / 2;
	s->s.sound = gi.soundIndex("world/hum1.wav");
	gi.linkEntity(s);
}


/*QUAKED trigger_disguise (.5 .5 .5) ? TOGGLE START_ON REMOVE x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Anything passing through this trigger when it is active will
be marked as disguised.

TOGGLE - field is turned off and on when used. (Paril N.B.: always the case)
START_ON - field is active when spawned.
REMOVE - field removes the disguise
*/

// unused
// constexpr uint32_t SPAWNFLAG_DISGUISE_TOGGLE	= 1;
constexpr SpawnFlags SPAWNFLAG_DISGUISE_START_ON = 2_spawnflag;
constexpr SpawnFlags SPAWNFLAG_DISGUISE_REMOVE = 4_spawnflag;

static TOUCH(trigger_disguise_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (other->client) {
		if (self->spawnFlags.has(SPAWNFLAG_DISGUISE_REMOVE))
			other->flags &= ~FL_DISGUISED;
		else
			other->flags |= FL_DISGUISED;
	}
}

static USE(trigger_disguise_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (self->solid == SOLID_NOT)
		self->solid = SOLID_TRIGGER;
	else
		self->solid = SOLID_NOT;

	gi.linkEntity(self);
}

void SP_trigger_disguise(gentity_t* self) {
	if (!level.campaign.disguiseIcon)
		level.campaign.disguiseIcon = gi.imageIndex("i_disguise");

	if (self->spawnFlags.has(SPAWNFLAG_DISGUISE_START_ON))
		self->solid = SOLID_TRIGGER;
	else
		self->solid = SOLID_NOT;

	self->touch = trigger_disguise_touch;
	self->use = trigger_disguise_use;
	self->moveType = MoveType::None;
	self->svFlags = SVF_NOCLIENT;

	gi.setModel(self, self->model);
	gi.linkEntity(self);
}

/*QUAKED trigger_safe_fall (.5 .5 .5) ?
Players that touch this trigger are granted one (1)
free safe fall damage exemption.

They must already be in the air to get this ability.
*/

static TOUCH(trigger_safe_fall_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool other_touching_self) -> void {
	if (other->client && !other->groundEntity)
		other->client->landmark_free_fall = true;
}

void SP_trigger_safe_fall(gentity_t* self) {
	InitTrigger(self);
	self->touch = trigger_safe_fall_touch;
	self->svFlags |= SVF_NOCLIENT;
	self->solid = SOLID_TRIGGER;
	gi.linkEntity(self);
}


/*QUAKED trigger_misc_camera (.5 .5 .5) ? MONSTER NOT_PLAYER TRIGGERED
Variable sized repeatable trigger for activating a misc_camera.
Must be targeted at ONLY ONE misc_camera.
"wait" - this is how long the targetted camera will stay on (unless its
		 path_corners make it turn off earlier).  If wait is -1, the camera
		 will stay on indefinitely.  Default wait is to use misc_camera's wait.
"delay" - this is how long the trigger will wait before reactivating itself.  Default
		 is 1.0.  NOTE: This allows the trigger to trigger a camera that's still on.
"target" - this is the camera to target
"pathtarget" - this is the targetname of the entity the camera should track.
			   The default is the entity that activated the trigger.
"message" - guess

HINT: If you fill a room with a trigger_misc_camera, then set the delay to .1 and the
wait to .2, then as long as the player is in the room, the camera will stay on.  Then
as soon as the player leaves the room, the camera will turn off.

sounds
1)	secret
2)	beep beep
3)	large switch
4)
*/


static void camera_trigger_fire(gentity_t* self) {
	// Check if trigger is on cooldown
	if (self->nextThink)
		return;

	gentity_t* cam = PickTarget(self->target);
	if (!cam || Q_strcasecmp(cam->className, "misc_camera")) {
		gi.Com_PrintFmt("{}: target {} is not a misc_camera.\n", *self, self->target);
		return;
	}

	// Print message to the activator
	if (self->message)
		gi.Center_Print(self->activator, self->message);

	// Play sound
	if (self->noiseIndex)
		gi.sound(self->activator, CHAN_AUTO, self->noiseIndex, 1, ATTN_NORM, 0);

	// Activate the camera, passing self as 'other' and the player as 'activator'.
	// This allows the camera to read this trigger's override properties.
	cam->use(cam, self, self->activator);

	// Set refire delay
	if (self->delay > 0) {
		self->think = multi_wait; // Reuse this simple timed function to clear nextThink
		self->nextThink = level.time + GameTime::from_sec(self->delay);
	}
}


static USE(Use_CameraTrigger) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->activator = activator;
	camera_trigger_fire(self);
}


static TOUCH(Touch_CameraTrigger) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	// Standard validation to check if 'other' can activate this trigger
	if (other->client) {
		if (self->spawnFlags.has(SPAWNFLAG_TRIGGER_NOT_PLAYER))
			return;
	}
	else if (other->svFlags & SVF_MONSTER) {
		if (!self->spawnFlags.has(SPAWNFLAG_TRIGGER_MONSTER))
			return;
	}
	else {
		return;
	}

	// Set activator and fire
	self->activator = other;
	camera_trigger_fire(self);
}



void SP_trigger_misc_camera(gentity_t* ent) {
	if (!ent->target) {
		gi.Com_PrintFmt("{}: trigger_misc_camera without a target.\n", *ent);
		FreeEntity(ent);
		return;
	}

	InitTrigger(ent);

	// Set sounds from 'sounds' key
	if (ent->sounds == 1)
		ent->noiseIndex = gi.soundIndex("misc/secret.wav");
	else if (ent->sounds == 2)
		ent->noiseIndex = gi.soundIndex("misc/talk.wav");
	else if (ent->sounds == 3)
		ent->noiseIndex = gi.soundIndex("misc/trigger1.wav");

	// 'delay' is the refire delay for this trigger. Default to 1.0 second.
	if (!ent->delay)
		ent->delay = 1.0f;

	ent->touch = Touch_CameraTrigger;
	ent->use = Use_CameraTrigger;

	// If TRIGGERED spawnflag is set, it must be used before it can be touched.
	if (ent->spawnFlags.has(SPAWNFLAG_TRIGGER_TRIGGERED)) {
		ent->solid = SOLID_NOT;
		ent->use = trigger_enable; // Use a generic enabler
	}

	gi.linkEntity(ent);
}
