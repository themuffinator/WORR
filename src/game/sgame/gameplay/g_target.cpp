/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

g_target.cpp (Game Target Entities) This file implements the logic for a wide variety of
"target_*" entities. These entities are the core of Quake II's scripting system, allowing
mappers to create dynamic and interactive levels. They are typically invisible and are activated
by other entities (like triggers or buttons). Key Responsibilities: - Event Triggers: Implements
entities that create visual or audio effects (`target_temp_entity`, `target_explosion`,
`target_speaker`). - Level Flow Control: Contains logic for entities that control the
progression of the game, such as changing levels (`target_changelevel`) or triggering events
based on player achievements (`target_secret`, `target_goal`). - World Interaction: Implements
entities that modify the game world's properties, such as `target_gravity` or `target_sky`. -
Scripting Utilities: Provides helper entities like `target_relay` (to chain triggers) and
`target_delay` (to time events).*/

#include "../g_local.hpp"
#include "../../bgame/char_array_utils.hpp"

/*QUAKED target_temp_entity (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fire an origin based temp entity event to the clients.
"style"		type byte
*/
static USE(Use_Target_Tent) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(ent->style);
	gi.WritePosition(ent->s.origin);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);
}

void SP_target_temp_entity(gentity_t* ent) {
	if (level.isN64 && ent->style == 27)
		ent->style = TE_TELEPORT_EFFECT;

	ent->use = Use_Target_Tent;
}

//==========================================================

/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) LOOPED-ON LOOPED-OFF RELIABLE x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
"noise" :	sound file to play
"volume" :	0.0 to 1.0
"attenuation"
-1 = none, send to whole level
1 = normal fighting sounds
2 = idle sound level
3 = ambient sound level

Normal sounds play each time the target is used.

LOOPED-ON and LOOPED-OFF spawnflags toggle a looping sound on/off.
The RELIABLE flag can be set for crucial voiceovers.

[Paril-KEX] looped sounds are by default atten 3 / vol 1, and the use function toggles it on/off.
*/

constexpr SpawnFlags SPAWNFLAG_SPEAKER_LOOPED_ON = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_SPEAKER_LOOPED_OFF = 2_spawnflag;
constexpr SpawnFlags SPAWNFLAG_SPEAKER_RELIABLE = 4_spawnflag;
constexpr SpawnFlags SPAWNFLAG_SPEAKER_NO_STEREO = 8_spawnflag;

static USE(Use_Target_Speaker) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	soundchan_t chan;

	if (ent->spawnFlags.has(SPAWNFLAG_SPEAKER_LOOPED_ON | SPAWNFLAG_SPEAKER_LOOPED_OFF)) { // looping sound toggles
		if (ent->s.sound)
			ent->s.sound = 0; // turn it off
		else
			ent->s.sound = ent->noiseIndex; // start it
	}
	else { // normal sound
		if (ent->spawnFlags.has(SPAWNFLAG_SPEAKER_RELIABLE))
			chan = CHAN_VOICE | CHAN_RELIABLE;
		else
			chan = CHAN_VOICE;
		// use a positionedSound, because this entity won't normally be
		// sent to any clients because it is invisible
		gi.positionedSound(ent->s.origin, ent, chan, ent->noiseIndex, ent->volume, ent->attenuation, 0);
	}
}

void SP_target_speaker(gentity_t* ent) {
	if (!st.noise) {
		gi.Com_PrintFmt("{}: no noise set\n", *ent);
		return;
	}

	if (!strstr(st.noise, ".wav"))
		ent->noiseIndex = gi.soundIndex(G_Fmt("{}.wav", st.noise).data());
	else
		ent->noiseIndex = gi.soundIndex(st.noise);

	if (!ent->volume)
		ent->volume = ent->s.loopVolume = 1.0;

	if (!ent->attenuation) {
		if (ent->spawnFlags.has(SPAWNFLAG_SPEAKER_LOOPED_OFF | SPAWNFLAG_SPEAKER_LOOPED_ON))
			ent->attenuation = ATTN_STATIC;
		else
			ent->attenuation = ATTN_NORM;
	}
	else if (ent->attenuation == -1) // use -1 so 0 defaults to 1
	{
		if (ent->spawnFlags.has(SPAWNFLAG_SPEAKER_LOOPED_OFF | SPAWNFLAG_SPEAKER_LOOPED_ON)) {
			ent->attenuation = ATTN_LOOP_NONE;
			ent->svFlags |= SVF_NOCULL;
		}
		else
			ent->attenuation = ATTN_NONE;
	}

	ent->s.loopAttenuation = ent->attenuation;

	// check for prestarted looping sound
	if (ent->spawnFlags.has(SPAWNFLAG_SPEAKER_LOOPED_ON))
		ent->s.sound = ent->noiseIndex;

	if (ent->spawnFlags.has(SPAWNFLAG_SPEAKER_NO_STEREO))
		ent->s.renderFX |= RF_NO_STEREO;

	ent->use = Use_Target_Speaker;

	// must link the entity so we get areas and clusters so
	// the server can determine who to send updates to
	gi.linkEntity(ent);
}


//==========================================================

/*QUAKED ambient_suck_wind (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
Legacy Quake 1 ambient wind suction sound.
Places a looped positional ambience using target_speaker.

Usage:
- Place anywhere you want a constant wind-suction hum.

Keys:
- volume       : 0.0 to 1.0 (optional, defaults to 1.0)
- attenuation  : -1, 1, 2, 3 (optional; overrides default)
				 -1 = none (global), 1 = normal, 2 = idle, 3 = static
Notes:
- Prefer target_speaker for new content. This exists for backward-compat only.
*/
void SP_ambient_suck_wind(gentity_t* ent) {
	st.noise = "ambience/suck1.wav";
	if (!ent->volume) ent->volume = 1.0f;
	if (!ent->attenuation) ent->attenuation = ATTN_STATIC;
	ent->spawnFlags |= SPAWNFLAG_SPEAKER_LOOPED_ON;
	SP_target_speaker(ent);
}

/*QUAKED ambient_drone (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
Legacy Quake 1 low drone ambience.
Creates a looped positional drone using target_speaker.

Keys:
- volume       : 0.0 to 1.0 (optional, defaults to 0.5)
- attenuation  : -1, 1, 2, 3 (optional)
*/
void SP_ambient_drone(gentity_t* ent) {
	st.noise = "ambience/drone6.wav";
	if (!ent->volume) ent->volume = 0.5f;
	if (!ent->attenuation) ent->attenuation = ATTN_STATIC;
	ent->spawnFlags |= SPAWNFLAG_SPEAKER_LOOPED_ON;
	SP_target_speaker(ent);
}

/*QUAKED ambient_flouro_buzz (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
Legacy fluorescent light buzz ambience.
Creates a looped positional buzz using target_speaker.

Keys:
- volume       : 0.0 to 1.0 (optional, defaults to 1.0)
- attenuation  : -1, 1, 2, 3 (optional)
*/
void SP_ambient_flouro_buzz(gentity_t* ent) {
	st.noise = "ambience/buzz1.wav";
	if (!ent->volume) ent->volume = 1.0f;
	if (!ent->attenuation) ent->attenuation = ATTN_STATIC;
	ent->spawnFlags |= SPAWNFLAG_SPEAKER_LOOPED_ON;
	SP_target_speaker(ent);
}

/*QUAKED ambient_drip (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
Legacy dripping water ambience.
Creates a looped positional drip using target_speaker.

Keys:
- volume       : 0.0 to 1.0 (optional, defaults to 0.5)
- attenuation  : -1, 1, 2, 3 (optional)
*/
void SP_ambient_drip(gentity_t* ent) {
	st.noise = "ambience/drip1.wav";
	if (!ent->volume) ent->volume = 0.5f;
	if (!ent->attenuation) ent->attenuation = ATTN_STATIC;
	ent->spawnFlags |= SPAWNFLAG_SPEAKER_LOOPED_ON;
	SP_target_speaker(ent);
}

/*QUAKED ambient_comp_hum (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
Legacy computer hum ambience.
Creates a looped positional hum using target_speaker.

Keys:
- volume       : 0.0 to 1.0 (optional, defaults to 1.0)
- attenuation  : -1, 1, 2, 3 (optional)
*/
void SP_ambient_comp_hum(gentity_t* ent) {
	st.noise = "ambience/comp1.wav";
	if (!ent->volume) ent->volume = 1.0f;
	if (!ent->attenuation) ent->attenuation = ATTN_STATIC;
	ent->spawnFlags |= SPAWNFLAG_SPEAKER_LOOPED_ON;
	SP_target_speaker(ent);
}

/*QUAKED ambient_thunder (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
Legacy distant thunder ambience.
Creates a looped positional thunder rumble using target_speaker.

Keys:
- volume       : 0.0 to 1.0 (optional, defaults to 0.5)
- attenuation  : -1, 1, 2, 3 (optional)
*/
void SP_ambient_thunder(gentity_t* ent) {
	st.noise = "ambience/thunder1.wav";
	if (!ent->volume) ent->volume = 0.5f;
	if (!ent->attenuation) ent->attenuation = ATTN_STATIC;
	ent->spawnFlags |= SPAWNFLAG_SPEAKER_LOOPED_ON;
	SP_target_speaker(ent);
}

/*QUAKED ambient_light_buzz (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
Legacy fluorescent light hum ambience.
Creates a looped positional hum using target_speaker.

Keys:
- volume       : 0.0 to 1.0 (optional, defaults to 0.5)
- attenuation  : -1, 1, 2, 3 (optional)
*/
void SP_ambient_light_buzz(gentity_t* ent) {
	st.noise = "ambience/fl_hum1.wav";
	if (!ent->volume) ent->volume = 0.5f;
	if (!ent->attenuation) ent->attenuation = ATTN_STATIC;
	ent->spawnFlags |= SPAWNFLAG_SPEAKER_LOOPED_ON;
	SP_target_speaker(ent);
}

/*QUAKED ambient_swamp1 (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
Legacy swamp ambience variant 1.
Creates a looped positional swamp bed using target_speaker.

Keys:
- volume       : 0.0 to 1.0 (optional, defaults to 0.5)
- attenuation  : -1, 1, 2, 3 (optional)
*/
void SP_ambient_swamp1(gentity_t* ent) {
	st.noise = "ambience/swamp1.wav";
	if (!ent->volume) ent->volume = 0.5f;
	if (!ent->attenuation) ent->attenuation = ATTN_STATIC;
	ent->spawnFlags |= SPAWNFLAG_SPEAKER_LOOPED_ON;
	SP_target_speaker(ent);
}

/*QUAKED ambient_swamp2 (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
Legacy swamp ambience variant 2.
Creates a looped positional swamp bed using target_speaker.

Keys:
- volume       : 0.0 to 1.0 (optional, defaults to 0.5)
- attenuation  : -1, 1, 2, 3 (optional)
*/
void SP_ambient_swamp2(gentity_t* ent) {
	st.noise = "ambience/swamp2.wav";
	if (!ent->volume) ent->volume = 0.5f;
	if (!ent->attenuation) ent->attenuation = ATTN_STATIC;
	ent->spawnFlags |= SPAWNFLAG_SPEAKER_LOOPED_ON;
	SP_target_speaker(ent);
}

/*QUAKED ambient_generic (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
Legacy generic ambient sound.
Creates a looped target_speaker using a custom sound path.

Keys:
- noise        : path to sound file (e.g. ambience/buzz1.wav). REQUIRED.
- volume       : 0.0 to 1.0 (optional, defaults to 0.5)
- delay        : Quake 1-style attenuation selector. Mapped as:
				 -1 = none (global), 1 = normal, 2 = idle, 3 = static (default)
				 This is translated to the speaker attenuation field.
- attenuation  : If provided, overrides mapping from delay.

Notes:
- If noise is missing, the entity is removed (matches Q1).
- Prefer target_speaker for new content.
*/
void SP_ambient_generic(gentity_t* ent) {
	if (!st.noise) {
		gi.Com_PrintFmt("{}: ambient_generic with no noise; removing\n", *ent);
		FreeEntity(ent);
		return;
	}

	// Defaults from Q1 script behavior
	if (!ent->volume)
		ent->volume = 0.5f;

	// Map Q1 "delay" to attenuation if attenuation not explicitly set
	if (!ent->attenuation) {
		const float q1Atten = ent->delay ? ent->delay : 3.0f;
		if (q1Atten <= -1.0f)      ent->attenuation = ATTN_NONE;
		else if (q1Atten <= 1.0f)  ent->attenuation = ATTN_NORM;
		else if (q1Atten <= 2.0f)  ent->attenuation = ATTN_IDLE;
		else                       ent->attenuation = ATTN_STATIC;
	}

	// Always loop like Q1 ambient
	ent->spawnFlags |= SPAWNFLAG_SPEAKER_LOOPED_ON;

	SP_target_speaker(ent);
}

//==========================================================

constexpr SpawnFlags SPAWNFLAG_HELP_HELP1 = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_SET_POI = 2_spawnflag;

extern void target_poi_use(gentity_t* ent, gentity_t* other, gentity_t* activator);
static USE(Use_Target_Help)(gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	if (!ent || !ent->message[0])
		return;

	const std::string_view newMessage = ent->message;

	HelpMessage* targetHelp = ent->spawnFlags.has(SPAWNFLAG_HELP_HELP1)
		? &game.help[0]
		: &game.help[1];

	if (targetHelp->view() != newMessage) {
		Q_strlcpy(targetHelp->message.data(), ent->message, targetHelp->message.size());
		targetHelp->modificationCount++;
	}

	if (ent->spawnFlags.has(SPAWNFLAG_SET_POI)) {
		target_poi_use(ent, other, activator);
	}
}

/*QUAKED target_help (1 0 1) (-16 -16 -24) (16 16 24) HELP1 SETPOI x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Sets primary or secondary objectives for the player to see in campaigns.

HELP1: sets the primary help message, otherwise the secondary help message is set
SETPOI: sets the point of interest for this help message

"message"	the message to display, in essence it is the primary or secondary mission objective.
			must be set, otherwise the target will not work.

"image"		the image to display for POI, if not set, defaults to "friend"
*/
void SP_target_help(gentity_t* ent) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		FreeEntity(ent);
		return;
	}

	if (!ent->message) {
		gi.Com_PrintFmt("{}: no message\n", *ent);
		FreeEntity(ent);
		return;
	}

	ent->use = Use_Target_Help;

	if (ent->spawnFlags.has(SPAWNFLAG_SET_POI)) {
		if (st.image)
			ent->noiseIndex = gi.imageIndex(st.image);
		else
			ent->noiseIndex = gi.imageIndex("friend");
	}
}

//==========================================================

/*QUAKED target_secret (1 0 1) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Counts a secret found in campaigns, must be targeted and is single use.
Not used in deathmatch.

"noise"		sound to play when the secret is found, defaults to "misc/secret.wav"
*/
static USE(use_target_secret) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	gi.sound(ent, CHAN_VOICE, ent->noiseIndex, 1, ATTN_NORM, 0);

	level.campaign.foundSecrets++;

	UseTargets(ent, activator);
	FreeEntity(ent);
}

static THINK(G_VerifyTargetted) (gentity_t* ent) -> void {
	if (!ent->targetName || !*ent->targetName)
		gi.Com_PrintFmt("WARNING: missing targetName on {}\n", *ent);
	else if (!G_FindByString<&gentity_t::target>(nullptr, ent->targetName))
		gi.Com_PrintFmt("WARNING: doesn't appear to be anything targeting {}\n", *ent);
}

void SP_target_secret(gentity_t* ent) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		FreeEntity(ent);
		return;
	}

	ent->think = G_VerifyTargetted;
	ent->nextThink = level.time + 10_ms;

	ent->use = use_target_secret;
	if (!st.noise)
		st.noise = "misc/secret.wav";
	ent->noiseIndex = gi.soundIndex(st.noise);
	ent->svFlags = SVF_NOCLIENT;
	level.campaign.totalSecrets++;
}

//==========================================================
/*
=====================
G_PlayerNotifyGoal
Notify the player of any new or updated mission objectives.
=====================
*/
void G_PlayerNotifyGoal(gentity_t* player) {
	if (!player || !player->client || deathmatch->integer)
		return;

	if (!player->client->pers.spawned)
		return;

	if ((level.time - player->client->resp.enterTime) < 300_ms)
		return;

	auto& client = *player->client;

	// N64-specific campaign goal logic
	if (level.campaign.goals) {
		// If help0 and help1 differ, we need to update help0 to current goal
		if (game.help[0].modificationCount != game.help[1].modificationCount) {
			std::string_view allGoals(level.campaign.goals);
			size_t goalIndex = level.campaign.goalNum;

			// Split goals by tabs
			size_t start = 0;
			for (size_t i = 0; i < goalIndex; ++i) {
				size_t nextTab = allGoals.find('\t', start);
				if (nextTab == std::string_view::npos) {
					gi.Com_Error("Invalid N64 goal data; tell Paril\n");
					return;
				}
				start = nextTab + 1;
			}

			size_t end = allGoals.find('\t', start);
			std::string_view currentGoal = allGoals.substr(start, end == std::string_view::npos ? end : end - start);

			Q_strlcpy(game.help[0].message.data(), currentGoal.data(),
				std::min(currentGoal.size() + 1, game.help[0].message.size()));

			game.help[1].modificationCount = game.help[0].modificationCount;
		}

		if (client.pers.game_help1changed != game.help[0].modificationCount) {
			gi.LocClient_Print(player, PRINT_TYPEWRITER, game.help[0].message.data());
			gi.localSound(player, player, CHAN_AUTO | CHAN_RELIABLE,
				gi.soundIndex("misc/talk.wav"), 1.0f, ATTN_NONE, 0.0f, GetUnicastKey());

			client.pers.game_help1changed = game.help[0].modificationCount;
		}

		return;
	}

	// Helper to handle goal change
	auto NotifyGoal = [&](int& clientModified, const HelpMessage& goal, const char* label) {
		if (clientModified != goal.modificationCount) {
			clientModified = goal.modificationCount;
			client.pers.helpChanged = 1;
			client.pers.help_time = level.time + 5_sec;

			if (!goal.empty()) {
				gi.LocClient_Print(player, PRINT_TYPEWRITER, label, goal.message.data());
			}
		}
		};

	NotifyGoal(client.pers.game_help1changed, game.help[0], "$g_primary_mission_objective");
	NotifyGoal(client.pers.game_help2changed, game.help[1], "$g_secondary_mission_objective");
}

/*QUAKED target_goal (1 0 1) (-8 -8 -8) (8 8 8) KEEP_MUSIC x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Counts a goal accomplished in campaigns, must be targeted and is single use.
Not used in deathmatch.

KEEP_MUSIC: if set, the music will not be stopped when the
			goal is found with no other goals to complete.

"noise"		sound to play when the goal is found, defaults to "misc/secret.wav"
*/
constexpr SpawnFlags SPAWNFLAG_GOAL_KEEP_MUSIC = 1_spawnflag;

static USE(use_target_goal) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	gi.sound(ent, CHAN_VOICE, ent->noiseIndex, 1, ATTN_NORM, 0);

	level.campaign.foundGoals++;

	if (level.campaign.foundGoals == level.campaign.totalGoals && !ent->spawnFlags.has(SPAWNFLAG_GOAL_KEEP_MUSIC)) {
		if (ent->sounds)
			gi.configString(CS_CDTRACK, G_Fmt("{}", ent->sounds).data());
		else
			gi.configString(CS_CDTRACK, "0");
	}

	// [Paril-KEX] n64 goals
	if (level.campaign.goals) {
		level.campaign.goalNum++;
		game.help[0].modificationCount++;

		for (auto player : active_players())
			G_PlayerNotifyGoal(player);
	}

	UseTargets(ent, activator);
	FreeEntity(ent);
}

void SP_target_goal(gentity_t* ent) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		FreeEntity(ent);
		return;
	}

	ent->use = use_target_goal;
	if (!st.noise)
		st.noise = "misc/secret.wav";
	ent->noiseIndex = gi.soundIndex(st.noise);
	ent->svFlags = SVF_NOCLIENT;
	level.campaign.totalGoals++;
}

//==========================================================

/*QUAKED target_explosion (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Spawns an explosion. Spawns a temporary entity that can deal radius damage.

"delay"		wait this long before going off
"dmg"		how much radius damage should be done, defaults to 0
"random"	random delay added to the delay, defaults to 0
*/
static THINK(target_explosion_explode) (gentity_t* self) -> void {
	float save;

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	RadiusDamage(self, self->activator, (float)self->dmg, nullptr, (float)self->dmg + 40, DamageFlags::Normal, ModID::Explosives);

	save = self->delay;
	self->delay = 0;
	UseTargets(self, self->activator);
	self->delay = save;
}

static USE(use_target_explosion) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->activator = activator;

	if (!self->delay) {
		target_explosion_explode(self);
		return;
	}

	self->think = target_explosion_explode;
	self->nextThink = level.time + GameTime::from_sec(self->delay + self->random);
}

void SP_target_explosion(gentity_t* ent) {
	ent->use = use_target_explosion;
	ent->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_changelevel (1 0 0) (-8 -8 -8) (8 8 8) END_OF_UNIT x x CLEAR_INVENTORY NO_END_OF_UNIT FADE_OUT IMMEDIATE_LEAVE x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Changes level to "map" when fired

END_OF_UNIT - if set, this is the end of the unit.
CLEAR_INVENTORY - if set, clears the player's inventory before changing level
NO_END_OF_UNIT - if set, this is not the end of the unit, even if it is the last level.
FADE_OUT - if set, fades out the screen before changing level

IMMEDIATE_LEAVE - if set, the player will leave the level immediately
					without waiting for the fade out or other delays

"map"		the map to change to, must be set.
*/
/*
=================
Use_target_changelevel
=================
*/
static USE(use_target_changelevel)(gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (level.intermission.time)
		return; // already activated

	// Ensure activator is valid player in SP/coop
	if (!deathmatch->integer && !coop->integer) {
		if (!activator || !activator->client || activator->health <= 0)
			return;
	}

	// In deathmatch, exit kills player
	if (deathmatch->integer) {
		if (other && other->client && other->maxHealth > 0) {
			Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin,
				10 * other->maxHealth, 1000, DamageFlags::Normal, ModID::ExitLevel);
		}
		return;
	}

	// Reset cross-level triggers if changing to a new unit
	if (strstr(self->map.data(), "*"))
		game.crossLevelFlags &= ~SFL_CROSS_TRIGGER_MASK;

	// Helper to unrotate a point by given angles
	const auto RotatePointInverse = [](const Vector3& point, const Vector3& angles) -> Vector3 {
		Vector3 out = RotatePointAroundVector({ 1, 0, 0 }, point, -angles[PITCH]);
		out = RotatePointAroundVector({ 0, 1, 0 }, out, -angles[ROLL]);
		return RotatePointAroundVector({ 0, 0, 1 }, out, -angles[YAW]);
		};

	// Handle landmark-relative transitions
	if (activator && activator->client) {
		activator->client->landmark_name = nullptr;
		activator->client->landmark_rel_pos = vec3_origin;

		self->targetEnt = PickTarget(self->target);
		if (self->targetEnt) {
			activator->client->landmark_name = CopyString(self->targetEnt->targetName, TAG_GAME);

			Vector3 rel = activator->s.origin - self->targetEnt->s.origin;
			activator->client->landmark_rel_pos = RotatePointInverse(rel, self->targetEnt->s.angles);
			activator->client->oldVelocity = RotatePointInverse(activator->client->oldVelocity, self->targetEnt->s.angles);
			activator->client->oldViewAngles = activator->client->ps.viewAngles - self->targetEnt->s.angles;
		}
	}

	BeginIntermission(self);
}

void SP_target_changelevel(gentity_t* ent) {
	if (CharArrayIsBlank(ent->map)) {
		gi.Com_PrintFmt("{}: no map\n", *ent);
		FreeEntity(ent);
		return;
	}

	ent->use = use_target_changelevel;
	ent->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_splash (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Creates a particle splash effect when used.

Set "sounds" to one of the following:
  1) sparks
  2) blue water
  3) brown water
  4) slime
  5) lava
  6) blood

"count"	how many pixels in the splash (default 32)
"dmg"	if set, does a radius damage at this location when it splashes
		useful for lava/sparks

N64 sparks are blue, not yellow.
*/

static USE(use_target_splash) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_SPLASH);
	gi.WriteByte(self->count);
	gi.WritePosition(self->s.origin);
	gi.WriteDir(self->moveDir);
	gi.WriteByte(self->sounds);
	gi.multicast(self->s.origin, MULTICAST_PVS, false);

	if (self->dmg)
		RadiusDamage(self, activator, (float)self->dmg, nullptr, (float)self->dmg + 40, DamageFlags::Normal, ModID::Splash);
}

void SP_target_splash(gentity_t* self) {
	self->use = use_target_splash;
	SetMoveDir(self->s.angles, self->moveDir);

	if (!self->count)
		self->count = 32;

	// N64 "sparks" are blue, not yellow.
	if (level.isN64 && self->sounds == 1)
		self->sounds = 7;

	self->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_spawner (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Set target to the type of entity you want spawned.
Useful for spawning monsters and gibs in the factory levels.

"target"	the type of entity to spawn, must be set
"speed"		if set, the spawned entity will move in the direction
			of the angles at this speed, otherwise it will just be dropped
"moveDir"	if set, the spawned entity will move in this direction
"angles"	the angles to spawn the entity at, defaults to (0, 0, 0)

For monsters:
	Set direction to the facing you want it to have.

For gibs:
	Set direction if you want it moving and
	speed how fast it should be moving otherwise it
	will just be dropped
*/
void ED_CallSpawn(gentity_t* ent);

static USE(use_target_spawner) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	// don't trigger spawn monsters in horde mode
	if (Game::Is(GameType::Horde) && !Q_strncasecmp("monster_", self->target, 8))
		return;

	gentity_t* ent;

	ent = Spawn();
	ent->className = self->target;
	ent->flags = self->flags;
	ent->s.origin = self->s.origin;
	ent->s.angles = self->s.angles;
	st = {};

	// [Paril-KEX] although I fixed these in our maps, this is just
	// in case anybody else does this by accident. Don't count these monsters
	// so they don't inflate the monster count.
	ent->monsterInfo.aiFlags |= AI_DO_NOT_COUNT;

	ED_CallSpawn(ent);
	gi.linkEntity(ent);

	KillBox(ent, false);
	if (self->speed)
		ent->velocity = self->moveDir;

	ent->s.renderFX |= RF_IR_VISIBLE;
}

void SP_target_spawner(gentity_t* self) {
	self->use = use_target_spawner;
	self->svFlags = SVF_NOCLIENT;
	if (self->speed) {
		SetMoveDir(self->s.angles, self->moveDir);
		self->moveDir *= self->speed;
	}
}

//==========================================================

/*QUAKED target_blaster (1 0 0) (-8 -8 -8) (8 8 8) NOTRAIL NOEFFECTS x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a blaster bolt in the set direction when triggered.

"target"	the target to fire at, if not set, fires in the direction of the angles
"angles"	the angles to fire at, defaults to (0, 0, 0)
"dmg"		how much damage the blaster bolt does, defaults to 15
"speed"		how fast the blaster bolt moves, defaults to 1000
"noise"		the sound to play when the blaster bolt is fired, defaults to "weapons/laser2.wav"
*/

constexpr SpawnFlags SPAWNFLAG_BLASTER_NOTRAIL = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_BLASTER_NOEFFECTS = 2_spawnflag;

static USE(use_target_blaster) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	Effect effect;

	if (self->spawnFlags.has(SPAWNFLAG_BLASTER_NOEFFECTS))
		effect = EF_NONE;
	else if (self->spawnFlags.has(SPAWNFLAG_BLASTER_NOTRAIL))
		effect = EF_HYPERBLASTER;
	else
		effect = EF_BLASTER;

	fire_blaster(self, self->s.origin, self->moveDir, self->dmg, (int)self->speed, effect, ModID::ShooterBlaster, false);
	gi.sound(self, CHAN_VOICE, self->noiseIndex, 1, ATTN_NORM, 0);
}

void SP_target_blaster(gentity_t* self) {
	self->use = use_target_blaster;
	SetMoveDir(self->s.angles, self->moveDir);

	if (st.noise)
		self->noiseIndex = gi.soundIndex(st.noise);
	else
		self->noiseIndex = gi.soundIndex("weapons/laser2.wav");

	if (!self->dmg)
		self->dmg = 15;
	if (!self->speed)
		self->speed = 1000;

	self->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_crosslevel_trigger (.5 .5 .5) (-8 -8 -8) (8 8 8) TRIGGER1 TRIGGER2 TRIGGER3 TRIGGER4 TRIGGER5 TRIGGER6 TRIGGER7 TRIGGER8 NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Once this trigger is touched/used, any trigger_crosslevel_target with the same trigger number
is automatically used when a level is started within the same unit. It is OK to check multiple triggers.
Message, delay, target, and killTarget also work.
*/
static USE(trigger_crosslevel_trigger_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	game.crossLevelFlags |= self->spawnFlags.value;
	FreeEntity(self);
}

void SP_target_crosslevel_trigger(gentity_t* self) {
	self->svFlags = SVF_NOCLIENT;
	self->use = trigger_crosslevel_trigger_use;
}

/*QUAKED target_crosslevel_target (.5 .5 .5) (-8 -8 -8) (8 8 8) TRIGGER1 TRIGGER2 TRIGGER3 TRIGGER4 TRIGGER5 TRIGGER6 TRIGGER7 TRIGGER8 x x x x x x x x TRIGGER9 TRIGGER10 TRIGGER11 TRIGGER12 TRIGGER13 TRIGGER14 TRIGGER15 TRIGGER16
Triggered by a trigger_crosslevel elsewhere within a unit. If multiple triggers are checked, all must be true.
Delay, target and killTarget also work.

"delay"		delay before using targets if the trigger has been activated (default 1)
*/
static THINK(target_crosslevel_target_think) (gentity_t* self) -> void {
	if (self->spawnFlags.value == (game.crossLevelFlags & SFL_CROSS_TRIGGER_MASK & self->spawnFlags.value)) {
		UseTargets(self, self);
		FreeEntity(self);
	}
}

void SP_target_crosslevel_target(gentity_t* self) {
	if (!self->delay)
		self->delay = 1;
	self->svFlags = SVF_NOCLIENT;

	self->think = target_crosslevel_target_think;
	self->nextThink = level.time + GameTime::from_sec(self->delay);
}

//==========================================================

/*QUAKED target_laser (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON RED GREEN BLUE YELLOW ORANGE FAT WINDOWSTOP NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
When triggered, fires a laser. You can either set a target or a direction.

START_ON	- if set, the laser will be on when spawned
FAT			- if set, the laser will be wider
WINDOWSTOP	- if set, the laser will stop at windows and not pass through them

In N64, WINDOWSTOP is used to make the laser a lightning bolt.
*/

constexpr SpawnFlags SPAWNFLAG_LASER_STOPWINDOW = 0x0080_spawnflag;

struct laser_pierce_t : pierce_args_t {
	gentity_t* self;
	int32_t count;
	bool damaged_thing = false;

	inline laser_pierce_t(gentity_t* self, int32_t count) :
		pierce_args_t(),
		self(self),
		count(count) {
	}

	// we hit an entity; return false to stop the piercing.
	// you can adjust the mask for the re-trace (for water, etc).
	virtual bool hit(contents_t& mask, Vector3& end) override {
		// hurt it if we can
		if (self->dmg > 0 && (tr.ent->takeDamage) && !(tr.ent->flags & FL_IMMUNE_LASER) && self->damage_debounce_time <= level.time) {
			damaged_thing = true;
			Damage(tr.ent, self, self->activator, self->moveDir, tr.endPos, vec3_origin, self->dmg, 1, DamageFlags::Energy, ModID::Laser);
		}

		// if we hit something that's not a monster or player or is immune to lasers, we're done
		if (!(tr.ent->svFlags & SVF_MONSTER) && (!tr.ent->client) && !(tr.ent->flags & FL_DAMAGEABLE)) {
			if (self->spawnFlags.has(SPAWNFLAG_LASER_ZAP)) {
				self->spawnFlags &= ~SPAWNFLAG_LASER_ZAP;
				gi.WriteByte(svc_temp_entity);
				gi.WriteByte(TE_LASER_SPARKS);
				gi.WriteByte(count);
				gi.WritePosition(tr.endPos);
				gi.WriteDir(tr.plane.normal);
				gi.WriteByte(self->s.skinNum);
				gi.multicast(tr.endPos, MULTICAST_PVS, false);
			}

			return false;
		}

		if (!mark(tr.ent))
			return false;

		return true;
	}
};

static THINK(target_laser_think) (gentity_t* self) -> void {
	int32_t count;

	if (self->spawnFlags.has(SPAWNFLAG_LASER_ZAP))
		count = 8;
	else
		count = 4;

	if (self->enemy) {
		Vector3 last_movedir = self->moveDir;
		Vector3 point = (self->enemy->absMin + self->enemy->absMax) * 0.5f;
		self->moveDir = point - self->s.origin;
		self->moveDir.normalize();
		if (self->moveDir != last_movedir)
			self->spawnFlags |= SPAWNFLAG_LASER_ZAP;
	}

	Vector3 start = self->s.origin;
	Vector3 end = start + (self->moveDir * 2048);

	laser_pierce_t args{
		self,
		count
	};

	contents_t mask = self->spawnFlags.has(SPAWNFLAG_LASER_STOPWINDOW) ? MASK_SHOT : (CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_PLAYER | CONTENTS_DEADMONSTER);

	pierce_trace(start, end, self, args, mask);

	self->s.oldOrigin = args.tr.endPos;

	if (args.damaged_thing)
		self->damage_debounce_time = level.time + 10_hz;

	self->nextThink = level.time + FRAME_TIME_S;
	gi.linkEntity(self);
}

static void target_laser_on(gentity_t* self) {
	if (!self->activator)
		self->activator = self;
	self->spawnFlags |= SPAWNFLAG_LASER_ZAP | SPAWNFLAG_LASER_ON;
	self->svFlags &= ~SVF_NOCLIENT;
	self->flags |= FL_TRAP;
	target_laser_think(self);
}

void target_laser_off(gentity_t* self) {
	self->spawnFlags &= ~SPAWNFLAG_LASER_ON;
	self->svFlags |= SVF_NOCLIENT;
	self->flags &= ~FL_TRAP;
	self->nextThink = 0_ms;
}

static USE(target_laser_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->activator = activator;
	if (self->spawnFlags.has(SPAWNFLAG_LASER_ON))
		target_laser_off(self);
	else
		target_laser_on(self);
}

static THINK(target_laser_start) (gentity_t* self) -> void {
	self->moveType = MoveType::None;
	self->solid = SOLID_NOT;
	self->s.renderFX |= RF_BEAM;
	self->s.modelIndex = MODELINDEX_WORLD; // must be non-zero

	// [Sam-KEX] On Q2N64, spawnflag of 128 turns it into a lightning bolt
	if (level.isN64) {
		// Paril: fix for N64
		if (self->spawnFlags.has(SPAWNFLAG_LASER_STOPWINDOW)) {
			self->spawnFlags &= ~SPAWNFLAG_LASER_STOPWINDOW;
			self->spawnFlags |= SPAWNFLAG_LASER_LIGHTNING;
		}
	}

	if (self->spawnFlags.has(SPAWNFLAG_LASER_LIGHTNING)) {
		self->s.renderFX |= RF_BEAM_LIGHTNING; // tell renderer it is lightning

		if (!self->s.skinNum)
			self->s.skinNum = 0xf3f3f1f1; // default lightning color
	}

	// set the beam diameter
	// [Paril-KEX] lab has this set prob before lightning was implemented
	if (!level.isN64 && self->spawnFlags.has(SPAWNFLAG_LASER_FAT))
		self->s.frame = 16;
	else
		self->s.frame = 4;

	// set the color
	if (!self->s.skinNum) {
		if (self->spawnFlags.has(SPAWNFLAG_LASER_RED))
			self->s.skinNum = 0xf2f2f0f0;
		else if (self->spawnFlags.has(SPAWNFLAG_LASER_GREEN))
			self->s.skinNum = 0xd0d1d2d3;
		else if (self->spawnFlags.has(SPAWNFLAG_LASER_BLUE))
			self->s.skinNum = 0xf3f3f1f1;
		else if (self->spawnFlags.has(SPAWNFLAG_LASER_YELLOW))
			self->s.skinNum = 0xdcdddedf;
		else if (self->spawnFlags.has(SPAWNFLAG_LASER_ORANGE))
			self->s.skinNum = 0xe0e1e2e3;
	}

	if (!self->enemy) {
		if (self->target) {
			gentity_t* targetEnt = G_FindByString<&gentity_t::targetName>(nullptr, self->target);
			if (!targetEnt)
				gi.Com_PrintFmt("{}: {} is a bad target.\n", *self, self->target);
			else {
				self->enemy = targetEnt;

				// N64 fix
				// FIXME: which map was this for again? oops
				// muff: it is down to one of these maps:
				// cargo, complex, core, jail, lab, orbit, process, storage
				if (level.isN64 && !strcmp(self->enemy->className, "func_train") && !(self->enemy->spawnFlags & SPAWNFLAG_TRAIN_START_ON))
					self->enemy->use(self->enemy, self, self);
			}
		}
		else {
			SetMoveDir(self->s.angles, self->moveDir);
		}
	}
	self->use = target_laser_use;
	self->think = target_laser_think;

	if (!self->dmg)
		self->dmg = 1;

	self->mins = { -8, -8, -8 };
	self->maxs = { 8, 8, 8 };
	gi.linkEntity(self);

	if (self->spawnFlags.has(SPAWNFLAG_LASER_ON))
		target_laser_on(self);
	else
		target_laser_off(self);
}

void SP_target_laser(gentity_t* self) {
	// let everything else get spawned before we start firing
	self->think = target_laser_start;
	self->flags |= FL_TRAP_LASER_FIELD;
	self->nextThink = level.time + 1_sec;
}

//==========================================================

/*QUAKED target_lightramp (0 .5 .8) (-8 -8 -8) (8 8 8) TOGGLE x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
speed		How many seconds the ramping will take
message		two letters; starting lightlevel and ending lightlevel
*/

constexpr SpawnFlags SPAWNFLAG_LIGHTRAMP_TOGGLE = 1_spawnflag;

/*
=============
Target_Lightramp_Think

Updates the light configuration string to follow the configured ramp over time.
=============
*/
static THINK(Target_Lightramp_Think) (gentity_t* self) -> void {
	std::array<char, 2> style{};

	const float delta = ((level.time - self->timeStamp) / gi.frameTimeSec).seconds();
	const int step = static_cast<int>(self->moveDir[0] + delta * self->moveDir[2]);

	style[0] = static_cast<char>('a' + step);
	style[1] = '\0';

	gi.configString(CS_LIGHTS + self->enemy->style, style.data());

	if ((level.time - self->timeStamp).seconds() < self->speed) {
		self->nextThink = level.time + FRAME_TIME_S;
	}
	else if (self->spawnFlags.has(SPAWNFLAG_LIGHTRAMP_TOGGLE)) {
		std::swap(self->moveDir[0], self->moveDir[1]);
		self->moveDir[2] *= -1;
	}
}

static USE(Target_Lightramp_Use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (!self->enemy) {
		for (gentity_t* e = nullptr; (e = G_FindByString<&gentity_t::targetName>(e, self->target)); ) {
			if (std::strcmp(e->className, "light") != 0) {
				gi.Com_PrintFmt("{}: target {} ({}) is not a light\n", *self, self->target, *e);
			}
			else {
				self->enemy = e;
			}
		}

		if (!self->enemy) {
			gi.Com_PrintFmt("{}: target {} not found\n", *self, self->target);
			FreeEntity(self);
			return;
		}
	}

	self->timeStamp = level.time;
	Target_Lightramp_Think(self);
}

void SP_target_lightramp(gentity_t* self) {
	if (!self->message || std::strlen(self->message) != 2 ||
		self->message[0] < 'a' || self->message[0] > 'z' ||
		self->message[1] < 'a' || self->message[1] > 'z' ||
		self->message[0] == self->message[1]) {
		gi.Com_PrintFmt("{}: bad ramp ({})\n", *self, self->message ? self->message : "null string");
		FreeEntity(self);
		return;
	}

	if (deathmatch->integer) {
		FreeEntity(self);
		return;
	}

	if (!self->target) {
		gi.Com_PrintFmt("{}: no target\n", *self);
		FreeEntity(self);
		return;
	}

	self->svFlags |= SVF_NOCLIENT;
	self->use = Target_Lightramp_Use;
	self->think = Target_Lightramp_Think;

	const float a = static_cast<float>(self->message[0] - 'a');
	const float b = static_cast<float>(self->message[1] - 'a');
	self->moveDir[0] = a;
	self->moveDir[1] = b;
	self->moveDir[2] = (b - a) / (self->speed / gi.frameTimeSec);
}

//==========================================================

/*QUAKED target_earthquake (1 0 0) (-8 -8 -8) (8 8 8) SILENT TOGGLE UNKNOWN_ROGUE ONE_SHOT x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
When triggered, this initiates a level-wide earthquake.
All players are affected with a screen shake.
"speed"		severity of the quake (default:200)
"count"		duration of the quake (default:5)
*/

constexpr SpawnFlags SPAWNFLAGS_EARTHQUAKE_SILENT = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAGS_EARTHQUAKE_TOGGLE = 2_spawnflag;
[[maybe_unused]] constexpr SpawnFlags SPAWNFLAGS_EARTHQUAKE_UNKNOWN_ROGUE = 4_spawnflag;
constexpr SpawnFlags SPAWNFLAGS_EARTHQUAKE_ONE_SHOT = 8_spawnflag;

static THINK(target_earthquake_think) (gentity_t* self) -> void {
	uint32_t i;
	gentity_t* e;

	if (!(self->spawnFlags & SPAWNFLAGS_EARTHQUAKE_SILENT)) {
		if (self->last_move_time < level.time) {
			gi.positionedSound(self->s.origin, self, CHAN_VOICE, self->noiseIndex, 1.0, ATTN_NONE, 0);
			self->last_move_time = level.time + 6.5_sec;
		}
	}

	for (i = 1, e = g_entities + i; i < globals.numEntities; i++, e++) {
		if (!e->inUse)
			continue;
		if (!e->client)
			break;

		e->client->feedback.quakeTime = level.time + 1000_ms;
	}

	if (level.time < self->timeStamp)
		self->nextThink = level.time + 10_hz;
}

static USE(target_earthquake_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (self->spawnFlags.has(SPAWNFLAGS_EARTHQUAKE_ONE_SHOT)) {
		uint32_t i;
		gentity_t* e;

		for (i = 1, e = g_entities + i; i < globals.numEntities; i++, e++) {
			if (!e->inUse)
				continue;
			if (!e->client)
				break;

			e->client->feedback.vDamagePitch = -self->speed * 0.1f;
			e->client->feedback.vDamageTime = level.time + DAMAGE_TIME();
		}

		return;
	}

	self->timeStamp = level.time + GameTime::from_sec(self->count);

	if (self->spawnFlags.has(SPAWNFLAGS_EARTHQUAKE_TOGGLE)) {
		if (self->style)
			self->nextThink = 0_ms;
		else
			self->nextThink = level.time + FRAME_TIME_S;

		self->style = ~self->style;
	}
	else {
		self->nextThink = level.time + FRAME_TIME_S;
		self->last_move_time = 0_ms;
	}

	self->activator = activator;
}

void SP_target_earthquake(gentity_t* self) {
	if (!self->targetName)
		gi.Com_PrintFmt("{}: untargeted\n", *self);

	if (level.isN64) {
		self->spawnFlags |= SPAWNFLAGS_EARTHQUAKE_TOGGLE;
		self->speed = 5;
	}

	if (!self->count)
		self->count = 5;

	if (!self->speed)
		self->speed = 200;

	self->svFlags |= SVF_NOCLIENT;
	self->think = target_earthquake_think;
	self->use = target_earthquake_use;

	if (!(self->spawnFlags & SPAWNFLAGS_EARTHQUAKE_SILENT))
		self->noiseIndex = gi.soundIndex("world/quake.wav");
}

/*QUAKED target_camera (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Creates a camera path as seen in the N64 version.
When triggered, the camera will move to the target and look at it.
Auto-removed in DM.

"target"		the target to move to, must be set
"speed"			the speed to move at, defaults to 55
"wait"			the time to wait at the target, defaults to 2 seconds
"killTarget"	the target to kill when the camera reaches the end of the path
"hackFlags"		used to set special flags for the camera
					- HACKFLAG_TELEPORT_OUT (2): teleports the player out of the camera view
					- HACKFLAG_SKIPPABLE (64): allows skipping the camera view by pressing any button
					- HACKFLAG_END_OF_UNIT (128): marks the end of a unit, requires a wait before changing level
"pathTarget"	the target to look at while moving, if not set, looks at the target
*/

constexpr size_t HACKFLAG_TELEPORT_OUT = 2;
constexpr size_t HACKFLAG_SKIPPABLE = 64;
constexpr size_t HACKFLAG_END_OF_UNIT = 128;

static void camera_lookat_pathtarget(gentity_t* self, Vector3 origin, Vector3* dest) {
	if (self->pathTarget) {
		gentity_t* pt = nullptr;
		pt = G_FindByString<&gentity_t::targetName>(pt, self->pathTarget);
		if (pt) {
			float yaw, pitch;
			Vector3 delta = pt->s.origin - origin;

			float d = delta[0] * delta[0] + delta[1] * delta[1];
			if (d == 0.0f) {
				yaw = 0.0f;
				pitch = (delta[2] > 0.0f) ? 90.0f : -90.0f;
			}
			else {
				yaw = atan2(delta[1], delta[0]) * (180.0f / PIf);
				pitch = atan2(delta[2], sqrt(d)) * (180.0f / PIf);
			}

			(*dest)[YAW] = yaw;
			(*dest)[PITCH] = -pitch;
			(*dest)[ROLL] = 0;
		}
	}
}

static THINK(update_target_camera) (gentity_t* self) -> void {
	bool do_skip = false;

	// only allow skipping after 2 seconds
	if ((self->hackFlags & HACKFLAG_SKIPPABLE) && level.time > 2_sec) {
		for (auto ce : active_clients()) {
			if (ce->client->buttons & BUTTON_ANY) {
				do_skip = true;
				break;
			}
		}
	}

	if (!do_skip && self->moveTarget) {
		self->moveInfo.remainingDistance -= (self->moveInfo.moveSpeed * gi.frameTimeSec) * 0.8f;

		if (self->moveInfo.remainingDistance <= 0) {
			if (self->moveTarget->hackFlags & HACKFLAG_TELEPORT_OUT) {
				if (self->enemy) {
					self->enemy->s.event = EV_PLAYER_TELEPORT;
					self->enemy->hackFlags = HACKFLAG_TELEPORT_OUT;
					self->enemy->pain_debounce_time = self->enemy->timeStamp = GameTime::from_sec(self->moveTarget->wait);
				}
			}

			self->s.origin = self->moveTarget->s.origin;
			self->nextThink = level.time + GameTime::from_sec(self->moveTarget->wait);
			if (self->moveTarget->target) {
				self->moveTarget = PickTarget(self->moveTarget->target);

				if (self->moveTarget) {
					self->moveInfo.moveSpeed = self->moveTarget->speed ? self->moveTarget->speed : 55;
					self->moveInfo.remainingDistance = (self->moveTarget->s.origin - self->s.origin).normalize();
					self->moveInfo.distance = self->moveInfo.remainingDistance;
				}
			}
			else
				self->moveTarget = nullptr;

			return;
		}
		else {
			float frac = 1.0f - (self->moveInfo.remainingDistance / self->moveInfo.distance);

			if (self->enemy && (self->enemy->hackFlags & HACKFLAG_TELEPORT_OUT))
				self->enemy->s.alpha = max(1.f / 255.f, frac);

			Vector3 delta = self->moveTarget->s.origin - self->s.origin;
			delta *= frac;
			Vector3 newpos = self->s.origin + delta;

			camera_lookat_pathtarget(self, newpos, &level.intermission.angles);
			level.intermission.origin = newpos;
			level.spawn.intermission = self;
			level.spawn.intermission->s.origin += delta;

			// move all clients to the intermission point
			for (auto ce : active_clients())
				MoveClientToIntermission(ce);
		}
	}
	else {
		if (self->killTarget) {
			// destroy dummy player
			if (self->enemy)
				FreeEntity(self->enemy);

			gentity_t* t = nullptr;
			level.intermission.time = 0_ms;
			level.intermission.set = true;

			while ((t = G_FindByString<&gentity_t::targetName>(t, self->killTarget))) {
				t->use(t, self, self->activator);
			}

			level.intermission.time = level.time;
			level.intermission.serverFrame = gi.ServerFrame();

			// end of unit requires a wait
			if (!level.changeMap.empty() && !strchr(level.changeMap.data(), '*'))
				level.intermission.postIntermission = true;
		}

		self->think = nullptr;
		return;
	}

	self->nextThink = level.time + FRAME_TIME_S;
}

void PlayerSetFrame(gentity_t* ent);

extern float xySpeed;

static THINK(target_camera_dummy_think) (gentity_t* self) -> void {
	// bit of a hack, but this will let the dummy
	// move like a player
	self->client = self->owner->client;
	xySpeed = sqrtf(self->velocity[_X] * self->velocity[_X] + self->velocity[_Y] * self->velocity[_Y]);
	PlayerSetFrame(self);
	self->client = nullptr;

	// alpha fade out for voops
	if (self->hackFlags & HACKFLAG_TELEPORT_OUT) {
		self->timeStamp = max(0_ms, self->timeStamp - 10_hz);
		self->s.alpha = max(1.f / 255.f, (self->timeStamp.seconds() / self->pain_debounce_time.seconds()));
	}

	self->nextThink = level.time + 10_hz;
}

static USE(use_target_camera) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (self->sounds)
		gi.configString(CS_CDTRACK, G_Fmt("{}", self->sounds).data());

	if (!self->target)
		return;

	self->moveTarget = PickTarget(self->target);

	if (!self->moveTarget)
		return;

	level.intermission.time = level.time;
	level.intermission.serverFrame = gi.ServerFrame();
	level.intermission.postIntermission = false;

	// spawn fake player dummy where we were
	if (activator->client) {
		gentity_t* dummy = self->enemy = Spawn();
		dummy->owner = activator;
		dummy->clipMask = activator->clipMask;
		dummy->s.origin = activator->s.origin;
		dummy->s.angles = activator->s.angles;
		dummy->groundEntity = activator->groundEntity;
		dummy->groundEntity_linkCount = dummy->groundEntity ? dummy->groundEntity->linkCount : 0;
		dummy->think = target_camera_dummy_think;
		dummy->nextThink = level.time + 10_hz;
		dummy->solid = SOLID_BBOX;
		dummy->moveType = MoveType::Step;
		dummy->mins = activator->mins;
		dummy->maxs = activator->maxs;
		dummy->s.modelIndex = dummy->s.modelIndex2 = MODELINDEX_PLAYER;
		dummy->s.skinNum = activator->s.skinNum;
		dummy->velocity = activator->velocity;
		dummy->s.renderFX = RF_MINLIGHT;
		dummy->s.frame = activator->s.frame;
		gi.linkEntity(dummy);
	}

	camera_lookat_pathtarget(self, self->s.origin, &level.intermission.angles);
	level.intermission.origin = self->s.origin;
	level.spawn.intermission = self;

	// move all clients to the intermission point
	for (auto ce : active_clients()) {
		// respawn any dead clients
		if (ce->health <= 0 || ce->client->eliminated) {
			// give us our max health back since it will reset
			// to pers.health; in instanced items we'd lose the items
			// we touched so we always want to respawn with our max.
			if (P_UseCoopInstancedItems())
				ce->client->pers.health = ce->client->pers.maxHealth = ce->maxHealth;

			ClientRespawn(ce);
		}

		MoveClientToIntermission(ce);
	}

	self->activator = activator;
	self->think = update_target_camera;
	self->nextThink = level.time + GameTime::from_sec(self->wait);
	self->moveInfo.moveSpeed = self->speed;

	self->moveInfo.remainingDistance = (self->moveTarget->s.origin - self->s.origin).normalize();
	self->moveInfo.distance = self->moveInfo.remainingDistance;

	if (self->hackFlags & HACKFLAG_END_OF_UNIT)
		EndOfUnitMessage();
}

void SP_target_camera(gentity_t* self) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		FreeEntity(self);
		return;
	}

	self->use = use_target_camera;
	self->svFlags = SVF_NOCLIENT;
}

/*QUAKED target_gravity (1 0 0) (-8 -8 -8) (8 8 8) NOTRAIL NOEFFECTS x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Changes gravity, as seen in the N64 version

"gravity"		the gravity to set, defaults to 800
*/

static USE(use_target_gravity) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	gi.cvarSet("g_gravity", G_Fmt("{}", self->gravity).data());
	level.gravity = self->gravity;
}

void SP_target_gravity(gentity_t* self) {
	self->use = use_target_gravity;
	self->gravity = atof(st.gravity);
}

/*QUAKED target_soundfx (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Plays a sound effect, as seen in the N64 version.
This differs from target_speaker in that it plays a sound from a preset list of sounds.

"noiseIndex"	the sound index to play, can be a sound index or a string:
			- 1: world/x_alarm.wav
			- 2: world/flyby1.wav
			- 4: world/amb12.wav
			- 5: world/amb17.wav
			- 7: world/bigpump2.wav
"volume"	the volume to play the sound at, defaults to 1.0
"attenuation"	the attenuation to use, defaults to 1.0, -1 means use default (0)
"delay"		the delay before playing the sound, defaults to 1 second
"target"	the target to use when the sound is played, if not set, plays immediately
"killTarget"	the target to kill when the sound is played, if not set, does nothing
*/

static THINK(update_target_soundfx) (gentity_t* self) -> void {
	gi.positionedSound(self->s.origin, self, CHAN_VOICE, self->noiseIndex, self->volume, self->attenuation, 0);
}

static USE(use_target_soundfx) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->think = update_target_soundfx;
	self->nextThink = level.time + GameTime::from_sec(self->delay);
}

void SP_target_soundfx(gentity_t* self) {
	if (!self->volume)
		self->volume = 1.0;

	if (!self->attenuation)
		self->attenuation = 1.0;
	else if (self->attenuation == -1) // use -1 so 0 defaults to 1
		self->attenuation = 0;

	self->noiseIndex = strtoul(st.noise, nullptr, 10);

	switch (self->noiseIndex) {
	case 1:
		self->noiseIndex = gi.soundIndex("world/x_alarm.wav");
		break;
	case 2:
		self->noiseIndex = gi.soundIndex("world/flyby1.wav");
		break;
	case 4:
		self->noiseIndex = gi.soundIndex("world/amb12.wav");
		break;
	case 5:
		self->noiseIndex = gi.soundIndex("world/amb17.wav");
		break;
	case 7:
		self->noiseIndex = gi.soundIndex("world/bigpump2.wav");
		break;
	default:
		gi.Com_PrintFmt("{}: unknown noise {}\n", *self, self->noiseIndex);
		return;
	}

	self->use = use_target_soundfx;
}

/*QUAKED target_light (1 0 0) (-8 -8 -8) (8 8 8) START_ON NO_LERP FLICKER x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Dynamic light entity that follows a lightStyle.

START_ON	the light starts on, defaults to off
NO_LERP		the light does not interpolate, defaults to false
FLICKER		the light flickers, defaults to false

"style"		the light style to use, must be set
"speed"		the speed to interpolate the light, defaults to 1.0
"count"		the starting color of the light, defaults to 0x00ff00ff (green)
"chain"		the target light to follow, if not set, uses the style's color
"target"	the target to use when the light is turned on, if not set, turns on immediately
"targetName"	the target name to use when the light is turned on, if not set, uses the entity's target name
"radius"	the radius of the light, defaults to 150
"killTarget"	the target to kill when the light is turned on, if not set, does nothing
"health"	the health of the light, if set, the light can be turned on and off
			by using the entity, defaults to 0 (off)

N64 uses different styles.
*/

constexpr SpawnFlags SPAWNFLAG_TARGET_LIGHT_START_ON = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_TARGET_LIGHT_NO_LERP = 2_spawnflag; // not used in N64, but I'll use it for this
constexpr SpawnFlags SPAWNFLAG_TARGET_LIGHT_FLICKER = 4_spawnflag;

static THINK(target_light_flicker_think) (gentity_t* self) -> void {
	if (brandom())
		self->svFlags ^= SVF_NOCLIENT;

	self->nextThink = level.time + 10_hz;
}

// think function handles interpolation from start to finish.
static THINK(target_light_think) (gentity_t* self) -> void {
	if (self->spawnFlags.has(SPAWNFLAG_TARGET_LIGHT_FLICKER))
		target_light_flicker_think(self);

	const char* style = gi.get_configString(CS_LIGHTS + self->style);
	self->delay += self->speed;

	int32_t index = ((int32_t)self->delay) % strlen(style);
	char style_value = style[index];
	float current_lerp = (float)(style_value - 'a') / (float)('z' - 'a');
	float lerp;

	if (!(self->spawnFlags & SPAWNFLAG_TARGET_LIGHT_NO_LERP)) {
		int32_t next_index = (index + 1) % static_cast<int32_t>(strlen(style));
		char next_style_value = style[next_index];

		float next_lerp = (float)(next_style_value - 'a') / (float)('z' - 'a');

		float mod_lerp = fmod(self->delay, 1.0f);
		lerp = (next_lerp * mod_lerp) + (current_lerp * (1.f - mod_lerp));
	}
	else
		lerp = current_lerp;

	int my_rgb = self->count;
	int target_rgb = self->chain->s.skinNum;

	int my_b = ((my_rgb >> 8) & 0xff);
	int my_g = ((my_rgb >> 16) & 0xff);
	int my_r = ((my_rgb >> 24) & 0xff);

	int target_b = ((target_rgb >> 8) & 0xff);
	int target_g = ((target_rgb >> 16) & 0xff);
	int target_r = ((target_rgb >> 24) & 0xff);

	float backlerp = 1.0f - lerp;

	int b = (target_b * lerp) + (my_b * backlerp);
	int g = (target_g * lerp) + (my_g * backlerp);
	int r = (target_r * lerp) + (my_r * backlerp);

	self->s.skinNum = (b << 8) | (g << 16) | (r << 24);

	self->nextThink = level.time + 10_hz;
}

static USE(target_light_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->health = ~self->health;

	if (self->health)
		self->svFlags &= ~SVF_NOCLIENT;
	else
		self->svFlags |= SVF_NOCLIENT;

	if (!self->health) {
		self->think = nullptr;
		self->nextThink = 0_ms;
		return;
	}

	// has dynamic light "target"
	if (self->chain) {
		self->think = target_light_think;
		self->nextThink = level.time + 10_hz;
	}
	else if (self->spawnFlags.has(SPAWNFLAG_TARGET_LIGHT_FLICKER)) {
		self->think = target_light_flicker_think;
		self->nextThink = level.time + 10_hz;
	}
}

void SP_target_light(gentity_t* self) {
	self->s.modelIndex = 1;
	self->s.renderFX = RF_CUSTOM_LIGHT;
	self->s.frame = st.radius ? st.radius : 150;
	self->count = self->s.skinNum;
	self->svFlags |= SVF_NOCLIENT;
	self->health = 0;

	if (self->target)
		self->chain = PickTarget(self->target);

	if (self->spawnFlags.has(SPAWNFLAG_TARGET_LIGHT_START_ON))
		target_light_use(self, self, self);

	if (!self->speed)
		self->speed = 1.0f;
	else
		self->speed = 0.1f / self->speed;

	if (level.isN64)
		self->style += 10;

	self->use = target_light_use;

	gi.linkEntity(self);
}

/*QUAKED target_poi (1 0 0) (-4 -4 -4) (4 4 4) NEAREST DUMMY DYNAMIC x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
[Paril-KEX] point of interest for help in player navigation.
Without any additional setup, targeting this entity will switch
the current POI in the level to the one this is linked to.

"count": if set, this value is the 'stage' linked to this POI. A POI
with this set that is activated will only take effect if the current
level's stage value is <= this value, and if it is, will also set
the current level's stage value to this value.

"style": only used for teamed POIs; the POI with the lowest style will
be activated when checking for which POI to activate. This is mainly
useful during development, to easily insert or change the order of teamed
POIs without needing to manually move the entity definitions around.

"team": if set, this will create a team of POIs. Teamed POIs act like
a single unit; activating any of them will do the same thing. When activated,
it will filter through all of the POIs on the team selecting the one that
best fits the current situation. This includes checking "count" and "style"
values. You can also set the NEAREST spawnflag on any of the teamed POIs,
which will additionally cause activation to prefer the nearest one to the player.
Killing a POI via killTarget will remove it from the chain, allowing you to
adjust valid POIs at runtime.

The DUMMY spawnflag is to allow you to use a single POI as a team member
that can be activated, if you're using killtargets to remove POIs.

The DYNAMIC spawnflag is for very specific circumstances where you want
to direct the player to the nearest teamed POI, but want the path to pick
the nearest at any given time rather than only when activated.

The DISABLED flag is mainly intended to work with DYNAMIC & teams; the POI
will be disabled until it is targeted, and afterwards will be enabled until
it is killed.
*/

constexpr SpawnFlags SPAWNFLAG_POI_NEAREST = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_POI_DUMMY = 2_spawnflag;
constexpr SpawnFlags SPAWNFLAG_POI_DYNAMIC = 4_spawnflag;
constexpr SpawnFlags SPAWNFLAG_POI_DISABLED = 8_spawnflag;

static float distance_to_poi(Vector3 start, Vector3 end) {
	PathRequest request;
	request.start = start;
	request.goal = end;
	request.moveDist = 64.f;
	request.pathFlags = PathFlags::All;
	request.nodeSearch.ignoreNodeFlags = true;
	request.nodeSearch.minHeight = 128.0f;
	request.nodeSearch.maxHeight = 128.0f;
	request.nodeSearch.radius = 1024.0f;
	request.pathPoints.count = 0;

	PathInfo info;

	if (gi.GetPathToGoal(request, info))
		return info.pathDistSqr;

	if (info.returnCode == PathReturnCode::NoNavAvailable)
		return (end - start).lengthSquared();

	return std::numeric_limits<float>::infinity();
}

USE(target_poi_use) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	// we were disabled, so remove the disable check
	if (ent->spawnFlags.has(SPAWNFLAG_POI_DISABLED))
		ent->spawnFlags &= ~SPAWNFLAG_POI_DISABLED;

	// early stage check
	if (ent->count && level.poi.currentStage > ent->count)
		return;

	// teamed POIs work a bit differently
	if (ent->team) {
		gentity_t* poi_master = ent->teamMaster;

		// unset ent, since we need to find one that matches
		ent = nullptr;

		float best_distance = std::numeric_limits<float>::infinity();
		int32_t best_style = std::numeric_limits<int32_t>::max();

		gentity_t* dummy_fallback = nullptr;

		for (gentity_t* poi = poi_master; poi; poi = poi->teamChain) {
			// currently disabled
			if (poi->spawnFlags.has(SPAWNFLAG_POI_DISABLED))
				continue;

			// ignore dummy POI
			if (poi->spawnFlags.has(SPAWNFLAG_POI_DUMMY)) {
				dummy_fallback = poi;
				continue;
			}
			// POI is not part of current stage
			else if (poi->count && level.poi.currentStage > poi->count)
				continue;
			// POI isn't the right style
			else if (poi->style > best_style)
				continue;

			float dist = distance_to_poi(activator->s.origin, poi->s.origin);

			// we have one already and it's farther away, don't bother
			if (poi_master->spawnFlags.has(SPAWNFLAG_POI_NEAREST) &&
				ent &&
				dist > best_distance)
				continue;

			// found a better style; overwrite dist
			if (poi->style < best_style) {
				// unless we weren't reachable...
				if (poi_master->spawnFlags.has(SPAWNFLAG_POI_NEAREST) && std::isinf(dist))
					continue;

				best_style = poi->style;
				if (poi_master->spawnFlags.has(SPAWNFLAG_POI_NEAREST))
					best_distance = dist;
				ent = poi;
				continue;
			}

			// if we're picking by nearest, check distance
			if (poi_master->spawnFlags.has(SPAWNFLAG_POI_NEAREST)) {
				if (dist < best_distance) {
					best_distance = dist;
					ent = poi;
					continue;
				}
			}
			else {
				// not picking by distance, so it's order of appearance
				ent = poi;
			}
		}

		// no valid POI found; this isn't always an error,
		// some valid techniques may require this to happen.
		if (!ent) {
			if (dummy_fallback && dummy_fallback->spawnFlags.has(SPAWNFLAG_POI_DYNAMIC))
				ent = dummy_fallback;
			else
				return;
		}

		// copy over POI stage value
		if (ent->count) {
			if (level.poi.currentStage <= ent->count)
				level.poi.currentStage = ent->count;
		}
	}
	else {
		if (ent->count) {
			if (level.poi.currentStage <= ent->count)
				level.poi.currentStage = ent->count;
			else
				return; // this POI is not part of our current stage
		}
	}

	// dummy POI; not valid
	if (!strcmp(ent->className, "target_poi") && ent->spawnFlags.has(SPAWNFLAG_POI_DUMMY) && !ent->spawnFlags.has(SPAWNFLAG_POI_DYNAMIC))
		return;

	level.poi.valid = true;
	level.poi.current = ent->s.origin;
	level.poi.currentImage = ent->noiseIndex;

	if (!strcmp(ent->className, "target_poi") && ent->spawnFlags.has(SPAWNFLAG_POI_DYNAMIC)) {
		level.poi.currentDynamic = nullptr;

		// pick the dummy POI, since it isn't supposed to get freed
		// FIXME maybe store the team string instead?

		for (gentity_t* m = ent->teamMaster; m; m = m->teamChain)
			if (m->spawnFlags.has(SPAWNFLAG_POI_DUMMY)) {
				level.poi.currentDynamic = m;
				break;
			}

		if (!level.poi.currentDynamic)
			gi.Com_PrintFmt("can't activate poi for {}; need DUMMY in chain\n", *ent);
	}
	else
		level.poi.currentDynamic = nullptr;
}

static THINK(target_poi_setup) (gentity_t* self) -> void {
	if (self->team) {
		// copy dynamic/nearest over to all teammates
		if (self->spawnFlags.has((SPAWNFLAG_POI_NEAREST | SPAWNFLAG_POI_DYNAMIC)))
			for (gentity_t* m = self->teamMaster; m; m = m->teamChain)
				m->spawnFlags |= self->spawnFlags & (SPAWNFLAG_POI_NEAREST | SPAWNFLAG_POI_DYNAMIC);

		for (gentity_t* m = self->teamMaster; m; m = m->teamChain) {
			if (strcmp(m->className, "target_poi"))
				gi.Com_PrintFmt("WARNING: {} is teamed with target_poi's; unintentional\n", *m);
		}
	}
}

void SP_target_poi(gentity_t* self) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		FreeEntity(self);
		return;
	}

	if (st.image)
		self->noiseIndex = gi.imageIndex(st.image);
	else
		self->noiseIndex = gi.imageIndex("friend");

	self->use = target_poi_use;
	self->svFlags |= SVF_NOCLIENT;
	self->think = target_poi_setup;
	self->nextThink = level.time + 1_ms;

	if (!self->team) {
		if (self->spawnFlags.has(SPAWNFLAG_POI_NEAREST))
			gi.Com_PrintFmt("{} has useless spawnflag 'NEAREST'\n", *self);
		if (self->spawnFlags.has(SPAWNFLAG_POI_DYNAMIC))
			gi.Com_PrintFmt("{} has useless spawnflag 'DYNAMIC'\n", *self);
	}
}

/*QUAKED target_music (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Change music when used
"sounds" set music track number to change to
*/

static USE(use_target_music) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	gi.configString(CS_CDTRACK, G_Fmt("{}", ent->sounds).data());
}

void SP_target_music(gentity_t* self) {
	self->use = use_target_music;
}

/*QUAKED target_healthbar (0 1 0) (-8 -8 -8) (8 8 8) PVS_ONLY x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Hook up health bars to monsters.
"delay" is how long to show the health bar for after death.
"message" is their name
*/

static USE(use_target_healthbar) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	gentity_t* target = PickTarget(ent->target);

	if (!target || ent->health != target->spawn_count) {
		if (target)
			gi.Com_PrintFmt("{}: target {} changed from what it used to be\n", *ent, *target);
		else
			gi.Com_PrintFmt("{}: no target\n", *ent);
		FreeEntity(ent);
		return;
	}

	for (size_t i = 0; i < MAX_HEALTH_BARS; i++) {
		if (level.campaign.health_bar_entities[i])
			continue;

		ent->enemy = target;
		level.campaign.health_bar_entities[i] = ent;
		gi.configString(CONFIG_HEALTH_BAR_NAME, ent->message);
		return;
	}

	gi.Com_PrintFmt("{}: too many health bars\n", *ent);
	FreeEntity(ent);
}

static THINK(check_target_healthbar) (gentity_t* ent) -> void {
	gentity_t* target = PickTarget(ent->target);
	if (!target || !(target->svFlags & SVF_MONSTER)) {
		if (target != nullptr) {
			gi.Com_PrintFmt("{}: target {} does not appear to be a monster\n", *ent, *target);
		}
		FreeEntity(ent);
		return;
	}

	// just for sanity check
	ent->health = target->spawn_count;
}

void SP_target_healthbar(gentity_t* self) {
	if (deathmatch->integer) {
		FreeEntity(self);
		return;
	}

	if (!self->target || !*self->target) {
		gi.Com_PrintFmt("{}: missing target\n", *self);
		FreeEntity(self);
		return;
	}

	if (!self->message) {
		gi.Com_PrintFmt("{}: missing message\n", *self);
		FreeEntity(self);
		return;
	}

	self->use = use_target_healthbar;
	self->think = check_target_healthbar;
	self->nextThink = level.time + 25_ms;
}

/*QUAKED target_autosave (0 1 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Auto save on command.
*/

static USE(use_target_autosave) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	GameTime save_time = GameTime::from_sec(gi.cvar("g_athena_auto_save_min_time", "60", CVAR_NOSET)->value);

	if (level.time - level.campaign.next_auto_save > save_time) {
		gi.AddCommandString("autosave\n");
		level.campaign.next_auto_save = level.time;
	}
}

void SP_target_autosave(gentity_t* self) {
	if (deathmatch->integer) {
		FreeEntity(self);
		return;
	}

	self->use = use_target_autosave;
}

/*QUAKED target_sky (0 1 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Change sky parameters.
"sky"	environment map name
"skyAxis"	vector axis for rotating sky
"skyRotate"	speed of rotation in degrees/second
*/
// Define constants used in the function for clarity.
namespace SkyUpdateFlags {
	constexpr int ROTATE_SPEED = 1 << 0; // 1
	constexpr int AUTO_ROTATE = 1 << 1; // 2
	constexpr int AXIS = 1 << 2; // 4
}
static USE(use_target_sky)(gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	// Update the skybox texture if a new one is specified.
	if (std::string_view sky_map = self->map.data(); !sky_map.empty()) {
		gi.configString(CS_SKY, sky_map.data());
	}

	// Update rotation properties (speed and auto-rotate enable).
	if (self->count & (SkyUpdateFlags::ROTATE_SPEED | SkyUpdateFlags::AUTO_ROTATE)) {
		std::string_view current_sky_rotate = gi.get_configString(CS_SKYROTATE);
		float rotate{};
		std::int32_t autorotate{};

		// Safely parse the existing "rotate autorotate" string.
		if (auto separator = current_sky_rotate.find(' '); separator != std::string_view::npos) {
			std::from_chars(current_sky_rotate.data(), current_sky_rotate.data() + separator, rotate);
			std::from_chars(current_sky_rotate.data() + separator + 1, current_sky_rotate.data() + current_sky_rotate.size(), autorotate);
		}

		// Override with new values from the entity if flags are set.
		if (self->count & SkyUpdateFlags::ROTATE_SPEED) {
			rotate = self->accel;
		}
		if (self->count & SkyUpdateFlags::AUTO_ROTATE) {
			autorotate = self->style;
		}

		// Set the updated config string using modern, type-safe formatting.
		gi.configString(CS_SKYROTATE, std::format("{} {}", rotate, autorotate).c_str());
	}

	// Update the sky's rotation axis if specified.
	if (self->count & SkyUpdateFlags::AXIS) {
		gi.configString(CS_SKYAXIS, std::format("{}", self->moveDir).c_str());
	}
}

/*
=============
SP_target_sky

Initializes the target_sky entity and copies configuration data with safe bounds checks.
=============
*/
void SP_target_sky(gentity_t* self) {
	self->use = use_target_sky;
	if (st.was_key_specified("sky") && st.sky) {
		const auto copied = Q_strlcpy(self->map.data(), st.sky, self->map.size());
		if (copied >= self->map.size()) {
			gi.Com_PrintFmt("{}: sky '{}' truncated to fit {} characters\n", *self, st.sky, self->map.size() - 1);
		}
	}
	if (st.was_key_specified("skyAxis")) {
		self->count |= 4;
		self->moveDir = st.skyAxis;
	}
	if (st.was_key_specified("skyRotate")) {
		self->count |= 1;
		self->accel = st.skyRotate;
	}
	if (st.was_key_specified("skyAutoRotate")) {
		self->count |= 2;
		self->style = st.skyAutoRotate;
	}
}


//==========================================================

/*QUAKED target_crossunit_trigger (.5 .5 .5) (-8 -8 -8) (8 8 8) TRIGGER1 TRIGGER2 TRIGGER3 TRIGGER4 TRIGGER5 TRIGGER6 TRIGGER7 TRIGGER8
Once this trigger is touched/used, any trigger_crossunit_target with the same trigger number is automatically used when a level is started within the same unit.  It is OK to check multiple triggers.  Message, delay, target, and killTarget also work.
*/
static USE(trigger_crossunit_trigger_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	game.crossUnitFlags |= self->spawnFlags.value;
	FreeEntity(self);
}

void SP_target_crossunit_trigger(gentity_t* self) {
	if (deathmatch->integer) {
		FreeEntity(self);
		return;
	}

	self->svFlags = SVF_NOCLIENT;
	self->use = trigger_crossunit_trigger_use;
}

/*QUAKED target_crossunit_target (.5 .5 .5) (-8 -8 -8) (8 8 8) TRIGGER1 TRIGGER2 TRIGGER3 TRIGGER4 TRIGGER5 TRIGGER6 TRIGGER7 TRIGGER8 - - - - - - - - TRIGGER9 TRIGGER10 TRIGGER11 TRIGGER12 TRIGGER13 TRIGGER14 TRIGGER15 TRIGGER16
Triggered by a trigger_crossunit elsewhere within a unit.
If multiple triggers are checked, all must be true. Delay, target and killTarget also work.

"delay"		delay before using targets if the trigger has been activated (default 1)
*/
static THINK(target_crossunit_target_think) (gentity_t* self) -> void {
	if (self->spawnFlags.value == (game.crossUnitFlags & SFL_CROSS_TRIGGER_MASK & self->spawnFlags.value)) {
		UseTargets(self, self);
		FreeEntity(self);
	}
}

void SP_target_crossunit_target(gentity_t* self) {
	if (deathmatch->integer) {
		FreeEntity(self);
		return;
	}

	if (!self->delay)
		self->delay = 1;
	self->svFlags = SVF_NOCLIENT;

	self->think = target_crossunit_target_think;
	self->nextThink = level.time + GameTime::from_sec(self->delay);
}

/*QUAKED target_achievement (.5 .5 .5) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Give an achievement.

"achievement"		cheevo to give
*/
static USE(use_target_achievement) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	gi.WriteByte(svc_achievement);
	gi.WriteString(self->map.data());
	gi.multicast(vec3_origin, MULTICAST_ALL, true);
}

/*
=============
SP_target_achievement

Initializes the target_achievement entity and ensures the achievement name fits within the buffer.
=============
*/
void SP_target_achievement(gentity_t* self) {
	if (deathmatch->integer) {
		FreeEntity(self);
		return;
	}

	if (st.achievement) {
		const auto copied = Q_strlcpy(self->map.data(), st.achievement, self->map.size());
		if (copied >= self->map.size()) {
			gi.Com_PrintFmt("{}: achievement '{}' truncated to fit {} characters\n", *self, st.achievement, self->map.size() - 1);
		}
	}
	self->use = use_target_achievement;
}


static USE(use_target_story) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	level.campaign.story_active = !!(self->message && *self->message);
	gi.configString(CONFIG_STORY, self->message ? self->message : "");
}

void SP_target_story(gentity_t* self) {
	if (deathmatch->integer) {
		FreeEntity(self);
		return;
	}

	self->use = use_target_story;
}

/*QUAKED target_mal_laser (1 0 0) (-4 -4 -4) (4 4 4) START_ON RED GREEN BLUE YELLOW ORANGE FAT x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Mal's laser
*/
static void target_mal_laser_on(gentity_t* self) {
	if (!self->activator)
		self->activator = self;
	self->spawnFlags |= SPAWNFLAG_LASER_ZAP | SPAWNFLAG_LASER_ON;
	self->svFlags &= ~SVF_NOCLIENT;
	self->flags |= FL_TRAP;
	// target_laser_think (self);
	self->nextThink = level.time + GameTime::from_sec(self->wait + self->delay);
}

static USE(target_mal_laser_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	self->activator = activator;
	if (self->spawnFlags.has(SPAWNFLAG_LASER_ON))
		target_laser_off(self);
	else
		target_mal_laser_on(self);
}

void mal_laser_think(gentity_t* self);

static THINK(mal_laser_think2) (gentity_t* self) -> void {
	self->svFlags |= SVF_NOCLIENT;
	self->think = mal_laser_think;
	self->nextThink = level.time + GameTime::from_sec(self->wait);
	self->spawnFlags |= SPAWNFLAG_LASER_ZAP;
}

THINK(mal_laser_think) (gentity_t* self) -> void {
	self->svFlags &= ~SVF_NOCLIENT;
	target_laser_think(self);
	self->think = mal_laser_think2;
	self->nextThink = level.time + 100_ms;
}

void SP_target_mal_laser(gentity_t* self) {
	self->moveType = MoveType::None;
	self->solid = SOLID_NOT;
	self->s.renderFX |= RF_BEAM;
	self->s.modelIndex = MODELINDEX_WORLD; // must be non-zero
	self->flags |= FL_TRAP_LASER_FIELD;

	// set the beam diameter
	if (self->spawnFlags.has(SPAWNFLAG_LASER_FAT))
		self->s.frame = 16;
	else
		self->s.frame = 4;

	// set the color
	if (self->spawnFlags.has(SPAWNFLAG_LASER_RED))
		self->s.skinNum = 0xf2f2f0f0;
	else if (self->spawnFlags.has(SPAWNFLAG_LASER_GREEN))
		self->s.skinNum = 0xd0d1d2d3;
	else if (self->spawnFlags.has(SPAWNFLAG_LASER_BLUE))
		self->s.skinNum = 0xf3f3f1f1;
	else if (self->spawnFlags.has(SPAWNFLAG_LASER_YELLOW))
		self->s.skinNum = 0xdcdddedf;
	else if (self->spawnFlags.has(SPAWNFLAG_LASER_ORANGE))
		self->s.skinNum = 0xe0e1e2e3;

	SetMoveDir(self->s.angles, self->moveDir);

	if (!self->delay)
		self->delay = 0.1f;

	if (!self->wait)
		self->wait = 0.1f;

	if (!self->dmg)
		self->dmg = 5;

	self->mins = { -8, -8, -8 };
	self->maxs = { 8, 8, 8 };

	self->nextThink = level.time + GameTime::from_sec(self->delay);
	self->think = mal_laser_think;

	self->use = target_mal_laser_use;

	gi.linkEntity(self);

	if (self->spawnFlags.has(SPAWNFLAG_LASER_ON))
		target_mal_laser_on(self);
	else
		target_laser_off(self);
}


//==========================================================

/*QUAKED target_steam (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Creates a steam effect (particles w/ velocity in a line).

speed = velocity of particles (default 50)
count = number of particles (default 32)
sounds = color of particles (default 8 for steam)
	the color range is from this color to this color + 6
wait = seconds to run before stopping (overrides default
	value derived from func_timer)

best way to use this is to tie it to a func_timer that "pokes"
it every second (or however long you set the wait time, above)

note that the width of the base is proportional to the speed
good colors to use:
6-9 - varying whites (darker to brighter)
224 - sparks
176 - blue water
80  - brown water
208 - slime
232 - blood
*/

static USE(use_target_steam) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	// FIXME - this needs to be a global
	static int nextID;
	Vector3	   point;

	if (nextID > 20000)
		nextID = nextID % 20000;

	nextID++;

	// automagically set wait from func_timer unless they set it already, or
	// default to 1000 if not called by a func_timer (eek!)
	if (!self->wait) {
		if (other)
			self->wait = other->wait * 1000;
		else
			self->wait = 1000;
	}

	if (self->enemy) {
		point = (self->enemy->absMin + self->enemy->absMax) * 0.5f;
		self->moveDir = point - self->s.origin;
		self->moveDir.normalize();
	}

	point = self->s.origin + (self->moveDir * (self->style * 0.5f));
	if (self->wait > 100) {
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_STEAM);
		gi.WriteShort(nextID);
		gi.WriteByte(self->count);
		gi.WritePosition(self->s.origin);
		gi.WriteDir(self->moveDir);
		gi.WriteByte(self->sounds & 0xff);
		gi.WriteShort((short int)(self->style));
		gi.WriteLong((int)(self->wait));
		gi.multicast(self->s.origin, MULTICAST_PVS, false);
	}
	else {
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_STEAM);
		gi.WriteShort((short int)-1);
		gi.WriteByte(self->count);
		gi.WritePosition(self->s.origin);
		gi.WriteDir(self->moveDir);
		gi.WriteByte(self->sounds & 0xff);
		gi.WriteShort((short int)(self->style));
		gi.multicast(self->s.origin, MULTICAST_PVS, false);
	}
}

static THINK(target_steam_start) (gentity_t* self) -> void {
	gentity_t* ent;

	self->use = use_target_steam;

	if (self->target) {
		ent = G_FindByString<&gentity_t::targetName>(nullptr, self->target);
		if (!ent)
			gi.Com_PrintFmt("{}: target {} not found\n", *self, self->target);
		self->enemy = ent;
	}
	else {
		SetMoveDir(self->s.angles, self->moveDir);
	}

	if (!self->count)
		self->count = 32;
	if (!self->style)
		self->style = 75;
	if (!self->sounds)
		self->sounds = 8;
	if (self->wait)
		self->wait *= 1000; // we want it in milliseconds, not seconds

	// paranoia is good
	self->sounds &= 0xff;
	self->count &= 0xff;

	self->svFlags = SVF_NOCLIENT;

	gi.linkEntity(self);
}

void SP_target_steam(gentity_t* self) {
	self->style = (int)self->speed;

	if (self->target) {
		self->think = target_steam_start;
		self->nextThink = level.time + 1_sec;
	}
	else
		target_steam_start(self);
}

//==========================================================
// target_anger
//==========================================================

static USE(target_anger_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	gentity_t* target;
	gentity_t* t;

	t = nullptr;
	target = G_FindByString<&gentity_t::targetName>(t, self->killTarget);

	if (target && self->target) {
		// Make whatever a "good guy" so the monster will try to kill it!
		if (!(target->svFlags & SVF_MONSTER)) {
			target->monsterInfo.aiFlags |= AI_GOOD_GUY | AI_DO_NOT_COUNT;
			target->svFlags |= SVF_MONSTER;
			target->health = 300;
		}

		t = nullptr;
		while ((t = G_FindByString<&gentity_t::targetName>(t, self->target))) {
			if (t == self) {
				gi.Com_Print("WARNING: entity used itself.\n");
			}
			else {
				if (t->use) {
					if (t->health <= 0)
						return;

					t->enemy = target;
					t->monsterInfo.aiFlags |= AI_TARGET_ANGER;
					FoundTarget(t);
				}
			}
			if (!self->inUse) {
				gi.Com_Print("entity was removed while using targets\n");
				return;
			}
		}
	}
}

/*QUAKED target_anger (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This trigger will cause an entity to be angry at another entity when a player touches it. Target the
entity you want to anger, and killTarget the entity you want it to be angry at.

target - entity to piss off
killTarget - entity to be pissed off at
*/
void SP_target_anger(gentity_t* self) {
	if (!self->target) {
		gi.Com_Print("target_anger without target!\n");
		FreeEntity(self);
		return;
	}
	if (!self->killTarget) {
		gi.Com_Print("target_anger without killTarget!\n");
		FreeEntity(self);
		return;
	}

	self->use = target_anger_use;
	self->svFlags = SVF_NOCLIENT;
}

// ***********************************
// target_killplayers
// ***********************************

USE(target_killplayers_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	gentity_t* ent;
	level.campaign.deadly_kill_box = true;

	// kill any visible monsters
	for (ent = g_entities; ent < &g_entities[globals.numEntities]; ent++) {
		if (!ent->inUse)
			continue;
		if (ent->health < 1)
			continue;
		if (!ent->takeDamage)
			continue;

		for (auto ce : active_clients()) {
			if (gi.inPVS(ce->s.origin, ent->s.origin, false)) {
				Damage(ent, self, self, vec3_origin, ent->s.origin, vec3_origin,
					ent->health, 0, DamageFlags::NoProtection, ModID::Telefragged);
				break;
			}
		}
	}

	// kill the players
	for (auto ce : active_clients())
		Damage(ce, self, self, vec3_origin, self->s.origin, vec3_origin, 100000, 0, DamageFlags::NoProtection, ModID::Telefragged);

	level.campaign.deadly_kill_box = false;
}

/*QUAKED target_killplayers (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
When triggered, this will kill all the players on the map.
*/
void SP_target_killplayers(gentity_t* self) {
	self->use = target_killplayers_use;
	self->svFlags = SVF_NOCLIENT;
}

/*QUAKED target_blacklight (1 0 1) (-16 -16 -24) (16 16 24) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Pulsing black light with sphere in the center
*/
static THINK(blacklight_think) (gentity_t* self) -> void {
	self->s.angles[PITCH] += frandom(10);
	self->s.angles[YAW] += frandom(10);
	self->s.angles[ROLL] += frandom(10);
	self->nextThink = level.time + FRAME_TIME_MS;
}

void SP_target_blacklight(gentity_t* ent) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		FreeEntity(ent);
		return;
	}

	ent->mins = {};
	ent->maxs = {};

	ent->s.effects |= (EF_TRACKERTRAIL | EF_TRACKER);
	ent->think = blacklight_think;
	ent->s.modelIndex = gi.modelIndex("models/items/spawngro3/tris.md2");
	ent->s.scale = 6.f;
	ent->s.skinNum = 0;
	ent->nextThink = level.time + FRAME_TIME_MS;
	gi.linkEntity(ent);
}

/*QUAKED target_orb (1 0 1) (-16 -16 -24) (16 16 24) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Translucent pulsing orb with speckles
*/
void SP_target_orb(gentity_t* ent) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		FreeEntity(ent);
		return;
	}

	ent->mins = {};
	ent->maxs = {};

	//	ent->s.effects |= EF_TRACKERTRAIL;
	ent->think = blacklight_think;
	ent->nextThink = level.time + 10_hz;
	ent->s.skinNum = 1;
	ent->s.modelIndex = gi.modelIndex("models/items/spawngro3/tris.md2");
	ent->s.frame = 2;
	ent->s.scale = 8.f;
	ent->s.effects |= EF_SPHERETRANS;
	gi.linkEntity(ent);
}

//==========================================================

/*QUAKED target_remove_powerups (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Takes away all the activator's powerups, techs, held items, keys and CTF flags.
*/
static USE(target_remove_powerups_use) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	if (!activator->client)
		return;

	activator->client->ResetPowerups();

	activator->client->pers.ammoMax.fill(50);

	for (int i = 0; i < static_cast<int>(AmmoID::_Total); i++) {
		activator->client->pers.ammoMax[i] = ammoStats[game.ruleset][i].max[0];
	}

	G_CapAllAmmo(activator);

	for (size_t i = 0; i < IT_TOTAL; i++) {
		if (!activator->client->pers.inventory[i])
			continue;

		if (itemList[i].flags & IF_KEY | IF_POWERUP | IF_TIMED | IF_SPHERE | IF_TECH) {
			if (itemList[i].id == IT_POWERUP_QUAD && g_quadhog->integer) {
				// spawn quad

			}
			activator->client->pers.inventory[i] = 0;
		}
		else if (itemList[i].flags & IF_POWER_ARMOR) {
			activator->client->pers.inventory[i] = 0;
			CheckPowerArmorState(activator);
		}
		else if (itemList[i].flags & IF_TECH) {
			activator->client->pers.inventory[i] = 0;
			Tech_DeadDrop(activator);
		}
		else if (itemList[i].id == IT_FLAG_BLUE) {
			activator->client->pers.inventory[i] = 0;
			CTF_ResetTeamFlag(Team::Blue);
		}
		else if (itemList[i].id == IT_FLAG_RED) {
			activator->client->pers.inventory[i] = 0;
			CTF_ResetTeamFlag(Team::Red);
		}
	}
}

void SP_target_remove_powerups(gentity_t* ent) {
	ent->use = target_remove_powerups_use;
}

//==========================================================

/*QUAKED target_remove_weapons (1 0 0) (-8 -8 -8) (8 8 8) BLASTER x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Takes away all the activator's weapons and ammo (except blaster).
BLASTER : also remove blaster
*/
static USE(target_remove_weapons_use) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	if (!activator->client)
		return;

	for (size_t i = 0; i < IT_TOTAL; i++) {
		if (!activator->client->pers.inventory[i])
			continue;

		if (itemList[i].flags & IF_WEAPON | IF_AMMO && itemList[i].id != IT_WEAPON_BLASTER)
			activator->client->pers.inventory[i] = 0;
	}

	NoAmmoWeaponChange(ent, false);

	activator->client->pers.weapon = activator->client->weapon.pending;
	if (activator->client->weapon.pending)
		activator->client->pers.selectedItem = activator->client->weapon.pending->id;
	activator->client->weapon.pending = nullptr;
	activator->client->pers.lastWeapon = activator->client->pers.weapon;
}

void SP_target_remove_weapons(gentity_t* ent) {
	ent->use = target_remove_weapons_use;
}

//==========================================================

/*QUAKED target_give (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Gives the activator the targetted item.
*/
static USE(target_give_use) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	if (!activator->client)
		return;

	ent->item->pickup(ent, activator);
}

void SP_target_give(gentity_t* ent) {
	gentity_t* targetEnt = PickTarget(ent->target);
	if (!targetEnt || !targetEnt->className[0]) {
		gi.Com_PrintFmt("{}: Invalid target entity, removing.\n", *ent);
		FreeEntity(ent);
		return;
	}

	Item* it = FindItemByClassname(targetEnt->className);
	if (!it || !it->pickup) {
		gi.Com_PrintFmt("{}: Targetted entity is not an item, removing.\n", *ent);
		FreeEntity(ent);
		return;
	}

	ent->item = it;
	ent->use = target_give_use;
	ent->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_delay (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Sets a delay before firing its targets.
"wait" seconds to pause before firing targets.
"random" delay variance, total delay = delay +/- random seconds
*/
static THINK(target_delay_think) (gentity_t* ent) -> void {
	UseTargets(ent, ent->activator);
}

static USE(target_delay_use) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	ent->nextThink = GameTime::from_ms(level.time.milliseconds() + (ent->wait + ent->random * crandom()) * 1000);
	ent->think = target_delay_think;
	ent->activator = activator;
}

void SP_target_delay(gentity_t* ent) {
	if (ent->delay)
		ent->wait = ent->delay;
	else if (!ent->wait)
		ent->wait = 1;
	ent->use = target_delay_use;
	ent->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_print (1 0 0) (-8 -8 -8) (8 8 8) REDTEAM BLUETEAM PRIVATE x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Sends a center-printed message to clients.
"message"	text to print
If "private", only the activator gets the message. If no checks, all clients get the message.
*/
static USE(target_print_use) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	if (activator && activator->client && ent->spawnFlags.has(4_spawnflag)) {
		gi.LocClient_Print(activator, PRINT_CENTER, "{}", ent->message);
		return;
	}

	if (ent->spawnFlags.has(3_spawnflag)) {
		if (ent->spawnFlags.has(1_spawnflag))
			BroadcastTeamMessage(Team::Red, PRINT_CENTER, G_Fmt("{}", ent->message).data());
		if (ent->spawnFlags.has(2_spawnflag))
			BroadcastTeamMessage(Team::Blue, PRINT_CENTER, G_Fmt("{}", ent->message).data());
		return;
	}

	gi.LocBroadcast_Print(PRINT_CENTER, "{}", ent->message);
}

void SP_target_print(gentity_t* ent) {
	if (!ent->message[0]) {
		gi.Com_PrintFmt("{}: No message, removing.\n", *ent);
		FreeEntity(ent);
		return;
	}
	ent->use = target_print_use;
	ent->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_teleporter (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The activator will be teleported to the targetted destination.
If no target set, it will find a player spawn point instead.
*/

static USE(target_teleporter_use) (gentity_t* ent, gentity_t* other, gentity_t* activator) -> void {
	if (!activator || (!activator->client && Q_strcasecmp(activator->className, "grenade")))
		return;

	// no target point to teleport to, teleport to a spawn point
	if (!ent->targetEnt) {
		TeleportPlayerToRandomSpawnPoint(activator, true);
		return;
	}

	TeleportPlayer(activator, ent->targetEnt->s.origin, ent->targetEnt->s.angles);
}

void SP_target_teleporter(gentity_t* ent) {
	if (ent->target && ent->target[0]) {
		ent->targetEnt = PickTarget(ent->target);
		if (!ent->targetEnt) {
			gi.Com_PrintFmt("{}: Couldn't find teleporter destination, removing.\n", *ent);
			FreeEntity(ent);
			return;
		}
	}

	ent->use = target_teleporter_use;
}

//==========================================================

/*QUAKED target_kill (.5 .5 .5) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Kills the activator.
*/

static USE(target_kill_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (!activator)
		return;
	Damage(activator, self, self, vec3_origin, self->s.origin, vec3_origin, 100000, 0, DamageFlags::NoProtection, ModID::Unknown);

}

void SP_target_kill(gentity_t* self) {
	self->use = target_kill_use;
	self->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_cvar (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
When targetted sets a cvar to a value.
"cvar" : name of cvar to set
"cvarValue" : value to set cvar to
*/
static USE(target_cvar_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (!activator || !activator->client)
		return;

	gi.cvarSet(st.cvar, st.cvarValue);
}

void SP_target_cvar(gentity_t* ent) {
	if (!st.cvar[0] || !st.cvarValue[0]) {
		FreeEntity(ent);
		return;
	}

	ent->use = target_cvar_use;
}

//==========================================================

/*QUAKED target_setskill (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Set skill level.
"message" : skill level to set to (0-3)

Skill levels are:
0 = Easy
1 = Medium
2 = Hard
3 = Nightmare/Hard+
*/
static USE(target_setskill_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (!activator || !activator->client)
		return;

	int skill_level = std::clamp(atoi(self->message), 0, 4);
	gi.cvarSet("skill", G_Fmt("{}", skill_level).data());
}

void SP_target_setskill(gentity_t* ent) {
	if (!ent->message[0]) {
		gi.Com_PrintFmt("{}: No message key set, removing.\n", *ent);
		FreeEntity(ent);
		return;
	}

	ent->use = target_setskill_use;
}
//==========================================================

/*QUAKED target_score (1 0 0) (-8 -8 -8) (8 8 8) TEAM x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
"count" number of points to adjust by, default 1

The activator is given this many points.

TEAM : also adjust team score
*/
static USE(target_score_use) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (!activator || !activator->client)
		return;

	G_AdjustPlayerScore(activator->client, self->count, Game::Is(GameType::TeamDeathmatch) || self->spawnFlags.has(1_spawnflag), self->count);
}

void SP_target_score(gentity_t* ent) {
	if (!ent->count)
		ent->count = 1;

	ent->use = target_score_use;
}

//==========================================================

/*QUAKED target_shooter_grenade (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a grenade in the set direction when triggered.

dmg		default is 120
speed	default is 600
*/

static USE(use_target_shooter_grenade) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	fire_grenade(self, self->s.origin, self->moveDir, self->dmg, (int)self->speed, 2.5_sec, self->dmg, (crandom_open() * 10.0f), (200 + crandom_open() * 10.0f), true);
	gi.sound(self, CHAN_VOICE, self->noiseIndex, 1, ATTN_NORM, 0);
}

void SP_target_shooter_grenade(gentity_t* self) {
	self->use = use_target_shooter_grenade;
	SetMoveDir(self->s.angles, self->moveDir);
	self->noiseIndex = gi.soundIndex("weapons/grenlf1a.wav");

	if (!self->dmg)
		self->dmg = 120;
	if (!self->speed)
		self->speed = 600;

	self->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_shooter_rocket (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a rocket in the set direction when triggered.

dmg		default is 120
speed	default is 650
*/

static USE(use_target_shooter_rocket) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	fire_rocket(self, self->s.origin, self->moveDir, self->dmg, (int)self->speed, self->dmg, self->dmg);
	gi.sound(self, CHAN_VOICE, self->noiseIndex, 1, ATTN_NORM, 0);
}

void SP_target_shooter_rocket(gentity_t* self) {
	self->use = use_target_shooter_rocket;
	SetMoveDir(self->s.angles, self->moveDir);
	self->noiseIndex = gi.soundIndex("weapons/rocklf1a.wav");

	if (!self->dmg)
		self->dmg = 120;
	if (!self->speed)
		self->speed = 650;

	self->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_shooter_bfg (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a BFG projectile in the set direction when triggered.

dmg			default is 200 in DM, 500 in campaigns
speed		default is 400
*/

static USE(use_target_shooter_bfg) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	fire_bfg(self, self->s.origin, self->moveDir, self->dmg, (int)self->speed, 1000);
	gi.sound(self, CHAN_VOICE, self->noiseIndex, 1, ATTN_NORM, 0);
}

void SP_target_shooter_bfg(gentity_t* self) {
	self->use = use_target_shooter_bfg;
	SetMoveDir(self->s.angles, self->moveDir);
	self->noiseIndex = gi.soundIndex("makron/bfg_fire.wav");

	if (!self->dmg)
		self->dmg = deathmatch->integer ? 200 : 500;
	if (!self->speed)
		self->speed = 400;

	self->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_shooter_prox (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a prox mine in the set direction when triggered.

dmg			default is 90
speed		default is 600
*/

static USE(use_target_shooter_prox) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	fire_prox(self, self->s.origin, self->moveDir, self->dmg, (int)self->speed);
	gi.sound(self, CHAN_VOICE, self->noiseIndex, 1, ATTN_NORM, 0);
}

void SP_target_shooter_prox(gentity_t* self) {
	self->use = use_target_shooter_prox;
	SetMoveDir(self->s.angles, self->moveDir);
	self->noiseIndex = gi.soundIndex("weapons/proxlr1a.wav");

	if (!self->dmg)
		self->dmg = 90;
	if (!self->speed)
		self->speed = 600;

	self->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_shooter_ionripper (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires an ionripper projectile in the set direction when triggered.

dmg			default is 20 in DM and 50 in campaigns
speed		default is 800
*/

static USE(use_target_shooter_ionripper) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	fire_ionripper(self, self->s.origin, self->moveDir, self->dmg, (int)self->speed, EF_IONRIPPER);
	gi.sound(self, CHAN_VOICE, self->noiseIndex, 1, ATTN_NORM, 0);
}

void SP_target_shooter_ionripper(gentity_t* self) {
	self->use = use_target_shooter_ionripper;
	SetMoveDir(self->s.angles, self->moveDir);
	self->noiseIndex = gi.soundIndex("weapons/rippfire.wav");

	if (!self->dmg)
		self->dmg = deathmatch->integer ? 20 : 50;
	if (!self->speed)
		self->speed = 800;

	self->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_shooter_phalanx (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a phalanx projectile in the set direction when triggered.

dmg			default is 80
speed		default is 725
*/

static USE(use_target_shooter_phalanx) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	fire_phalanx(self, self->s.origin, self->moveDir, self->dmg, (int)self->speed, 120, 30);
	gi.sound(self, CHAN_VOICE, self->noiseIndex, 1, ATTN_NORM, 0);
}

void SP_target_shooter_phalanx(gentity_t* self) {
	self->use = use_target_shooter_phalanx;
	SetMoveDir(self->s.angles, self->moveDir);
	self->noiseIndex = gi.soundIndex("weapons/plasshot.wav");

	if (!self->dmg)
		self->dmg = 80;
	if (!self->speed)
		self->speed = 725;

	self->svFlags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_shooter_flechette (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a flechette in the set direction when triggered.

dmg			default is 10
speed		default is 1150
*/

static USE(use_target_shooter_flechette) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	fire_flechette(self, self->s.origin, self->moveDir, self->dmg, (int)self->speed, 0);
	gi.sound(self, CHAN_VOICE, self->noiseIndex, 1, ATTN_NORM, 0);
}

void SP_target_shooter_flechette(gentity_t* self) {
	self->use = use_target_shooter_flechette;
	SetMoveDir(self->s.angles, self->moveDir);
	self->noiseIndex = gi.soundIndex("weapons/nail1.wav");

	if (!self->dmg)
		self->dmg = 10;
	if (!self->speed)
		self->speed = 1150;

	self->svFlags = SVF_NOCLIENT;
}

/*QUAKED trap_spikeshooter (0 .5 .8) (-8 -8 -8) (8 8 8) SUPERSPIKE LASER
When triggered, fires a spike (flechette) or a single laser pulse in the set direction.

Keys:
- angles       : orientation of fire direction (required unless you set "mangle"/editor handles)
- dmg          : damage per shot (default spikes 10, superspike 25, laser 15)
- speed        : projectile speed (default spikes 500, laser 1200)
- targetname   : fires when triggered

Spawnflags:
- SUPERSPIKE   : 1 = use stronger spike damage (25)
- LASER        : 2 = fire a laser pulse instead of a spike

Notes:
- This is a compatibility wrapper for Quake 1 maps.
- For flexible projectile shooters, prefer target_shooter_* entities.
*/
constexpr SpawnFlags SPAWNFLAG_SPIKESHOOTER_SUPERSPIKE = 1_spawnflag;
constexpr SpawnFlags SPAWNFLAG_SPIKESHOOTER_LASER = 2_spawnflag;

/*
===============
use_trap_spikeshooter
===============
*/
static USE(use_trap_spikeshooter) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	if (self->spawnFlags.has(SPAWNFLAG_SPIKESHOOTER_LASER)) {
		// Laser pulse: use blaster-style bolt as a close analogue to Q1's LaunchLaser()
		const int   dmg = self->dmg ? self->dmg : 15;
		const int   speed = self->speed ? (int)self->speed : 1200;
		fire_blaster(self, self->s.origin, self->moveDir, dmg, speed, EF_BLASTER, ModID::Blaster, true);
	}
	else {
		// Spike: use your flechette as the Q2 analogue to Q1 spikes
		int dmg = self->dmg ? self->dmg : 10;
		if (self->spawnFlags.has(SPAWNFLAG_SPIKESHOOTER_SUPERSPIKE) && !self->dmg)
			dmg = 25; // superspike default if mapper didn't specify dmg

		const int speed = self->speed ? (int)self->speed : 500;

		fire_flechette(self, self->s.origin, self->moveDir, dmg, speed, 0);
		gi.sound(self, CHAN_VOICE, gi.soundIndex("weapons/spike2.wav"), 1, ATTN_NORM, 0);
	}
}

/*
===============
Think_trap_shooter
===============
*/
static THINK(Think_trap_shooter) (gentity_t* self) -> void {
	// Fire once, then reschedule
	use_trap_spikeshooter(self, nullptr, nullptr);

	float wait = self->wait > 0.0f ? self->wait : 1.0f;
	self->nextThink = level.time + GameTime::from_sec(wait);
}

/*
===============
SP_trap_spikeshooter
===============
*/
void SP_trap_spikeshooter(gentity_t* self) {
	// Direction
	SetMoveDir(self->s.angles, self->moveDir);

	// Defaults if mapper did not set them
	if (!self->speed) {
		self->speed = self->spawnFlags.has(SPAWNFLAG_SPIKESHOOTER_LASER) ? 1200.0f : 500.0f;
	}
	if (!self->dmg) {
		self->dmg = self->spawnFlags.has(SPAWNFLAG_SPIKESHOOTER_LASER)
			? 15.0f
			: (self->spawnFlags.has(SPAWNFLAG_SPIKESHOOTER_SUPERSPIKE) ? 25.0f : 10.0f);
	}

	// Invisible logic entity
	self->svFlags |= SVF_NOCLIENT;

	// Triggered fire
	self->use = use_trap_spikeshooter;

	gi.linkEntity(self);
}

/*QUAKED trap_shooter (0 .5 .8) (-8 -8 -8) (8 8 8) SUPERSPIKE LASER
Continuously fires spikes (flechettes) or laser pulses.

Keys:
- angles       : orientation of fire direction
- dmg          : damage per shot (defaults like trap_spikeshooter)
- speed        : projectile speed (defaults like trap_spikeshooter)
- wait         : time between shots in seconds (default 1.0)
- delay        : initial delay before first shot (optional; compatibility helper)

Spawnflags:
- SUPERSPIKE   : 1 = stronger spike damage baseline
- LASER        : 2 = fire a laser pulse instead of a spike

Notes:
- Equivalent to Q1 trap_shooter behavior.
*/
/*
===============
SP_trap_shooter
===============
*/
void SP_trap_shooter(gentity_t* self) {
	// Initialize as a spikeshooter first
	SP_trap_spikeshooter(self);

	// Continuous firing
	if (self->wait <= 0.0f)
		self->wait = 1.0f;

	// Support an initial delay via "delay" key if your spawn temp supports it; otherwise start immediately
	const float initialDelay = self->delay > 0.0f ? self->delay : 0.0f;

	self->think = Think_trap_shooter;
	self->nextThink = level.time + GameTime::from_sec(initialDelay);
}

/*QUAKED target_railgun (1 0 0) (-8 -8 -8) (8 8 8)
Fires a railgun shot in set direction when triggered

dmg     default is 150
*/
static USE(use_target_railgun) (gentity_t* self, gentity_t* other, gentity_t* activator) -> void {
	fire_rail(self, self->s.origin, self->moveDir, self->dmg, 200);
	gi.sound(self, CHAN_VOICE, self->noiseIndex, 1, ATTN_NORM, 0);
}

void SP_target_railgun(gentity_t* self) {
	self->use = use_target_railgun;
	SetMoveDir(self->s.angles, self->moveDir);
	self->noiseIndex = gi.soundIndex("weapons/railgf1a.wav");

	if (!self->dmg)
		self->dmg = 150;

	self->svFlags = SVF_NOCLIENT;
}
