/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_items.cpp (Game Items) This file defines all the items available in the game, including
weapons, ammo, armor, powerups, keys, and special tech items. It is the central repository for
item properties and behavior. Key Responsibilities: - Item Definition: The `itemList` array
serves as the master database for all items, defining their classnames, models, sounds, and
flags. - Pickup Logic: Contains the `Pickup_*` functions that are called when a player touches
an item, determining if the player can take it and what happens when they do (e.g., adding to
inventory, auto-using). - Use Logic: Implements the `Use_*` functions for activatable items like
powerups or the grappling hook. - Drop Logic: Handles the `Drop_*` functions for when a player
manually drops an item or dies. - Initialization: `InitItems` and `SetItemNames` are called at
startup to precache assets and set up server configuration strings for all items.*/

#include "../g_local.hpp"
#include "g_proball.hpp"
#include "../bots/bot_includes.hpp"
#include "../monsters/m_player.hpp"	//doppelganger

#include <array>
#include <limits>

bool Pickup_Weapon(gentity_t* ent, gentity_t* other);
void Use_Weapon(gentity_t* ent, Item* inv);

void Weapon_Blaster(gentity_t* ent);
void Weapon_Shotgun(gentity_t* ent);
void Weapon_SuperShotgun(gentity_t* ent);
void Weapon_Machinegun(gentity_t* ent);
void Weapon_Chaingun(gentity_t* ent);
void Weapon_HyperBlaster(gentity_t* ent);
void Weapon_RocketLauncher(gentity_t* ent);
void Weapon_HandGrenade(gentity_t* ent);
void Weapon_GrenadeLauncher(gentity_t* ent);
void Weapon_Railgun(gentity_t* ent);
void Weapon_BFG(gentity_t* ent);
void Weapon_IonRipper(gentity_t* ent);
void Weapon_PlasmaGun(gentity_t* ent);
void Weapon_Phalanx(gentity_t* ent);
void Weapon_Trap(gentity_t* ent);
void Weapon_ChainFist(gentity_t* ent);
void Weapon_Disruptor(gentity_t* ent);
void Weapon_ETF_Rifle(gentity_t* ent);
void Weapon_PlasmaBeam(gentity_t* ent);
void Weapon_Thunderbolt(gentity_t* ent);
void Weapon_Tesla(gentity_t* ent);
void Weapon_ProxLauncher(gentity_t* ent);

void Use_Quad(gentity_t* ent, Item* item);
static GameTime quad_drop_timeout_hack;
void Use_Haste(gentity_t* ent, Item* item);
static GameTime haste_drop_timeout_hack;
void Use_Double(gentity_t* ent, Item* item);
static GameTime double_drop_timeout_hack;
void Use_Invisibility(gentity_t* ent, Item* item);
static GameTime invisibility_drop_timeout_hack;
void Use_BattleSuit(gentity_t* ent, Item* item);
static GameTime protection_drop_timeout_hack;
void Use_Regeneration(gentity_t* ent, Item* item);
static GameTime regeneration_drop_timeout_hack;
void Use_EmpathyShield(gentity_t* ent, Item* item);
static GameTime empathy_shield_drop_timeout_hack;
void Use_AntiGravBelt(gentity_t* ent, Item* item);
static GameTime antigrav_belt_drop_timeout_hack;

static void UsedMessage(gentity_t* ent, Item* item) {
	if (!ent || !item)
		return;

	if (item->id == IT_ADRENALINE && !match_holdableAdrenaline->integer)
		return;

	gi.LocClient_Print(ent, PRINT_CENTER, "Used {}", item->pickupName);
}

void SelectNextItem(gentity_t* ent, item_flags_t itflags) {
	gclient_t* cl = ent->client;
	if (cl->menu.current) {
		NextMenuItem(ent);
		return;
	}
	if (level.intermission.time) return;
	if (cl->follow.target) {
		FollowNext(ent);
		return;
	}

	item_id_t current_item = cl->pers.selectedItem;
	for (int i = 1; i <= IT_TOTAL; ++i) {
		item_id_t index = static_cast<item_id_t>((static_cast<int>(current_item) + i) % IT_TOTAL);
		if (cl->pers.inventory[index]) {
			Item* it = &itemList[index];
			if (it->use && (it->flags & itflags)) {
				cl->pers.selectedItem = index;
				cl->pers.selected_item_time = level.time + SELECTED_ITEM_TIME;
				return;
			}
		}
	}
}

void SelectPrevItem(gentity_t* ent, item_flags_t itflags) {
	gclient_t* cl = ent->client;
	if (cl->menu.current) {
		PreviousMenuItem(ent);
		return;
	}
	if (level.intermission.time) return;
	if (cl->follow.target) {
		FollowPrev(ent);
		return;
	}

	item_id_t current_item = cl->pers.selectedItem;
	for (int i = 1; i <= IT_TOTAL; ++i) {
		item_id_t index = static_cast<item_id_t>((static_cast<int>(current_item) + IT_TOTAL - i) % IT_TOTAL);
		if (cl->pers.inventory[index]) {
			Item* it = &itemList[index];
			if (it->use && (it->flags & itflags)) {
				cl->pers.selectedItem = index;
				cl->pers.selected_item_time = level.time + SELECTED_ITEM_TIME;
				return;
			}
		}
	}
}

void ValidateSelectedItem(gentity_t* ent) {
	gclient_t* cl = ent->client;
	if (cl->pers.inventory[cl->pers.selectedItem])
		return; // valid
	SelectNextItem(ent, IF_ANY);
}

//======================================================================

/*
===============
G_CanDropItem
===============
*/
static inline bool G_CanDropItem(const Item& item) {
	if (!item.drop)
		return false;
	else if ((item.flags & IF_WEAPON) && !(item.flags & IF_AMMO) && deathmatch->integer && match_weaponsStay->integer)
		return false;

	if (item.id == IT_FLAG_RED || item.id == IT_FLAG_BLUE) {
		if (!(match_dropCmdFlags->integer & 1))
			return false;
	}
	else if (item.flags & IF_POWERUP) {
		if (!(match_dropCmdFlags->integer & 2))
			return false;
	}
	else if (item.flags & IF_WEAPON || item.flags & IF_AMMO) {
		if (!(match_dropCmdFlags->integer & 4))
			return false;
		else if (!ItemSpawnsEnabled()) {
			return false;
		}
	}

	return true;
}

static TOUCH(drop_temp_touch) (gentity_t* ent, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (other == ent->owner)
		return;

	Touch_Item(ent, other, tr, otherTouchingSelf);
}

static THINK(drop_make_touchable) (gentity_t* ent) -> void {
	ent->touch = Touch_Item;
	if (deathmatch->integer) {
		if (!strcmp(ent->className, "ammo_pack"))
			ent->nextThink = level.time + 119_sec;
		else
			ent->nextThink = level.time + 29_sec;
		ent->think = FreeEntity;
	}
}

static inline void SetDroppedItemBounds(gentity_t* e, float scale = 1.0f) {
	if (!e)
		return;

	const float s = std::max(0.001f, scale);
	const Vector3 extent = { 15.0f * s, 15.0f * s, 15.0f * s };

	// Keep the dropped item's origin centered within its bounds so the world model
	// doesn't clip through the floor once physics settles.
	e->mins = -extent;
	e->maxs = extent;
}

/**
 * Creates and spawns an item dropped by a player.
 * This function centralizes the logic for creating a dropped item entity,
 * ensuring correct bounds, a safe spawn position, and proper physics setup.
 *
 * @param owner The player entity dropping the item.
 * @param item The item definition to drop.
 * @param count The quantity of the item (used for ammo).
 * @return A pointer to the spawned gentity_t, or nullptr on failure.
 */
static gentity_t* CreateDroppedItem(gentity_t* owner, Item* item, int count) {
	// --- Safety Checks ---
	if (!owner || !owner->client || !item || !item->worldModel) {
		return nullptr;
	}

	gentity_t* dropped = Spawn();
	if (!dropped) {
		return nullptr;
	}

	// --- Basic Item Setup ---
	dropped->item = item;
	dropped->count = count;
	dropped->className = item->className;
	dropped->spawnFlags = SPAWNFLAG_ITEM_DROPPED_PLAYER;
	dropped->s.effects = item->worldModelFlags;
	dropped->s.renderFX = RF_GLOW | RF_NO_LOD | RF_IR_VISIBLE;
	gi.setModel(dropped, item->worldModel);

	// --- Bounding Box Fix ---
	// Use a bottom-aligned bounding box so the item rests on the floor
	// instead of sinking halfway into it.
	SetDroppedItemBounds(dropped);

	// --- Physics and Ownership ---
	dropped->solid = SOLID_TRIGGER;
	dropped->moveType = MoveType::Toss;
	dropped->owner = owner;

	if (coop->integer && P_UseCoopInstancedItems()) {
		dropped->svFlags |= SVF_INSTANCED;
	}

	// --- Safe Spawn Position Calculation ---
	// Calculate a spawn point in front of the player that is not inside a wall.
	Vector3 forward, right, start, desired, offset = { 24, 0, -16 };
	AngleVectors(owner->client->vAngle, forward, right, nullptr);
	start = owner->s.origin;
	desired = G_ProjectSource(start, offset, forward, right);

	trace_t tr = gi.trace(start, dropped->mins, dropped->maxs, desired, owner, MASK_SOLID);
	dropped->s.origin = tr.endPos;

	G_FixStuckObject(dropped, dropped->s.origin);

	// --- Initial Velocity and Timers ---
	dropped->velocity = forward * 100.0f;
	dropped->velocity[_Z] = 300.0f;

	dropped->touch = drop_temp_touch; // Temporarily prevent self-pickup
	dropped->think = drop_make_touchable;
	dropped->nextThink = level.time + 1_sec;

	gi.linkEntity(dropped);
	return dropped;
}

/*
===============
SetScaledItemBounds
Apply ent->s.scale to a cubic item bounding box
===============
*/
static inline void SetScaledItemBounds(gentity_t* e, float baseHalf = 15.0f) {
	if (!e)
		return;

	// Ensure scale is always positive and non-zero
	const float s = std::max(0.001f, e->s.scale);

	// Calculate scaled half-size
	const float hx = baseHalf * s;
	const float hy = baseHalf * s;
	const float hz = baseHalf * s;

	// Assign mins/maxs
	e->mins = { -hx, -hy, -hz };
	e->maxs = { hx,  hy,  hz };
}

/*
=============
HighValuePickupCounter
=============
*/
static void HighValuePickupCounter(gentity_t* ent, gentity_t* other) {
	const auto index = static_cast<int>(ent->item->highValue);
	const GameTime delay = level.time - ent->timeStamp;

	// Per-client stats
	other->client->pers.match.pickupCounts[index]++;
	other->client->pers.match.pickupDelay[index] += delay;

	// Global match stats
	level.match.pickupCounts[index]++;
	level.match.pickupDelay[index] += delay;
}

// ***************************
//  DOPPELGANGER
// ***************************

gentity_t* Sphere_Spawn(gentity_t* owner, SpawnFlags spawnFlags);

static DIE(doppelganger_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	gentity_t* sphere;
	float	 dist;
	Vector3	 dir;

	if ((self->enemy) && (self->enemy != self->teamMaster)) {
		dir = self->enemy->s.origin - self->s.origin;
		dist = dir.length();

		if (dist > 80.f) {
			if (dist > 768) {
				sphere = Sphere_Spawn(self, SF_SPHERE_HUNTER | SF_DOPPELGANGER);
				sphere->pain(sphere, attacker, 0, 0, mod);
			}
			else {
				sphere = Sphere_Spawn(self, SF_SPHERE_VENGEANCE | SF_DOPPELGANGER);
				sphere->pain(sphere, attacker, 0, 0, mod);
			}
		}
	}

	self->takeDamage = static_cast<int>(DamageFlags::Normal);

	// [Paril-KEX]
	RadiusDamage(self, self->teamMaster, 160.f, self, 140.f, DamageFlags::Normal, ModID::Doppelganger_Explode);

	if (self->teamChain)
		BecomeExplosion1(self->teamChain);
	BecomeExplosion1(self);
}

static PAIN(doppelganger_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	self->enemy = other;
}

static THINK(doppelganger_timeout) (gentity_t* self) -> void {
	doppelganger_die(self, self, self, 9999, self->s.origin, ModID::Unknown);
}

static THINK(body_think) (gentity_t* self) -> void {
	float r;

	if (std::fabs(self->ideal_yaw - anglemod(self->s.angles[YAW])) < 2) {
		if (self->timeStamp < level.time) {
			r = frandom();
			if (r < 0.10f) {
				self->ideal_yaw = frandom(350.0f);
				self->timeStamp = level.time + 1_sec;
			}
		}
	}
	else
		M_ChangeYaw(self);

	if (self->teleportTime <= level.time) {
		self->s.frame++;
		if (self->s.frame > FRAME_stand40)
			self->s.frame = FRAME_stand01;

		self->teleportTime = level.time + 10_hz;
	}

	self->nextThink = level.time + FRAME_TIME_MS;
}

void fire_doppelganger(gentity_t* ent, const Vector3& start, const Vector3& aimDir) {
	gentity_t* base;
	gentity_t* body;
	Vector3	 dir;
	Vector3	 forward, right, up;
	int		 number;

	dir = VectorToAngles(aimDir);
	AngleVectors(dir, forward, right, up);

	base = Spawn();
	base->s.origin = start;
	base->s.angles = dir;
	base->moveType = MoveType::Toss;
	base->solid = SOLID_BBOX;
	base->s.renderFX |= RF_IR_VISIBLE;
	base->s.angles[PITCH] = 0;
	base->mins = { -16, -16, -24 };
	base->maxs = { 16, 16, 32 };
	base->s.modelIndex = gi.modelIndex("models/objects/dopplebase/tris.md2");
	base->s.alpha = 0.1f;
	base->teamMaster = ent;
	base->flags |= (FL_DAMAGEABLE | FL_TRAP);
	base->takeDamage = true;
	base->health = 30;
	base->pain = doppelganger_pain;
	base->die = doppelganger_die;

	base->nextThink = level.time + 30_sec;
	base->think = doppelganger_timeout;

	base->className = "doppelganger";

	gi.linkEntity(base);

	body = Spawn();
	number = body->s.number;
	body->s = ent->s;
	body->s.sound = 0;
	body->s.event = EV_NONE;
	body->s.number = number;
	body->yawSpeed = 30;
	body->ideal_yaw = 0;
	body->s.origin = start;
	body->s.origin[_Z] += 8;
	body->teleportTime = level.time + 10_hz;
	body->think = body_think;
	body->nextThink = level.time + FRAME_TIME_MS;
	gi.linkEntity(body);

	base->teamChain = body;
	body->teamMaster = base;

	// [Paril-KEX]
	body->owner = ent;
	gi.sound(body, CHAN_AUTO, gi.soundIndex("medic_commander/monsterspawn1.wav"), 1.f, ATTN_NORM, 0.f);
}

//======================================================================

constexpr GameTime DEFENDER_LIFESPAN = 10_sec;	//30_sec;
constexpr GameTime HUNTER_LIFESPAN = 10_sec;	//30_sec;
constexpr GameTime VENGEANCE_LIFESPAN = 10_sec;	//30_sec;
constexpr GameTime MINIMUM_FLY_TIME = 10_sec;	//15_sec;

void LookAtKiller(gentity_t* self, gentity_t* inflictor, gentity_t* attacker);

void vengeance_touch(gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf);
void hunter_touch(gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf);

// *************************
// General Sphere Code
// *************************

// =================
// sphere_think_explode
// =================
static THINK(sphere_think_explode) (gentity_t* self) -> void {
	if (self->owner && self->owner->client && !(self->spawnFlags & SF_DOPPELGANGER)) {
		self->owner->client->ownedSphere = nullptr;
	}
	BecomeExplosion1(self);
}

// =================
// sphere_explode
// =================
static DIE(sphere_explode) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	sphere_think_explode(self);
}

// =================
// sphere_if_idle_die - if the sphere is not currently attacking, blow up.
// =================
static DIE(sphere_if_idle_die) (gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, const Vector3& point, const MeansOfDeath& mod) -> void {
	if (!self->enemy)
		sphere_think_explode(self);
}

// *************************
// Sphere Movement
// *************************

static void sphere_fly(gentity_t* self) {
	Vector3 dest, dir;

	if (level.time >= GameTime::from_sec(self->wait)) {
		sphere_think_explode(self);
		return;
	}

	dest = self->owner->s.origin;
	dest[2] = self->owner->absMax[2] + 4;

	if (level.time.seconds() == level.time.seconds<int>()) {
		if (!visible(self, self->owner)) {
			self->s.origin = dest;
			gi.linkEntity(self);
			return;
		}
	}

	dir = dest - self->s.origin;
	self->velocity = dir * 5;
}

static void sphere_chase(gentity_t* self, int stupidChase) {
	Vector3 dest, dir;
	float  dist;

	if (!self || !self->enemy)
		return;

	if (level.time >= GameTime::from_sec(self->wait) || (self->enemy && self->enemy->health < 1)) {
		sphere_think_explode(self);
		return;
	}

	dest = self->enemy->s.origin;
	if (self->enemy->client)
		dest[2] += self->enemy->viewHeight;

	if (visible(self, self->enemy) || stupidChase) {
		// if moving, hunter sphere uses active sound
		if (!stupidChase)
			self->s.sound = gi.soundIndex("spheres/h_active.wav");

		dir = dest - self->s.origin;
		dir.normalize();
		self->s.angles = VectorToAngles(dir);
		self->velocity = dir * 300;	// 500;
		self->monsterInfo.savedGoal = dest;
	}
	else if (!self->monsterInfo.savedGoal) {
		dir = self->enemy->s.origin - self->s.origin;
		dist = dir.normalize();
		self->s.angles = VectorToAngles(dir);

		// if lurking, hunter sphere uses lurking sound
		self->s.sound = gi.soundIndex("spheres/h_lurk.wav");
		self->velocity = {};
	}
	else {
		dir = self->monsterInfo.savedGoal - self->s.origin;
		dist = dir.normalize();

		if (dist > 1) {
			self->s.angles = VectorToAngles(dir);

			if (dist > 500)
				self->velocity = dir * 500;
			else if (dist < 20)
				self->velocity = dir * (dist / gi.frameTimeSec);
			else
				self->velocity = dir * dist;

			// if moving, hunter sphere uses active sound
			if (!stupidChase)
				self->s.sound = gi.soundIndex("spheres/h_active.wav");
		}
		else {
			dir = self->enemy->s.origin - self->s.origin;
			dist = dir.normalize();
			self->s.angles = VectorToAngles(dir);

			// if not moving, hunter sphere uses lurk sound
			if (!stupidChase)
				self->s.sound = gi.soundIndex("spheres/h_lurk.wav");

			self->velocity = {};
		}
	}
}

// *************************
// Attack related stuff
// *************************

[[maybe_unused]] static void sphere_fire(gentity_t* self, gentity_t* enemy) {
	Vector3 dest;
	Vector3 dir;

	if (!enemy || level.time >= GameTime::from_sec(self->wait)) {
		sphere_think_explode(self);
		return;
	}

	dest = enemy->s.origin;
	self->s.effects |= EF_ROCKET;

	dir = dest - self->s.origin;
	dir.normalize();
	self->s.angles = VectorToAngles(dir);
	self->velocity = dir * 1000;

	self->touch = vengeance_touch;
	self->think = sphere_think_explode;
	self->nextThink = GameTime::from_sec(self->wait);
}

static void sphere_touch(gentity_t* self, gentity_t* other, const trace_t& tr, MeansOfDeath mod) {
	if (self->spawnFlags.has(SF_DOPPELGANGER)) {
		if (other == self->teamMaster)
			return;

		self->takeDamage = false;
		self->owner = self->teamMaster;
		self->teamMaster = nullptr;
	}
	else {
		if (other == self->owner)
			return;
		// PMM - don't blow up on bodies
		if (!strcmp(other->className, "bodyque"))
			return;
	}

	if (tr.surface && (tr.surface->flags & SURF_SKY)) {
		FreeEntity(self);
		return;
	}

	if (self->owner) {
		if (other->takeDamage) {
			Damage(other, self, self->owner, self->velocity, self->s.origin, tr.plane.normal,
				10000, 1, DamageFlags::DestroyArmor, mod);
		}
		else {
			RadiusDamage(self, self->owner, 512, self->owner, 256, DamageFlags::Normal, mod);
		}
	}

	sphere_think_explode(self);
}

TOUCH(vengeance_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	if (self->spawnFlags.has(SF_DOPPELGANGER))
		sphere_touch(self, other, tr, ModID::Doppelganger_Vengeance);
	else
		sphere_touch(self, other, tr, ModID::VengeanceSphere);
}

TOUCH(hunter_touch) (gentity_t* self, gentity_t* other, const trace_t& tr, bool otherTouchingSelf) -> void {
	gentity_t* owner;
	// don't blow up if you hit the world.... sheesh.
	if (other == world)
		return;

	if (self->owner) {
		// if owner is flying with us, make sure they stop too.
		owner = self->owner;
		if (owner->flags & FL_SAM_RAIMI) {
			owner->velocity = {};
			owner->moveType = MoveType::None;
			gi.linkEntity(owner);
		}
	}

	if (self->spawnFlags.has(SF_DOPPELGANGER))
		sphere_touch(self, other, tr, ModID::Doppelganger_Hunter);
	else
		sphere_touch(self, other, tr, ModID::HunterSphere);
}

static void defender_shoot(gentity_t* self, gentity_t* enemy) {
	Vector3 dir;
	Vector3 start;

	if (!(enemy->inUse) || enemy->health <= 0)
		return;

	if (enemy->client && enemy->client->eliminated)
		return;

	if (enemy == self->owner)
		return;

	dir = enemy->s.origin - self->s.origin;
	dir.normalize();

	if (self->monsterInfo.attackFinished > level.time)
		return;

	if (!visible(self, self->enemy))
		return;

	start = self->s.origin;
	start[2] += 2;
	fire_greenblaster(self->owner, start, dir, 10, 1000, EF_BLASTER, 0);

	self->monsterInfo.attackFinished = level.time + 400_ms;
}

// *************************
// Activation Related Stuff
// *************************

static void body_gib(gentity_t* self) {
	gi.sound(self, CHAN_BODY, gi.soundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);
	ThrowGibs(self, 50, {
		{ 4, "models/objects/gibs/sm_meat/tris.md2" },
		{ "models/objects/gibs/skull/tris.md2" }
		});
}

static PAIN(hunter_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	gentity_t* owner;
	float	 dist;
	Vector3	 dir;

	if (self->enemy)
		return;

	owner = self->owner;

	if (!(self->spawnFlags & SF_DOPPELGANGER)) {
		if (owner && (owner->health > 0))
			return;

		if (other == owner)
			return;
	}
	else {
		// if fired by a doppelganger, set it to 10 second timeout
		self->wait = (level.time + MINIMUM_FLY_TIME).seconds();
	}

	if ((GameTime::from_sec(self->wait) - level.time) < MINIMUM_FLY_TIME)
		self->wait = (level.time + MINIMUM_FLY_TIME).seconds();
	self->s.effects |= EF_BLASTER | EF_TRACKER;
	self->touch = hunter_touch;
	self->enemy = other;

	// if we're not owned by a player, no sam raimi
	// if we're spawned by a doppelganger, no sam raimi
	if (self->spawnFlags.has(SF_DOPPELGANGER) || !(owner && owner->client))
		return;

	// sam raimi cam is disabled if FORCE_RESPAWN is set.
	// sam raimi cam is also disabled if g_huntercam->value is 0.
	if (!match_doForceRespawn->integer && g_huntercam->integer) {
		dir = other->s.origin - self->s.origin;
		dist = dir.length();

		if (owner && (dist >= 192)) {
			// detach owner from body and send him flying
			owner->moveType = MoveType::FlyMissile;

			// gib like we just died, even though we didn't, really.
			body_gib(owner);

			// move the sphere to the owner's current viewpoint.
			// we know it's a valid spot (or will be momentarily)
			self->s.origin = owner->s.origin;
			self->s.origin[_Z] += owner->viewHeight;

			// move the player's origin to the sphere's new origin
			owner->s.origin = self->s.origin;
			owner->s.angles = self->s.angles;
			owner->client->vAngle = self->s.angles;
			owner->mins = { -5, -5, -5 };
			owner->maxs = { 5, 5, 5 };
			owner->client->ps.fov = 140;
			owner->s.modelIndex = 0;
			owner->s.modelIndex2 = 0;
			owner->viewHeight = 8;
			owner->solid = SOLID_NOT;
			owner->flags |= FL_SAM_RAIMI;
			gi.linkEntity(owner);

			self->solid = SOLID_BBOX;
			gi.linkEntity(self);
		}
	}
}

static PAIN(defender_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	if (other == self->owner)
		return;

	self->enemy = other;
}

static PAIN(vengeance_pain) (gentity_t* self, gentity_t* other, float kick, int damage, const MeansOfDeath& mod) -> void {
	if (self->enemy)
		return;

	if (!(self->spawnFlags & SF_DOPPELGANGER)) {
		if (self->owner && self->owner->health >= 25)
			return;

		if (other == self->owner)
			return;
	}
	else {
		self->wait = (level.time + MINIMUM_FLY_TIME).seconds();
	}

	if ((GameTime::from_sec(self->wait) - level.time) < MINIMUM_FLY_TIME)
		self->wait = (level.time + MINIMUM_FLY_TIME).seconds();
	self->s.effects |= EF_ROCKET;
	self->touch = vengeance_touch;
	self->enemy = other;
}

// *************************
// Think Functions
// *************************

static THINK(defender_think) (gentity_t* self) -> void {
	if (!self->owner) {
		FreeEntity(self);
		return;
	}

	// if we've exited the level, just remove ourselves.
	if (level.intermission.time) {
		sphere_think_explode(self);
		return;
	}

	if (self->owner->health <= 0 || self->owner->client->eliminated) {
		sphere_think_explode(self);
		return;
	}

	self->s.frame++;
	if (self->s.frame > 19)
		self->s.frame = 0;

	if (self->enemy) {
		if (self->enemy->health > 0)
			defender_shoot(self, self->enemy);
		else
			self->enemy = nullptr;
	}

	sphere_fly(self);

	if (self->inUse)
		self->nextThink = level.time + 10_hz;
}

static THINK(hunter_think) (gentity_t* self) -> void {
	// if we've exited the level, just remove ourselves.
	if (level.intermission.time) {
		sphere_think_explode(self);
		return;
	}

	gentity_t* owner = self->owner;

	if (!owner && !(self->spawnFlags & SF_DOPPELGANGER)) {
		FreeEntity(self);
		return;
	}

	if (owner)
		self->ideal_yaw = owner->s.angles[YAW];
	else if (self->enemy) // fired by doppelganger
	{
		Vector3 dir = self->enemy->s.origin - self->s.origin;
		self->ideal_yaw = vectoyaw(dir);
	}

	M_ChangeYaw(self);

	if (self->enemy) {
		sphere_chase(self, 0);

		// deal with sam raimi cam
		if (owner && (owner->flags & FL_SAM_RAIMI)) {
			if (self->inUse) {
				owner->moveType = MoveType::FlyMissile;
				LookAtKiller(owner, self, self->enemy);
				// owner is flying with us, move him too
				owner->moveType = MoveType::FlyMissile;
				owner->viewHeight = (int)(self->s.origin[_Z] - owner->s.origin[_Z]);
				owner->s.origin = self->s.origin;
				owner->velocity = self->velocity;
				owner->mins = {};
				owner->maxs = {};
				gi.linkEntity(owner);
			}
			else // sphere timed out
			{
				owner->velocity = {};
				owner->moveType = MoveType::None;
				gi.linkEntity(owner);
			}
		}
	}
	else
		sphere_fly(self);

	if (self->inUse)
		self->nextThink = level.time + 10_hz;
}

static THINK(vengeance_think) (gentity_t* self) -> void {
	// if we've exited the level, just remove ourselves.
	if (level.intermission.time) {
		sphere_think_explode(self);
		return;
	}

	if (!(self->owner) && !(self->spawnFlags & SF_DOPPELGANGER)) {
		FreeEntity(self);
		return;
	}

	if (self->enemy)
		sphere_chase(self, 1);
	else
		sphere_fly(self);

	if (self->inUse)
		self->nextThink = level.time + 10_hz;
}

// =================
gentity_t* Sphere_Spawn(gentity_t* owner, SpawnFlags spawnFlags) {
	gentity_t* sphere;

	sphere = Spawn();
	sphere->s.origin = owner->s.origin;
	sphere->s.origin[_Z] = owner->absMax[2];
	sphere->s.angles[YAW] = owner->s.angles[YAW];
	sphere->solid = SOLID_BBOX;
	sphere->clipMask = MASK_PROJECTILE;
	sphere->s.renderFX = RF_FULLBRIGHT | RF_IR_VISIBLE;
	sphere->moveType = MoveType::FlyMissile;

	if (spawnFlags.has(SF_DOPPELGANGER))
		sphere->teamMaster = owner->teamMaster;
	else
		sphere->owner = owner;

	sphere->className = "sphere";
	sphere->yawSpeed = 40;
	sphere->monsterInfo.attackFinished = 0_ms;
	sphere->spawnFlags = spawnFlags; // need this for the HUD to recognize sphere
	sphere->takeDamage = true;	// false;
	sphere->health = 20;

	switch ((spawnFlags & SF_SPHERE_TYPE).value) {
	case SF_SPHERE_DEFENDER.value:
		sphere->s.modelIndex = gi.modelIndex("models/items/defender/tris.md2");
		sphere->s.modelIndex2 = gi.modelIndex("models/items/shell/tris.md2");
		sphere->s.sound = gi.soundIndex("spheres/d_idle.wav");
		sphere->pain = defender_pain;
		sphere->wait = (level.time + DEFENDER_LIFESPAN).seconds();
		sphere->die = sphere_explode;
		sphere->think = defender_think;
		break;
	case SF_SPHERE_HUNTER.value:
		sphere->s.modelIndex = gi.modelIndex("models/items/hunter/tris.md2");
		sphere->s.sound = gi.soundIndex("spheres/h_idle.wav");
		sphere->wait = (level.time + HUNTER_LIFESPAN).seconds();
		sphere->pain = hunter_pain;
		sphere->die = sphere_if_idle_die;
		sphere->think = hunter_think;
		break;
	case SF_SPHERE_VENGEANCE.value:
		sphere->s.modelIndex = gi.modelIndex("models/items/vengnce/tris.md2");
		sphere->s.sound = gi.soundIndex("spheres/v_idle.wav");
		sphere->wait = (level.time + VENGEANCE_LIFESPAN).seconds();
		sphere->pain = vengeance_pain;
		sphere->die = sphere_if_idle_die;
		sphere->think = vengeance_think;
		sphere->aVelocity = { 30, 30, 0 };
		break;
	default:
		gi.Com_Print("Tried to create an invalid sphere\n");
		FreeEntity(sphere);
		return nullptr;
	}

	sphere->nextThink = level.time + 10_hz;

	gi.linkEntity(sphere);

	return sphere;
}

// =================
// Own_Sphere - attach the sphere to the client so we can
//		directly access it later
// =================
static void Own_Sphere(gentity_t* self, gentity_t* sphere) {
	if (!sphere)
		return;

	// ownership only for players
	if (self->client) {
		// if they don't have one
		if (!(self->client->ownedSphere)) {
			self->client->ownedSphere = sphere;
		}
		// they already have one, take care of the old one
		else {
			if (self->client->ownedSphere->inUse) {
				FreeEntity(self->client->ownedSphere);
				self->client->ownedSphere = sphere;
			}
			else {
				self->client->ownedSphere = sphere;
			}
		}
	}
}

void Defender_Launch(gentity_t* self) {
	gentity_t* sphere;

	sphere = Sphere_Spawn(self, SF_SPHERE_DEFENDER);
	Own_Sphere(self, sphere);
}

void Hunter_Launch(gentity_t* self) {
	gentity_t* sphere;

	sphere = Sphere_Spawn(self, SF_SPHERE_HUNTER);
	Own_Sphere(self, sphere);
}

void Vengeance_Launch(gentity_t* self) {
	gentity_t* sphere;

	sphere = Sphere_Spawn(self, SF_SPHERE_VENGEANCE);
	Own_Sphere(self, sphere);
}

//======================================================================

static gentity_t* QuadHog_FindSpawn() {
	return SelectDeathmatchSpawnPoint(nullptr, vec3_origin, true, true, false, true).spot;
}

static void QuadHod_ClearAll() {
	gentity_t* ent;

	for (ent = g_entities; ent < &g_entities[globals.numEntities]; ent++) {

		if (!ent->inUse)
			continue;

		if (ent->client) {
			ent->client->PowerupTimer(PowerupTimer::QuadDamage) = 0_ms;
			ent->client->pers.inventory[IT_POWERUP_QUAD] = 0;
			continue;
		}

		if (!ent->className)
			continue;

		if (!ent->item)
			continue;

		if (ent->item->id != IT_POWERUP_QUAD)
			continue;

		FreeEntity(ent);
	}
}

void QuadHog_Spawn(Item* item, gentity_t* spot, bool reset) {
	gentity_t* ent;
	Vector3	 forward, right;
	Vector3	 angles = vec3_origin;

	QuadHod_ClearAll();

	ent = Spawn();

	ent->className = item->className;
	ent->item = item;
	ent->spawnFlags = SPAWNFLAG_ITEM_DROPPED;
	ent->s.effects = item->worldModelFlags | EF_COLOR_SHELL;
	ent->s.renderFX = RF_GLOW | RF_NO_LOD | RF_SHELL_BLUE;
	SetScaledItemBounds(ent, 15.0f);
	gi.setModel(ent, item->worldModel);
	ent->solid = SOLID_TRIGGER;
	ent->moveType = MoveType::Toss;
	ent->touch = Touch_Item;
	ent->owner = ent;
	ent->nextThink = level.time + 30_sec;
	ent->think = QuadHog_DoSpawn;

	angles[PITCH] = 0;
	angles[YAW] = (float)irandom(360);
	angles[ROLL] = 0;

	AngleVectors(angles, forward, right, nullptr);
	ent->s.origin = spot->s.origin;
	ent->s.origin[_Z] += 16;
	ent->velocity = forward * 100;
	ent->velocity[_Z] = 300;

	gi.LocBroadcast_Print(PRINT_CENTER, "The Quad {}!\n", reset ? "respawned" : "has spawned");
	gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundIndex("misc/alarm.wav"), 1, ATTN_NONE, 0);

	gi.linkEntity(ent);
}

THINK(QuadHog_DoSpawn) (gentity_t* ent) -> void {
	gentity_t* spot;
	Item* it = GetItemByIndex(IT_POWERUP_QUAD);

	if (!it)
		return;

	if ((spot = QuadHog_FindSpawn()) != nullptr)
		QuadHog_Spawn(it, spot, false);

	if (ent)
		FreeEntity(ent);
}

THINK(QuadHog_DoReset) (gentity_t* ent) -> void {
	gentity_t* spot;
	Item* it = GetItemByIndex(IT_POWERUP_QUAD);

	if (!it)
		return;

	if ((spot = QuadHog_FindSpawn()) != nullptr)
		QuadHog_Spawn(it, spot, true);

	if (ent)
		FreeEntity(ent);
}

void QuadHog_SetupSpawn(GameTime delay) {
	gentity_t* ent;

	if (!g_quadhog->integer)
		return;

	ent = Spawn();
	ent->nextThink = level.time + delay;
	ent->think = QuadHog_DoSpawn;
}

//======================================================================

//======================================================================
// TECH
//======================================================================

/*
===============
Constants and small helpers
===============
*/
constexpr GameTime TECH_TIMEOUT = 60_sec; // seconds before techs spawn again

/*
===============
TechSfxVolume
===============
*/
static inline float TechSfxVolume(const gentity_t* ent) {
	return (ent && ent->client && ent->client->PowerupCount(PowerupCount::SilencerShots)) ? 0.2f : 1.0f;
}

/*
===============
TechTickReady
Once-per-second SFX throttle for tech sounds
===============
*/
static inline bool TechTickReady(gentity_t* ent) {
	if (!ent || !ent->client)
		return false;
	if (ent->client->tech.soundTime < level.time) {
		ent->client->tech.soundTime = level.time + 1_sec;
		return true;
	}
	return false;
}

/*
===============
FindTechSpawn
===============
*/
static gentity_t* FindTechSpawn() {
	return SelectDeathmatchSpawnPoint(nullptr, vec3_origin, true, true, false, true).spot;
}

/*
===============
Tech_Held
Returns the Item* of the tech the player holds, or nullptr
===============
*/
Item* Tech_Held(gentity_t* ent) {
	if (!ent || !ent->client)
		return nullptr;

	for (size_t i = 0; i < q_countof(tech_ids); ++i) {
		if (ent->client->pers.inventory[tech_ids[i]])
			return GetItemByIndex(tech_ids[i]);
	}
	return nullptr;
}

/*
===============
Tech_PlayerHasATech
Sends periodic reminder, returns true if player holds any tech
===============
*/
static bool Tech_PlayerHasATech(gentity_t* ent) {
	if (!ent || !ent->client)
		return false;

	if (Tech_Held(ent) != nullptr) {
		if (level.time - ent->client->tech.lastMessageTime > 10_sec) {
			ent->client->tech.lastMessageTime = level.time;
			// Optional: gi.LocCenter_Print(ent, "$g_already_have_tech");
		}
		return true;
	}
	return false;
}

/*
===============
Tech_Pickup
===============
*/
bool Tech_Pickup(gentity_t* ent, gentity_t* other) {
	// client only gets one tech
	if (!other || !other->client || Tech_PlayerHasATech(other))
		return false;

	other->client->pers.inventory[ent->item->id]++;
	other->client->tech.regenTime = level.time;
	return true;
}

/*
===============
Tech_Think
Respawn tech at a valid point, or retry later
===============
*/
static THINK(Tech_Think) (gentity_t* tech) -> void {
	if (!tech || !tech->item) {
		if (tech) FreeEntity(tech);
		return;
	}

	if (gentity_t* spot = FindTechSpawn()) {
		// Reuse spawn primitive
		Vector3 forward, right;
		Vector3 angles = { 0.0f, (float)irandom(360), 0.0f };

		AngleVectors(angles, forward, right, nullptr);

		gentity_t* ent = Spawn();
		if (!ent) {
			tech->nextThink = level.time + TECH_TIMEOUT;
			tech->think = Tech_Think;
			return;
		}

		ent->className = tech->item->className;
		ent->item = tech->item;
		ent->spawnFlags = SPAWNFLAG_ITEM_DROPPED;
		ent->s.effects = tech->item->worldModelFlags;
		ent->s.renderFX = RF_GLOW | RF_NO_LOD;

		SetScaledItemBounds(ent);
		gi.setModel(ent, ent->item->worldModel);

		ent->solid = SOLID_TRIGGER;
		ent->moveType = MoveType::Toss;
		ent->touch = Touch_Item;
		ent->owner = ent;

		ent->s.origin = spot->s.origin;
		ent->s.origin[_Z] += 16.0f;
		ent->velocity = forward * 100.0f;
		ent->velocity[_Z] = 300.0f;

		ent->nextThink = level.time + TECH_TIMEOUT;
		ent->think = Tech_Think;

		gi.linkEntity(ent);
		FreeEntity(tech);
	}
	else {
		tech->nextThink = level.time + TECH_TIMEOUT;
		tech->think = Tech_Think;
	}
}

/*
===============
Tech_Make_Touchable
===============
*/
static THINK(Tech_Make_Touchable) (gentity_t* tech) -> void {
	if (!tech)
		return;
	tech->touch = Touch_Item;
	tech->nextThink = level.time + TECH_TIMEOUT;
	tech->think = Tech_Think;
}

/*
===============
Tech_Drop
===============
*/
void Tech_Drop(gentity_t* ent, Item* item) {
	if (!ent || !item || !ent->client)
		return;

	gentity_t* tech = Drop_Item(ent, item);
	if (!tech)
		return;

	tech->nextThink = level.time + 1_sec;
	tech->think = Tech_Make_Touchable;

	ent->client->pers.inventory[item->id] = 0;
}

/*
===============
Tech_DeadDrop
===============
*/
void Tech_DeadDrop(gentity_t* ent) {
	if (!ent || !ent->client)
		return;

	for (size_t i = 0; i < q_countof(tech_ids); ++i) {
		const item_id_t tid = tech_ids[i];
		if (!ent->client->pers.inventory[tid])
			continue;

		gentity_t* dropped = Drop_Item(ent, GetItemByIndex(tid));
		if (dropped) {
			// hack velocity to bounce randomly
			dropped->velocity[_X] = crandom_open() * 300.0f;
			dropped->velocity[_Y] = crandom_open() * 300.0f;
			dropped->nextThink = level.time + TECH_TIMEOUT;
			dropped->think = Tech_Think;
			dropped->owner = nullptr;
		}
		ent->client->pers.inventory[tid] = 0;
	}
}

/*
===============
Tech_Spawn
===============
*/
static void Tech_Spawn(Item* item, gentity_t* spot) {
	if (!item || !spot)
		return;

	gentity_t* ent = Spawn();
	if (!ent)
		return;

	Vector3 forward, right;
	Vector3 angles = { 0.0f, (float)irandom(360), 0.0f };

	ent->className = item->className;
	ent->item = item;
	ent->spawnFlags = SPAWNFLAG_ITEM_DROPPED;
	ent->s.effects = item->worldModelFlags;
	ent->s.renderFX = RF_GLOW | RF_NO_LOD;

	SetScaledItemBounds(ent);
	gi.setModel(ent, item->worldModel);

	ent->solid = SOLID_TRIGGER;
	ent->moveType = MoveType::Toss;
	ent->touch = Touch_Item;
	ent->owner = ent;

	AngleVectors(angles, forward, right, nullptr);
	ent->s.origin = spot->s.origin;
	ent->s.origin[_Z] += 16.0f;
	ent->velocity = forward * 100.0f;
	ent->velocity[_Z] = 300.0f;

	ent->nextThink = level.time + TECH_TIMEOUT;
	ent->think = Tech_Think;

	gi.linkEntity(ent);
}

/*
===============
AllowTechs
===============
*/
static bool AllowTechs() {
	// "auto" => only in CTF, not in instagib/nadefest/ball
	if (!strcmp(g_allowTechs->string, "auto"))
		return !!(Game::Is(GameType::CaptureTheFlag) && !g_instaGib->integer && !g_nadeFest->integer && Game::IsNot(GameType::ProBall));

	// explicit on/off obeys global item spawn toggle
	return !!(g_allowTechs->integer && ItemSpawnsEnabled());
}

/*
===============
Tech_SpawnAll
===============
*/
static THINK(Tech_SpawnAll) (gentity_t* ent) -> void {
	if (!AllowTechs()) {
		if (ent) FreeEntity(ent);
		return;
	}

	int num = 0;
	if (!strcmp(g_allowTechs->string, "auto"))
		num = 1;
	else
		num = g_allowTechs->integer;

	if (num <= 0) {
		if (ent) FreeEntity(ent);
		return;
	}

	for (size_t i = 0; i < q_countof(tech_ids); ++i) {
		Item* it = GetItemByIndex(tech_ids[i]);
		if (!it)
			continue;

		for (int j = 0; j < num; ++j) {
			if (gentity_t* spot = FindTechSpawn())
				Tech_Spawn(it, spot);
		}
	}

	if (ent)
		FreeEntity(ent);
}

/*
===============
Tech_SetupSpawn
===============
*/
void Tech_SetupSpawn() {
	if (!AllowTechs())
		return;

	gentity_t* ent = Spawn();
	if (!ent)
		return;

	ent->nextThink = level.time + 2_sec;
	ent->think = Tech_SpawnAll;
}

/*
===============
Tech_Reset
===============
*/
void Tech_Reset() {
	// Remove all active tech entities
	for (uint32_t i = 1; i < globals.numEntities; ++i) {
		gentity_t* e = g_entities + i;
		if (!e->inUse)
			continue;
		if (e->item && (e->item->flags & IF_TECH))
			FreeEntity(e);
	}
	Tech_SetupSpawn();
}

/*
===============
Tech_ApplyDisruptorShield
Halves damage if the player holds Disruptor Shield, with optional silenced volume
===============
*/
int Tech_ApplyDisruptorShield(gentity_t* ent, int dmg) {
	if (!ent || !ent->client || !dmg)
		return dmg;

	if (ent->client->pers.inventory[IT_TECH_DISRUPTOR_SHIELD]) {
		static const int snd = []() { return gi.soundIndex("ctf/tech1.wav"); }();
		gi.sound(ent, CHAN_AUX, snd, TechSfxVolume(ent), ATTN_NORM, 0);
		return dmg / 2;
	}
	return dmg;
}

/*
===============
Tech_ApplyPowerAmpSound
Plays periodic sound if the player holds Power Amp (quad variant if active)
===============
*/
bool Tech_ApplyPowerAmpSound(gentity_t* ent) {
	if (!ent || !ent->client)
		return false;

	if (ent->client->pers.inventory[IT_TECH_POWER_AMP]) {
		if (TechTickReady(ent)) {
			const bool quad = (ent->client->PowerupTimer(PowerupTimer::QuadDamage) > level.time);
			static const int snd_amp = []() { return gi.soundIndex("ctf/tech2.wav"); }();
			static const int snd_ampx = []() { return gi.soundIndex("ctf/tech2x.wav"); }();
			gi.sound(ent, CHAN_AUX, quad ? snd_ampx : snd_amp, TechSfxVolume(ent), ATTN_NORM, 0);
		}
		return true;
	}
	return false;
}

/*
===============
Tech_ApplyTimeAccel
===============
*/
bool Tech_ApplyTimeAccel(gentity_t* ent) {
	return (ent && ent->client && ent->client->pers.inventory[IT_TECH_TIME_ACCEL]);
}

/*
===============
Tech_ApplyTimeAccelSound
===============
*/
void Tech_ApplyTimeAccelSound(gentity_t* ent) {
	if (!ent || !ent->client)
		return;

	if (ent->client->pers.inventory[IT_TECH_TIME_ACCEL] && TechTickReady(ent)) {
		static const int snd = []() { return gi.soundIndex("ctf/tech3.wav"); }();
		gi.sound(ent, CHAN_AUX, snd, TechSfxVolume(ent), ATTN_NORM, 0);
	}
}

/*
===============
Tech_ApplyAutoDoc
Regenerate health/armor with mode-aware limits and SFX
===============
*/
void Tech_ApplyAutoDoc(gentity_t* ent) {
	if (!ent || !ent->client)
		return;
	gclient_t* cl = ent->client;

	if (ent->health <= 0 || cl->eliminated)
		return;

	// Mode flags
	const bool mod = (g_instaGib->integer || g_nadeFest->integer);
	const bool no_health = mod || Game::Has(GameFlags::Arena) || !game.map.spawnHealth;

	// Max values
	const int max = g_vampiric_damage->integer
		? (int)ceil(g_vampiric_health_max->integer / 2.0)
		: (mod ? 100 : 150);

	// Honor silenced volume
	const float volume = TechSfxVolume(ent);

	// In special modes, ensure regenTime gets initialized once
	if (mod && !cl->tech.regenTime) {
		cl->tech.regenTime = level.time;
		return;
	}

	// Must have the tech unless in those special modes
	if (!(cl->pers.inventory[IT_TECH_AUTODOC] || mod))
		return;

	bool madeNoise = false;

	if (cl->tech.regenTime < level.time) {
		cl->tech.regenTime = level.time;

		// Health first (unless vampiric mode forbids)
		if (!g_vampiric_damage->integer) {
			if (ent->health < max) {
				ent->health += 5;
				if (ent->health > max) ent->health = max;
				cl->tech.regenTime += 1_sec;
				madeNoise = true;
			}
		}

		// If we did not add health and health is allowed, try armor
		if (!no_health && !madeNoise) {
			const int index = ArmorIndex(ent);
			if (index && cl->pers.inventory[index] < max) {
				cl->pers.inventory[index] += g_vampiric_damage->integer ? 10 : 5;
				if (cl->pers.inventory[index] > max) cl->pers.inventory[index] = max;
				cl->tech.regenTime += 1_sec;
				madeNoise = true;
			}
		}
	}

	if (madeNoise && TechTickReady(ent)) {
		static const int snd = []() { return gi.soundIndex("ctf/tech4.wav"); }();
		gi.sound(ent, CHAN_AUX, snd, volume, ATTN_NORM, 0);
	}
}

/*
===============
Tech_HasRegeneration
===============
*/
bool Tech_HasRegeneration(gentity_t* ent) {
	if (!ent || !ent->client) return false;
	if (ent->client->pers.inventory[IT_TECH_AUTODOC]) return true;
	if (g_instaGib->integer) return true;
	if (g_nadeFest->integer) return true;
	return false;
}

// ===============================================

/*
===============
GetItemByIndex
===============
*/
Item* GetItemByIndex(item_id_t index) {
	if (index <= IT_NULL || index >= IT_TOTAL)
		return nullptr;

	return &itemList[index];
}

static Item* ammoList[static_cast<int>(AmmoID::_Total)];

Item* GetItemByAmmo(AmmoID ammo) {
	return ammoList[static_cast<int>(ammo)];
}

static Item* powerupList[POWERUP_MAX];

Item* GetItemByPowerup(powerup_t powerup) {
	return powerupList[powerup];
}

/*
===============
FindItemByClassname

===============
*/
Item* FindItemByClassname(const char* className) {
	for (auto& item : itemList) {
		if (!item.className)
			continue;
		if (!Q_strcasecmp(item.className, className))
			return &item;
	}

	return nullptr;
}

/*
===============
FindItem

===============
*/
Item* FindItem(const char* pickupName) {
	for (auto& item : itemList) {
		if (!item.useName)
			continue;
		if (!Q_strcasecmp(item.useName, pickupName))
			return &item;
	}

	return nullptr;
}

//======================================================================

static inline item_flags_t GetSubstituteItemFlags(item_id_t id) {
	const Item* item = GetItemByIndex(id);

	// we want to stay within the item class
	item_flags_t flags = item->flags & IF_TYPE_MASK;

	if ((flags & (IF_WEAPON | IF_AMMO)) == (IF_WEAPON | IF_AMMO))
		flags = IF_AMMO;

	return flags;
}

static inline item_id_t FindSubstituteItem(gentity_t* ent) {
	// never replace flags
	if (ent->item->id == IT_FLAG_RED || ent->item->id == IT_FLAG_BLUE || ent->item->id == IT_TAG_TOKEN)
		return IT_NULL;

	// never replace meaty goodness
	if (ent->item->id == IT_FOODCUBE)
		return IT_NULL;

	// stimpack/shard randomizes
	if (ent->item->id == IT_HEALTH_SMALL ||
		ent->item->id == IT_ARMOR_SHARD)
		return brandom() ? IT_HEALTH_SMALL : IT_ARMOR_SHARD;

	// health is special case
	if (ent->item->id == IT_HEALTH_MEDIUM ||
		ent->item->id == IT_HEALTH_LARGE) {
		float rnd = frandom();

		if (rnd < 0.6f)
			return IT_HEALTH_MEDIUM;
		else
			return IT_HEALTH_LARGE;
	}

	// mega health is special case
	if (ent->item->id == IT_HEALTH_MEGA ||
		ent->item->id == IT_ADRENALINE) {
		float rnd = frandom();

		if (rnd < 0.6f)
			return IT_HEALTH_MEGA;
		else
			return IT_ADRENALINE;
	}

	// armor is also special case
	else if (ent->item->id == IT_ARMOR_JACKET ||
		ent->item->id == IT_ARMOR_COMBAT ||
		ent->item->id == IT_ARMOR_BODY ||
		ent->item->id == IT_POWER_SCREEN ||
		ent->item->id == IT_POWER_SHIELD) {
		float rnd = frandom();

		if (rnd < 0.4f)
			return IT_ARMOR_JACKET;
		else if (rnd < 0.6f)
			return IT_ARMOR_COMBAT;
		else if (rnd < 0.8f)
			return IT_ARMOR_BODY;
		else if (rnd < 0.9f)
			return IT_POWER_SCREEN;
		else
			return IT_POWER_SHIELD;
	}

	item_flags_t myflags = GetSubstituteItemFlags(ent->item->id);

	std::array<item_id_t, MAX_ITEMS> possible_items{};
	size_t possible_item_count = 0;

	// gather matching items
	for (item_id_t i = static_cast<item_id_t>(IT_NULL + 1); i < IT_TOTAL; i = static_cast<item_id_t>(static_cast<int32_t>(i) + 1)) {
		const Item* it = GetItemByIndex(i);
		item_flags_t itflags = it->flags;
		bool add = false, subtract = false;

		if (game.item_inhibit_pu && itflags & (IF_POWERUP | IF_SPHERE)) {
			add = game.item_inhibit_pu > 0 ? true : false;
			subtract = game.item_inhibit_pu < 0 ? true : false;
		}
		else if (game.item_inhibit_pa && itflags & IF_POWER_ARMOR) {
			add = game.item_inhibit_pa > 0 ? true : false;
			subtract = game.item_inhibit_pa < 0 ? true : false;
		}
		else if (game.item_inhibit_ht && itflags & IF_HEALTH) {
			add = game.item_inhibit_ht > 0 ? true : false;
			subtract = game.item_inhibit_ht < 0 ? true : false;
		}
		else if (game.item_inhibit_ar && itflags & IF_ARMOR) {
			add = game.item_inhibit_ar > 0 ? true : false;
			subtract = game.item_inhibit_ar < 0 ? true : false;
		}
		else if (game.item_inhibit_am && itflags & IF_AMMO) {
			add = game.item_inhibit_am > 0 ? true : false;
			subtract = game.item_inhibit_am < 0 ? true : false;
		}
		else if (game.item_inhibit_wp && itflags & IF_WEAPON) {
			add = game.item_inhibit_wp > 0 ? true : false;
			subtract = game.item_inhibit_wp < 0 ? true : false;
		}

		if (subtract)
			continue;

		if (!add) {
			if (!itflags || (itflags & (IF_NOT_GIVEABLE | IF_TECH | IF_NOT_RANDOM)) || !it->pickup || !it->worldModel)
				continue;

			if (!game.map.spawnPowerups && itflags & (IF_POWERUP | IF_SPHERE))
				continue;

			if (!game.map.spawnBFG && ent->item->id == IT_WEAPON_BFG)
				continue;

			if (g_no_spheres->integer && itflags & IF_SPHERE)
				continue;

			if (g_no_nukes->integer && i == IT_AMMO_NUKE)
				continue;

			if (g_no_mines->integer &&
				(i == IT_AMMO_PROX || i == IT_AMMO_TESLA || i == IT_AMMO_TRAP || i == IT_WEAPON_PROXLAUNCHER))
				continue;
		}

		itflags = GetSubstituteItemFlags(i);

		if ((itflags & IF_TYPE_MASK) == (myflags & IF_TYPE_MASK))
			possible_items[possible_item_count++] = i;
	}

	if (!possible_item_count)
		return IT_NULL;

	return possible_items[irandom(possible_item_count)];
}

/*
=================
DoRandomRespawn
=================
*/
item_id_t DoRandomRespawn(gentity_t* ent) {
	if (!ent->item)
		return IT_NULL; // why

	item_id_t id = FindSubstituteItem(ent);

	if (id == IT_NULL)
		return IT_NULL;

	return id;
}

/*
=================
RespawnItem
=================
*/
THINK(RespawnItem)(gentity_t* ent) -> void {
	if (!ent)
		return;

	// Handle team-chained items
	if (ent->team) {
		gentity_t* master = ent->teamMaster;
		if (!master) {
			gi.Com_ErrorFmt("{}: {} has no valid teamMaster", __FUNCTION__, *ent);
			return;
		}

		gentity_t* current = ent;

		// For weapon stay in CTF, always respawn only the master item
		if (Game::Is(GameType::CaptureTheFlag) && match_weaponsStay->integer && master->item && (master->item->flags & IF_WEAPON)) {
			ent = master;
		}
		else {
			// Hide current item
			current->svFlags |= SVF_NOCLIENT;
			current->solid = SOLID_NOT;
			gi.linkEntity(current);

			// Reset all timers and determine current index
			int count = 0, current_index = 0;
			for (gentity_t* scan = master; scan; scan = scan->chain, count++) {
				scan->nextThink = 0_sec;
				if (scan == current)
					current_index = count;
			}

			int choice = (current_index + 1) % count;
			gentity_t* selected = master;
			for (int i = 0; i < choice && selected; i++)
				selected = selected->chain;

			if (!selected) {
				gi.Com_ErrorFmt("{}: team chain traversal failed", __FUNCTION__);
				return;
			}

			ent = selected;
		}
	}

	// Make item visible and solid again
	ent->svFlags &= ~(SVF_NOCLIENT | SVF_RESPAWNING);
	ent->solid = SOLID_TRIGGER;
	gi.linkEntity(ent);

	// Trigger visual effect unless match just began
	if (level.time > level.levelStartTime + 100_ms)
		ent->s.event = EV_ITEM_RESPAWN;

	// Random item respawn handling
	if (g_dm_random_items->integer) {
		if (item_id_t new_item = DoRandomRespawn(ent)) {
			ent->item = GetItemByIndex(new_item);
			ent->className = ent->item->className;
			ent->s.effects = ent->item->worldModelFlags;
			gi.setModel(ent, ent->item->worldModel);
		}
	}

	// Powerup sound notification
	if (deathmatch->integer && (ent->item->flags & IF_POWERUP)) {
		gi.positionedSound(world->s.origin, world, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX,
			gi.soundIndex("items/poweruprespawn.wav"), 1, ATTN_NONE, 0);
	}
}

void SetRespawn(gentity_t* ent, GameTime delay, bool hide_self) {
	if (!deathmatch->integer)
		return;

	if (ent->spawnFlags.has(SPAWNFLAG_ITEM_DROPPED))
		return;

	if ((ent->item->flags & IF_AMMO) && ent->spawnFlags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER))
		return;

	// already respawning
	if (ent->think && ent->nextThink >= level.time)
		return;

	ent->flags |= FL_RESPAWN;

	if (hide_self) {
		ent->svFlags |= (SVF_NOCLIENT | SVF_RESPAWNING);
		ent->solid = SOLID_NOT;
		gi.linkEntity(ent);
	}

	GameTime t = 0_sec;
	if (ent->random) {
		t += GameTime::from_ms((crandom() * ent->random) * 1000);
		if (t < FRAME_TIME_MS)
			t = FRAME_TIME_MS;
	}

	delay *= match_itemsRespawnRate->value;

	ent->nextThink = level.time + delay + t;

	// 4x longer delay in horde
	if (Game::Is(GameType::Horde))
		ent->nextThink += delay * 3;

	ent->think = RespawnItem;
}

//======================================================================

void Use_Teleporter(gentity_t* ent, Item* item) {
	gentity_t* fx = Spawn();
	fx->className = "telefx";
	fx->s.event = EV_PLAYER_TELEPORT;
	fx->s.origin = ent->s.origin;
	fx->s.origin[_Z] += 1.0f;
	fx->s.angles = ent->s.angles;
	fx->nextThink = level.time + 100_ms;
	fx->solid = SOLID_NOT;
	fx->think = FreeEntity;
	gi.linkEntity(fx);
	TeleportPlayerToRandomSpawnPoint(ent, true);

	ent->client->pers.inventory[item->id]--;
	UsedMessage(ent, item);
}

bool Pickup_Teleporter(gentity_t* ent, gentity_t* other) {
	if (!deathmatch->integer)
		return false;
	if (other->client->pers.inventory[ent->item->id])
		return false;

	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, 120_sec);
	return true;
}

//======================================================================

static bool IsInstantItemsEnabled() {
	if (deathmatch->integer && match_instantItems->integer)
		return true;
	if (!deathmatch->integer && level.instantItems)
		return true;

	return false;
}

static bool Pickup_AllowPowerupPickup(gentity_t* ent, gentity_t* other) {
	int quantity = other->client->pers.inventory[ent->item->id];
	if ((skill->integer == 0 && quantity >= 4) ||
		(skill->integer == 1 && quantity >= 3) ||
		(skill->integer == 2 && quantity >= 2) ||
		(skill->integer == 3 && quantity >= 1) ||
		(skill->integer > 3))
		return false;

	if (coop->integer && !P_UseCoopInstancedItems() && (ent->item->flags & IF_STAY_COOP) && (quantity > 0))
		return false;

	if (deathmatch->integer) {
		if (g_quadhog->integer && ent->item->id == IT_POWERUP_QUAD)
			return true;

		if (match_powerupMinPlayerLock->integer > 0 && level.pop.num_playing_clients < match_powerupMinPlayerLock->integer) {
			if (level.time - other->client->lastPowerupMessageTime > 5_sec) {
				gi.LocClient_Print(other, PRINT_CENTER, ".There must be {}+ players in the match\nto pick this up :(", match_powerupMinPlayerLock->integer);
				other->client->lastPowerupMessageTime = level.time;
			}
			return false;
		}
	}

	return true;
}

bool Pickup_Powerup(gentity_t* ent, gentity_t* other) {
	if (!Pickup_AllowPowerupPickup(ent, other))
		return false;

	other->client->pers.inventory[ent->item->id]++;

	if (g_quadhog->integer && ent->item->id == IT_POWERUP_QUAD) {
		if (ent->item->use)
			ent->item->use(other, ent->item);
		FreeEntity(ent);
		return true;
	}

	bool is_dropped_from_death = ent->spawnFlags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER) && !ent->spawnFlags.has(SPAWNFLAG_ITEM_DROPPED);

	if (IsInstantItemsEnabled() || is_dropped_from_death) {
		bool use = false;
		GameTime t = (deathmatch->integer || !is_dropped_from_death) ? GameTime::from_sec(ent->count) : (ent->nextThink - level.time);
		switch (ent->item->id) {
		case IT_POWERUP_QUAD:
			quad_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_HASTE:
			haste_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_BATTLESUIT:
			protection_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_DOUBLE:
			double_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_INVISIBILITY:
			invisibility_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_REGEN:
			regeneration_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_EMPATHY_SHIELD:
			empathy_shield_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_ANTIGRAV_BELT:
			antigrav_belt_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_SPAWN_PROTECTION:
			use = true;
			break;
		default:
			break;
		}

		if (use && ent->item->use)
			ent->item->use(other, ent->item);
	}

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) && ec->client->sess.pc.follow_powerup) {
			ec->client->follow.target = other;
			ec->client->follow.update = true;
			ClientUpdateFollowers(ec);
		}
	}

	if (!is_dropped_from_death) {
		int count = 0;

		if (ent->count)
			count = ent->count;
		else {
			if (!ent->spawnFlags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER))
				count = 120;
			else
				count = ent->item->quantity;
		}

		HighValuePickupCounter(ent, other);
		SetRespawn(ent, GameTime::from_sec(count));
	}

	return true;
}

static bool Pickup_AllowTimedItemPickup(gentity_t* ent, gentity_t* other) {
	int quantity = other->client->pers.inventory[ent->item->id];
	if (deathmatch->integer) {
		if ((ent->item->id == IT_ADRENALINE || ent->item->id == IT_TELEPORTER) && quantity > 0)
			return false;
	}
	else {
		if ((skill->integer == 0 && quantity >= 3) ||
			(skill->integer == 1 && quantity >= 2) ||
			(skill->integer >= 2 && quantity >= 1))
			return false;

		if (coop->integer && !P_UseCoopInstancedItems() && (ent->item->flags & IF_STAY_COOP) && (quantity > 0))
			return false;
	}

	return true;
}

bool Pickup_TimedItem(gentity_t* ent, gentity_t* other) {
	if (!Pickup_AllowTimedItemPickup(ent, other))
		return false;

	other->client->pers.inventory[ent->item->id]++;

	bool is_dropped_from_death = ent->spawnFlags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER) && !ent->spawnFlags.has(SPAWNFLAG_ITEM_DROPPED);

	if ((IsInstantItemsEnabled() && !(ent->item->id == IT_ADRENALINE && match_holdableAdrenaline->integer)) || is_dropped_from_death)
		ent->item->use(other, ent->item);
	else {
		bool msg = false;
		if (ent->item->id == IT_ADRENALINE && !other->client->pers.holdable_item_msg_adren) {
			other->client->pers.holdable_item_msg_adren = msg = true;
		}
		else if (ent->item->id == IT_TELEPORTER && !other->client->pers.holdable_item_msg_tele) {
			other->client->pers.holdable_item_msg_tele = msg = true;
		}
		else if (ent->item->id == IT_DOPPELGANGER && !other->client->pers.holdable_item_msg_doppel) {
			other->client->pers.holdable_item_msg_doppel = msg = true;
		}
		if (msg)
			gi.LocClient_Print(other, PRINT_CENTER, "$map_this_item_must_be_activated_to_use_it");
	}

	if (!is_dropped_from_death) {
		HighValuePickupCounter(ent, other);
		SetRespawn(ent, GameTime::from_sec(ent->item->quantity));
	}
	return true;
}

//======================================================================

void Use_Defender(gentity_t* ent, Item* item) {
	if (!ent || !ent->client) {
		gi.Com_PrintFmt("Use_Defender: ent or ent->client is null\n");
		return;
	}

	if (ent->client->ownedSphere) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_only_one_sphere_time");
		return;
	}

	ent->client->pers.inventory[item->id]--;

	Defender_Launch(ent);
}

void Use_Hunter(gentity_t* ent, Item* item) {
	if (!ent || !ent->client) {
		gi.Com_PrintFmt("Use_Hunter: ent or ent->client is null\n");
		return;
	}

	if (ent->client->ownedSphere) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_only_one_sphere_time");
		return;
	}

	ent->client->pers.inventory[item->id]--;

	Hunter_Launch(ent);
}

void Use_Vengeance(gentity_t* ent, Item* item) {
	if (!ent || !ent->client) {
		gi.Com_PrintFmt("Use_Vengeance: ent or ent->client is null\n");
		return;
	}

	if (ent->client->ownedSphere) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_only_one_sphere_time");
		return;
	}

	ent->client->pers.inventory[item->id]--;

	Vengeance_Launch(ent);
}

bool Pickup_Sphere(gentity_t* ent, gentity_t* other) {
	if (!other || !other->client) {
		gi.Com_PrintFmt("Use_Vengeance: other or other->client is null\n");
		return false;
	}

	if (other->client->ownedSphere) {
		//		gi.LocClient_Print(other, PRINT_HIGH, "$g_only_one_sphere_customer");
		return false;
	}

	int quantity = other->client->pers.inventory[ent->item->id];
	if ((skill->integer == 1 && quantity >= 2) || (skill->integer >= 2 && quantity >= 1))
		return false;

	if ((coop->integer) && !P_UseCoopInstancedItems() && (ent->item->flags & IF_STAY_COOP) && (quantity > 0))
		return false;

	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, GameTime::from_sec(ent->item->quantity));

	if (deathmatch->integer && IsInstantItemsEnabled()) {
		if (ent->item->use)
			ent->item->use(other, ent->item);
		else
			gi.Com_Print("Powerup has no use function!\n");
	}

	return true;
}

//======================================================================

void Use_IR(gentity_t* ent, Item* item) {
	ent->client->pers.inventory[item->id]--;

	ent->client->PowerupTimer(PowerupTimer::IrGoggles) =
		max(level.time, ent->client->PowerupTimer(PowerupTimer::IrGoggles)) + 60_sec;

	gi.sound(ent, CHAN_ITEM, gi.soundIndex("misc/ir_start.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void Use_Nuke(gentity_t* ent, Item* item) {
	Vector3 forward, right, start;

	ent->client->pers.inventory[item->id]--;

	AngleVectors(ent->client->vAngle, forward, right, nullptr);

	start = ent->s.origin;
	fire_nuke(ent, start, forward, 100);
}

bool Pickup_Nuke(gentity_t* ent, gentity_t* other) {
	int quantity = other->client->pers.inventory[ent->item->id];

	if (quantity >= 1)
		return false;

	if (coop->integer && !P_UseCoopInstancedItems() && (ent->item->flags & IF_STAY_COOP) && (quantity > 0))
		return false;

	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, GameTime::from_sec(ent->item->quantity));

	return true;
}

//======================================================================

/*
===============
Use_Doppelganger
===============
*/
void Use_Doppelganger(gentity_t* ent, Item* item) {
	if (!ent || !item || !ent->client)
		return;

	// Must have one to use
	if (ent->client->pers.inventory[item->id] <= 0)
		return;

	constexpr float kCreateDist = 48.0f;
	constexpr float kSpawnClear = 32.0f;
	constexpr float kGroundUp = 64.0f;
	constexpr float kGrowSize = 24.0f;
	constexpr float kGrowTime = 48.0f;

	Vector3 forward, right, spawnPt;

	// Aim straight ahead using view yaw
	const Vector3 ang = { 0.0f, ent->client->vAngle[YAW], 0.0f };
	AngleVectors(ang, forward, right, nullptr);

	const Vector3 createPt = ent->s.origin + forward * kCreateDist;

	// Validate a clear spawn point in front of the player and on ground
	if (!FindSpawnPoint(createPt, ent->mins, ent->maxs, spawnPt, kSpawnClear))
		return;
	if (!CheckGroundSpawnPoint(spawnPt, ent->mins, ent->maxs, kGroundUp, false))
		return;

	// Consume, notify, effects, and spawn
	--ent->client->pers.inventory[item->id];
	UsedMessage(ent, item);

	SpawnGrow_Spawn(spawnPt, kGrowSize, kGrowTime);
	fire_doppelganger(ent, spawnPt, forward);
}

bool Pickup_Doppelganger(gentity_t* ent, gentity_t* other) {
	if (!deathmatch->integer)
		return false;

	if (other->client->pers.inventory[ent->item->id])
		return false;

	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, GameTime::from_sec(ent->item->quantity));

	return true;
}

//======================================================================

bool Pickup_General(gentity_t* ent, gentity_t* other) {
	if (other->client->pers.inventory[ent->item->id])
		return false;

	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, GameTime::from_sec(ent->item->quantity));

	return true;
}

bool Pickup_Ball(gentity_t* ent, gentity_t* other) {
	if (!other || !other->client)
		return false;

	other->client->pers.inventory[ent->item->id] = 1;
	ProBall::OnBallPickedUp(ent, other);
	Ball_OnPickup(ent, other);

	return true;
}

// Replace the old Drop_Weapon with this one.
void Drop_Weapon(gentity_t* ent, Item* item) {
	if (!item || !G_CanDropItem(*item)) {
		return;
	}

	if (CreateDroppedItem(ent, item, 1)) {
		ent->client->pers.inventory[item->id] = 0;
		// After dropping the current weapon, switch to the next best one.
		NoAmmoWeaponChange(ent, true);
	}
}

// Add this helper function near the top of g_items.cpp
static void P_ClearPowerup(gentity_t* ent, Item* item) {
	if (!ent || !ent->client || !item || !(item->flags & IF_POWERUP)) {
		return;
	}

	if (const auto timer = PowerupTimerForItem(item->id)) {
		ent->client->PowerupTimer(*timer) = 0_ms;
	}
	else if (const auto count = PowerupCountForItem(item->id)) {
		ent->client->PowerupCount(*count) = 0;
	}
}

// Replace the old Drop_General with this one.
void Drop_General(gentity_t* ent, Item* item) {
	if (g_quadhog->integer && item->id == IT_POWERUP_QUAD) {
		return;
	}

	if (CreateDroppedItem(ent, item, 1)) {
		ent->client->pers.inventory[item->id]--;
		// If the dropped item was an active powerup, clear its effect.
		P_ClearPowerup(ent, item);
	}
}

//======================================================================

void Use_Adrenaline(gentity_t* ent, Item* item) {
	ent->maxHealth += deathmatch->integer ? 5 : 1;

	if (ent->health < ent->maxHealth)
		ent->health = ent->maxHealth;

	gi.sound(ent, CHAN_ITEM, gi.soundIndex("items/m_health.wav"), 1, ATTN_NORM, 0);

	ent->client->pu_regen_time_blip = level.time + 100_ms;

	ent->client->pers.inventory[item->id]--;
	UsedMessage(ent, item);
}

bool Pickup_LegacyHead(gentity_t* ent, gentity_t* other) {
	other->maxHealth += 5;
	other->health += 5;

	SetRespawn(ent, GameTime::from_sec(ent->item->quantity));

	return true;
}

void CheckPowerArmorState(gentity_t* ent) {
	bool has_enough_cells;
	bool has_power_armor = ent->client->pers.inventory[IT_POWER_SCREEN] ||
		ent->client->pers.inventory[IT_POWER_SHIELD];

	return;
	if (!ent->client->pers.inventory[IT_AMMO_CELLS])

		has_enough_cells = false;
	else if (ent->client->pers.autoshield >= AUTO_SHIELD_AUTO)
		has_enough_cells = (ent->flags & FL_WANTS_POWER_ARMOR) && ent->client->pers.inventory[IT_AMMO_CELLS] > ent->client->pers.autoshield;
	else
		has_enough_cells = true;

	if (ent->flags & FL_POWER_ARMOR) {
		// ran out of cells for power armor / lost power armor
		if (!has_enough_cells || !has_power_armor) {
			ent->flags &= ~FL_POWER_ARMOR;
			gi.sound(ent, CHAN_AUTO, gi.soundIndex("misc/power2.wav"), 1, ATTN_NORM, 0);
		}
	}
	else if (ent->client->pers.autoshield != AUTO_SHIELD_MANUAL && has_enough_cells && !has_power_armor) {
		// special case for power armor, for auto-shields
		ent->flags |= FL_POWER_ARMOR;
		gi.sound(ent, CHAN_AUTO, gi.soundIndex("misc/power1.wav"), 1, ATTN_NORM, 0);
	}
}

static item_id_t G_AmmoConvertID(item_id_t original_id) {
	item_id_t new_id = original_id;
	if (new_id == IT_AMMO_SHELLS_LARGE || new_id == IT_AMMO_SHELLS_SMALL)
		new_id = IT_AMMO_SHELLS;
	else if (new_id == IT_AMMO_BULLETS_LARGE || new_id == IT_AMMO_BULLETS_SMALL)
		new_id = IT_AMMO_BULLETS;
	else if (new_id == IT_AMMO_CELLS_LARGE || new_id == IT_AMMO_CELLS_SMALL)
		new_id = IT_AMMO_CELLS;
	else if (new_id == IT_AMMO_ROCKETS_SMALL)
		new_id = IT_AMMO_ROCKETS;
	else if (new_id == IT_AMMO_SLUGS_LARGE || new_id == IT_AMMO_SLUGS_SMALL)
		new_id = IT_AMMO_SLUGS;

	return new_id;
}

void G_CapAllAmmo(gentity_t* ent) {
	if (!ent || !ent->client)
		return;

	if (ent->client->pers.inventory[IT_AMMO_SHELLS] > ent->client->pers.ammoMax[static_cast<int>(AmmoID::Shells)])
		ent->client->pers.inventory[IT_AMMO_SHELLS] = ent->client->pers.ammoMax[static_cast<int>(AmmoID::Shells)];
	if (ent->client->pers.inventory[IT_AMMO_BULLETS] > ent->client->pers.ammoMax[static_cast<int>(AmmoID::Bullets)])
		ent->client->pers.inventory[IT_AMMO_BULLETS] = ent->client->pers.ammoMax[static_cast<int>(AmmoID::Bullets)];
	if (ent->client->pers.inventory[IT_AMMO_GRENADES] > ent->client->pers.ammoMax[static_cast<int>(AmmoID::Grenades)])
		ent->client->pers.inventory[IT_AMMO_GRENADES] = ent->client->pers.ammoMax[static_cast<int>(AmmoID::Grenades)];
	if (ent->client->pers.inventory[IT_AMMO_ROCKETS] > ent->client->pers.ammoMax[static_cast<int>(AmmoID::Rockets)])
		ent->client->pers.inventory[IT_AMMO_ROCKETS] = ent->client->pers.ammoMax[static_cast<int>(AmmoID::Rockets)];
	if (ent->client->pers.inventory[IT_AMMO_CELLS] > ent->client->pers.ammoMax[static_cast<int>(AmmoID::Cells)])
		ent->client->pers.inventory[IT_AMMO_CELLS] = ent->client->pers.ammoMax[static_cast<int>(AmmoID::Cells)];
	if (ent->client->pers.inventory[IT_AMMO_SLUGS] > ent->client->pers.ammoMax[static_cast<int>(AmmoID::Slugs)])
		ent->client->pers.inventory[IT_AMMO_SLUGS] = ent->client->pers.ammoMax[static_cast<int>(AmmoID::Slugs)];
	if (ent->client->pers.inventory[IT_AMMO_TRAP] > ent->client->pers.ammoMax[static_cast<int>(AmmoID::Traps)])
		ent->client->pers.inventory[IT_AMMO_TRAP] = ent->client->pers.ammoMax[static_cast<int>(AmmoID::Traps)];
	if (ent->client->pers.inventory[IT_AMMO_FLECHETTES] > ent->client->pers.ammoMax[static_cast<int>(AmmoID::Flechettes)])
		ent->client->pers.inventory[IT_AMMO_FLECHETTES] = ent->client->pers.ammoMax[static_cast<int>(AmmoID::Flechettes)];
	if (ent->client->pers.inventory[IT_AMMO_ROUNDS] > ent->client->pers.ammoMax[static_cast<int>(AmmoID::Rounds)])
		ent->client->pers.inventory[IT_AMMO_ROUNDS] = ent->client->pers.ammoMax[static_cast<int>(AmmoID::Rounds)];
	if (ent->client->pers.inventory[IT_AMMO_TESLA] > ent->client->pers.ammoMax[static_cast<int>(AmmoID::TeslaMines)])
		ent->client->pers.inventory[IT_AMMO_TESLA] = ent->client->pers.ammoMax[static_cast<int>(AmmoID::TeslaMines)];
}

static inline bool G_AddAmmoAndCap(gentity_t* other, item_id_t id, int32_t max, int32_t quantity) {
	item_id_t new_id = G_AmmoConvertID(id);

	if (other->client->pers.inventory[new_id] == AMMO_INFINITE)
		return false;

	if (other->client->pers.inventory[new_id] >= max)
		return false;

	if (quantity == AMMO_INFINITE) {
		other->client->pers.inventory[new_id] = AMMO_INFINITE;
	}
	else {
		other->client->pers.inventory[new_id] += quantity;
		if (other->client->pers.inventory[new_id] > max)
			other->client->pers.inventory[new_id] = max;
	}

	if (new_id == IT_AMMO_CELLS)
		CheckPowerArmorState(other);
	return true;
}

static inline void G_AdjustAmmoCap(gentity_t* other, AmmoID ammo, int16_t new_max) {
	other->client->pers.ammoMax[static_cast<int>(ammo)] = max(other->client->pers.ammoMax[static_cast<int>(ammo)], new_max);
}

static inline bool G_AddAmmoAndCapQuantity(gentity_t* other, AmmoID ammo, int32_t quantity) {
	Item* item = GetItemByAmmo(ammo);

	if (!item) {
		gi.Com_PrintFmt("Missing item for ammo {}\n", static_cast<int>(ammo));
		return false;
	}
	return G_AddAmmoAndCap(other, item->id, other->client->pers.ammoMax[static_cast<int>(ammo)], quantity);
}

[[maybe_unused]] static inline bool G_AddIDAmmoAndCapQuantity(gentity_t* other, item_id_t ammoID) {
	Item* item = GetItemByAmmo((AmmoID)itemList[ammoID].tag);

	if (!item) {
		gi.Com_PrintFmt("Missing item for ammo {}\n", static_cast<int>(ammoID));
		return 0;
	}
	return G_AddAmmoAndCap(other, ammoID, other->client->pers.ammoMax[itemList[static_cast<int>(ammoID)].tag], ammoStats[game.ruleset][item->tag].ammo_pu);
}

bool Pickup_Bandolier(gentity_t* ent, gentity_t* other) {
	// Ensure the entity picking up the item is a valid player.
	if (!other || !other->client) {
		return false;
	}

	// A bandolier increases max ammo capacity and gives a bonus for each ammo type.
	for (int i = 0; i < static_cast<int>(AmmoID::_Total); i++) {
		auto current_ammo_id = static_cast<AmmoID>(i);

		// Adjust the player's max ammo capacity to the bandolier level.
		G_AdjustAmmoCap(other, current_ammo_id, ammoStats[game.ruleset][i].max[1]);

		// Add the corresponding amount of ammo for a bandolier pickup.
		G_AddAmmoAndCapQuantity(other, current_ammo_id, ammoStats[game.ruleset][i].bando_pu);
	}

	// Log the high-value pickup and schedule the item to respawn.
	HighValuePickupCounter(ent, other);
	SetRespawn(ent, GameTime::from_sec(ent->item->quantity));

	return true;
}

void G_CheckAutoSwitch(gentity_t* ent, Item* item, bool is_new);
bool Pickup_Pack(gentity_t* ent, gentity_t* other) {
	// Ensure the entity picking up the item is a valid player.
	if (!other || !other->client) {
		return false;
	}

	// Handle the special case for Quake 1-style deathmatch backpacks,
	// which contain a specific weapon and ammo counts.
	if (ent->pack_weapon) {
		// Grant the ammo stored in the dropped pack.
		for (int i = 0; i < static_cast<int>(AmmoID::_Total); i++) {
			G_AddAmmoAndCapQuantity(other, static_cast<AmmoID>(i), ent->pack_ammo_count[i]);
		}

		// Check if the weapon is new for the player before adding it.
		const bool is_new_weapon = !other->client->pers.inventory[ent->pack_weapon->id];
		other->client->pers.inventory[ent->pack_weapon->id]++;

		// Trigger a weapon switch if appropriate.
		G_CheckAutoSwitch(other, ent->pack_weapon, is_new_weapon);
		return true;
	}

	// Handle the standard ammo pack pickup.
	for (int i = 0; i < static_cast<int>(AmmoID::_Total); i++) {
		// Increase the player's max ammo capacity for each type.
		G_AdjustAmmoCap(other, static_cast<AmmoID>(i), ammoStats[game.ruleset][i].max[2]);
		// Add a standard amount of ammo for each type.
		G_AddAmmoAndCapQuantity(other, static_cast<AmmoID>(i), ammoStats[game.ruleset][i].ammopack_pu);
	}

	// Special check to auto-switch to grenades if they are newly acquired.
	if (Item* grenade_item = GetItemByIndex(IT_AMMO_GRENADES)) {
		const bool is_new_grenade = !other->client->pers.inventory[IT_AMMO_GRENADES];
		G_CheckAutoSwitch(other, grenade_item, is_new_grenade);
	}

	// Log the high-value pickup and set the item to respawn.
	HighValuePickupCounter(ent, other);
	SetRespawn(ent, GameTime::from_sec(ent->item->quantity));

	return true;
}

/*
===============
Drop_Backpack
===============
*/
void Drop_Backpack(gentity_t* ent) {
	if (!deathmatch->integer)

		if (notRS(Quake1))
			return;

	if (Game::Is(GameType::Horde))
		return;

	if (!ent || !ent->client)
		return;

	gentity_t* dropped = Drop_Item(ent, &itemList[IT_PACK]);
	dropped->spawnFlags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	dropped->svFlags &= ~SVF_INSTANCED;

	dropped->pack_weapon = ent->client->pers.weapon;
	if (!dropped->pack_weapon) {
		FreeEntity(dropped);
		return;
	}

	int i;
	bool drop = false;

	for (i = IT_AMMO_SHELLS; i <= IT_AMMO_ROUNDS; i++) {
		if (ent->client->pers.inventory[i]) {
			int ammo = itemList[i].tag;

			if (ammo < 0 || ammo >= static_cast<int>(AmmoID::_Total))
				break;

			drop = true;
			dropped->pack_ammo_count[ammo] = ent->client->pers.inventory[i];
		}
	}

	if (!drop) {
		FreeEntity(dropped);
		return;
	}
};


//======================================================================

static void Use_Powerup_BroadcastMsg(gentity_t* ent, Item* item, const char* sound_name, const char* announcer_name) {
	if (!deathmatch->integer)
		return;

	if (g_quadhog->integer && item->id == IT_POWERUP_QUAD) {
		gi.LocBroadcast_Print(PRINT_CENTER, "{} is the Quad Hog!\n", ent->client->sess.netName);
	}

	gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundIndex(sound_name), 1, ATTN_NONE, 0);
	AnnouncerSound(world, announcer_name);
}

void Use_Quad(gentity_t* ent, Item* item) {
	GameTime timeout;

	ent->client->pers.inventory[item->id]--;

	if (quad_drop_timeout_hack) {
		timeout = quad_drop_timeout_hack;
		quad_drop_timeout_hack = 0_ms;
	}
	else {
		timeout = 30_sec;
	}

	auto& quadTime = ent->client->PowerupTimer(PowerupTimer::QuadDamage);
	quadTime = max(level.time, quadTime) + timeout;

	Use_Powerup_BroadcastMsg(ent, item, "items/damage.wav", "quad_damage");
}
// =====================================================================

void Use_Haste(gentity_t* ent, Item* item) {
	GameTime timeout;

	ent->client->pers.inventory[item->id]--;

	if (haste_drop_timeout_hack) {
		timeout = haste_drop_timeout_hack;
		haste_drop_timeout_hack = 0_ms;
	}
	else {
		timeout = 30_sec;
	}

	auto& hasteTime = ent->client->PowerupTimer(PowerupTimer::Haste);
	hasteTime = max(level.time, hasteTime) + timeout;

	Use_Powerup_BroadcastMsg(ent, item, "items/quadfire1.wav", "haste");
}

//======================================================================

void Use_Double(gentity_t* ent, Item* item) {
	GameTime timeout;

	ent->client->pers.inventory[item->id]--;

	if (double_drop_timeout_hack) {
		timeout = double_drop_timeout_hack;
		double_drop_timeout_hack = 0_ms;
	}
	else {
		timeout = 30_sec;
	}

	auto& doubleTime = ent->client->PowerupTimer(PowerupTimer::DoubleDamage);
	doubleTime = max(level.time, doubleTime) + timeout;

	Use_Powerup_BroadcastMsg(ent, item, "misc/ddamage1.wav", "damage");
}

//======================================================================

void Use_Breather(gentity_t* ent, Item* item) {
	ent->client->pers.inventory[item->id]--;
	auto& rebreatherTime = ent->client->PowerupTimer(PowerupTimer::Rebreather);
	rebreatherTime = max(level.time, rebreatherTime) + 45_sec;
}

//======================================================================

void Use_EnviroSuit(gentity_t* ent, Item* item) {
	ent->client->pers.inventory[item->id]--;
	auto& enviroTime = ent->client->PowerupTimer(PowerupTimer::EnviroSuit);
	enviroTime = max(level.time, enviroTime) + 30_sec;
}

//======================================================================

void Use_EmpathyShield(gentity_t* ent, Item* item) {
	ent->client->pers.inventory[item->id]--;
	auto& empathyTime = ent->client->PowerupTimer(PowerupTimer::EmpathyShield);
	empathyTime = max(level.time, empathyTime) + 30_sec;

	Use_Powerup_BroadcastMsg(ent, item, "items/empathy_use.wav", "empathy_shield");
}

//======================================================================

void Use_AntiGravBelt(gentity_t* ent, Item* item) {
	ent->client->pers.inventory[item->id]--;
	auto& antiGravTime = ent->client->PowerupTimer(PowerupTimer::AntiGravBelt);
	antiGravTime = max(level.time, antiGravTime) + 45_sec;
}

//======================================================================

void Use_BattleSuit(gentity_t* ent, Item* item) {
	GameTime timeout;

	ent->client->pers.inventory[item->id]--;

	if (protection_drop_timeout_hack) {
		timeout = protection_drop_timeout_hack;
		protection_drop_timeout_hack = 0_ms;
	}
	else {
		timeout = 30_sec;
	}

	auto& battleSuitTime = ent->client->PowerupTimer(PowerupTimer::BattleSuit);
	battleSuitTime = max(level.time, battleSuitTime) + timeout;

	Use_Powerup_BroadcastMsg(ent, item, "items/protect.wav", "battlesuit");
}

//======================================================================

void Use_Spawn_Protection(gentity_t* ent, Item* item) {
	GameTime timeout = 3_sec;

	ent->client->pers.inventory[item->id]--;

	auto& spawnProtectionTime = ent->client->PowerupTimer(PowerupTimer::SpawnProtection);
	spawnProtectionTime = max(level.time, spawnProtectionTime) + timeout;
}

//======================================================================

void Use_Regeneration(gentity_t* ent, Item* item) {
	GameTime timeout;

	ent->client->pers.inventory[item->id]--;

	if (regeneration_drop_timeout_hack) {
		timeout = regeneration_drop_timeout_hack;
		regeneration_drop_timeout_hack = 0_ms;
	}
	else {
		timeout = 30_sec;
	}

	auto& regenTime = ent->client->PowerupTimer(PowerupTimer::Regeneration);
	regenTime = max(level.time, regenTime) + timeout;

	Use_Powerup_BroadcastMsg(ent, item, "items/protect.wav", "regeneration");
}

void Use_Invisibility(gentity_t* ent, Item* item) {
	GameTime timeout;

	ent->client->pers.inventory[item->id]--;

	if (invisibility_drop_timeout_hack) {
		timeout = invisibility_drop_timeout_hack;
		invisibility_drop_timeout_hack = 0_ms;
	}
	else {
		timeout = 30_sec;
	}

	auto& invisTime = ent->client->PowerupTimer(PowerupTimer::Invisibility);
	invisTime = max(level.time, invisTime) + timeout;

	Use_Powerup_BroadcastMsg(ent, item, "items/protect.wav", "invisibility");
}

//======================================================================

void Use_Silencer(gentity_t* ent, Item* item) {
	ent->client->pers.inventory[item->id]--;
	ent->client->PowerupCount(PowerupCount::SilencerShots) += 30;
}

//======================================================================

bool Pickup_Key(gentity_t* ent, gentity_t* other) {
	if (coop->integer) {
		if (ent->item->id == IT_KEY_POWER_CUBE || ent->item->id == IT_KEY_EXPLOSIVE_CHARGES) {
			if (other->client->pers.powerCubes & ((ent->spawnFlags & SPAWNFLAG_EDITOR_MASK).value >> 8))
				return false;
			other->client->pers.inventory[ent->item->id]++;
			other->client->pers.powerCubes |= ((ent->spawnFlags & SPAWNFLAG_EDITOR_MASK).value >> 8);
		}
		else {
			if (other->client->pers.inventory[ent->item->id])
				return false;
			other->client->pers.inventory[ent->item->id] = 1;
		}
		return true;
	}
	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, 30_sec);
	return true;
}

//======================================================================

bool Add_Ammo(gentity_t* ent, Item* item, int count) {
	if (!ent->client || item->tag < static_cast<int>(AmmoID::Bullets) || item->tag >= static_cast<int>(AmmoID::_Total))
		return false;

	return G_AddAmmoAndCap(ent, item->id, ent->client->pers.ammoMax[item->tag], ammoStats[game.ruleset][item->tag].ammo_pu);
}

// we just got weapon `item`, check if we should switch to it
void G_CheckAutoSwitch(gentity_t* ent, Item* item, bool is_new) {
	// already using or switching to
	if (ent->client->pers.weapon == item ||
		ent->client->weapon.pending == item)
		return;
	// need ammo
	else if (item->ammo) {
		int32_t required_ammo = (item->flags & IF_AMMO) ? 1 : item->quantity;

		if (ent->client->pers.inventory[item->ammo] < required_ammo)
			return;
	}

	const WeaponAutoSwitch autoswitch = ent->client->pers.autoswitch;
	if (autoswitch == WeaponAutoSwitch::Never)
		return;

	if ((item->flags & IF_AMMO) && autoswitch == WeaponAutoSwitch::Always_No_Ammo)
		return;

	bool allowSwitch = true;

	if (autoswitch == WeaponAutoSwitch::Smart) {
		// smartness algorithm: in DM, we will always switch if we have the blaster out
		// otherwise leave our active weapon alone
		if (deathmatch->integer) {
			// wor: make it smarter!
			// switch to better weapons
			if (ent->client->pers.weapon) {
				switch (ent->client->pers.weapon->id) {
				case IT_WEAPON_CHAINFIST:
					// always switch from chainfist
					break;
				case IT_WEAPON_BLASTER:
					// should never auto-switch to chainfist
					if (item->id == IT_WEAPON_CHAINFIST)
						return;
					break;
				case IT_WEAPON_SHOTGUN:
					if (RS(Quake1)) {
						// always switch from sg in Q1
					}
					else {
						// switch only to SSG
						if (item->id != IT_WEAPON_SSHOTGUN)
							allowSwitch = false;
					}
					break;
				case IT_WEAPON_MACHINEGUN:
					if (RS(Quake3Arena)) {
						// always switch from mg in Q3A
					}
					else {
						// switch only to CG
						if (item->id != IT_WEAPON_CHAINGUN)
							allowSwitch = false;
					}
					break;
				default:
					// otherwise don't switch!
					allowSwitch = false;
					break;
				}
			}
		}
		// in SP, only switch if it's a new weapon, or we have the blaster out
		else if (!deathmatch->integer && !(ent->client->pers.weapon && ent->client->pers.weapon->id == IT_WEAPON_BLASTER) && !is_new) {
			allowSwitch = false;
		}
	}

	if (!allowSwitch)
		return;

	Client_RebuildWeaponPreferenceOrder(*ent->client);
	const auto& order = ent->client->sess.weaponPrefOrder;

	auto priority_of = [&order](item_id_t id) {
		if (!id)
			return std::numeric_limits<size_t>::max();

		for (size_t i = 0; i < order.size(); ++i) {
			if (order[i] == id)
				return i;
		}

		return std::numeric_limits<size_t>::max();
		};

	const size_t pickupPriority = priority_of(item->id);
	const size_t currentPriority = ent->client->pers.weapon ? priority_of(ent->client->pers.weapon->id) : std::numeric_limits<size_t>::max();

	if (pickupPriority >= currentPriority)
		return;

	// switch!
	ent->client->weapon.pending = item;
}

bool Pickup_Ammo(gentity_t* ent, gentity_t* other) {
	bool weapon = !!(ent->item->flags & IF_WEAPON);
	int	 count, oldcount;

	if (weapon && InfiniteAmmoOn(ent->item))
		count = AMMO_INFINITE;
	else if (ent->count)
		count = ent->count;
	else {
		if (ent->item->id == IT_AMMO_SLUGS) {
			switch (game.ruleset) {
			case Ruleset::Quake1:
				count = 1;
				break;
			case Ruleset::Quake3Arena:
				count = 10;
				break;
			default:
				count = 6;
				break;
			}
		}
		else {
			count = ent->item->quantity;
		}
	}

	oldcount = other->client->pers.inventory[G_AmmoConvertID(ent->item->id)];

	if (!Add_Ammo(other, ent->item, count))
		return false;

	if (weapon)
		G_CheckAutoSwitch(other, ent->item, !oldcount);

	SetRespawn(ent, 30_sec);
	return true;
}

void Drop_Ammo(gentity_t* ent, Item* item)
{
	if (InfiniteAmmoOn(item)) {
		return;
	}

	// Determine the amount of ammo to drop, ensuring we don't drop more than we have.
	int quantity = ammoStats[game.ruleset][item->tag].ammo_pu;
	int current_ammo = ent->client->pers.inventory[item->id];

	if (current_ammo <= 0) {
		return;
	}

	int drop_count = std::min(quantity, current_ammo);

	// Create the dropped item using the new helper function.
	gentity_t* dropped = CreateDroppedItem(ent, item, drop_count);
	if (!dropped) {
		return; // Failed to spawn the item.
	}

	// --- Ammo Subtraction Fix ---
	// Directly and safely subtract the ammo from the player's inventory.
	ent->client->pers.inventory[item->id] -= drop_count;

	// If this was the last of the ammo for the current weapon, switch away.
	if (ent->client->pers.inventory[item->id] < 1) {
		if (item == ent->client->pers.weapon || item == ent->client->weapon.pending) {
			NoAmmoWeaponChange(ent, true);
		}
	}

	// For Power Armor cells, update the armor state.
	if (item->tag == static_cast<int>(AmmoID::Cells)) {
		CheckPowerArmorState(ent);
	}
}

//======================================================================

static THINK(MegaHealth_think) (gentity_t* self) -> void {
	int32_t health = self->maxHealth;
	if (health < self->owner->maxHealth)
		health = self->owner->maxHealth;

	if (self->health > 0 && self->owner->health > health && !Tech_HasRegeneration(self->owner)) {
		self->nextThink = level.time + 1_sec;
		self->owner->health--;
		self->health--;
		return;
	}

	SetRespawn(self, 20_sec);

	if (self->spawnFlags.has(SPAWNFLAG_ITEM_DROPPED))
		FreeEntity(self);
}

bool Pickup_Health(gentity_t* ent, gentity_t* other) {
	int health_flags = (ent->style ? ent->style : ent->item->tag);

	if (!(health_flags & HEALTH_IGNORE_MAX))
		if (other->health >= other->maxHealth)
			return false;

	int count = ent->count ? ent->count : ent->item->quantity;
	int max = RS(Quake3Arena) ? other->maxHealth * 2 : 250;

	if (deathmatch->integer && other->health >= max && count > 25)
		return false;

	if (RS(Quake3Arena) && !ent->count) {
		switch (ent->item->id) {
		case IT_HEALTH_SMALL:
			count = 5;
			break;
		case IT_HEALTH_MEDIUM:
			count = 25;
			break;
		case IT_HEALTH_LARGE:
			count = 50;
			break;
		default:
			break;
		}
	}

	other->health += count;

	if (Game::Has(GameFlags::CTF) && other->health > max && count > 25)
		other->health = max;

	if (!(health_flags & HEALTH_IGNORE_MAX))
		if (other->health > other->maxHealth)
			other->health = other->maxHealth;

	if (RS(Quake3Arena) && (health_flags & HEALTH_IGNORE_MAX)) {
		if (other->health > other->maxHealth * 2)
			other->health = other->maxHealth * 2;
	}

	if (!(RS(Quake3Arena)) && (ent->item->tag & HEALTH_TIMED) && !Tech_HasRegeneration(other)) {
		if (!deathmatch->integer) {
			// mega health doesn't need to be special in SP
			// since it never respawns.
			other->client->pers.megaTime = 5_sec;
		}
		else {
			ent->think = MegaHealth_think;
			ent->nextThink = level.time + 5_sec;
			ent->owner = other;
			ent->flags |= FL_RESPAWN;
			ent->svFlags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;
			HighValuePickupCounter(ent, other);

			//muff: set health as amount to rot player by, maxHealth is the limit of the player's health to rot to

			ent->health = ent->owner->health - ent->owner->maxHealth;
			ent->maxHealth = ent->owner->maxHealth;
		}
	}
	else {
		SetRespawn(ent, RS(Quake3Arena) ? 60_sec : 30_sec);
	}

	return true;
}

//======================================================================

item_id_t ArmorIndex(gentity_t* ent) {
	if (ent->svFlags & SVF_MONSTER)
		return ent->monsterInfo.armorType;

	if (ent->client) {
		if (RS(Quake3Arena)) {
			if (ent->client->pers.inventory[IT_ARMOR_JACKET] > 0 ||
				ent->client->pers.inventory[IT_ARMOR_COMBAT] > 0 ||
				ent->client->pers.inventory[IT_ARMOR_BODY] > 0)
				return IT_ARMOR_COMBAT;
		}
		else {
			if (ent->client->pers.inventory[IT_ARMOR_JACKET] > 0)
				return IT_ARMOR_JACKET;
			else if (ent->client->pers.inventory[IT_ARMOR_COMBAT] > 0)
				return IT_ARMOR_COMBAT;
			else if (ent->client->pers.inventory[IT_ARMOR_BODY] > 0)
				return IT_ARMOR_BODY;
		}
	}

	return IT_NULL;
}

static bool Pickup_Armor_Q3(gentity_t* ent, gentity_t* other, int32_t base_count) {
	if (other->client->pers.inventory[IT_ARMOR_COMBAT] >= other->client->pers.maxHealth * 2)
		return false;

	if (ent->item->id == IT_ARMOR_SHARD && !ent->count)
		base_count = 5;

	other->client->pers.inventory[IT_ARMOR_COMBAT] += base_count;
	if (other->client->pers.inventory[IT_ARMOR_COMBAT] > other->client->pers.maxHealth * 2)
		other->client->pers.inventory[IT_ARMOR_COMBAT] = other->client->pers.maxHealth * 2;

	other->client->pers.inventory[IT_ARMOR_SHARD] = 0;
	other->client->pers.inventory[IT_ARMOR_JACKET] = 0;
	other->client->pers.inventory[IT_ARMOR_BODY] = 0;

	HighValuePickupCounter(ent, other);
	SetRespawn(ent, 25_sec);

	return true;
}

bool Pickup_Armor(gentity_t* ent, gentity_t* other) {
	item_id_t			 old_armor_index;
	const gitem_armor_t* oldinfo;
	const gitem_armor_t* newinfo;
	int					 newcount;
	float				 salvage;
	int					 salvagecount;

	// get info on new armor
	newinfo = &armor_stats[game.ruleset][ent->item->quantity];

	// [Paril-KEX] for g_start_items
	int32_t base_count = ent->count ? ent->count : newinfo ? newinfo->base_count : 0;

	if (RS(Quake3Arena))
		return Pickup_Armor_Q3(ent, other, base_count);

	old_armor_index = ArmorIndex(other);

	// handle armor shards specially
	if (ent->item->id == IT_ARMOR_SHARD) {
		if (!old_armor_index)
			other->client->pers.inventory[IT_ARMOR_JACKET] = base_count;
		else
			other->client->pers.inventory[old_armor_index] += base_count;
	}
	// if player has no armor, just use it
	else if (!old_armor_index) {
		other->client->pers.inventory[ent->item->id] = base_count;
	}

	// use the better armor
	else {
		// get info on old armor
		if (old_armor_index == IT_ARMOR_JACKET)
			oldinfo = &armor_stats[game.ruleset][Armor::Jacket];
		else if (old_armor_index == IT_ARMOR_COMBAT)
			oldinfo = &armor_stats[game.ruleset][Armor::Combat];
		else
			oldinfo = &armor_stats[game.ruleset][Armor::Body];

		if (newinfo->normal_protection > oldinfo->normal_protection) {
			// calc new armor values
			salvage = oldinfo->normal_protection / newinfo->normal_protection;
			salvagecount = (int)(salvage * other->client->pers.inventory[old_armor_index]);
			newcount = base_count + salvagecount;
			if (newcount > newinfo->max_count)
				newcount = newinfo->max_count;

			// zero count of old armor so it goes away
			other->client->pers.inventory[old_armor_index] = 0;

			// change armor to new item with computed value
			other->client->pers.inventory[ent->item->id] = newcount;
		}
		else {
			// calc new armor values
			salvage = newinfo->normal_protection / oldinfo->normal_protection;
			salvagecount = (int)(salvage * base_count);
			newcount = other->client->pers.inventory[old_armor_index] + salvagecount;
			if (newcount > oldinfo->max_count)
				newcount = oldinfo->max_count;

			if (RS(Quake1) && other->client->pers.inventory[old_armor_index] * oldinfo->normal_protection >= newcount * newinfo->normal_protection)
				return false;

			// if we're already maxed out then we don't need the new armor
			if (other->client->pers.inventory[old_armor_index] >= newcount)
				return false;

			// update current armor value
			other->client->pers.inventory[old_armor_index] = newcount;
		}
	}

	switch (ent->item->id) {
	case IT_ARMOR_COMBAT:
	case IT_ARMOR_BODY:
		HighValuePickupCounter(ent, other);
		break;
	default:
		break;
	}

	HighValuePickupCounter(ent, other);
	SetRespawn(ent, 20_sec);

	return true;
}

//======================================================================

item_id_t PowerArmorType(gentity_t* ent) {
	if (!ent->client)
		return IT_NULL;

	if (!(ent->flags & FL_POWER_ARMOR))
		return IT_NULL;

	if (ent->client->pers.inventory[IT_POWER_SHIELD] > 0)
		return IT_POWER_SHIELD;

	if (ent->client->pers.inventory[IT_POWER_SCREEN] > 0)
		return IT_POWER_SCREEN;

	return IT_NULL;
}

void Use_PowerArmor(gentity_t* ent, Item* item) {
	if (ent->flags & FL_POWER_ARMOR) {
		ent->flags &= ~(FL_POWER_ARMOR | FL_WANTS_POWER_ARMOR);
		gi.sound(ent, CHAN_AUTO, gi.soundIndex("misc/power2.wav"), 1, ATTN_NORM, 0);
	}
	else {
		if (!ent->client->pers.inventory[IT_AMMO_CELLS]) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_no_cells_power_armor");
			return;
		}

		ent->flags |= FL_POWER_ARMOR;

		if (ent->client->pers.autoshield != AUTO_SHIELD_MANUAL &&
			ent->client->pers.inventory[IT_AMMO_CELLS] > ent->client->pers.autoshield)
			ent->flags |= FL_WANTS_POWER_ARMOR;

		gi.sound(ent, CHAN_AUTO, gi.soundIndex("misc/power1.wav"), 1, ATTN_NORM, 0);
	}
}

bool Pickup_PowerArmor(gentity_t* ent, gentity_t* other) {
	other->client->pers.inventory[ent->item->id]++;

	HighValuePickupCounter(ent, other);
	SetRespawn(ent, GameTime::from_sec(ent->item->quantity));

	// give some cells as a bonus
	G_AddAmmoAndCapQuantity(other, AmmoID::Cells, 20);

	if (deathmatch->integer) {
		if (!(ent->spawnFlags & SPAWNFLAG_ITEM_DROPPED))
			SetRespawn(ent, GameTime::from_sec(ent->item->quantity));
		// auto-use for DM only if we didn't already have one
		if (!other->client->pers.inventory[ent->item->id])
			CheckPowerArmorState(other);
	}
	else
		CheckPowerArmorState(other);

	return true;
}

void Drop_PowerArmor(gentity_t* ent, Item* item) {
	if ((ent->flags & FL_POWER_ARMOR) && (ent->client->pers.inventory[item->id] == 1))
		Use_PowerArmor(ent, item);
	Drop_General(ent, item);
}

//======================================================================

/*
===============
Entity_IsVisibleToPlayer
===============
*/
bool Entity_IsVisibleToPlayer(gentity_t* ent, gentity_t* player) {
	// Q2Eaks make eyecam chase target invisible, but keep other client visible
	if (g_eyecam->integer && player->client->follow.target && ent == player->client->follow.target)
		return false;
	else if (ent->client)
		return true;

	const int index = static_cast<int>(player->s.number) - 1;

	if (index < 0 || index >= MAX_CLIENTS)
		return false;

	return !ent->itemPickedUpBy[index];
}

/*
===============
IsInstancedCoop
===============
*/
static inline bool IsInstancedCoop() {
	return coop->integer && P_UseCoopInstancedItems();
}

/*
===============
IsTeamPingItem
===============
*/
static inline bool IsTeamPingItem(int id) {
	switch (id) {
	case IT_ARMOR_BODY:
	case IT_POWER_SCREEN:
	case IT_POWER_SHIELD:
	case IT_ADRENALINE:
	case IT_HEALTH_MEGA:
	case IT_POWERUP_QUAD:
	case IT_POWERUP_DOUBLE:
	case IT_POWERUP_BATTLESUIT:
	case IT_POWERUP_HASTE:
	case IT_POWERUP_INVISIBILITY:
	case IT_POWERUP_REGEN:
	case IT_FLAG_RED:
	case IT_FLAG_BLUE:
	case IT_FLAG_NEUTRAL:
		return true;
	default:
		return false;
	}
}

/*
===============
BroadcastTeamPickupPing
Sends POI ping and TTS line to teammates/spectators following teammates
===============
*/
static void BroadcastTeamPickupPing(gentity_t* picker, const Item* it) {
	if (!picker || !picker->client || !it)
		return;

	const uint32_t key = GetUnicastKey();

	for (auto ec : active_clients()) {
		if (!ec || !ec->inUse || !ec->client)
			continue;

		// do not notify the picker
		if (ec == picker)
			continue;

		gclient_t* pcl = ec->client;

		bool sameTeam = false;
		if (ClientIsPlaying(pcl)) {
			sameTeam = OnSameTeam(picker, ec);
		}
		else {
			gentity_t* target = pcl->follow.target;
			sameTeam = (target && target->inUse && target->client && OnSameTeam(picker, target));
		}
		if (!sameTeam)
			continue;

		gi.WriteByte(svc_poi);
		gi.WriteShort(POI_PING + (picker->s.number - 1));
		gi.WriteShort(5000);
		gi.WritePosition(picker->s.origin);
		gi.WriteShort(gi.imageIndex(it->icon));
		gi.WriteByte(215);
		gi.WriteByte(POI_FLAG_NONE);
		gi.unicast(ec, false);
		gi.localSound(ec, CHAN_AUTO, gi.soundIndex("misc/help_marker.wav"), 1.0f, ATTN_NONE, 0.0f, key);

		// Build message without G_Fmt to avoid temporary-view pitfalls
		std::string msg;
		if (pcl->sess.team != Team::Spectator)
			msg += "[TEAM]: ";
		msg += (picker->client ? picker->client->sess.netName : "unknown");
		msg += " got the ";
		msg += it->useName ? it->useName : "item";
		msg += ".\n";
		gi.LocClient_Print(ec, PRINT_TTS, msg.c_str());
	}
}

/*
===============
ShouldRemoveItemAfterPickup
Encapsulates the post-pickup removal rules
===============
*/
static bool ShouldRemoveItemAfterPickup(const gentity_t* ent, const Item* it) {
	const bool dm = deathmatch->integer != 0;

	if (coop->integer) {
		if (IsInstancedCoop()) {
			// only dropped player items get deleted permanently
			return ent->spawnFlags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER);
		}
		// without instanced items: remove if dropped; otherwise keep IF_STAY_COOP
		const bool wasDropped = ent->spawnFlags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER);
		const bool staysInCoop = (it->flags & IF_STAY_COOP) != 0;
		return wasDropped || !staysInCoop;
	}

	// singleplayer or DM:
	// remove if not DM, or if DM and this entity was dropped
	return !dm || ent->spawnFlags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER);
}

/*
===============
Touch_Item
===============
*/
TOUCH(Touch_Item) (gentity_t* ent, gentity_t* other, const trace_t& tr, bool /*otherTouchingSelf*/) -> void {
	// Basic guards
	if (!other || !other->client)
		return;
	if (other->health < 1)
		return; // dead people cannot pick up
	if (!ent || !ent->item || !ent->item->pickup)
		return; // not a grabbable item

	// Blow up if touching slime or lava
	if (tr.contents & (CONTENTS_SLIME | CONTENTS_LAVA)) {
		BecomeExplosion1(ent);
		return;
	}

	Item* it = ent->item;

	// Instanced-coop per-player pickup gate
	if (IsInstancedCoop()) {
		const int32_t idx = other->s.number - 1;
		if (idx < 0 || idx >= MAX_CLIENTS)
			return;
		if (ent->itemPickedUpBy[idx])
			return; // this player already took their instance
	}

	// Cannot pickup during countdown
	if (ItemPickupsAreDisabled())
		return;

	// Attempt pickup
	const bool pickedUp = it->pickup(ent, other);

	// Keep selected-item sanity in sync regardless of pickup success
	ValidateSelectedItem(other);

	if (pickedUp) {
		// Feedback flash
		other->client->feedback.bonusAlpha = 0.25f;

		// HUD pickup widgets
		other->client->ps.stats[STAT_PICKUP_ICON] = gi.imageIndex(it->icon);
		other->client->ps.stats[STAT_PICKUP_STRING] =
			static_cast<int32_t>(CS_ITEMS) + static_cast<int32_t>(it->id);
		other->client->pickupMessageTime = level.time + 3_sec;

		// If usable and we hold at least one, make it selected
		if (it->use && other->client->pers.inventory[it->id]) {
			other->client->ps.stats[STAT_SELECTED_ITEM] = other->client->pers.selectedItem = it->id;
			other->client->ps.stats[STAT_SELECTED_ITEM_NAME] = 0; // name already shown by pickup string
		}

		// Pickup sound
		if (ent->noiseIndex) {
			gi.sound(other, CHAN_ITEM, ent->noiseIndex, 1.0f, ATTN_NORM, 0);
		}
		else if (it->pickupSound) {
			gi.sound(other, CHAN_ITEM, gi.soundIndex(it->pickupSound), 1.0f, ATTN_NORM, 0);
			// ent->s.sound = gi.soundIndex(it->pickupSound);
			// ent->s.loopVolume = 1.0f;
		}

		// Mark instanced-coop per-player pickup and mirror message if needed
		if (IsInstancedCoop()) {
			const int32_t playerNumber = other->s.number - 1;
			if (playerNumber >= 0 && playerNumber < MAX_CLIENTS && !ent->itemPickedUpBy[playerNumber]) {
				ent->itemPickedUpBy[playerNumber] = true;

				// When instanced, allow message to reach everyone (relays need separate fixes)
				if (ent->message)
					PrintActivationMessage(ent, other, false);
			}
		}

		// Team POI ping for notable items in DM
		if (deathmatch->integer && IsTeamPingItem(it->id)) {
			BroadcastTeamPickupPing(other, it);
		}
	}

	// Fire targets once per item entity, with DM/instanced-coop message suppression
	if (!(ent->spawnFlags & SPAWNFLAG_ITEM_TARGETS_USED)) {
		const bool suppressMsg = deathmatch->integer || IsInstancedCoop();
		const char* messageBackup = nullptr;

		if (suppressMsg)
			std::swap(messageBackup, ent->message);

		UseTargets(ent, other);

		if (suppressMsg)
			std::swap(messageBackup, ent->message);

		ent->spawnFlags |= SPAWNFLAG_ITEM_TARGETS_USED;
	}

	// Post-pickup removal/respawn handling
	if (pickedUp) {
		if (ShouldRemoveItemAfterPickup(ent, it)) {
			if (ent->flags & FL_RESPAWN) {
				ent->flags &= ~FL_RESPAWN;
				ent->volume = 0;
			}
			else {
				FreeEntity(ent);
			}
		}
	}
}

/*
===============
Drop_Item
===============
*/
gentity_t* Drop_Item(gentity_t* ent, Item* item) {
	if (!ent || !item || !item->worldModel) return nullptr;

	gentity_t* dropped = Spawn();
	if (!dropped) return nullptr;

	dropped->item = item;
	dropped->spawnFlags = SPAWNFLAG_ITEM_DROPPED;
	dropped->className = item->className;
	dropped->s.effects = item->worldModelFlags;
	gi.setModel(dropped, item->worldModel);
	dropped->s.renderFX = RF_GLOW | RF_NO_LOD | RF_IR_VISIBLE;

	// Dropped items should default to a normal visual scale
	if (dropped->s.scale <= 0.0f)
		dropped->s.scale = 1.0f;

	// scale the bbox
	const float s = std::max(0.001f, dropped->s.scale); // safety
	SetDroppedItemBounds(dropped, s);

	dropped->solid = SOLID_TRIGGER;
	dropped->moveType = MoveType::Toss;
	dropped->touch = drop_temp_touch;
	dropped->owner = ent;

	Vector3 forward, right;
	if (ent->client) AngleVectors(ent->client->vAngle, forward, right, nullptr);
	else             AngleVectors(ent->s.angles, forward, right, nullptr);

	// scale the spawn offset so big items clear the player
	const Vector3 offset = Vector3{ 24, 0, -16 } *s;
	const Vector3 start = ent->s.origin;
	const Vector3 desired = ent->client ? G_ProjectSource(start, offset, forward, right)
		: (ent->absMin + ent->absMax) / 2;

	const trace_t tr = gi.trace(start, dropped->mins, dropped->maxs, desired, ent, MASK_SOLID);
	dropped->s.origin = tr.endPos;

	G_FixStuckObject(dropped, dropped->s.origin);

	// optionally scale toss impulse a bit; keep Z punch readable
	dropped->velocity = forward * (100.0f * std::sqrt(s));
	dropped->velocity[_Z] = 300.0f * std::sqrt(s);

	dropped->think = drop_make_touchable;
	dropped->nextThink = level.time + 1_sec;

	if (coop->integer && P_UseCoopInstancedItems())
		dropped->svFlags |= SVF_INSTANCED;

	gi.linkEntity(dropped);
	return dropped;
}

/*
===============
Use_Item
===============
*/
static USE(Use_Item) (gentity_t* ent, gentity_t* /*other*/, gentity_t* /*activator*/) -> void {
	if (!ent)
		return;

	// Make the item visible to clients and stop further use-calls
	ent->svFlags &= ~SVF_NOCLIENT;
	ent->use = nullptr;

	const bool noTouch = ent->spawnFlags.has(SPAWNFLAG_ITEM_NO_TOUCH);
	if (noTouch) {
		ent->solid = SOLID_BBOX;
		ent->touch = nullptr;
	}
	else {
		ent->solid = SOLID_TRIGGER;
		ent->touch = Touch_Item;
	}

	gi.linkEntity(ent);
}

//======================================================================

/*
===============
FinishSpawningItem
Previously 'droptofloor'
===============
*/
static THINK(FinishSpawningItem) (gentity_t* ent) -> void {
	if (!ent)
		return;

	// Set bounding box size with scale applied
	if (strcmp(ent->className, "item_foodcube") == 0) {
		const Vector3 base = { 8, 8, 8 };
		ent->mins = -base * ent->s.scale;
		ent->maxs = base * ent->s.scale;
	}
	else {
		SetScaledItemBounds(ent, 15.0f);
	}

	// Assign model
	gi.setModel(ent, ent->model ? ent->model : ent->item->worldModel);

	ent->solid = SOLID_TRIGGER;
	ent->touch = Touch_Item;

	// Movement setup
	if (ent->spawnFlags.has(SPAWNFLAG_ITEM_SUSPENDED)) {
		ent->moveType = MoveType::None;
	}
	else {
		ent->moveType = MoveType::Toss;

		// Drop to floor
		const Vector3 dest = ent->s.origin + Vector3{ 0, 0, -4096 };
		trace_t tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);

		if (tr.startSolid) {
			// Try to unstick
			if (G_FixStuckObject(ent, ent->s.origin) == StuckResult::NoGoodPosition) {
				if (strcmp(ent->className, "item_foodcube") == 0) {
					ent->velocity[_Z] = 0;
				}
				else {
					gi.Com_PrintFmt("{}: {}: startSolid\n", __FUNCTION__, *ent);
					FreeEntity(ent);
					return;
				}
			}
		}
		else {
			ent->s.origin = tr.endPos;
		}
	}

	// Teamed item handling
	if (ent->team) {
		ent->flags &= ~FL_TEAMSLAVE;
		ent->chain = ent->teamChain;
		ent->teamChain = nullptr;

		ent->svFlags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;

		if (ent == ent->teamMaster) {
			ent->nextThink = level.time + 10_hz;
			ent->think = RespawnItem;
		}
		else {
			ent->nextThink = 0_sec;
		}
	}

	// No-touch items
	if (ent->spawnFlags.has(SPAWNFLAG_ITEM_NO_TOUCH)) {
		ent->solid = SOLID_BBOX;
		ent->touch = nullptr;

		if (!ent->spawnFlags.has(SPAWNFLAG_ITEM_SUSPENDED)) {
			ent->s.effects &= ~(EF_ROTATE | EF_BOB);
		}
		else {
			ent->s.effects = (EF_ROTATE | EF_BOB);
		}
		ent->s.renderFX &= ~RF_GLOW;
	}

	// Trigger-spawn items
	if (ent->spawnFlags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN)) {
		ent->svFlags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		ent->use = Use_Item;
	}

	if (ent->item && ent->item->id == IT_BALL) {
		Ball_RegisterSpawn(ent);
		if (Game::Is(GameType::ProBall))
			ProBall::RegisterBallSpawn(ent);
		return;
	}

	// Powerups in deathmatch spawn with a delay
	if (deathmatch->integer && (ent->item->flags & IF_POWERUP)) {
		ent->svFlags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		ent->nextThink = level.time + GameTime::from_sec(irandom(30, 60));
		ent->think = RespawnItem;
		return;
	}

	ent->waterType = gi.pointContents(ent->s.origin);
	gi.linkEntity(ent);

}

/*
===============
PrecacheItem

Precaches all data needed for a given item.
This will be called for each item spawned in a level,
and for each item in each client's inventory.
===============
*/
void PrecacheItem(Item* it) {
	if (!it)
		return;

	// Avoid duplicate work and recursion loops
	if (it->precached)
		return;
	it->precached = true;

	// Core assets
	if (it->pickupSound) gi.soundIndex(it->pickupSound);
	if (it->worldModel)  gi.modelIndex(it->worldModel);
	if (it->viewModel)   gi.modelIndex(it->viewModel);
	if (it->icon)        gi.imageIndex(it->icon);

	// Precache ammo, if any
	if (it->ammo) {
		if (Item* ammo = GetItemByIndex(it->ammo)) {
			if (ammo != it)
				PrecacheItem(ammo);
		}
	}

	// Parse space-separated precache list
	std::string_view s = it->precaches;
	if (s.empty())
		return;

	char buf[MAX_QPATH];

	auto is_space = [](char c) { return c == ' ' || c == '\t'; };
	auto tolower3 = [](char c)->char { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };

	while (!s.empty()) {
		// skip leading ws
		while (!s.empty() && is_space(s.front())) s.remove_prefix(1);
		if (s.empty()) break;

		// take token until space/tab
		size_t i = 0;
		while (i < s.size() && !is_space(s[i])) ++i;
		std::string_view tok = s.substr(0, i);
		s.remove_prefix(i);

		// validate
		if (tok.size() < 5 || tok.size() >= static_cast<size_t>(MAX_QPATH)) {
			gi.Com_PrintFmt("PrecacheItem: {} has bad precache token '{}'\n",
				it->className ? it->className : "(null)", std::string(tok).c_str());
			continue;
		}

		// find extension
		size_t dot = tok.find_last_of('.');
		if (dot == std::string_view::npos || dot + 3 >= tok.size()) {
			gi.Com_PrintFmt("PrecacheItem: {} token missing/short extension '{}'\n",
				it->className ? it->className : "(null)", std::string(tok).c_str());
			continue;
		}

		char e0 = tolower3(tok[dot + 1]);
		char e1 = tolower3(tok[dot + 2]);
		char e2 = tolower3(tok[dot + 3]);

		// copy into NUL-terminated buffer for gi APIs
		const size_t n = tok.size();
		memcpy(buf, tok.data(), n);
		buf[n] = '\0';

		// route by extension
		if ((e0 == 'm' && e1 == 'd' && e2 == '2') || (e0 == 's' && e1 == 'p' && e2 == '2')) {
			gi.modelIndex(buf);
		}
		else if (e0 == 'w' && e1 == 'a' && e2 == 'v') {
			gi.soundIndex(buf);
		}
		else if (e0 == 'p' && e1 == 'c' && e2 == 'x') {
			gi.imageIndex(buf);
		}
		else {
			// Unknown ext: keep going, but log once per token
			gi.Com_PrintFmt("PrecacheItem: {} unknown extension in token '{}'\n",
				it->className ? it->className : "(null)", buf);
		}
	}
}

/*
===============
CheckItemEnabled
===============
*/
bool CheckItemEnabled(Item* item) {
	if (!item)
		return false;

	// Non-DM restrictions
	if (!deathmatch->integer) {
		if (item->pickup == Pickup_Doppelganger || item->pickup == Pickup_Nuke)
			return false;
		if (item->use == Use_Vengeance || item->use == Use_Hunter)
			return false;
		if (item->use == Use_Teleporter)
			return false;
		return true;
	}

	// Map-specific disable cvar: "<mapname>_disable_<classname>"
	{
		const char* mapName = !level.mapName.empty() ? level.mapName.data() : "";
		const char* cls = item->className ? item->className : "";
		std::string cvarName = std::string(mapName) + "_disable_" + cls;
		if (gi.cvar(cvarName.c_str(), "0", CVAR_NOFLAGS)->integer)
			return false;
	}

	// Global disable cvar: "disable_<classname>"
	{
		const char* cls = item->className ? item->className : "";
		std::string cvarName = std::string("disable_") + cls;
		if (gi.cvar(cvarName.c_str(), "0", CVAR_NOFLAGS)->integer)
			return false;
	}

	// Do not spawn flags unless CTF is enabled
	if (!Game::Has(GameFlags::CTF) && (item->id == IT_FLAG_RED || item->id == IT_FLAG_BLUE))
		return false;

	// Global item spawn disable
	if (!ItemSpawnsEnabled()) {
		if (item->flags & (IF_ARMOR | IF_POWER_ARMOR | IF_TIMED | IF_POWERUP | IF_SPHERE | IF_HEALTH | IF_AMMO | IF_WEAPON))
			return false;
	}

	// Q1 ruleset: disable the pack
	if (item->id == IT_PACK && RS(Quake1))
		return false;

	// Inhibit groups: choose first matching class (preserves original else-if precedence)
	bool add = false, subtract = false;
	if (game.item_inhibit_pu && (item->flags & (IF_POWERUP | IF_SPHERE))) {
		add = game.item_inhibit_pu > 0;
		subtract = game.item_inhibit_pu < 0;
	}
	else if (game.item_inhibit_pa && (item->flags & IF_POWER_ARMOR)) {
		add = game.item_inhibit_pa > 0;
		subtract = game.item_inhibit_pa < 0;
	}
	else if (game.item_inhibit_ht && (item->flags & IF_HEALTH)) {
		add = game.item_inhibit_ht > 0;
		subtract = game.item_inhibit_ht < 0;
	}
	else if (game.item_inhibit_ar && (item->flags & IF_ARMOR)) {
		add = game.item_inhibit_ar > 0;
		subtract = game.item_inhibit_ar < 0;
	}
	else if (game.item_inhibit_am && (item->flags & IF_AMMO)) {
		add = game.item_inhibit_am > 0;
		subtract = game.item_inhibit_am < 0;
	}
	else if (game.item_inhibit_wp && (item->flags & IF_WEAPON)) {
		add = game.item_inhibit_wp > 0;
		subtract = game.item_inhibit_wp < 0;
	}

	if (subtract)
		return false;

	// Ball gametype: only the ball spawns
	if (Game::Is(GameType::ProBall) && item->id != IT_BALL)
		return false;

	// Map-level toggles (only if not force-added by inhibit)
	if (!add) {
		if (!game.map.spawnArmor && (item->flags & IF_ARMOR))
			return false;

		if (!game.map.spawnPowerArmor && (item->flags & IF_POWER_ARMOR))
			return false;

		// Note: && binds tighter than ||
		if ((!game.map.spawnPowerups && (item->flags & IF_POWERUP)) ||
			((CooperativeModeOn() || !deathmatch->integer) && skill->integer > 3))
			return false;

		if (!game.map.spawnBFG && item->id == IT_WEAPON_BFG)
			return false;

		if (g_no_items->integer) {
			if (item->flags & (IF_TIMED | IF_POWERUP | IF_SPHERE))
				return false;
			if (item->pickup == Pickup_Doppelganger)
				return false;
		}

		if ((!game.map.spawnHealth || g_vampiric_damage->integer) && (item->flags & IF_HEALTH))
			return false;

		if (g_no_mines->integer) {
			if (item->id == IT_WEAPON_PROXLAUNCHER || item->id == IT_AMMO_PROX || item->id == IT_AMMO_TESLA || item->id == IT_AMMO_TRAP)
				return false;
		}

		if (g_no_nukes->integer && item->id == IT_AMMO_NUKE)
			return false;

		if (g_no_spheres->integer && (item->flags & IF_SPHERE))
			return false;
	}

	// Infinite ammo rules: hide most ammo and capacity boosters
	if (InfiniteAmmoOn(item)) {
		if ((item->flags & IF_AMMO) &&
			item->id != IT_AMMO_GRENADES &&
			item->id != IT_AMMO_TRAP &&
			item->id != IT_AMMO_TESLA)
			return false;

		if (item->id == IT_PACK || item->id == IT_BANDOLIER)
			return false;
	}

	return true;
}

/*
============
CheckItemReplacements
============
*/
Item* CheckItemReplacements(Item* item) {
	cvar_t* cv;

	cv = gi.cvar(G_Fmt("{}_replace_{}", level.mapName.data(), item->className).data(), "", CVAR_NOFLAGS);
	if (*cv->string) {
		Item* out = FindItemByClassname(cv->string);
		return out ? out : item;
	}

	cv = gi.cvar(G_Fmt("replace_{}", item->className).data(), "", CVAR_NOFLAGS);
	if (*cv->string) {
		Item* out = FindItemByClassname(cv->string);
		return out ? out : item;
	}

	if (InfiniteAmmoOn(item)) {
		// [Paril-KEX] some item swappage 
		// BFG too strong in Infinite Ammo mode
		if (item->id == IT_WEAPON_BFG)
			return GetItemByIndex(IT_WEAPON_DISRUPTOR);

		if (item->id == IT_POWER_SHIELD || item->id == IT_POWER_SCREEN)
			return GetItemByIndex(IT_ARMOR_BODY);
	}

	return item;
}

/*
============
Item_TriggeredSpawn

Create the item marked for spawn creation
============
*/
static USE(Item_TriggeredSpawn) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->svFlags &= ~SVF_NOCLIENT;
	self->use = nullptr;

	if (self->spawnFlags.has(SPAWNFLAG_ITEM_TOSS_SPAWN)) {
		self->moveType = MoveType::Toss;
		Vector3 forward, right;

		AngleVectors(self->s.angles, forward, right, nullptr);
		self->s.origin = self->s.origin;
		self->s.origin[_Z] += 16;
		self->velocity = forward * 100;
		self->velocity[_Z] = 300;
	}

	if (self->item->id != IT_KEY_POWER_CUBE && self->item->id != IT_KEY_EXPLOSIVE_CHARGES) // leave them be on key_power_cube
		self->spawnFlags &= SPAWNFLAG_ITEM_NO_TOUCH;

	FinishSpawningItem(self);
}

/*
============
SetTriggeredSpawn

Sets up an item to spawn in later.
============
*/
static void SetTriggeredSpawn(gentity_t* ent) {
	// don't do anything on key_power_cubes.
	if (ent->item->id == IT_KEY_POWER_CUBE || ent->item->id == IT_KEY_EXPLOSIVE_CHARGES)
		return;

	ent->think = nullptr;
	ent->nextThink = 0_ms;
	ent->use = Item_TriggeredSpawn;
	ent->svFlags |= SVF_NOCLIENT;
	ent->solid = SOLID_NOT;
}

/*
===============
SpawnItem

Sets the clipping size and schedules planting on the floor.

Items are deferred because they might sit on entities that have not spawned yet.
===============
*/
bool SpawnItem(gentity_t* ent, Item* item) {
	if (!ent || !item) {
		if (ent) FreeEntity(ent);
		return false;
	}

	// Apply replacement/alias, then validate enablement
	item = CheckItemReplacements(item);
	if (!item || !CheckItemEnabled(item)) {
		FreeEntity(ent);
		return false;
	}

	// Keys: allow trigger-spawn and optional no-touch presentation
	if (item->flags & IF_KEY) {
		if (ent->spawnFlags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN)) {
			ent->svFlags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;
			ent->use = Use_Item;
		}
		if (ent->spawnFlags.has(SPAWNFLAG_ITEM_NO_TOUCH)) {
			ent->solid = SOLID_BBOX;
			ent->touch = nullptr;
			ent->s.effects &= ~(EF_ROTATE | EF_BOB);
			ent->s.renderFX &= ~RF_GLOW;
		}
	}
	else if (ent->spawnFlags.value >= SPAWNFLAG_ITEM_MAX.value) {
		// Sanity on unknown spawn flags
		ent->spawnFlags = SPAWNFLAG_NONE;
		gi.Com_PrintFmt("{} has invalid spawnFlags set\n", *ent);
	}

	// Finalize class name and cache assets
	ent->className = item->className;
	PrecacheItem(item);

	// Coop special handling
	const bool inCoop = !!coop->integer;

	// Power cube bits (coop)
	if (inCoop && (item->id == IT_KEY_POWER_CUBE || item->id == IT_KEY_EXPLOSIVE_CHARGES)) {
		ent->spawnFlags.value |= (1u << (8 + level.powerCubes));
		level.powerCubes++;
	}

	// Coop instanced items (KEX behavior)
	if (inCoop && P_UseCoopInstancedItems())
		ent->svFlags |= SVF_INSTANCED;

	// Core entity setup
	ent->item = item;
	ent->timeStamp = level.time;

	ent->nextThink = level.time + 20_hz;      // start after other solids
	ent->think = FinishSpawningItem;          // will size bbox and drop-to-floor

	ent->s.effects = item->worldModelFlags;
	ent->s.renderFX = RF_GLOW | RF_NO_LOD;

	if (!ent->s.scale)
		ent->s.scale = 1.0f;

	// Allow mapper override models (just ensure cached)
	if (ent->model)
		gi.modelIndex(ent->model);

	// Suspended items bob/rotate by default
	if (ent->spawnFlags.has(SPAWNFLAG_ITEM_SUSPENDED))
		ent->s.effects |= (EF_ROTATE | EF_BOB);

	// Triggered spawns start hidden/inactive until used
	if (ent->spawnFlags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN))
		SetTriggeredSpawn(ent);

	// CTF flags have server-animated setup
	if (item->id == IT_FLAG_RED || item->id == IT_FLAG_BLUE)
		ent->think = CTF_FlagSetup;

	// Track weapon counts for this map
	if ((item->flags & IF_WEAPON) && item->id >= FIRST_WEAPON && item->id <= LAST_WEAPON) {
		const int windex = static_cast<int>(item->id) - FIRST_WEAPON;
		level.weaponCount[windex]++;
	}

	// Lock powerups visually if player count below threshold
	if ((item->flags & IF_POWERUP) && match_powerupMinPlayerLock->integer > 0) {
		if (level.pop.num_playing_clients < match_powerupMinPlayerLock->integer) {
			ent->s.renderFX |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
			ent->s.effects |= EF_COLOR_SHELL;
		}
	}

	// Allow disabling EF_BOB globally for floor items (not suspended)
	if (!g_itemBobbing->integer && !ent->spawnFlags.has(SPAWNFLAG_ITEM_SUSPENDED))
		ent->s.effects &= ~EF_BOB;

	// Foodcube: choose pickup sound by amount
	if (item->id == IT_FOODCUBE) {
		const char* sizeCode =
			(ent->count < 10) ? "s" :
			(ent->count < 25) ? "n" :
			(ent->count < 50) ? "l" : "m";

		std::array<char, 32> path{};
		std::snprintf(path.data(), path.size(), "items/%s_health.wav", sizeCode);
		ent->noiseIndex = gi.soundIndex(path.data());
	}

	return true;
}

/*
===============
P_ToggleFlashlight
===============
*/
void P_ToggleFlashlight(gentity_t* ent, bool state) {
	if (!ent)
		return;

	const bool isOn = (ent->flags & FL_FLASHLIGHT) != 0;
	if (isOn == state)
		return;

	// Toggle flashlight flag
	ent->flags ^= FL_FLASHLIGHT;

        // Choose sound based on new state
        const char* suffix = (ent->flags & FL_FLASHLIGHT) ? "on" : "off";
        std::array<char, 32> path{};
        std::snprintf(path.data(), path.size(), "items/flashlight_%s.wav", suffix);

        gi.sound(ent, CHAN_AUTO, gi.soundIndex(path.data()), 1.0f, ATTN_STATIC, 0);
}

/*
===============
Use_Flashlight
===============
*/
void Use_Flashlight(gentity_t* ent, Item* inv) {
	if (!ent)
		return;

	P_ToggleFlashlight(ent, (ent->flags & FL_FLASHLIGHT) == 0);
}

constexpr size_t MAX_TEMP_POI_POINTS = 128;

/*
===============
Compass_Update
===============
*/
void Compass_Update(gentity_t* ent, bool first) {
	if (!ent)
		return;

	Vector3*& points = level.poi.points[ent->s.number - 1];
	if (!points) // deleted or never allocated
		return;

	auto& comp = ent->client->compass;

	if (!comp.drawPoints)
		return;
	if (comp.drawTime >= level.time)
		return;

	// Distance + visibility check
	const Vector3& currentPoint = points[comp.drawIndex];
	float distance = (currentPoint - ent->s.origin).length();
	if (distance > 4096.f || !gi.inPHS(ent->s.origin, currentPoint, false)) {
		comp.drawPoints = false;
		return;
	}

	// Write network data
	gi.WriteByte(svc_help_path);
	gi.WriteByte(first ? 1 : 0);
	gi.WritePosition(currentPoint);

	if (comp.drawIndex == comp.drawCount - 1) {
		gi.WriteDir((comp.poiLocation - currentPoint).normalized());
	}
	else {
		gi.WriteDir((points[comp.drawIndex + 1] - currentPoint).normalized());
	}

	gi.unicast(ent, false);

	P_SendLevelPOI(ent);

	gi.localSound(ent, currentPoint, world, CHAN_AUTO,
		gi.soundIndex("misc/help_marker.wav"),
		1.0f, ATTN_NORM, 0.0f, GetUnicastKey());

	// If done, stop drawing
	if (comp.drawIndex == comp.drawCount - 1) {
		comp.drawPoints = false;
		return;
	}

	// Advance
	comp.drawIndex++;
	comp.drawTime = level.time + 200_ms;
}

/*
===============
Use_Compass
===============
*/
void Use_Compass(gentity_t* ent, Item* inv) {
	if (!ent)
		return;

	// In deathmatch, compass acts as ready-up
	if (deathmatch->integer) {
		ClientSetReadyStatus(ent, false, true);
		return;
	}

	if (!level.poi.valid) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$no_valid_poi");
		return;
	}

	// Fire dynamic POI trigger if present
	if (level.poi.currentDynamic)
		level.poi.currentDynamic->use(level.poi.currentDynamic, ent, ent);

	// Assign destination + image
	auto& comp = ent->client->compass;
	comp.poiLocation = level.poi.current;
	comp.poiImage = level.poi.currentImage;

	// Ensure we have a path buffer for this client
	Vector3*& points = level.poi.points[ent->s.number - 1];
	if (!points) {
		points = static_cast<Vector3*>(
			gi.TagMalloc(sizeof(Vector3) * (MAX_TEMP_POI_POINTS + 1), TAG_LEVEL));
	}

	// Build path request
	PathRequest request{};
	request.start = ent->s.origin;
	request.goal = level.poi.current;
	request.moveDist = 64.f;
	request.pathFlags = PathFlags::All;
	request.nodeSearch.ignoreNodeFlags = true;
	request.nodeSearch.minHeight = 128.f;
	request.nodeSearch.maxHeight = 128.f;
	request.nodeSearch.radius = 1024.f;
	request.pathPoints.array = points + 1;
	request.pathPoints.count = MAX_TEMP_POI_POINTS;

	PathInfo info{};

	if (gi.GetPathToGoal(request, info)) {
		// Initialize path draw state
		comp.drawPoints = true;
		comp.drawCount = std::min<size_t>(info.numPathPoints, MAX_TEMP_POI_POINTS);
		comp.drawIndex = 1;

		// Skip points too close to player start
		for (int i = 1; i < 1 + comp.drawCount; i++) {
			float d = (points[i] - ent->s.origin).length();
			if (d > 192.f)
				break;
			comp.drawIndex = i;
		}

		// Add a helper point if player is facing away from path start
		const Vector3& firstPoint = points[comp.drawIndex];
		float facingDot = (firstPoint - ent->s.origin).normalized().dot(ent->client->vForward);
		if (facingDot < 0.3f) {
			Vector3 p = ent->s.origin + (ent->client->vForward * 64.f);
			trace_t tr = gi.traceLine(ent->s.origin + Vector3{ 0.f, 0.f, (float)ent->viewHeight },
				p, nullptr, MASK_SOLID);

			comp.drawIndex--;
			comp.drawCount++;

			if (tr.fraction < 1.0f)
				tr.endPos += tr.plane.normal * 8.f;

			points[comp.drawIndex] = tr.endPos;
		}

		comp.drawTime = 0_ms;
		Compass_Update(ent, true);
	}
	else {
		// Fallback if no path
		P_SendLevelPOI(ent);
		gi.localSound(ent, CHAN_AUTO,
			gi.soundIndex("misc/help_marker.wav"),
			1.f, ATTN_NORM, 0, GetUnicastKey());
	}
}

void Use_Ball(gentity_t* ent, Item* /*item*/) {
	if (!ent || !ent->client)
		return;
	if (!Game::Is(GameType::ProBall))
		return;
	if (!Ball_PlayerHasBall(ent))
		return;

	if (ent->client->ball.nextPassTime > level.time)
		return;

	Vector3 angles = {
			std::max(-62.5f, ent->client->vAngle[PITCH]),
			ent->client->vAngle[YAW],
			ent->client->vAngle[ROLL]
	};

	Vector3 start{}, dir{};
	P_ProjectSource(ent, angles, { 2.f, 0.f, -14.f }, start, dir);

	if (ProBall::ThrowBall(ent, start, dir)) {
		ent->client->ball.nextPassTime = level.time + Ball_GetPassCooldown();
		ent->client->ball.nextDropTime = std::max(ent->client->ball.nextDropTime, level.time + 200_ms);
	}
}

void Drop_Ball(gentity_t* ent, Item* /*item*/) {
	if (!ent || !ent->client)
		return;
	if (!Game::Is(GameType::ProBall))
		return;
	if (!Ball_PlayerHasBall(ent))
		return;

	if (ent->client->ball.nextDropTime > level.time)
		return;

	if (ProBall::DropBall(ent, nullptr, false)) {
		ent->client->ball.nextDropTime = level.time + Ball_GetDropCooldown();
		ent->client->ball.nextPassTime = std::max(ent->client->ball.nextPassTime, level.time + 200_ms);
	}
}

//======================================================================

// The item list has moved to g_item_list.cpp.

/*
===============
InitItems
===============
*/
void InitItems() {
	// 1) Validate enum <-> table mapping
	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		if (itemList[i].id != i) {
			gi.Com_ErrorFmt("Item {} has wrong enum ID {} (should be {})",
				itemList[i].pickupName, (int32_t)itemList[i].id, (int32_t)i);
		}
	}

	// 2) Build circular chains efficiently (one pass, O(n))
	// Each chain uses itemList[headIdx] as the head whose chainNext points to itself.
	// We append others so the last->chainNext == head.
	Item* chainLast[IT_TOTAL] = {};       // last node for each head (nullptr if none yet)
	bool  chainInit[IT_TOTAL] = {};       // whether head was initialized

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		Item* it = &itemList[i];

		// No chain specified
		if (!it->chain)
			continue;

		// Already linked
		if (it->chainNext)
			continue;

		const item_id_t headIdx = it->chain;

		// Validate head index range
		if (headIdx < IT_NULL || headIdx >= IT_TOTAL) {
			gi.Com_ErrorFmt("Invalid item chain {} for {}", (int32_t)headIdx, it->pickupName);
			continue;
		}

		Item* head = &itemList[headIdx];

		// Initialize head once
		if (!chainInit[headIdx]) {
			if (!head->chainNext)
				head->chainNext = head;   // head self-loop
			chainLast[headIdx] = head;    // last initially is head
			chainInit[headIdx] = true;
		}

		// If this item IS the head, nothing to append
		if (it == head)
			continue;

		// Append 'it' to the circular list if not already linked
		if (!it->chainNext) {
			it->chainNext = head;                 // new tail points to head
			chainLast[headIdx]->chainNext = it;   // old tail points to new tail
			chainLast[headIdx] = it;              // advance tail
		}
	}

	// 3) Set up ammo and powerup lookup tables, and apply coop drop rule in a single pass
	const bool coopActive = (coop->integer != 0);
	const bool coopInstanced = coopActive && P_UseCoopInstancedItems();

	for (auto& it : itemList) {
		// Ammo table
		if ((it.flags & IF_AMMO) && it.tag >= static_cast<int>(AmmoID::Bullets) && it.tag < static_cast<int>(AmmoID::_Total)) {
			if (it.id <= IT_AMMO_ROUNDS)
				ammoList[it.tag] = &it;
		}
		// Powerup wheel table (non-weapon)
		else if ((it.flags & IF_POWERUP_WHEEL) && !(it.flags & IF_WEAPON) && it.tag >= POWERUP_SCREEN && it.tag < POWERUP_MAX) {
			powerupList[it.tag] = &it;
		}

		// Coop: if not using instanced items, items marked IF_STAY_COOP should not have a drop function
		if (coopActive && !coopInstanced && (it.flags & IF_STAY_COOP)) {
			it.drop = nullptr;
		}
	}
}

/*
===============
SetItemNames

Called by worldspawn
===============
*/
void SetItemNames() {
	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
				gi.configString(static_cast<int32_t>(CS_ITEMS) + static_cast<int32_t>(i),
					itemList[i].pickupName);

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		itemList[i].ammoWheelIndex = -1;
		itemList[i].weaponWheelIndex = -1;
		itemList[i].powerupWheelIndex = -1;
	}

	// [Paril-KEX] set ammo wheel indices first
	int32_t cs_index = 0;

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		if (!(itemList[i].flags & IF_AMMO))
			continue;

		if (cs_index >= MAX_WHEEL_ITEMS) {
			gi.Com_PrintFmt("SetItemNames: too many ammo wheel items (max {}); truncating list.\n", MAX_WHEEL_ITEMS);
			break;
		}

		gi.configString(CS_WHEEL_AMMO + cs_index, G_Fmt("{}|{}", (int32_t)i, gi.imageIndex(itemList[i].icon)).data());
		itemList[i].ammoWheelIndex = cs_index;
		cs_index++;
	}

	// set weapon wheel indices
	cs_index = 0;

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		if (!(itemList[i].flags & IF_WEAPON))
			continue;

		if (cs_index >= MAX_WHEEL_ITEMS) {
			gi.Com_PrintFmt("SetItemNames: too many weapon wheel items (max {}); truncating list.\n", MAX_WHEEL_ITEMS);
			break;
		}

		int32_t min_ammo = (itemList[i].flags & IF_AMMO) ? 1 : itemList[i].quantity;

		gi.configString(CS_WHEEL_WEAPONS + cs_index, G_Fmt("{}|{}|{}|{}|{}|{}|{}|{}",
			(int32_t)i,
			gi.imageIndex(itemList[i].icon),
			itemList[i].ammo ? GetItemByIndex(itemList[i].ammo)->ammoWheelIndex : -1,
			min_ammo,
			(itemList[i].flags & IF_POWERUP_WHEEL) ? 1 : 0,
			itemList[i].sortID,
			itemList[i].quantityWarn,
			G_CanDropItem(itemList[i]) ? 1 : 0
		).data());
		itemList[i].weaponWheelIndex = cs_index;
		cs_index++;
	}

	// set powerup wheel indices
	cs_index = 0;

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		if (!(itemList[i].flags & IF_POWERUP_WHEEL) || (itemList[i].flags & IF_WEAPON))
			continue;

		if (cs_index >= MAX_WHEEL_ITEMS) {
			gi.Com_PrintFmt("SetItemNames: too many powerup wheel items (max {}); truncating list.\n", MAX_WHEEL_ITEMS);
			break;
		}

		gi.configString(CS_WHEEL_POWERUPS + cs_index, G_Fmt("{}|{}|{}|{}|{}|{}",
			(int32_t)i,
			gi.imageIndex(itemList[i].icon),
			(itemList[i].flags & IF_POWERUP_ONOFF) ? 1 : 0,
			itemList[i].sortID,
			G_CanDropItem(itemList[i]) ? 1 : 0,
			itemList[i].ammo ? GetItemByIndex(itemList[i].ammo)->ammoWheelIndex : -1
		).data());
		itemList[i].powerupWheelIndex = cs_index;
		cs_index++;
	}
}
