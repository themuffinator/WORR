/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

g_item_list.cpp implementation.*/

#include "../g_local.hpp"
void Drop_Ammo(gentity_t* ent, Item* item);
void Drop_Ball(gentity_t* ent, Item* item);
void Drop_General(gentity_t* ent, Item* item);
void Drop_PowerArmor(gentity_t* ent, Item* item);
void Drop_Weapon(gentity_t* ent, Item* item);
bool Pickup_Ammo(gentity_t* ent, gentity_t* other);
bool Pickup_Armor(gentity_t* ent, gentity_t* other);
bool Pickup_Ball(gentity_t* ent, gentity_t* other);
bool Pickup_Bandolier(gentity_t* ent, gentity_t* other);
bool Pickup_Doppelganger(gentity_t* ent, gentity_t* other);
bool Pickup_General(gentity_t* ent, gentity_t* other);
bool Pickup_Health(gentity_t* ent, gentity_t* other);
bool Pickup_Key(gentity_t* ent, gentity_t* other);
bool Pickup_LegacyHead(gentity_t* ent, gentity_t* other);
bool Pickup_Nuke(gentity_t* ent, gentity_t* other);
bool Pickup_Pack(gentity_t* ent, gentity_t* other);
bool Pickup_PowerArmor(gentity_t* ent, gentity_t* other);
bool Pickup_Powerup(gentity_t* ent, gentity_t* other);
bool Pickup_Sphere(gentity_t* ent, gentity_t* other);
bool Pickup_Teleporter(gentity_t* ent, gentity_t* other);
bool Pickup_TimedItem(gentity_t* ent, gentity_t* other);
void Tech_Drop(gentity_t* ent, Item* item);
bool Tech_Pickup(gentity_t* ent, gentity_t* other);
void Use_Adrenaline(gentity_t* ent, Item* item);
void Use_AntiGravBelt(gentity_t* ent, Item* item);
void Use_Ball(gentity_t* ent, Item* item);
void Use_BattleSuit(gentity_t* ent, Item* item);
void Use_Breather(gentity_t* ent, Item* item);
void Use_Compass(gentity_t* ent, Item* inv);
void Use_Defender(gentity_t* ent, Item* item);
void Use_Doppelganger(gentity_t* ent, Item* item);
void Use_Double(gentity_t* ent, Item* item);
void Use_EmpathyShield(gentity_t* ent, Item* item);
void Use_EnviroSuit(gentity_t* ent, Item* item);
void Use_Flashlight(gentity_t* ent, Item* inv);
void Use_Haste(gentity_t* ent, Item* item);
void Use_Hunter(gentity_t* ent, Item* item);
void Use_IR(gentity_t* ent, Item* item);
void Use_Invisibility(gentity_t* ent, Item* item);
void Use_Nuke(gentity_t* ent, Item* item);
void Use_PowerArmor(gentity_t* ent, Item* item);
void Use_Quad(gentity_t* ent, Item* item);
void Use_Regeneration(gentity_t* ent, Item* item);
void Use_Silencer(gentity_t* ent, Item* item);
void Use_Spawn_Protection(gentity_t* ent, Item* item);
void Use_Teleporter(gentity_t* ent, Item* item);
void Use_Vengeance(gentity_t* ent, Item* item);
bool Pickup_Weapon(gentity_t* ent, gentity_t* other);
void Use_Weapon(gentity_t* ent, Item* item);
void Weapon_BFG(gentity_t* ent);
void Weapon_Blaster(gentity_t* ent);
void Weapon_ChainFist(gentity_t* ent);
void Weapon_Chaingun(gentity_t* ent);
void Weapon_Disruptor(gentity_t* ent);
void Weapon_ETF_Rifle(gentity_t* ent);
void Weapon_Grapple(gentity_t* ent);
void Weapon_GrenadeLauncher(gentity_t* ent);
void Weapon_HandGrenade(gentity_t* ent);
void Weapon_HyperBlaster(gentity_t* ent);
void Weapon_IonRipper(gentity_t* ent);
void Weapon_Machinegun(gentity_t* ent);
void Weapon_Phalanx(gentity_t* ent);
void Weapon_PlasmaGun(gentity_t* ent);
void Weapon_PlasmaBeam(gentity_t* ent);
void Weapon_ProxLauncher(gentity_t* ent);
void Weapon_Railgun(gentity_t* ent);
void Weapon_RocketLauncher(gentity_t* ent);
void Weapon_Shotgun(gentity_t* ent);
void Weapon_SuperShotgun(gentity_t* ent);
void Weapon_Tesla(gentity_t* ent);
void Weapon_Thunderbolt(gentity_t* ent);
void Weapon_Trap(gentity_t* ent);
bool CTF_PickupFlag(gentity_t* ent, gentity_t* other);
void CTF_DropFlag(gentity_t* ent, Item* item);

// clang-format off
std::array<Item, IT_TOTAL> itemList = { {
	{ },	// leave index 0 alone

//
// ARMOR
//

/*QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/armor/body/tris.md2"
*/
	{
		/* id */ IT_ARMOR_BODY,
		/* className */ "item_armor_body",
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/ar3_pkup.wav",
		/* worldModel */ "models/items/armor/body/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_bodyarmor",
		/* useName */   "Body Armor",
		/* pickupName */   "$item_body_armor",
		/* pickupNameDefinitive */ "$item_body_armor_def",
		/* quantity */ Armor::Body,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR,
		/* viewWeaponModel */ nullptr
	},

/*QUAKED item_armor_combat (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_ARMOR_COMBAT,
		/* className */ "item_armor_combat",
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/ar1_pkup.wav",
		/* worldModel */ "models/items/armor/combat/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_combatarmor",
		/* useName */  "Combat Armor",
		/* pickupName */  "$item_combat_armor",
		/* pickupNameDefinitive */ "$item_combat_armor_def",
		/* quantity */ Armor::Combat,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR,
		/* viewWeaponModel */ nullptr
	},

/*QUAKED item_armor_jacket (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_ARMOR_JACKET,
		/* className */ "item_armor_jacket",
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/ar1_pkup.wav",
		/* worldModel */ "models/items/armor/jacket/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_jacketarmor",
		/* useName */  "Jacket Armor",
		/* pickupName */  "$item_jacket_armor",
		/* pickupNameDefinitive */ "$item_jacket_armor_def",
		/* quantity */ Armor::Jacket,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR,
		/* viewWeaponModel */ nullptr
	},

/*QUAKED item_armor_shard (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_ARMOR_SHARD,
		/* className */ "item_armor_shard",
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/ar2_pkup.wav",
		/* worldModel */ "models/items/armor/shard/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_armor_shard",
		/* useName */  "Armor Shard",
		/* pickupName */  "$item_armor_shard",
		/* pickupNameDefinitive */ "$item_armor_shard_def",
		/* quantity */ Armor::Shard,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR
	},

/*QUAKED item_power_screen (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_POWER_SCREEN,
		/* className */ "item_power_screen",
		/* pickup */ Pickup_PowerArmor,
		/* use */ Use_PowerArmor,
		/* drop */ Drop_PowerArmor,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/ar3_pkup.wav",
		/* worldModel */ "models/items/armor/screen/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_powerscreen",
		/* useName */  "Power Screen",
		/* pickupName */  "$item_power_screen",
		/* pickupNameDefinitive */ "$item_power_screen_def",
		/* quantity */ 60,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_SCREEN,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "misc/power2.wav misc/power1.wav"
	},

/*QUAKED item_power_shield (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_POWER_SHIELD,
		/* className */ "item_power_shield",
		/* pickup */ Pickup_PowerArmor,
		/* use */ Use_PowerArmor,
		/* drop */ Drop_PowerArmor,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/ar3_pkup.wav",
		/* worldModel */ "models/items/armor/shield/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_powershield",
		/* useName */  "Power Shield",
		/* pickupName */  "$item_power_shield",
		/* pickupNameDefinitive */ "$item_power_shield_def",
		/* quantity */ 60,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_SHIELD,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "misc/power2.wav misc/power1.wav"
	},

//
// WEAPONS 
//

/* weapon_grapple (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_WEAPON_GRAPPLE,
		/* className */ "weapon_grapple",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_Grapple,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_flareg/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/grapple/tris.md2",
		/* icon */ "w_grapple",
		/* useName */  "Grapple",
		/* pickupName */  "$item_grapple",
		/* pickupNameDefinitive */ "$item_grapple_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON | IF_NO_HASTE | IF_POWERUP_WHEEL | IF_NOT_RANDOM,
		/* viewWeaponModel */ "#w_grapple.md2",
		/* armorInfo */ nullptr,
		/* tag */ 0,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/grapple/grfire.wav weapons/grapple/grpull.wav weapons/grapple/grhang.wav weapons/grapple/grreset.wav weapons/grapple/grhit.wav weapons/grapple/grfly.wav"
	},

/* weapon_blaster (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_WEAPON_BLASTER,
		/* className */ "weapon_blaster",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_Blaster,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_blast/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_blast/tris.md2",
		/* icon */ "w_blaster",
		/* useName */  "Blaster",
		/* pickupName */  "$item_blaster",
		/* pickupNameDefinitive */ "$item_blaster_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP | IF_NOT_RANDOM,
		/* viewWeaponModel */ "#w_blaster.md2",
		/* armorInfo */ nullptr,
		/* tag */ 0,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/blastf1a.wav misc/lasfly.wav"
	},

/*QUAKED weapon_chainfist (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_chainf/tris.md2"
*/
	{
		/* id */ IT_WEAPON_CHAINFIST,
		/* className */ "weapon_chainfist",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_ChainFist,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_chainf/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_chainf/tris.md2",
		/* icon */ "w_chainfist",
		/* useName */  "Chainfist",
		/* pickupName */  "$item_chainfist",
		/* pickupNameDefinitive */ "$item_chainfist_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP | IF_NO_HASTE,
		/* viewWeaponModel */ "#w_chainfist.md2",
		/* armorInfo */ nullptr,
		/* tag */ 0,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/sawidle.wav weapons/sawhit.wav weapons/sawslice.wav",
	},

/*QUAKED weapon_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_shotg/tris.md2"
*/
	{
		/* id */ IT_WEAPON_SHOTGUN,
		/* className */ "weapon_shotgun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_Shotgun,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_shotg/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_shotg/tris.md2",
		/* icon */ "w_shotgun",
		/* useName */  "Shotgun",
		/* pickupName */  "$item_shotgun",
		/* pickupNameDefinitive */ "$item_shotgun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_SHELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_shotgun.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Shells),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/shotgf1b.wav weapons/shotgr1b.wav"
	},

/*QUAKED weapon_supershotgun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_shotg2/tris.md2"
*/
	{
		/* id */ IT_WEAPON_SSHOTGUN,
		/* className */ "weapon_supershotgun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_SuperShotgun,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_shotg2/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_shotg2/tris.md2",
		/* icon */ "w_sshotgun",
		/* useName */  "Super Shotgun",
		/* pickupName */  "$item_super_shotgun",
		/* pickupNameDefinitive */ "$item_super_shotgun_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_SHELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_sshotgun.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Shells),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/sshotf1b.wav",
		/* sortID */ 0,
		/* quantityWarn */ 10
	},

/*QUAKED weapon_machinegun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_machn/tris.md2"
*/
	{
		/* id */ IT_WEAPON_MACHINEGUN,
		/* className */ "weapon_machinegun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_Machinegun,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_machn/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_machn/tris.md2",
		/* icon */ "w_machinegun",
		/* useName */  "Machinegun",
		/* pickupName */  "$item_machinegun",
		/* pickupNameDefinitive */ "$item_machinegun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_BULLETS,
		/* chain */ IT_WEAPON_MACHINEGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_machinegun.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Bullets),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/machgf1b.wav weapons/machgf2b.wav weapons/machgf3b.wav weapons/machgf4b.wav weapons/machgf5b.wav",
		/* sortID */ 0,
		/* quantityWarn */ 30
	},

/*QUAKED weapon_etf_rifle (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_etf_rifle/tris.md2"
*/
	{
		/* id */ IT_WEAPON_ETF_RIFLE,
		/* className */ "weapon_etf_rifle",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_ETF_Rifle,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_etf_rifle/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_etf_rifle/tris.md2",
		/* icon */ "w_etf_rifle",
		/* useName */  "ETF Rifle",
		/* pickupName */  "$item_etf_rifle",
		/* pickupNameDefinitive */ "$item_etf_rifle_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_FLECHETTES,
		/* chain */ IT_WEAPON_MACHINEGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_etfrifle.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Flechettes),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/nail1.wav models/proj/flechette/tris.md2",
		/* sortID */ 0,
		/* quantityWarn */ 30
	},

/*QUAKED weapon_chaingun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_chain/tris.md2"
*/
	{
		/* id */ IT_WEAPON_CHAINGUN,
		/* className */ "weapon_chaingun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_Chaingun,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_chain/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_chain/tris.md2",
		/* icon */ "w_chaingun",
		/* useName */  "Chaingun",
		/* pickupName */  "$item_chaingun",
		/* pickupNameDefinitive */ "$item_chaingun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_BULLETS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_chaingun.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Bullets),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/chngnu1a.wav weapons/chngnl1a.wav weapons/machgf3b.wav weapons/chngnd1a.wav",
		/* sortID */ 0,
		/* quantityWarn */ 60
	},

/*QUAKED ammo_grenades (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_AMMO_GRENADES,
		/* className */ "ammo_grenades",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponThink */ Weapon_HandGrenade,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/items/ammo/grenades/medium/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ "models/weapons/v_handgr/tris.md2",
		/* icon */ "a_grenades",
		/* useName */  "Grenades",
		/* pickupName */  "$item_grenades",
		/* pickupNameDefinitive */ "$item_grenades_def",
		/* quantity */ 5,
		/* ammo */ IT_AMMO_GRENADES,
		/* chain */ IT_AMMO_GRENADES,
		/* flags */ IF_AMMO | IF_WEAPON,
		/* viewWeaponModel */ "#a_grenades.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Grenades),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/hgrent1a.wav weapons/hgrena1b.wav weapons/hgrenc1b.wav weapons/hgrenb1a.wav weapons/hgrenb2a.wav models/objects/grenade3/tris.md2",
		/* sortID */ 0,
		/* quantityWarn */ 2
	},

/*QUAKED ammo_trap (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/weapons/g_trap/tris.md2"
*/
	{
		/* id */ IT_AMMO_TRAP,
		/* className */ "ammo_trap",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponThink */ Weapon_Trap,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/weapons/g_trap/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_trap/tris.md2",
		/* icon */ "a_trap",
		/* useName */  "Trap",
		/* pickupName */  "$item_trap",
		/* pickupNameDefinitive */ "$item_trap_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_TRAP,
		/* chain */ IT_AMMO_GRENADES,
		/* flags */ IF_AMMO | IF_WEAPON | IF_NO_INFINITE_AMMO,
		/* viewWeaponModel */ "#a_trap.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Traps),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "misc/fhit3.wav weapons/trapcock.wav weapons/traploop.wav weapons/trapsuck.wav weapons/trapdown.wav items/s_health.wav items/n_health.wav items/l_health.wav items/m_health.wav models/weapons/z_trap/tris.md2",
		/* sortID */ 0,
		/* quantityWarn */ 1
	},

/*QUAKED ammo_tesla (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/am_tesl/tris.md2"
*/
	{
		/* id */ IT_AMMO_TESLA,
		/* className */ "ammo_tesla",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponThink */ Weapon_Tesla,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/ammo/am_tesl/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ "models/weapons/v_tesla/tris.md2",
		/* icon */ "a_tesla",
		/* useName */  "Tesla",
		/* pickupName */  "$item_tesla",
		/* pickupNameDefinitive */ "$item_tesla_def",
		/* quantity */ 3,
		/* ammo */ IT_AMMO_TESLA,
		/* chain */ IT_AMMO_GRENADES,
		/* flags */ IF_AMMO | IF_WEAPON | IF_NO_INFINITE_AMMO,
		/* viewWeaponModel */ "#a_tesla.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::TeslaMines),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/teslaopen.wav weapons/hgrenb1a.wav weapons/hgrenb2a.wav models/weapons/g_tesla/tris.md2",
		/* sortID */ 0,
		/* quantityWarn */ 1
	},

/*QUAKED weapon_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_launch/tris.md2"
*/
	{
		/* id */ IT_WEAPON_GLAUNCHER,
		/* className */ "weapon_grenadelauncher",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_GrenadeLauncher,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_launch/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_launch/tris.md2",
		/* icon */ "w_glauncher",
		/* useName */  "Grenade Launcher",
		/* pickupName */  "$item_grenade_launcher",
		/* pickupNameDefinitive */ "$item_grenade_launcher_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_GRENADES,
		/* chain */ IT_WEAPON_GLAUNCHER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_glauncher.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Grenades),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "models/objects/grenade4/tris.md2 weapons/grenlf1a.wav weapons/grenlr1b.wav weapons/grenlb1b.wav"
	},

/*QUAKED weapon_proxlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_plaunch/tris.md2"
*/
	{
		/* id */ IT_WEAPON_PROXLAUNCHER,
		/* className */ "weapon_proxlauncher",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_ProxLauncher,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_plaunch/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_plaunch/tris.md2",
		/* icon */ "w_proxlaunch",
		/* useName */  "Prox Launcher",
		/* pickupName */  "$item_prox_launcher",
		/* pickupNameDefinitive */ "$item_prox_launcher_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_PROX,
		/* chain */ IT_WEAPON_GLAUNCHER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_plauncher.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::ProxMines),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/grenlf1a.wav weapons/grenlr1b.wav weapons/grenlb1b.wav weapons/proxwarn.wav weapons/proxopen.wav",
	},

/*QUAKED weapon_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_rocket/tris.md2"
*/
	{
		/* id */ IT_WEAPON_RLAUNCHER,
		/* className */ "weapon_rocketlauncher",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_RocketLauncher,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_rocket/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_rocket/tris.md2",
		/* icon */ "w_rlauncher",
		/* useName */  "Rocket Launcher",
		/* pickupName */  "$item_rocket_launcher",
		/* pickupNameDefinitive */ "$item_rocket_launcher_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_ROCKETS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_rlauncher.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Rockets),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "models/objects/rocket/tris.md2 weapons/rockfly.wav weapons/rocklf1a.wav weapons/rocklr1b.wav models/objects/debris2/tris.md2"
	},

/*QUAKED weapon_hyperblaster (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_hyperb/tris.md2"
*/
	{
		/* id */ IT_WEAPON_HYPERBLASTER,
		/* className */ "weapon_hyperblaster",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_HyperBlaster,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_hyperb/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_hyperb/tris.md2",
		/* icon */ "w_hyperblaster",
		/* useName */  "HyperBlaster",
		/* pickupName */  "$item_hyperblaster",
		/* pickupNameDefinitive */ "$item_hyperblaster_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_hyperblaster.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Cells),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/hyprbu1a.wav weapons/hyprbl1a.wav weapons/hyprbf1a.wav weapons/hyprbd1a.wav misc/lasfly.wav",
		/* sortID */ 0,
		/* quantityWarn */ 30
	},

/*QUAKED weapon_boomer (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_boom/tris.md2"
*/
	{
		/* id */ IT_WEAPON_IONRIPPER,
		/* className */ "weapon_boomer",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_IonRipper,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_boom/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_boomer/tris.md2",
		/* icon */ "w_ripper",
		/* useName */  "Ionripper",
		/* pickupName */  "$item_ionripper",
		/* pickupNameDefinitive */ "$item_ionripper_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_ripper.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Cells),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/rippfire.wav models/objects/boomrang/tris.md2 misc/lasfly.wav",
		/* sortID */ 0,
		/* quantityWarn */ 30
	},

/*QUAKED weapon_plasmagun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_plasmr/tris.md2"
*/
	{
		/* id */ IT_WEAPON_PLASMAGUN,
		/* className */ "weapon_plasmagun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_PlasmaGun,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_plasmr/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_plasmr/tris.md2",
		/* icon */ "w_plasmarifle",
		/* useName */  "Plasma Gun",
		/* pickupName */  "Plasma Gun",
		/* pickupNameDefinitive */ "Plasma Gun",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_plasmarifle.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Cells),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/plsmfire.wav weapons/plsmexpl.wav weapons/plsmhumm.wav sprites/s_pls1.sp2 sprites/s_pls2.sp2",
		/* sortID */ 0,
		/* quantityWarn */ 30
	},

/*QUAKED weapon_plasmabeam (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_beamer/tris.md2"
*/
	{
		/* id */ IT_WEAPON_PLASMABEAM,
		/* className */ "weapon_plasmabeam",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_PlasmaBeam,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_beamer/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_beamer/tris.md2",
		/* icon */ "w_heatbeam",
		/* useName */  "Plasma Beam",
		/* pickupName */  "$item_plasma_beam",
		/* pickupNameDefinitive */ "$item_plasma_beam_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_plasma.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Cells),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/bfg__l1a.wav weapons/bfg_hum.wav",
		/* sortID */ 0,
		/* quantityWarn */ 50
	},

/*QUAKED weapon_lightning (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_light/tris.md2"
*/
	{
		/* id */ IT_WEAPON_THUNDERBOLT,
		/* className */ "weapon_lightning",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_Thunderbolt,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_light/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_light/tris.md2",
		/* icon */ "w_heatbeam",
		/* useName */  "Thunderbolt",
		/* pickupName */  "Thunderbolt",
		/* pickupNameDefinitive */ "Thunderbolt",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_THUNDERBOLT,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_light.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Cells),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/lstart.wav weapons/lhit.wav",
		/* sortID */ 0,
		/* quantityWarn */ 50
	},

/*QUAKED weapon_railgun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_rail/tris.md2"
*/
	{
		/* id */ IT_WEAPON_RAILGUN,
		/* className */ "weapon_railgun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_Railgun,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_rail/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_rail/tris.md2",
		/* icon */ "w_railgun",
		/* useName */  "Railgun",
		/* pickupName */  "$item_railgun",
		/* pickupNameDefinitive */ "$item_railgun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_SLUGS,
		/* chain */ IT_WEAPON_RAILGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_railgun.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Slugs),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/rg_hum.wav"
	},

/*QUAKED weapon_phalanx (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_shotx/tris.md2"
*/
	{
		/* id */ IT_WEAPON_PHALANX,
		/* className */ "weapon_phalanx",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_Phalanx,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_shotx/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_shotx/tris.md2",
		/* icon */ "w_phallanx",
		/* useName */  "Phalanx",
		/* pickupName */  "$item_phalanx",
		/* pickupNameDefinitive */ "$item_phalanx_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_MAGSLUG,
		/* chain */ IT_WEAPON_RAILGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_phalanx.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::MagSlugs),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/plasshot.wav sprites/s_photon.sp2 weapons/rockfly.wav"
	},

/*QUAKED weapon_bfg (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_bfg/tris.md2"
*/
	{
		/* id */ IT_WEAPON_BFG,
		/* className */ "weapon_bfg",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_BFG,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_bfg/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_bfg/tris.md2",
		/* icon */ "w_bfg",
		/* useName */  "BFG10K",
		/* pickupName */  "$item_bfg10k",
		/* pickupNameDefinitive */ "$item_bfg10k_def",
		/* quantity */ 50,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_BFG,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_bfg.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Cells),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "sprites/s_bfg1.sp2 sprites/s_bfg2.sp2 sprites/s_bfg3.sp2 weapons/bfg__f1y.wav weapons/bfg__l1a.wav weapons/bfg__x1b.wav weapons/bfg_hum.wav",
		/* sortID */ 0,
		/* quantityWarn */ 50
	},

/*QUAKED weapon_disintegrator (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_dist/tris.md2"
*/
	{
		/* id */ IT_WEAPON_DISRUPTOR,
		/* className */ "weapon_disintegrator",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponThink */ Weapon_Disruptor,
		/* pickupSound */ "misc/w_pkup.wav",
		/* worldModel */ "models/weapons/g_dist/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ "models/weapons/v_dist/tris.md2",
		/* icon */ "w_disintegrator",
		/* useName */  "Disruptor",
		/* pickupName */  "$item_disruptor",
		/* pickupNameDefinitive */ "$item_disruptor_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_ROUNDS,
		/* chain */ IT_WEAPON_BFG,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* viewWeaponModel */ "#w_disrupt.md2",
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Rounds),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "models/proj/disintegrator/tris.md2 weapons/disrupt.wav weapons/disint2.wav weapons/disrupthit.wav",
	},

	//
	// AMMO ITEMS
	//

/*QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/shells/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_SHELLS,
		/* className */ "ammo_shells",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/items/ammo/shells/medium/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_shells",
		/* useName */  "Shells",
		/* pickupName */  "$item_shells",
		/* pickupNameDefinitive */ "$item_shells_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Shells),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_bullets (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/bullets/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_BULLETS,
		/* className */ "ammo_bullets",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/items/ammo/bullets/medium/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_bullets",
		/* useName */  "Bullets",
		/* pickupName */  "$item_bullets",
		/* pickupNameDefinitive */ "$item_bullets_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Bullets),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_cells (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/cells/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_CELLS,
		/* className */ "ammo_cells",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/items/ammo/cells/medium/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_cells",
		/* useName */  "Cells",
		/* pickupName */  "$item_cells",
		/* pickupNameDefinitive */ "$item_cells_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Cells),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/rockets/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_ROCKETS,
		/* className */ "ammo_rockets",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/items/ammo/rockets/medium/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_rockets",
		/* useName */  "Rockets",
		/* pickupName */  "$item_rockets",
		/* pickupNameDefinitive */ "$item_rockets_def",
		/* quantity */ 5,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Rockets),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_slugs (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/slugs/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_SLUGS,
		/* className */ "ammo_slugs",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/items/ammo/slugs/medium/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_slugs",
		/* useName */  "Slugs",
		/* pickupName */  "$item_slugs",
		/* pickupNameDefinitive */ "$item_slugs_def",
		/* quantity */ 5,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Slugs),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_magslug (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/objects/ammo/tris.md2"
*/
	{
		/* id */ IT_AMMO_MAGSLUG,
		/* className */ "ammo_magslug",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/objects/ammo/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_mslugs",
		/* useName */  "Mag Slug",
		/* pickupName */  "$item_mag_slug",
		/* pickupNameDefinitive */ "$item_mag_slug_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::MagSlugs),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_flechettes (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/am_flechette/tris.md2"
*/
	{
		/* id */ IT_AMMO_FLECHETTES,
		/* className */ "ammo_flechettes",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/ammo/am_flechette/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_flechettes",
		/* useName */  "Flechettes",
		/* pickupName */  "$item_flechettes",
		/* pickupNameDefinitive */ "$item_flechettes_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Flechettes),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_prox (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/am_prox/tris.md2"
*/
	{
		/* id */ IT_AMMO_PROX,
		/* className */ "ammo_prox",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/ammo/am_prox/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_prox",
		/* useName */  "Prox Mines",
		/* pickupName */  "Prox Mines",
		/* pickupNameDefinitive */ "Prox Mines",
		/* quantity */ 5,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::ProxMines),
		/* highValue*/ HighValueItems::None,
		/* precaches */ "models/weapons/g_prox/tris.md2 weapons/proxwarn.wav"
	},

/*QUAKED ammo_nuke (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/g_nuke/tris.md2"
*/
	{
		/* id */ IT_AMMO_NUKE,
		/* className */ "ammo_nuke",
		/* pickup */ Pickup_Nuke,
		/* use */ Use_Nuke,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/weapons/g_nuke/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_nuke",
		/* useName */  "A-M Bomb",
		/* pickupName */  "$item_am_bomb",
		/* pickupNameDefinitive */ "$item_am_bomb_def",
		/* quantity */ 300,
		/* ammo */ IT_AMMO_NUKE,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_AM_BOMB,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "weapons/nukewarn2.wav world/rumble.wav"
	},

/*QUAKED ammo_disruptor (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/am_disr/tris.md2"
*/
	{
		/* id */ IT_AMMO_ROUNDS,
		/* className */ "ammo_disruptor",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/ammo/am_disr/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_disruptor",
		/* useName */  "Rounds",
		/* pickupName */  "$item_rounds",
		/* pickupNameDefinitive */ "$item_rounds_def",
		/* quantity */ 3,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Rounds),
		/* highValue*/ HighValueItems::None
	},

//
// POWERUP ITEMS
//
/*QUAKED item_quad (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/quaddama/tris.md2"
*/
	{
		/* id */ IT_POWERUP_QUAD,
		/* className */ "item_quad",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Quad,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/quaddama/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_quad",
		/* useName */  "Quad Damage",
		/* pickupName */  "$item_quad_damage",
		/* pickupNameDefinitive */ "$item_quad_damage_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_QUAD,
		/* highValue*/ HighValueItems::QuadDamage,
		/* precaches */ "items/damage.wav items/damage2.wav items/damage3.wav ctf/tech2x.wav"
	},

/*QUAKED item_quadfire (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/quadfire/tris.md2"
*/
	{
		/* id */ IT_POWERUP_HASTE,
		/* className */ "item_quadfire",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Haste,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/quadfire/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_quadfire",
		/* useName */  "Haste",
		/* pickupName */  "Haste",
		/* pickupNameDefinitive */ "Haste",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_HASTE,
		/* highValue*/ HighValueItems::Haste,
		/* precaches */ "items/quadfire1.wav items/quadfire2.wav items/quadfire3.wav"
	},

/*QUAKED item_invulnerability (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/invulner/tris.md2"
*/
	{
		/* id */ IT_POWERUP_BATTLESUIT,
		/* className */ "item_invulnerability",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_BattleSuit,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/invulner/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_invulnerability",
		/* useName */  "Battle Suit",
		/* pickupName */  "Battle Suit",
		/* pickupNameDefinitive */ "Battle Suit",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_BATTLESUIT,
		/* highValue*/ HighValueItems::BattleSuit,
		/* precaches */ "items/protect.wav items/protect2.wav items/protect4.wav"
	},

/*QUAKED item_invisibility (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/cloaker/tris.md2"
*/
	{
		/* id */ IT_POWERUP_INVISIBILITY,
		/* className */ "item_invisibility",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Invisibility,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/cloaker/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_cloaker",
		/* useName */  "Invisibility",
		/* pickupName */  "$item_invisibility",
		/* pickupNameDefinitive */ "$item_invisibility_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_INVISIBILITY,
		/* highValue*/ HighValueItems::Invisibility
	},

/*QUAKED item_silencer (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/silencer/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SILENCER,
		/* className */ "item_silencer",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_Silencer,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/silencer/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_silencer",
		/* useName */  "Silencer",
		/* pickupName */  "$item_silencer",
		/* pickupNameDefinitive */ "$item_silencer_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_SILENCER,
		/* highValue*/ HighValueItems::None
	},

/*QUAKED item_breather (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/breather/tris.md2"
*/
	{
		/* id */ IT_POWERUP_REBREATHER,
		/* className */ "item_breather",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_Breather,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/breather/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_rebreather",
		/* useName */  "Rebreather",
		/* pickupName */  "$item_rebreather",
		/* pickupNameDefinitive */ "$item_rebreather_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_TIMED | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_REBREATHER,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "items/airout.wav"
	},

/*QUAKED item_enviro (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/enviro/tris.md2"
*/
	{
		/* id */ IT_POWERUP_ENVIROSUIT,
		/* className */ "item_enviro",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_EnviroSuit,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/enviro/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_envirosuit",
		/* useName */  "Environment Suit",
		/* pickupName */  "$item_environment_suit",
		/* pickupNameDefinitive */ "$item_environment_suit_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_TIMED | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_ENVIROSUIT,
		/* highValue*/ HighValueItems::EmpathyShield,
		/* precaches */ "items/airout.wav"
	},

/*QUAKED item_empathy (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/empathy/tris.md2"
*/
	{
		/* id */ IT_POWERUP_EMPATHY_SHIELD,
		/* className */ "item_empathy",		// original: item_artifact_empathy_shields
		/* pickup */ Pickup_Powerup,
		/* use */ Use_EmpathyShield,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/empathy/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_fixme",
		/* useName */  "Empathy Shield",
		/* pickupName */  "Empathy Shield",
		/* pickupNameDefinitive */ "Empathy Shield",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_EMPATHY_SHIELD,
		/* highValue*/ HighValueItems::EmpathyShield,
		/* precaches */ "items/empathy_use.wav items/empathy_hit.wav items/suit2.wav"
	},

/*QUAKED item_antigrav (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/empathy/tris.md2"
*/
	{
		/* id */ IT_POWERUP_ANTIGRAV_BELT,
		/* className */ "item_antigrav",		// original: item_powerup_belt
		/* pickup */ Pickup_Powerup,
		/* use */ Use_AntiGravBelt,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/belt_pickup.wav",
		/* worldModel */ "models/items/antigrav/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_fixme",
		/* useName */  "Anti-Gravity Belt",
		/* pickupName */  "Anti-Gravity Belt",
		/* pickupNameDefinitive */ "Anti-Gravity Belt",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_ANTIGRAV_BELT,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "items/belt_pickup.wav belt/belt_use.wav items/suit2.wav"
	},

/*QUAKED item_ancient_head (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Special item that gives +2 to maximum health
model="models/items/c_head/tris.md2"
*/
	{
		/* id */ IT_ANCIENT_HEAD,
		/* className */ "item_ancient_head",
		/* pickup */ Pickup_LegacyHead,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/c_head/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_fixme",
		/* useName */  "Ancient Head",
		/* pickupName */  "$item_ancient_head",
		/* pickupNameDefinitive */ "$item_ancient_head_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH | IF_NOT_RANDOM,
	},

/*QUAKED item_legacy_head (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Special item that gives +5 to maximum health.
model="models/items/legacyhead/tris.md2"
*/
	{
		/* id */ IT_LEGACY_HEAD,
		/* className */ "item_legacy_head",
		/* pickup */ Pickup_LegacyHead,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/legacyhead/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_fixme",
		/* useName */  "Ranger's Head",
		/* pickupName */  "Ranger's Head",
		/* pickupNameDefinitive */ "Ranger's Head",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH | IF_NOT_RANDOM,
	},

/*QUAKED item_adrenaline (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Gives +1 to maximum health, +5 in deathmatch.
model="models/items/adrenal/tris.md2"
*/
	{
		/* id */ IT_ADRENALINE,
		/* className */ "item_adrenaline",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_Adrenaline,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/adrenal/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_adrenaline",
		/* useName */  "Adrenaline",
		/* pickupName */  "$item_adrenaline",
		/* pickupNameDefinitive */ "$item_adrenaline_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_ADRENALINE,
		/* highValue*/ HighValueItems::None,
		/* precache */ "items/n_health.wav"
	},

/*QUAKED item_bandolier (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/band/tris.md2"
*/
	{
		/* id */ IT_BANDOLIER,
		/* className */ "item_bandolier",
		/* pickup */ Pickup_Bandolier,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/band/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_bandolier",
		/* useName */  "Bandolier",
		/* pickupName */  "$item_bandolier",
		/* pickupNameDefinitive */ "$item_bandolier_def",
		/* quantity */ 30,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED
	},

/*QUAKED item_pack (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/pack/tris.md2"
*/
	{
		/* id */ IT_PACK,
		/* className */ "item_pack",
		/* pickup */ Pickup_Pack,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/pack/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_pack",
		/* useName */  "Ammo Pack",
		/* pickupName */  "$item_ammo_pack",
		/* pickupNameDefinitive */ "$item_ammo_pack_def",
		/* quantity */ 90,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED
	},

/*QUAKED item_ir_goggles (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Infrared vision.
model="models/items/goggles/tris.md2"
*/
	{
		/* id */ IT_IR_GOGGLES,
		/* className */ "item_ir_goggles",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_IR,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/goggles/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_ir",
		/* useName */  "IR Goggles",
		/* pickupName */  "$item_ir_goggles",
		/* pickupNameDefinitive */ "$item_ir_goggles_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_IR_GOGGLES,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "misc/ir_start.wav"
	},

/*QUAKED item_double (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ddamage/tris.md2"
*/
	{
		/* id */ IT_POWERUP_DOUBLE,
		/* className */ "item_double",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Double,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/ddamage/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_double",
		/* useName */  "Double Damage",
		/* pickupName */  "$item_double_damage",
		/* pickupNameDefinitive */ "$item_double_damage_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_DOUBLE,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "misc/ddamage1.wav misc/ddamage2.wav misc/ddamage3.wav ctf/tech2x.wav"
	},

/*QUAKED item_sphere_vengeance (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/vengnce/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SPHERE_VENGEANCE,
		/* className */ "item_sphere_vengeance",
		/* pickup */ Pickup_Sphere,
		/* use */ Use_Vengeance,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/vengnce/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_vengeance",
		/* useName */  "vengeance sphere",
		/* pickupName */  "$item_vengeance_sphere",
		/* pickupNameDefinitive */ "$item_vengeance_sphere_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_SPHERE | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_SPHERE_VENGEANCE,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "spheres/v_idle.wav"
	},

/*QUAKED item_sphere_hunter (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/hunter/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SPHERE_HUNTER,
		/* className */ "item_sphere_hunter",
		/* pickup */ Pickup_Sphere,
		/* use */ Use_Hunter,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/hunter/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_hunter",
		/* useName */  "hunter sphere",
		/* pickupName */  "$item_hunter_sphere",
		/* pickupNameDefinitive */ "$item_hunter_sphere_def",
		/* quantity */ 120,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_SPHERE | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_SPHERE_HUNTER,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "spheres/h_idle.wav spheres/h_active.wav spheres/h_lurk.wav"
	},

/*QUAKED item_sphere_defender (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/defender/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SPHERE_DEFENDER,
		/* className */ "item_sphere_defender",
		/* pickup */ Pickup_Sphere,
		/* use */ Use_Defender,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/defender/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_defender",
		/* useName */  "defender sphere",
		/* pickupName */  "$item_defender_sphere",
		/* pickupNameDefinitive */ "$item_defender_sphere_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_SPHERE | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_SPHERE_DEFENDER,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "models/objects/laser/tris.md2 models/items/shell/tris.md2 spheres/d_idle.wav"
	},

/*QUAKED item_doppleganger (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/dopple/tris.md2"
*/
	{
		/* id */ IT_DOPPELGANGER,
		/* className */ "item_doppleganger",
		/* pickup */ Pickup_Doppelganger,
		/* use */ Use_Doppelganger,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/dopple/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_doppleganger",
		/* useName */  "Doppelganger",
		/* pickupName */  "$item_doppleganger",
		/* pickupNameDefinitive */ "$item_doppleganger_def",
		/* quantity */ 90,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_DOPPELGANGER,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "models/objects/dopplebase/tris.md2 models/items/spawngro3/tris.md2 medic_commander/monsterspawn1.wav models/items/hunter/tris.md2 models/items/vengnce/tris.md2",
	},

/* Tag Token */
	{
		/* id */ IT_TAG_TOKEN,
		/* className */ nullptr,
		/* pickup */ nullptr,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/tagtoken/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB | EF_TAGTRAIL,
		/* viewModel */ nullptr,
		/* icon */ "i_tagtoken",
		/* useName */  "Tag Token",
		/* pickupName */  "$item_tag_token",
		/* pickupNameDefinitive */ "$item_tag_token_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_NOT_GIVEABLE
	},

//
// KEYS
//
/*QUAKED key_data_cd (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key for computer centers.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/data_cd/tris.md2"
*/
	{
		/* id */ IT_KEY_DATA_CD,
		/* className */ "key_data_cd",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/keys/data_cd/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "k_datacd",
		/* useName */  "Data CD",
		/* pickupName */  "$item_data_cd",
		/* pickupNameDefinitive */ "$item_data_cd_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_power_cube (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN NO_TOUCH x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Power Cubes for warehouse.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/power/tris.md2"
*/
	{
		/* id */ IT_KEY_POWER_CUBE,
		/* className */ "key_power_cube",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/keys/power/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "k_powercube",
		/* useName */  "Power Cube",
		/* pickupName */  "$item_power_cube",
		/* pickupNameDefinitive */ "$item_power_cube_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_explosive_charges (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN NO_TOUCH x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Explosive Charges - for N64.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/n64/charge/tris.md2"
*/
	{
		/* id */ IT_KEY_EXPLOSIVE_CHARGES,
		/* className */ "key_explosive_charges",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/n64/charge/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "n64/i_charges",
		/* useName */  "Explosive Charges",
		/* pickupName */  "$item_explosive_charges",
		/* pickupNameDefinitive */ "$item_explosive_charges_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_yellow_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Normal door key - Yellow - for N64.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/n64/yellow_key/tris.md2"
*/
	{
		/* id */ IT_KEY_YELLOW,
		/* className */ "key_yellow_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/n64/yellow_key/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "n64/i_yellow_key",
		/* useName */  "Yellow Key",
		/* pickupName */  "$item_yellow_key",
		/* pickupNameDefinitive */ "$item_yellow_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_power_core (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Power Core key - for N64.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/n64/power_core/tris.md2"
*/
	{
		/* id */ IT_KEY_POWER_CORE,
		/* className */ "key_power_core",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/n64/power_core/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "k_pyramid",
		/* useName */  "Power Core",
		/* pickupName */  "$item_power_core",
		/* pickupNameDefinitive */ "$item_power_core_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_pyramid (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key for the entrance of jail3.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/pyramid/tris.md2"
*/
	{
		/* id */ IT_KEY_PYRAMID,
		/* className */ "key_pyramid",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/keys/pyramid/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "k_pyramid",
		/* useName */  "Pyramid Key",
		/* pickupName */  "$item_pyramid_key",
		/* pickupNameDefinitive */ "$item_pyramid_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_data_spinner (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key for the city computer.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/spinner/tris.md2"
*/
	{
		/* id */ IT_KEY_DATA_SPINNER,
		/* className */ "key_data_spinner",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/keys/spinner/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "k_dataspin",
		/* useName */  "Data Spinner",
		/* pickupName */  "$item_data_spinner",
		/* pickupNameDefinitive */ "$item_data_spinner_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_pass (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Security pass for the security level.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/pass/tris.md2"
*/
	{
		/* id */ IT_KEY_PASS,
		/* className */ "key_pass",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/keys/pass/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "k_security",
		/* useName */  "Security Pass",
		/* pickupName */  "$item_security_pass",
		/* pickupNameDefinitive */ "$item_security_pass_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_blue_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Normal door key - Blue.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/key/tris.md2"
*/
	{
		/* id */ IT_KEY_BLUE_KEY,
		/* className */ "key_blue_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/keys/key/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "k_bluekey",
		/* useName */  "Blue Key",
		/* pickupName */  "$item_blue_key",
		/* pickupNameDefinitive */ "$item_blue_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_red_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Normal door key - Red.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/red_key/tris.md2"
*/
	{
		/* id */ IT_KEY_RED_KEY,
		/* className */ "key_red_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/keys/red_key/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "k_redkey",
		/* useName */  "Red Key",
		/* pickupName */  "$item_red_key",
		/* pickupNameDefinitive */ "$item_red_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_green_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Normal door key - Green.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/green_key/tris.md2"
*/
	{
		/* id */ IT_KEY_GREEN_KEY,
		/* className */ "key_green_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/keys/green_key/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "k_green",
		/* useName */  "Green Key",
		/* pickupName */  "$item_green_key",
		/* pickupNameDefinitive */ "$item_green_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_commander_head (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key - Tank Commander's Head.
model="models/monsters/commandr/head/tris.md2"
*/
	{
		/* id */ IT_KEY_COMMANDER_HEAD,
		/* className */ "key_commander_head",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/monsters/commandr/head/tris.md2",
		/* worldModelFlags */ EF_GIB,
		/* viewModel */ nullptr,
		/* icon */ "k_comhead",
		/* useName */  "Commander's Head",
		/* pickupName */  "$item_commanders_head",
		/* pickupNameDefinitive */ "$item_commanders_head_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_airstrike_target (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key - Airstrike Target for strike.
model="models/items/keys/target/tris.md2"
*/
	{
		/* id */ IT_KEY_AIRSTRIKE,
		/* className */ "key_airstrike_target",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/keys/target/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_airstrike",
		/* useName */  "Airstrike Marker",
		/* pickupName */  "$item_airstrike_marker",
		/* pickupNameDefinitive */ "$item_airstrike_marker_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_nuke_container (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_nuke/tris.md2"
*/
	{
		/* id */ IT_KEY_NUKE_CONTAINER,
		/* className */ "key_nuke_container",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/weapons/g_nuke/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_contain",
		/* useName */  "Antimatter Pod",
		/* pickupName */  "$item_antimatter_pod",
		/* pickupNameDefinitive */ "$item_antimatter_pod_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY,
	},

/*QUAKED key_nuke (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_nuke/tris.md2"
*/
	{
		/* id */ IT_KEY_NUKE,
		/* className */ "key_nuke",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/weapons/g_nuke/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_nuke",
		/* useName */  "Antimatter Bomb",
		/* pickupName */  "$item_antimatter_bomb",
		/* pickupNameDefinitive */ "$item_antimatter_bomb_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY,
	},

/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Health - Stimpack.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/healing/stimpack/tris.md2"
*/
	{
		/* id */ IT_HEALTH_SMALL,
		/* className */ "item_health_small",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/s_health.wav",
		/* worldModel */ "models/items/healing/stimpack/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "i_health",
		/* useName */  "Health",
		/* pickupName */  "$item_stimpack",
		/* pickupNameDefinitive */ "$item_stimpack_def",
		/* quantity */ 3,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ HEALTH_IGNORE_MAX,
		/* highValue*/ HighValueItems::None
	},

/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Health - First Aid.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/healing/medium/tris.md2"
*/
	{
		/* id */ IT_HEALTH_MEDIUM,
		/* className */ "item_health",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/n_health.wav",
		/* worldModel */ "models/items/healing/medium/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "i_health",
		/* useName */  "Health",
		/* pickupName */  "$item_small_medkit",
		/* pickupNameDefinitive */ "$item_small_medkit_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH
	},

/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Health - Medkit.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/healing/large/tris.md2"
*/
	{
		/* id */ IT_HEALTH_LARGE,
		/* className */ "item_health_large",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/l_health.wav",
		/* worldModel */ "models/items/healing/large/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "i_health",
		/* useName */  "Health",
		/* pickupName */  "$item_large_medkit",
		/* pickupNameDefinitive */ "$item_large_medkit",
		/* quantity */ 25,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH
	},

/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Health - Mega Health.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/mega_h/tris.md2"
*/
	{
		/* id */ IT_HEALTH_MEGA,
		/* className */ "item_health_mega",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/m_health.wav",
		/* worldModel */ "models/items/mega_h/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "p_megahealth",
		/* useName */  "Mega Health",
		/* pickupName */  "Mega Health",
		/* pickupNameDefinitive */ "Mega Health",
		/* quantity */ 100,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ HEALTH_IGNORE_MAX | HEALTH_TIMED
	},

/*QUAKED item_flag_team_red (1 0.2 0) (-16 -16 -24) (16 16 32) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Red Flag for CTF.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="players/male/flag1.md2"
*/
	{
		/* id */ IT_FLAG_RED,
		/* className */ ITEM_CTF_FLAG_RED,
		/* pickup */ CTF_PickupFlag,
		/* use */ nullptr,
		/* drop */ CTF_DropFlag, //Should this be null if we don't want players to drop it manually?
		/* weaponThink */ nullptr,
		/* pickupSound */ "ctf/flagtk.wav",
		/* worldModel */ "players/male/flag1.md2",
		/* worldModelFlags */ EF_FLAG_RED,
		/* viewModel */ nullptr,
		/* icon */ "i_ctf1",
		/* useName */  "Red Flag",
		/* pickupName */  "$item_red_flag",
		/* pickupNameDefinitive */ "$item_red_flag_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_NONE,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ 0,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "ctf/flagcap.wav"
	},

/*QUAKED item_flag_team_blue (1 0.2 0) (-16 -16 -24) (16 16 32) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Blue Flag for CTF.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="players/male/flag2.md2"
*/
	{
		/* id */ IT_FLAG_BLUE,
		/* className */ ITEM_CTF_FLAG_BLUE,
		/* pickup */ CTF_PickupFlag,
		/* use */ nullptr,
		/* drop */ CTF_DropFlag,
		/* weaponThink */ nullptr,
		/* pickupSound */ "ctf/flagtk.wav",
		/* worldModel */ "players/male/flag2.md2",
		/* worldModelFlags */ EF_FLAG_BLUE,
		/* viewModel */ nullptr,
		/* icon */ "i_ctf2",
		/* useName */  "Blue Flag",
		/* pickupName */  "$item_blue_flag",
		/* pickupNameDefinitive */ "$item_blue_flag_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_NONE,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ 0,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "ctf/flagcap.wav"
	},

/*QUAKED item_flag_team_neutral (1 0.2 0) (-16 -16 -24) (16 16 32) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Neutral Flag for One Flag.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="players/male/flag2.md2"
*/
	{
		/* id */ IT_FLAG_NEUTRAL,
		/* className */ ITEM_CTF_FLAG_NEUTRAL,
		/* pickup */ CTF_PickupFlag,
		/* use */ nullptr,
		/* drop */ CTF_DropFlag,
		/* weaponThink */ nullptr,
		/* pickupSound */ "ctf/flagtk.wav",
		/* worldModel */ "players/male/flag2.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "i_ctfn",
		/* useName */  "Neutral Flag",
		/* pickupName */  "Neutral Flag",
		/* pickupNameDefinitive */ "Neutral Flag",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_NONE,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ 0,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "ctf/flagcap.wav"
	},

/* Disruptor Shield Tech */
	{
		/* id */ IT_TECH_DISRUPTOR_SHIELD,
		/* className */ "item_tech1",
		/* pickup */ Tech_Pickup,
		/* use */ nullptr,
		/* drop */ Tech_Drop,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/ctf/resistance/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "tech1",
		/* useName */  "Disruptor Shield",
		/* pickupName */  "$item_disruptor_shield",
		/* pickupNameDefinitive */ "$item_disruptor_shield_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_TECH_DISRUPTOR_SHIELD,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "ctf/tech1.wav"
	},

/* Power Amplifier Tech */
	{
		/* id */ IT_TECH_POWER_AMP,
		/* className */ "item_tech2",
		/* pickup */ Tech_Pickup,
		/* use */ nullptr,
		/* drop */ Tech_Drop,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/ctf/strength/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "tech2",
		/* useName */  "Power Amplifier",
		/* pickupName */  "$item_power_amplifier",
		/* pickupNameDefinitive */ "$item_power_amplifier_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_TECH_POWER_AMP,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "ctf/tech2.wav ctf/tech2x.wav"
	},

/* Time Accel Tech */
	{
		/* id */ IT_TECH_TIME_ACCEL,
		/* className */ "item_tech3",
		/* pickup */ Tech_Pickup,
		/* use */ nullptr,
		/* drop */ Tech_Drop,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/ctf/haste/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "tech3",
		/* useName */  "Time Accel",
		/* pickupName */  "$item_time_accel",
		/* pickupNameDefinitive */ "$item_time_accel_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_TECH_TIME_ACCEL,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "ctf/tech3.wav"
	},

/* AutoDoc Tech */
	{
		/* id */ IT_TECH_AUTODOC,
		/* className */ "item_tech4",
		/* pickup */ Tech_Pickup,
		/* use */ nullptr,
		/* drop */ Tech_Drop,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/ctf/regeneration/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "tech4",
		/* useName */  "AutoDoc",
		/* pickupName */  "$item_autodoc",
		/* pickupNameDefinitive */ "$item_autodoc_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_TECH_AUTODOC,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "ctf/tech4.wav"
	},

/*QUAKED ammo_shells_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/shells/large/tris.md2"
*/
	{
		/* id */ IT_AMMO_SHELLS_LARGE ,
		/* className */ "ammo_shells_large",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/vault/items/ammo/shells/large/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_shells",
		/* useName */  "Large Shells",
		/* pickupName */  "Large Shells",
		/* pickupNameDefinitive */ "Large Shells",
		/* quantity */ 20,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Shells),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_shells_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/shells/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_SHELLS_SMALL,
		/* className */ "ammo_shells_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/vault/items/ammo/shells/small/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_shells",
		/* useName */  "Small Shells",
		/* pickupName */  "Small Shells",
		/* pickupNameDefinitive */ "Small Shells",
		/* quantity */ 6,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Shells),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_bullets_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/bullets/large/tris.md2"
*/
	{
		/* id */ IT_AMMO_BULLETS_LARGE,
		/* className */ "ammo_bullets_large",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/vault/items/ammo/bullets/large/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_bullets",
		/* useName */  "Large Bullets",
		/* pickupName */  "Large Bullets",
		/* pickupNameDefinitive */ "Large Bullets",
		/* quantity */ 100,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Bullets),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_bullets_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/bullets/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_BULLETS_SMALL,
		/* className */ "ammo_bullets_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/vault/items/ammo/bullets/small/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_bullets",
		/* useName */  "Small Bullets",
		/* pickupName */  "Small Bullets",
		/* pickupNameDefinitive */ "Small Bullets",
		/* quantity */ 25,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Bullets),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_cells_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/cells/large/tris.md2"
*/
	{
		/* id */ IT_AMMO_CELLS_LARGE,
		/* className */ "ammo_cells_large",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/vault/items/ammo/cells/large/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_cells",
		/* useName */  "Large Cells",
		/* pickupName */  "Large Cells",
		/* pickupNameDefinitive */ "Large Cells",
		/* quantity */ 100,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Cells),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_cells_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/cells/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_CELLS_SMALL,
		/* className */ "ammo_cells_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/vault/items/ammo/cells/small/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_cells",
		/* useName */  "Small Cells",
		/* pickupName */  "Small Cells",
		/* pickupNameDefinitive */ "Small Cells",
		/* quantity */ 20,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Cells),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_rockets_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/rockets/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_ROCKETS_SMALL,
		/* className */ "ammo_rockets_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/vault/items/ammo/rockets/small/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_rockets",
		/* useName */  "Small Rockets",
		/* pickupName */  "Small Rockets",
		/* pickupNameDefinitive */ "Small Rockets",
		/* quantity */ 2,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Rockets),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_slugs_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/slugs/large/tris.md2"
*/
	{
		/* id */ IT_AMMO_SLUGS_LARGE,
		/* className */ "ammo_slugs_large",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/vault/items/ammo/slugs/large/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_slugs",
		/* useName */  "Large Slugs",
		/* pickupName */  "Large Slugs",
		/* pickupNameDefinitive */ "Large Slugs",
		/* quantity */ 20,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Slugs),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED ammo_slugs_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/slugs/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_SLUGS_SMALL,
		/* className */ "ammo_slugs_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponThink */ nullptr,
		/* pickupSound */ "misc/am_pkup.wav",
		/* worldModel */ "models/vault/items/ammo/slugs/small/tris.md2",
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "a_slugs",
		/* useName */  "Small Slugs",
		/* pickupName */  "Small Slugs",
		/* pickupNameDefinitive */ "Small Slugs",
		/* quantity */ 3,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ static_cast<int>(AmmoID::Slugs),
		/* highValue*/ HighValueItems::None
	},

/*QUAKED item_teleporter (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/nuke/tris.md2"
*/
	{
		/* id */ IT_TELEPORTER,
		/* className */ "item_teleporter",
		/* pickup */ Pickup_Teleporter,
		/* use */ Use_Teleporter,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/vault/items/ammo/nuke/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_fixme",
		/* useName */  "Personal Teleporter",
		/* pickupName */  "Personal Teleporter",
		/* pickupNameDefinitive */ "Personal Teleporter",
		/* quantity */ 120,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF
	},

/*QUAKED item_regen (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/invulner/tris.md2"
*/
	{
		/* id */ IT_POWERUP_REGEN,
		/* className */ "item_regen",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Regeneration,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/invulner/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_fixme",
		/* useName */  "Regeneration",
		/* pickupName */  "Regeneration",
		/* pickupNameDefinitive */ "Regeneration",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_REGENERATION,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "items/protect.wav"
	},

/*QUAKED item_foodcube (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Meaty cube o' health
model="models/objects/trapfx/tris.md2"
*/
	{
		/* id */ IT_FOODCUBE,
		/* className */ "item_foodcube",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/n_health.wav",
		/* worldModel */ "models/objects/trapfx/tris.md2",
		/* worldModelFlags */ EF_GIB,
		/* viewModel */ nullptr,
		/* icon */ "i_health",
		/* useName */  "Meaty Cube",
		/* pickupName */  "Meaty Cube",
		/* pickupNameDefinitive */ "Meaty Cube",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ HEALTH_IGNORE_MAX,
		/* highValue*/ HighValueItems::None
	},

/*QUAKED item_ball (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Big ol' ball
models/items/ammo/grenades/medium/tris.md2"
*/
	{
		/* id */ IT_BALL,
		/* className */ "item_ball",
		/* pickup */ Pickup_Ball,
		/* use */ Use_Ball,
		/* drop */ Drop_Ball,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/ammo/grenades/medium/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_help",
		/* useName */  "Ball",
		/* pickupName */  "Ball",
		/* pickupNameDefinitive */ "Ball",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_POWERUP | IF_POWERUP_WHEEL | IF_NOT_RANDOM,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_BALL,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "",
		/* sortID */ -1
	},

/*QUAKED item_spawn_protect (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/invulner/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SPAWN_PROTECTION,
		/* className */ "item_spawn_protect",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Spawn_Protection,
		/* drop */ Drop_General,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/invulner/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_invulnerability",
		/* useName */  "Spawn Protection",
		/* pickupName */  "Spawn Protection",
		/* pickupNameDefinitive */ "Spawn Protection",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_SPAWN_PROTECTION,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "items/protect.wav items/protect2.wav items/protect4.wav"
	},

/* Flashlight */
	{
		/* id */ IT_FLASHLIGHT,
		/* className */ "item_flashlight",
		/* pickup */ Pickup_General,
		/* use */ Use_Flashlight,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/items/flashlight/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "p_torch",
		/* useName */  "Flashlight",
		/* pickupName */  "$item_flashlight",
		/* pickupNameDefinitive */ "$item_flashlight_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF | IF_NOT_RANDOM,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_FLASHLIGHT,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "items/flashlight_on.wav items/flashlight_off.wav",
		/* sortID */ -1
	},

/* Compass */
	{
		/* id */ IT_COMPASS,
		/* className */ "item_compass",
		/* pickup */ nullptr,
		/* use */ Use_Compass,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ nullptr,
		/* worldModel */ nullptr,
		/* worldModelFlags */ EF_NONE,
		/* viewModel */ nullptr,
		/* icon */ "p_compass",
		/* useName */  "Compass",
		/* pickupName */  "$item_compass",
		/* pickupNameDefinitive */ "$item_compass_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ POWERUP_COMPASS,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "misc/help_marker.wav",
		/* sortID */ -2
	},

/* Harvester Skull */
	{
		/* id */ IT_HARVESTER_SKULL,
		/* className */ "item_harvester_skull",
		/* pickup */ Harvester_PickupSkull,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponThink */ nullptr,
		/* pickupSound */ "items/pkup.wav",
		/* worldModel */ "models/objects/gibs/skull/tris.md2",
		/* worldModelFlags */ EF_ROTATE | EF_BOB,
		/* viewModel */ nullptr,
		/* icon */ "i_skull",
		/* useName */  "Skull",
		/* pickupName */  "Skull",
		/* pickupNameDefinitive */ "Skull",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_NOT_RANDOM | IF_NOT_GIVEABLE,
		/* viewWeaponModel */ nullptr,
		/* armorInfo */ nullptr,
		/* tag */ 0,
		/* highValue*/ HighValueItems::None,
		/* precaches */ "",
		/* sortID */ -2
	},
} };
// clang-format on
