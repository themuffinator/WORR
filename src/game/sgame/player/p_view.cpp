/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

p_view.cpp (Player View) This file is responsible for calculating and applying all client-side
view modifications that are not part of the core player movement. This includes effects like
weapon kickback, view bobbing, damage feedback, and falling effects. It also contains the
server-side logic for lag compensation. Key Responsibilities: - `ClientEndServerFrame`: The main
entry point called each frame to update the player's view state (`player_state_t`). - View
Bobbing: `P_CalcBob` calculates the up-and-down and side-to-side motion of the view as the
player moves. - Damage Feedback: Calculates view kicks (`P_DamageFeedback`) and screen blends
when the player takes damage. - Weapon Kick: `P_AddWeaponKick` applies the recoil effect to the
player's view when they fire a weapon. - Lag Compensation: Implements the `LagCompensate`
system, which temporarily moves other players back in time to their positions as seen by the
attacker, ensuring accurate hit detection in a networked environment.*/

#include "../g_local.hpp"
#include "../monsters/m_player.hpp"
#include "../bots/bot_includes.hpp"

#include <array>

static gentity_t* currentPlayer{};
static gclient_t* currentClient{};

static Vector3 forward, right, up;
float		  xySpeed = 0.0f;

float bobMove = 0.0f;
int	  bobCycle = 0, bobCycleRun = 0;	  // odd cycles are right foot going forward
float bobFracSin = 0.0f; // std::sinf(bobfrac*M_PI)

namespace {

constexpr GameTime kVoteMenuUpdateInterval = 250_ms;
constexpr GameTime kMapSelectorUpdateInterval = 200_ms;
constexpr GameTime kMatchStatsUpdateInterval = 1_sec;

void UpdateCgameUiMenus(gentity_t* ent) {
	if (!ent || !ent->client)
		return;

	auto* cl = ent->client;

	const bool voteActive = Vote_Menu_Active(ent);
	if (!voteActive) {
		if (cl->ui.voteActive) {
			CloseActiveMenu(ent);
		}
	} else {
		if (!cl->ui.voteActive) {
			OpenVoteMenu(ent);
		} else if (cl->ui.voteNextUpdate <= level.time) {
			RefreshVoteMenu(ent);
			cl->ui.voteNextUpdate = level.time + kVoteMenuUpdateInterval;
		}
	}

	const bool mapSelectorActive = (level.mapSelector.voteStartTime != 0_sec);
	if (!mapSelectorActive) {
		if (cl->ui.mapSelectorActive) {
			CloseActiveMenu(ent);
		}
	} else {
		if (!cl->ui.mapSelectorActive) {
			OpenMapSelectorMenu(ent);
		} else if (cl->ui.mapSelectorNextUpdate <= level.time) {
			RefreshMapSelectorMenu(ent);
			cl->ui.mapSelectorNextUpdate = level.time + kMapSelectorUpdateInterval;
		}
	}

	if (!g_matchstats || !g_matchstats->integer) {
		if (cl->ui.matchStatsActive) {
			CloseActiveMenu(ent);
		}
	} else if (cl->ui.matchStatsActive) {
		if (cl->ui.matchStatsNextUpdate <= level.time) {
			RefreshMatchStatsMenu(ent);
			cl->ui.matchStatsNextUpdate = level.time + kMatchStatsUpdateInterval;
		}
	}
}

} // namespace

/*
===============
SkipViewModifiers
===============
*/
static inline bool SkipViewModifiers() {
	if (g_skip_view_modifiers->integer && g_cheats->integer)
		return true;

	// don't do bobbing, etc on grapple
	if (currentClient->grapple.entity &&
		currentClient->grapple.state > GrappleState::Fly) {
		return true;
	}

	// spectator mode
	if (!ClientIsPlaying(currentClient))
		return true;

	return false;
}

/*
===============
P_CalcRoll
===============
*/
static float P_CalcRoll(const Vector3& angles, const Vector3& velocity) {
	if (SkipViewModifiers())
		return 0.0f;

	// Project velocity onto the right vector
	float side = velocity.dot(right);
	const float sign = (side < 0.0f) ? -1.0f : 1.0f;
	side = std::fabs(side);

	const float maxRoll = g_rollAngle->value;
	const float rollSpeed = g_rollSpeed->value;

	// Scale roll by speed up to maximum
	float roll = (side < rollSpeed)
		? (side * maxRoll / rollSpeed)
		: maxRoll;

	return roll * sign;
}

/*
===============
P_DamageFeedback

Handles color blends, view kicks, and damage indicators.
===============
*/
static void P_DamageFeedback(gentity_t* player) {
	if (!player || !player->client)
		return;

	gclient_t* client = player->client;

	// -----------------------------------------------------
	// Flash the backgrounds behind status numbers
	// -----------------------------------------------------
	int16_t flashes = 0;
	if (client->damage.blood)
		flashes |= 1;
	if (client->damage.armor && !(player->flags & FL_GODMODE))
		flashes |= 2;

	if (flashes) {
		client->feedback.flashTime = level.time + 100_ms;
		client->ps.stats[STAT_FLASHES] = flashes;
	}
	else if (client->feedback.flashTime < level.time) {
		client->ps.stats[STAT_FLASHES] = 0;
	}

	// -----------------------------------------------------
	// Total damage this frame
	// -----------------------------------------------------
	float count = static_cast<float>(client->damage.blood + client->damage.armor + client->damage.powerArmor);
	if (count <= 0.0f)
		return; // no damage

	// -----------------------------------------------------
	// Trigger pain animation
	// -----------------------------------------------------
	if (client->anim.priority < ANIM_PAIN && player->s.modelIndex == MODELINDEX_PLAYER) {
		static int painCycle;

		client->anim.priority = ANIM_PAIN;

		if (client->ps.pmove.pmFlags & PMF_DUCKED) {
			player->s.frame = FRAME_crpain1 - 1;
			client->anim.end = FRAME_crpain4;
		}
		else {
			painCycle = (painCycle + 1) % 3;
			switch (painCycle) {
			case 0: player->s.frame = FRAME_pain101 - 1; client->anim.end = FRAME_pain104; break;
			case 1: player->s.frame = FRAME_pain201 - 1; client->anim.end = FRAME_pain204; break;
			case 2: player->s.frame = FRAME_pain301 - 1; client->anim.end = FRAME_pain304; break;
			}
		}

		client->anim.time = 0_ms;
	}

	// -----------------------------------------------------
	// Clamp visible effect
	// -----------------------------------------------------
	const float realCount = count;
	if (client->damage.blood) {
		if (count < 10.0f) count = 10.0f;
	}
	else {
		if (count > 2.0f) count = 2.0f;
	}

	// -----------------------------------------------------
	// Pain sounds
	// -----------------------------------------------------
	if ((level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE)) {
		player->pain_debounce_time = level.time + 700_ms;

		constexpr std::array<const char*, 8> painSounds{ {
			"*pain25_1.wav", "*pain25_2.wav",
			"*pain50_1.wav", "*pain50_2.wav",
			"*pain75_1.wav", "*pain75_2.wav",
			"*pain100_1.wav", "*pain100_2.wav"
		} };

		int index = (player->health < 25) ? 0 :
			(player->health < 50) ? 2 :
			(player->health < 75) ? 4 : 6;

		if (brandom()) index |= 1;

		gi.sound(player, CHAN_VOICE, gi.soundIndex(painSounds[index]), 1.0f, ATTN_NORM, 0);
		G_PlayerNoise(player, player->s.origin, PlayerNoise::Self); // alert monsters
	}

	// -----------------------------------------------------
	// Damage blend (color flash)
	// -----------------------------------------------------
	client->feedback.damageAlpha = std::max(0.0f, client->feedback.damageAlpha);

	if (client->damage.blood || (client->feedback.damageAlpha + count * 0.06f) < 0.15f) {
		client->feedback.damageAlpha += count * 0.06f;
		client->feedback.damageAlpha = std::clamp(client->feedback.damageAlpha, 0.06f, 0.4f);
	}

	static const Vector3 armorColor{ 1.0f, 1.0f, 1.0f };
	static const Vector3 powerColor{ 0.0f, 1.0f, 0.0f };
	static const Vector3 bloodColor{ 1.0f, 0.0f, 0.0f };

	Vector3 blend{};
	if (client->damage.powerArmor)
		blend += powerColor * (client->damage.powerArmor / realCount);
	if (client->damage.blood)
		blend += bloodColor * std::max(15.0f, (client->damage.blood / realCount));
	if (client->damage.armor)
		blend += armorColor * (client->damage.armor / realCount);

	client->feedback.damageBlend = blend.normalized();

	// -----------------------------------------------------
	// View angle kicks
	// -----------------------------------------------------
	float kick = static_cast<float>(std::abs(client->damage.knockback));
	if (kick > 0.0f && player->health > 0) {
		kick = kick * 100.0f / player->health;
		const float minKick = std::min(count * 0.5f, 50.0f);
		kick = std::clamp(kick, minKick, 50.0f);

		Vector3 dir = (client->damage.origin - player->s.origin).normalized();
		client->feedback.vDamageRoll = kick * dir.dot(right) * 0.3f;
		client->feedback.vDamagePitch = kick * -dir.dot(forward) * 0.3f;
		client->feedback.vDamageTime = level.time + DAMAGE_TIME();
	}

	// -----------------------------------------------------
	// Damage indicators (Paril-KEX)
	// -----------------------------------------------------
	if (client->numDamageIndicators > 0) {
		gi.WriteByte(svc_damage);
		gi.WriteByte(client->numDamageIndicators);

		for (size_t i = 0; i < client->numDamageIndicators; i++) {
			auto& indicator = client->damageIndicators[i];

			// encode total damage into 5 bits
			uint8_t encoded = std::clamp(
				(indicator.health + indicator.power + indicator.armor) / 3, 1, 0x1F);

			// encode types
			if (indicator.health) encoded |= 0x20;
			if (indicator.armor)  encoded |= 0x40;
			if (indicator.power)  encoded |= 0x80;

			gi.WriteByte(encoded);
			gi.WriteDir((player->s.origin - indicator.from).normalized());
		}

		gi.unicast(player, false);
	}

	// -----------------------------------------------------
	// Reset damage totals
	// -----------------------------------------------------
	client->damage = {};
	client->numDamageIndicators = 0;
}

/*
===============
OffsetThirdPersonDeathView

Applies a third-person camera offset behind the dead player,
similar to Quake III Arena, using Vector3 and direct trace calls.
===============
*/
static void OffsetThirdPersonDeathView(gentity_t* ent) {
	if (!ent || !ent->client)
		return;

	// Ensure the corpse is visible to its owner before manipulating the
	// third-person camera.
	ent->svFlags &= ~SVF_INSTANCED;
	ent->s.instanceBits = 0;

	// Force model visibility (critical for third-person)
	ent->svFlags &= ~SVF_NOCLIENT;
	ent->flags &= ~FL_NOVISIBLE;

	static const Vector3 mins{ -4.0f, -4.0f, -4.0f };
	static const Vector3 maxs{ 4.0f, 4.0f, 4.0f };
	constexpr float focusDist = 512.0f;
	constexpr float camRange = 80.0f;
	constexpr float camAngleDeg = 0.0f;

	const float forwardScale = cosf(camAngleDeg * PIf / 180.0f);
	const float sideScale = std::sinf(camAngleDeg * PIf / 180.0f);

	// Eye origin at the player's view height.
	Vector3 viewOrigin = ent->s.origin;
	viewOrigin.z += static_cast<float>(ent->viewHeight);

	// Determine the focus direction based on the killer's yaw.
	Vector3 focusAngles = ent->client->ps.viewAngles;
	focusAngles[YAW] = ent->client->killerYaw;
	if (focusAngles[PITCH] > 45.0f)
		focusAngles[PITCH] = 45.0f;

	Vector3 focusForward;
	AngleVectors(focusAngles, focusForward, nullptr, nullptr);
	Vector3 focusPoint = viewOrigin + focusForward * focusDist;

	// Base third-person camera orientation.
	Vector3 cameraAngles = ent->client->ps.viewAngles;
	cameraAngles[YAW] = ent->client->killerYaw;
	cameraAngles[PITCH] *= 0.5f;

	Vector3 forward, right, up;
	AngleVectors(cameraAngles, forward, right, up);

	Vector3 desiredPos = viewOrigin;
	desiredPos.z += 8.0f;
	desiredPos -= forward * (camRange * forwardScale);
	desiredPos -= right * (camRange * sideScale);

	// Prevent the camera from clipping into world geometry.
	trace_t tr = gi.trace(viewOrigin, mins, maxs, desiredPos, ent, MASK_SOLID);
	if (tr.fraction < 1.0f) {
		desiredPos = tr.endPos;
		desiredPos.z += (1.0f - tr.fraction) * 32.0f;

		tr = gi.trace(viewOrigin, mins, maxs, desiredPos, ent, MASK_SOLID);
		desiredPos = tr.endPos;
	}

	Vector3 thirdPersonOffset = desiredPos - ent->s.origin;

	constexpr GameTime deathViewBlendTime = 200_ms;
	auto& deathView = ent->client->deathView;

	if (deathView.active) {
		GameTime elapsed = level.time - deathView.startTime;
		if (elapsed < 0_ms)
			elapsed = 0_ms;

		float alpha;
		if (deathViewBlendTime > 0_ms) {
			alpha = static_cast<float>(elapsed.milliseconds()) /
				static_cast<float>(deathViewBlendTime.milliseconds());
			alpha = std::clamp(alpha, 0.0f, 1.0f);
		}
		else {
			alpha = 1.0f;
		}

		Vector3 startOffset = deathView.startOffset;
		ent->client->ps.viewOffset = startOffset + (thirdPersonOffset - startOffset) * alpha;

		if (alpha >= 1.0f)
			deathView.active = false;
	}
	else {
		ent->client->ps.viewOffset = thirdPersonOffset;
	}

	Vector3 toFocus = focusPoint - desiredPos;
	float focusDistFlat = std::max(1.0f, std::sqrt(toFocus.x * toFocus.x + toFocus.y * toFocus.y));

	ent->client->ps.viewAngles[PITCH] = -RAD2DEG(std::atan2(toFocus.z, focusDistFlat));

	float yawDeg = RAD2DEG(std::atan2(toFocus.y, toFocus.x));
	if (yawDeg < 0.0f)
		yawDeg += 360.0f;
	ent->client->ps.viewAngles[YAW] = yawDeg;
	ent->client->ps.viewAngles[ROLL] = 0.0f;
}

/*
===============
G_CalcViewOffset

Auto pitching on slopes?

  fall from 128: 400 = 160000
  fall from 256: 580 = 336400
  fall from 384: 720 = 518400
  fall from 512: 800 = 640000
  fall from 640: 960 =

  damage = deltavelocity*deltavelocity  * 0.0001

===============
*/
static void G_CalcViewOffset(gentity_t* ent) {
	float  bob;
	float  ratio;
	float  delta;
	Vector3 v;

	//===================================

	// base angles
	Vector3& angles = ent->client->ps.kickAngles;

	// if dead, fix the angle and don't add any kick
	if (ent->deadFlag && ClientIsPlaying(ent->client)) {
		angles = {};

		if (ent->flags & FL_SAM_RAIMI) {
			ent->client->ps.viewAngles[ROLL] = 0;
			ent->client->ps.viewAngles[PITCH] = 0;
		}
		else {
			ent->client->ps.viewAngles[ROLL] = 40;
			ent->client->ps.viewAngles[PITCH] = -15;
		}
		ent->client->ps.viewAngles[YAW] = ent->client->killerYaw;
		OffsetThirdPersonDeathView(ent);
		return;
	}

	ent->client->deathView = {};

	if (!ent->client->pers.bob_skip && !SkipViewModifiers()) {
		// add angles based on weapon kick
		angles = P_CurrentKickAngles(ent);

		// add angles based on damage kick
		if (ent->client->feedback.vDamageTime > level.time) {
			// [Paril-KEX] 100ms of slack is added to account for
			// visual difference in higher tickrates
			GameTime diff = ent->client->feedback.vDamageTime - level.time;

			// slack time remaining
			if (DAMAGE_TIME_SLACK()) {
				if (diff > DAMAGE_TIME() - DAMAGE_TIME_SLACK())
					ratio = (DAMAGE_TIME() - diff).seconds() / DAMAGE_TIME_SLACK().seconds();
				else
					ratio = diff.seconds() / (DAMAGE_TIME() - DAMAGE_TIME_SLACK()).seconds();
			}
			else
				ratio = diff.seconds() / (DAMAGE_TIME() - DAMAGE_TIME_SLACK()).seconds();

			angles[PITCH] += ratio * ent->client->feedback.vDamagePitch;
			angles[ROLL] += ratio * ent->client->feedback.vDamageRoll;
		}

		// add pitch based on fall kick
		if (ent->client->feedback.fallTime > level.time) {
			// [Paril-KEX] 100ms of slack is added to account for
			// visual difference in higher tickrates
			GameTime diff = ent->client->feedback.fallTime - level.time;

			// slack time remaining
			if (DAMAGE_TIME_SLACK()) {
				if (diff > FALL_TIME() - DAMAGE_TIME_SLACK())
					ratio = (FALL_TIME() - diff).seconds() / DAMAGE_TIME_SLACK().seconds();
				else
					ratio = diff.seconds() / (FALL_TIME() - DAMAGE_TIME_SLACK()).seconds();
			}
			else
				ratio = diff.seconds() / (FALL_TIME() - DAMAGE_TIME_SLACK()).seconds();
			angles[PITCH] += ratio * ent->client->feedback.fallValue;
		}

		// add angles based on velocity
		if (!ent->client->pers.bob_skip && !SkipViewModifiers()) {
			delta = ent->velocity.dot(forward);
			angles[PITCH] += delta * run_pitch->value;

			delta = ent->velocity.dot(right);
			angles[ROLL] += delta * run_roll->value;

			// add angles based on bob
			delta = bobFracSin * bob_pitch->value * xySpeed;
			if ((ent->client->ps.pmove.pmFlags & PMF_DUCKED) && ent->groundEntity)
				delta *= 6; // crouching
			delta = min(delta, 1.2f);
			angles[PITCH] += delta;
			delta = bobFracSin * bob_roll->value * xySpeed;
			if ((ent->client->ps.pmove.pmFlags & PMF_DUCKED) && ent->groundEntity)
				delta *= 6; // crouching
			delta = min(delta, 1.2f);
			if (bobCycle & 1)
				delta = -delta;
			angles[ROLL] += delta;
		}

		// add earthquake angles
		if (ent->client->feedback.quakeTime > level.time) {
			float factor = min(1.0f, (ent->client->feedback.quakeTime.seconds() / level.time.seconds()) * 0.25f);

			angles.x += crandom_open() * factor;
			angles.z += crandom_open() * factor;
			angles.y += crandom_open() * factor;
		}
	}

	// [Paril-KEX] std::clamp angles
	for (int i = 0; i < 3; i++)
		angles[i] = std::clamp(angles[i], -31.f, 31.f);

	//===================================

	// base origin

	v = {};

	// add fall height

	if (!ent->client->pers.bob_skip && !SkipViewModifiers()) {
		if (ent->client->feedback.fallTime > level.time) {
			// [Paril-KEX] 100ms of slack is added to account for
			// visual difference in higher tickrates
			GameTime diff = ent->client->feedback.fallTime - level.time;

			// slack time remaining
			if (DAMAGE_TIME_SLACK()) {
				if (diff > FALL_TIME() - DAMAGE_TIME_SLACK())
					ratio = (FALL_TIME() - diff).seconds() / DAMAGE_TIME_SLACK().seconds();
				else
					ratio = diff.seconds() / (FALL_TIME() - DAMAGE_TIME_SLACK()).seconds();
			}
			else
				ratio = diff.seconds() / (FALL_TIME() - DAMAGE_TIME_SLACK()).seconds();
			v[2] -= ratio * ent->client->feedback.fallValue * 0.4f;
		}

		// add bob height
		bob = bobFracSin * xySpeed * bob_up->value;
		if (bob > 6)
			bob = 6;
		// gi.DebugGraph (bob *2, 255);
		v[2] += bob;
	}

	// add kick offset

	if (!ent->client->pers.bob_skip && !SkipViewModifiers())
		v += P_CurrentKickOrigin(ent);

	// absolutely bound offsets
	// so the view can never be outside the player box

	if (v[0] < -14)
		v[0] = -14;
	else if (v[0] > 14)
		v[0] = 14;
	if (v[1] < -14)
		v[1] = -14;
	else if (v[1] > 14)
		v[1] = 14;
	if (v[2] < -22)
		v[2] = -22;
	else if (v[2] > 30)
		v[2] = 30;

	ent->client->ps.viewOffset = v;
}

/*
==============
G_CalcGunOffset
==============
*/
static void G_CalcGunOffset(gentity_t* ent) {
	int	  i;

	if (ent->client->pers.weapon &&
		!((ent->client->pers.weapon->id == IT_WEAPON_PLASMABEAM || ent->client->pers.weapon->id == IT_WEAPON_GRAPPLE) && ent->client->weaponState == WeaponState::Firing)
		&& !SkipViewModifiers()) {
		// gun angles from bobbing
		ent->client->ps.gunAngles[ROLL] = xySpeed * bobFracSin * 0.005f;
		ent->client->ps.gunAngles[YAW] = xySpeed * bobFracSin * 0.01f;
		if (bobCycle & 1) {
			ent->client->ps.gunAngles[ROLL] = -ent->client->ps.gunAngles[ROLL];
			ent->client->ps.gunAngles[YAW] = -ent->client->ps.gunAngles[YAW];
		}

		ent->client->ps.gunAngles[PITCH] = xySpeed * bobFracSin * 0.005f;

		Vector3 viewangles_delta = ent->client->oldViewAngles - ent->client->ps.viewAngles;

		for (i = 0; i < 3; i++)
			ent->client->slow_view_angles[i] += viewangles_delta[i];

		// gun angles from delta movement
		for (i = 0; i < 3; i++) {
			float& d = ent->client->slow_view_angles[i];

			if (!d)
				continue;

			if (d > 180)
				d -= 360;
			if (d < -180)
				d += 360;
			if (d > 45)
				d = 45;
			if (d < -45)
				d = -45;

			// [Sam-KEX] Apply only half-delta. Makes the weapons look less detatched from the player.
			if (i == ROLL)
				ent->client->ps.gunAngles[i] += (0.1f * d) * 0.5f;
			else
				ent->client->ps.gunAngles[i] += (0.2f * d) * 0.5f;

			float reduction_factor = viewangles_delta[i] ? 0.05f : 0.15f;

			if (d > 0)
				d = max(0.f, d - gi.frameTimeMs * reduction_factor);
			else if (d < 0)
				d = min(0.f, d + gi.frameTimeMs * reduction_factor);
		}

		// [Paril-KEX] cl_rollhack
		ent->client->ps.gunAngles[ROLL] = -ent->client->ps.gunAngles[ROLL];
	}
	else {
		for (i = 0; i < 3; i++)
			ent->client->ps.gunAngles[i] = 0;
	}

	// gun height
	ent->client->ps.gunOffset = {};

	// gun_x / gun_y / gun_z are development tools
	for (i = 0; i < 3; i++) {
		ent->client->ps.gunOffset[i] += forward[i] * (gun_y->value);
		ent->client->ps.gunOffset[i] += right[i] * gun_x->value;
		ent->client->ps.gunOffset[i] += up[i] * (-gun_z->value);
	}
}

/*
=============
G_CalcBlend
=============
*/

[[nodiscard]] static float G_PowerUpFadeAlpha(GameTime left, float max_alpha = 0.08f) {
	if (left.milliseconds() > 3000)
		return max_alpha;

	float phase = static_cast<float>(left.milliseconds()) * 2.0f * M_PI / 1000.0f;
	return (std::sin(phase) * 0.5f + 0.5f) * max_alpha;
}

static void G_CalcBlend(gentity_t* ent) {
	GameTime remaining;
	ent->client->ps.screenBlend = {};
	ent->client->ps.damageBlend = {};

	// Base blend from current view contents (water, slime, lava).
	const Vector3 viewOrg = ent->s.origin + ent->client->ps.viewOffset +
		Vector3{ 0.0f, 0.0f, static_cast<float>(ent->client->ps.pmove.viewHeight) };
	const contents_t contents = gi.pointContents(viewOrg);
	if (contents & (CONTENTS_SOLID | CONTENTS_LAVA)) {
		G_AddBlend(1.0f, 0.3f, 0.0f, 0.6f, ent->client->ps.screenBlend);
	}
	else if (contents & CONTENTS_SLIME) {
		G_AddBlend(0.0f, 0.1f, 0.05f, 0.6f, ent->client->ps.screenBlend);
	}
	else if (contents & CONTENTS_WATER) {
		G_AddBlend(0.5f, 0.3f, 0.2f, 0.4f, ent->client->ps.screenBlend);
	}

	auto BlendPowerup = [&](GameTime end_time, float r, float g, float b, float max_alpha, const char* sound = nullptr) {
		if (end_time <= level.time)
			return;

		remaining = end_time - level.time;
		if (remaining.milliseconds() == 3000 && sound)
			gi.sound(ent, CHAN_ITEM, gi.soundIndex(sound), 1, ATTN_NORM, 0);

		float alpha = max_alpha;
		if (G_PowerUpExpiringRelative(remaining))
			alpha = G_PowerUpFadeAlpha(remaining, max_alpha);

		G_AddBlend(r, g, b, alpha, ent->client->ps.screenBlend);
		};

	// Powerups
	if (ent->client->PowerupTimer(PowerupTimer::SpawnProtection) > level.time) {
		G_AddBlend(1, 0, 0, 0.05f, ent->client->ps.screenBlend);
	}
	BlendPowerup(ent->client->PowerupTimer(PowerupTimer::QuadDamage), 0, 0, 1, 0.08f, "items/damage2.wav");
	BlendPowerup(ent->client->PowerupTimer(PowerupTimer::Haste), 1, 0.2f, 0.5f, 0.08f, "items/quadfire2.wav");
	BlendPowerup(ent->client->PowerupTimer(PowerupTimer::DoubleDamage), 0.25f, 0.88f, 0.82f, 0.08f, "misc/ddamage2.wav");
	BlendPowerup(ent->client->PowerupTimer(PowerupTimer::EmpathyShield), 0.9f, 0.1f, 0.1f, 0.08f, "items/suit2.wav");
	BlendPowerup(ent->client->PowerupTimer(PowerupTimer::AntiGravBelt), 0.1f, 0.1f, 0.1f, 0.04f, "items/suit2.wav");
	BlendPowerup(ent->client->PowerupTimer(PowerupTimer::BattleSuit), 0.9f, 0.7f, 0, 0.08f, "items/protect2.wav");
	BlendPowerup(ent->client->PowerupTimer(PowerupTimer::Invisibility), 0.8f, 0.8f, 0.8f, 0.08f, "items/protect2.wav");
	BlendPowerup(ent->client->PowerupTimer(PowerupTimer::EnviroSuit), 0, 1, 0, 0.08f, "items/airout.wav");
	BlendPowerup(ent->client->PowerupTimer(PowerupTimer::Rebreather), 0.4f, 1, 0.4f, 0.04f, "items/airout.wav");

	// Freeze effect
	if (Game::Is(GameType::FreezeTag) && ent->client->eliminated && !ent->client->follow.target) {
		G_AddBlend(0.5f, 0.5f, 0.6f, 0.4f, ent->client->ps.screenBlend);
	}

	// Nuke effect
	if (ent->client->nukeTime > level.time) {
		float brightness = (ent->client->nukeTime - level.time).seconds() / 2.0f;
		G_AddBlend(1, 1, 1, brightness, ent->client->ps.screenBlend);
	}

	// IR goggles
	if (ent->client->PowerupTimer(PowerupTimer::IrGoggles) > level.time) {
		remaining = ent->client->PowerupTimer(PowerupTimer::IrGoggles) - level.time;
		if (G_PowerUpExpiringRelative(remaining)) {
			ent->client->ps.rdFlags |= RDF_IRGOGGLES;
			G_AddBlend(1, 0, 0, 0.2f, ent->client->ps.screenBlend);
		}
		else {
			ent->client->ps.rdFlags &= ~RDF_IRGOGGLES;
		}
	}
	else {
		ent->client->ps.rdFlags &= ~RDF_IRGOGGLES;
	}

	// Damage blend
	if (ent->client->feedback.damageAlpha > 0)
		G_AddBlend(ent->client->feedback.damageBlend[0], ent->client->feedback.damageBlend[1], ent->client->feedback.damageBlend[2], ent->client->feedback.damageAlpha, ent->client->ps.damageBlend);

	// Drowning
	if (ent->airFinished < level.time + 9_sec) {
		static const Vector3 drown_color{ 0.1f, 0.1f, 0.2f };
		constexpr float max_drown_alpha = 0.75f;
		float alpha = (ent->airFinished < level.time) ? 1 : (1.f - ((ent->airFinished - level.time).seconds() / 9.0f));
		G_AddBlend(drown_color[0], drown_color[1], drown_color[2], std::min(alpha, max_drown_alpha), ent->client->ps.damageBlend);
	}

	// Decay blend values
	ent->client->feedback.damageAlpha = std::max(0.0f, ent->client->feedback.damageAlpha - gi.frameTimeSec * 0.6f);
	ent->client->feedback.bonusAlpha = std::max(0.0f, ent->client->feedback.bonusAlpha - gi.frameTimeSec);
}
/*
=============
P_WorldEffects
=============
*/
static void P_WorldEffects() {
	if (level.timeoutActive)
		return;

	// Freecam or following
	if (currentPlayer->moveType == MoveType::FreeCam || currentPlayer->client->follow.target) {
		currentPlayer->airFinished = level.time + 12_sec;
		return;
	}

	constexpr int32_t max_drown_dmg = 15;

	const auto waterLevel = currentPlayer->waterLevel;
	const auto oldWaterLevel = currentClient->oldWaterLevel;
	currentClient->oldWaterLevel = waterLevel;

	const bool breather = currentClient->PowerupTimer(PowerupTimer::Rebreather) > level.time;
	const bool enviroSuit = currentClient->PowerupTimer(PowerupTimer::EnviroSuit) > level.time;
	const bool battleSuit = currentClient->PowerupTimer(PowerupTimer::BattleSuit) > level.time;
	const bool spawnProtection = currentClient->PowerupTimer(PowerupTimer::SpawnProtection) > level.time;
	const bool anyProtection = breather || enviroSuit || battleSuit || spawnProtection;

	auto PlaySound = [](gentity_t* ent, soundchan_t chan, const char* sfx) {
		gi.sound(ent, chan, gi.soundIndex(sfx), 1, ATTN_NORM, 0);
		};
	auto PlayerSfxNoise = []() {
		G_PlayerNoise(currentPlayer, currentPlayer->s.origin, PlayerNoise::Self);
		};

	// Water enter
	if (!oldWaterLevel && waterLevel) {
		PlayerSfxNoise();
		const int waterType = currentPlayer->waterType;
		if (waterType & CONTENTS_LAVA)
			PlaySound(currentPlayer, CHAN_BODY, "player/lava_in.wav");
		else if (waterType & (CONTENTS_SLIME | CONTENTS_WATER))
			PlaySound(currentPlayer, CHAN_BODY, "player/watr_in.wav");

		currentPlayer->flags |= FL_INWATER;
		currentPlayer->damage_debounce_time = level.time - 1_sec;
	}

	// Water exit
	if (oldWaterLevel && !waterLevel) {
		PlayerSfxNoise();
		PlaySound(currentPlayer, CHAN_BODY, "player/watr_out.wav");
		currentPlayer->flags &= ~FL_INWATER;
	}

	// Head submerged
	if (oldWaterLevel != WATER_UNDER && waterLevel == WATER_UNDER) {
		PlaySound(currentPlayer, CHAN_BODY, "player/watr_un.wav");
	}

	// Head resurfaces
	if (currentPlayer->health > 0 && oldWaterLevel == WATER_UNDER && waterLevel != WATER_UNDER) {
		if (currentPlayer->airFinished < level.time) {
			PlaySound(currentPlayer, CHAN_VOICE, "player/gasp1.wav");
			PlayerSfxNoise();
		}
		else if (currentPlayer->airFinished < level.time + 11_sec) {
			PlaySound(currentPlayer, CHAN_VOICE, "player/gasp2.wav");
		}
	}

	// Drowning
	if (waterLevel == WATER_UNDER) {
		if (anyProtection) {
			currentPlayer->airFinished = level.time + 10_sec;
			if (((currentClient->PowerupTimer(PowerupTimer::Rebreather) - level.time).milliseconds() % 2500) == 0) {
				const char* breathSound = currentClient->breatherSound ? "player/u_breath2.wav" : "player/u_breath1.wav";
				PlaySound(currentPlayer, CHAN_AUTO, breathSound);
				currentClient->breatherSound ^= 1;
				PlayerSfxNoise();
			}
		}

		if (currentPlayer->airFinished < level.time && currentPlayer->health > 0) {
			if (currentClient->nextDrownTime < level.time) {
				currentClient->nextDrownTime = level.time + 1_sec;

				currentPlayer->dmg = std::min(currentPlayer->dmg + 2, max_drown_dmg);
				const char* sfx = (currentPlayer->health <= currentPlayer->dmg)
					? "*drown1.wav"
					: (brandom() ? "*gurp1.wav" : "*gurp2.wav");
				PlaySound(currentPlayer, CHAN_VOICE, sfx);

				currentPlayer->pain_debounce_time = level.time;

				Damage(currentPlayer, world, world, vec3_origin, currentPlayer->s.origin,
					vec3_origin, currentPlayer->dmg, 0, DamageFlags::NoArmor, ModID::Drowning);
			}
		}
		else if (currentPlayer->airFinished <= level.time + 3_sec &&
			currentClient->nextDrownTime < level.time) {
			std::string name = fmt::format("player/wade{}.wav", 1 + (static_cast<int32_t>(level.time.seconds()) % 3));
			PlaySound(currentPlayer, CHAN_VOICE, name.c_str());
			currentClient->nextDrownTime = level.time + 1_sec;
		}
	}
	else {
		currentPlayer->airFinished = level.time + 12_sec;
		currentPlayer->dmg = 2;
	}

	// Lava or slime damage
	if (waterLevel && (currentPlayer->waterType & (CONTENTS_LAVA | CONTENTS_SLIME)) &&
		currentPlayer->slime_debounce_time <= level.time) {

		const bool immune = enviroSuit || battleSuit || spawnProtection;
		const int waterType = currentPlayer->waterType;

		if (waterType & CONTENTS_LAVA) {
			if (currentPlayer->health > 0 && currentPlayer->pain_debounce_time <= level.time) {
				PlaySound(currentPlayer, CHAN_VOICE, brandom() ? "player/burn1.wav" : "player/burn2.wav");
				if (immune)
					PlaySound(currentPlayer, CHAN_AUX, "items/protect3.wav");
				currentPlayer->pain_debounce_time = level.time + 1_sec;
			}
			const int dmg = (spawnProtection ? 0 : (enviroSuit || battleSuit ? 1 : 3)) * waterLevel;
			Damage(currentPlayer, world, world, vec3_origin, currentPlayer->s.origin, vec3_origin, dmg, 0, DamageFlags::Normal, ModID::Lava);
		}

		if (waterType & CONTENTS_SLIME) {
			if (!(enviroSuit || battleSuit)) {
				Damage(currentPlayer, world, world, vec3_origin, currentPlayer->s.origin,
					vec3_origin, 1 * waterLevel, 0, DamageFlags::Normal, ModID::Slime);
			}
			else if (currentPlayer->health > 0 && currentPlayer->pain_debounce_time <= level.time) {
				PlaySound(currentPlayer, CHAN_AUX, "items/protect3.wav");
				currentPlayer->pain_debounce_time = level.time + 1_sec;
			}
		}
		currentPlayer->slime_debounce_time = level.time + 10_hz;
	}
}

constexpr int32_t kShellLightRadius = 60;

static int32_t ShellLightColor(renderfx_t shell_fx) {
	const bool red = (shell_fx & RF_SHELL_RED) != RF_NONE;
	const bool green = (shell_fx & RF_SHELL_GREEN) != RF_NONE;
	const bool blue = (shell_fx & RF_SHELL_BLUE) != RF_NONE;

	if (!red && !green && !blue)
		return 0;

	const int32_t r = red ? 255 : 0;
	const int32_t g = green ? 255 : 0;
	const int32_t b = blue ? 255 : 0;

	return (r << 24) | (g << 16) | (b << 8);
}

static void UpdateShellLight(gentity_t* ent) {
	if (!ent || !ent->client)
		return;

	gentity_t* light = ent->client->shellLight;
	if (light && (!light->inUse || light->owner != ent)) {
		ent->client->shellLight = nullptr;
		light = nullptr;
	}

	const renderfx_t shell_fx = ent->s.renderFX & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
	const bool has_shell = (ent->s.effects & EF_COLOR_SHELL) && shell_fx != RF_NONE;

	if (!has_shell) {
		if (light) {
			light->svFlags |= SVF_NOCLIENT;
			gi.unlinkEntity(light);
		}
		return;
	}

	const int32_t color = ShellLightColor(shell_fx);
	if (!color) {
		if (light) {
			light->svFlags |= SVF_NOCLIENT;
			gi.unlinkEntity(light);
		}
		return;
	}

	if (!light) {
		light = Spawn();
		light->className = "player_shell_light";
		light->s.modelIndex = 1;
		light->s.renderFX = RF_CUSTOM_LIGHT;
		light->solid = SOLID_NOT;
		light->moveType = MoveType::None;
		light->owner = ent;
		ent->client->shellLight = light;
	}

	light->svFlags &= ~SVF_NOCLIENT;
	light->s.renderFX = RF_CUSTOM_LIGHT;
	light->s.origin = ent->s.origin;
	light->s.frame = kShellLightRadius;
	light->s.skinNum = color;
	gi.linkEntity(light);
}

void PlayerShellLight_Destroy(gentity_t* player) {
	if (!player || !player->client)
		return;

	gentity_t* light = player->client->shellLight;
	if (light && light->inUse)
		FreeEntity(light);

	player->client->shellLight = nullptr;
}

/*
===============
ClientSetEffects
===============
*/
static void ClientSetEffects(gentity_t* ent) {
	ent->s.effects = EF_NONE;
	ent->s.renderFX &= RF_STAIR_STEP;
	ent->s.renderFX |= RF_IR_VISIBLE;
	ent->s.alpha = 1.0;

	// Early check for third-person death cam
	// If active, apply minimal effects and ensure player model visibility
	if (ent->health <= 0 && ClientIsPlaying(ent->client)) {
		// Force model visibility (critical for third-person)
		ent->svFlags &= ~SVF_NOCLIENT;
		ent->flags &= ~FL_NOVISIBLE;

		UpdateShellLight(ent);

		// Link entity to propagate changes
		gi.linkEntity(ent);

		// Exit early to prevent other effects from interfering
		return;
	}

	if (ent->health <= 0 || ent->client->eliminated || level.intermission.time) {
		UpdateShellLight(ent);
		return;
	}

	if (ent->flags & FL_FLASHLIGHT)
		ent->s.effects |= EF_FLASHLIGHT;

	if (ent->flags & FL_DISGUISED)
		ent->s.renderFX |= RF_USE_DISGUISE;

	if (ent->powerArmorTime > level.time) {
		switch (PowerArmorType(ent)) {
		case IT_POWER_SCREEN:
			ent->s.effects |= EF_POWERSCREEN;
			break;
		case IT_POWER_SHIELD:
			ent->s.effects |= EF_COLOR_SHELL;
			ent->s.renderFX |= RF_SHELL_GREEN;
			break;
		default:
			break;
		}
	}

	if (ent->client->pu_regen_time_blip > level.time) {
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderFX |= RF_SHELL_RED;
	}

	if (ent->client->pu_time_spawn_protection_blip > level.time) {
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderFX |= RF_SHELL_RED;
	}

	CTF_ClientEffects(ent);

	if (Game::Is(GameType::ProBall) && ent->client->pers.inventory[IT_BALL] > 0) {
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderFX |= RF_SHELL_RED | RF_SHELL_GREEN;
	}

	if (ent->client->PowerupTimer(PowerupTimer::QuadDamage) > level.time)
		if (G_PowerUpExpiring(ent->client->PowerupTimer(PowerupTimer::QuadDamage)))
			ent->s.effects |= EF_QUAD;
	if (ent->client->PowerupTimer(PowerupTimer::BattleSuit) > level.time)
		if (G_PowerUpExpiring(ent->client->PowerupTimer(PowerupTimer::BattleSuit)))
			ent->s.effects |= EF_PENT;
	if (ent->client->PowerupTimer(PowerupTimer::Haste) > level.time)
		if (G_PowerUpExpiring(ent->client->PowerupTimer(PowerupTimer::Haste)))
			ent->s.effects |= EF_DUALFIRE;
	if (ent->client->PowerupTimer(PowerupTimer::DoubleDamage) > level.time)
		if (G_PowerUpExpiring(ent->client->PowerupTimer(PowerupTimer::DoubleDamage)))
			ent->s.effects |= EF_DOUBLE;
	if (ent->client->PowerupTimer(PowerupTimer::EmpathyShield) > level.time)
		if (G_PowerUpExpiring(ent->client->PowerupTimer(PowerupTimer::EmpathyShield))) {
			ent->s.effects |= EF_EMPATHY;
			ent->s.effects |= EF_COLOR_SHELL;
			ent->s.renderFX |= RF_SHELL_RED;
		}
	if ((ent->client->ownedSphere) && (ent->client->ownedSphere->spawnFlags == SF_SPHERE_DEFENDER))
		ent->s.effects |= EF_HALF_DAMAGE;
	if (ent->client->trackerPainTime > level.time)
		ent->s.effects |= EF_TRACKERTRAIL;
	if (ent->client->PowerupTimer(PowerupTimer::Invisibility) > level.time) {
		if (ent->client->invisibility_fade_time <= level.time)
			ent->s.alpha = 0.05f;
		else {
			float x = (ent->client->invisibility_fade_time - level.time).seconds() / INVISIBILITY_TIME.seconds();
			ent->s.alpha = std::clamp(x, 0.0125f, 0.2f);
		}
	}

	UpdateShellLight(ent);
}

/*
===============
ClientSetEvent
===============
*/
static void ClientSetEvent(gentity_t* ent) {
	if (level.timeoutActive)
		return;

	if (ent->s.event)
		return;

	if (RS(Quake1))
		return;

	if (ent->client->ps.pmove.pmFlags & PMF_ON_LADDER) {
		if (g_ladderSteps->integer > 1 || (g_ladderSteps->integer == 1 && !deathmatch->integer)) {
			if (currentClient->last_ladder_sound < level.time &&
				(currentClient->last_ladder_pos - ent->s.origin).length() > 48.f) {
				ent->s.event = EV_LADDER_STEP;
				currentClient->last_ladder_pos = ent->s.origin;
				currentClient->last_ladder_sound = level.time + LADDER_SOUND_TIME;
			}
		}
	}
	else if (ent->groundEntity && xySpeed > 225) {
		if ((int)(currentClient->feedback.bobTime + bobMove) != bobCycleRun)
			ent->s.event = EV_FOOTSTEP;
	}
}

static inline bool IsFollowFootstepEvent(entity_event_t event) {
	switch (event) {
	case EV_FOOTSTEP:
	case EV_LADDER_STEP:
		return true;
	default:
		return false;
	}
}

static void ForwardFollowFootsteps(gentity_t* ent) {
	if (!g_eyecam->integer)
		return;

	if (!IsFollowFootstepEvent(ent->s.event))
		return;

	for (auto spec : active_clients()) {
		if (!spec->client || spec->client->follow.target != ent)
			continue;
		if (spec->s.event)
			continue;
		spec->s.event = ent->s.event;
	}
}

/*
===============
ClientSetSound
===============
*/
static void ClientSetSound(gentity_t* ent) {
	if (level.timeoutActive)
		return;

	// help beep (no more than three times)
	if (ent->client->pers.helpChanged && ent->client->pers.helpChanged <= 3 && ent->client->pers.help_time < level.time) {
		if (ent->client->pers.helpChanged == 1) // [KEX] haleyjd: once only
			gi.sound(ent, CHAN_AUTO, gi.soundIndex("misc/pc_up.wav"), 1, ATTN_STATIC, 0);
		ent->client->pers.helpChanged++;
		ent->client->pers.help_time = level.time + 5_sec;
	}

	// reset defaults
	ent->s.sound = 0;
	ent->s.loopAttenuation = 0;
	ent->s.loopVolume = 0;

	if (ent->waterLevel && (ent->waterType & (CONTENTS_LAVA | CONTENTS_SLIME))) {
		ent->s.sound = snd_fry;
		return;
	}

	if (ent->deadFlag || !ClientIsPlaying(ent->client) || ent->client->eliminated)
		return;

	if (ent->client->weaponSound)
		ent->s.sound = ent->client->weaponSound;
	else if (ent->client->pers.weapon) {
		switch (ent->client->pers.weapon->id) {
		case IT_WEAPON_RAILGUN:
			ent->s.sound = gi.soundIndex("weapons/rg_hum.wav");
			break;
		case IT_WEAPON_BFG:
		case IT_WEAPON_PLASMABEAM:
			ent->s.sound = gi.soundIndex("weapons/bfg_hum.wav");
			break;
		case IT_WEAPON_PHALANX:
			ent->s.sound = gi.soundIndex("weapons/phaloop.wav");
			break;
		default:
			break;
		}
	}

	// [Paril-KEX] if no other sound is playing, play appropriate grapple sounds
	if (!ent->s.sound && ent->client->grapple.entity) {
		if (ent->client->grapple.state == GrappleState::Pull)
			ent->s.sound = gi.soundIndex("weapons/grapple/grpull.wav");
		else if (ent->client->grapple.state == GrappleState::Fly)
			ent->s.sound = gi.soundIndex("weapons/grapple/grfly.wav");
		else if (ent->client->grapple.state == GrappleState::Hang)
			ent->s.sound = gi.soundIndex("weapons/grapple/grhang.wav");
	}

	// weapon sounds play at a higher attn
	ent->s.loopAttenuation = ATTN_NORM;
}

/*
===============
PlayerSetFrame
===============
*/
void PlayerSetFrame(gentity_t* ent) {
	gclient_t* client;
	bool	   duck, run;

	if (ent->s.modelIndex != MODELINDEX_PLAYER)
		return; // not in the player model

	client = ent->client;

	duck = client->ps.pmove.pmFlags & PMF_DUCKED ? true : false;
	run = xySpeed ? true : false;

	// check for stand/duck and stop/go transitions
	if (duck != client->anim.duck && client->anim.priority < ANIM_DEATH)
		goto newAnim;
	if (run != client->anim.run && client->anim.priority == ANIM_BASIC)
		goto newAnim;
	if (!ent->groundEntity && client->anim.priority <= ANIM_WAVE)
		goto newAnim;

	if (client->anim.time > level.time)
		return;
	else if ((client->anim.priority & ANIM_REVERSED) && (ent->s.frame > client->anim.end)) {
		if (client->anim.time <= level.time) {
			ent->s.frame--;
			client->anim.time = level.time + 10_hz;
		}
		return;
	}
	else if (!(client->anim.priority & ANIM_REVERSED) && (ent->s.frame < client->anim.end)) {
		// continue an animation
		if (client->anim.time <= level.time) {
			ent->s.frame++;
			client->anim.time = level.time + 10_hz;
		}
		return;
	}

	if (client->anim.priority == ANIM_DEATH)
		return; // stay there
	if (client->anim.priority == ANIM_JUMP) {
		if (!ent->groundEntity)
			return; // stay there
		ent->client->anim.priority = ANIM_WAVE;

		if (duck) {
			ent->s.frame = FRAME_jump6;
			ent->client->anim.end = FRAME_jump4;
			ent->client->anim.priority |= ANIM_REVERSED;
		}
		else {
			ent->s.frame = FRAME_jump3;
			ent->client->anim.end = FRAME_jump6;
		}
		ent->client->anim.time = level.time + 10_hz;
		return;
	}

newAnim:
	// return to either a running or standing frame
	client->anim.priority = ANIM_BASIC;
	client->anim.duck = duck;
	client->anim.run = run;
	client->anim.time = level.time + 10_hz;

	if (!ent->groundEntity) {
		// if on grapple, don't go into jump frame, go into standing
		// frame
		if (client->grapple.entity) {
			if (duck) {
				ent->s.frame = FRAME_crstnd01;
				client->anim.end = FRAME_crstnd19;
			}
			else {
				ent->s.frame = FRAME_stand01;
				client->anim.end = FRAME_stand40;
			}
		}
		else {
			client->anim.priority = ANIM_JUMP;

			if (duck) {
				if (ent->s.frame != FRAME_crwalk2)
					ent->s.frame = FRAME_crwalk1;
				client->anim.end = FRAME_crwalk2;
			}
			else {
				if (ent->s.frame != FRAME_jump2)
					ent->s.frame = FRAME_jump1;
				client->anim.end = FRAME_jump2;
			}
		}
	}
	else if (run) { // running
		if (duck) {
			ent->s.frame = FRAME_crwalk1;
			client->anim.end = FRAME_crwalk6;
		}
		else {
			ent->s.frame = FRAME_run1;
			client->anim.end = FRAME_run6;
		}
	}
	else { // standing
		if (duck) {
			ent->s.frame = FRAME_crstnd01;
			client->anim.end = FRAME_crstnd19;
		}
		else {
			ent->s.frame = FRAME_stand01;
			client->anim.end = FRAME_stand40;
		}
	}
}

/*
=================
P_RunMegaHealth
=================
*/
static void P_RunMegaHealth(gentity_t* ent) {
	if (!ent->client->pers.megaTime)
		return;
	else if (ent->health <= ent->maxHealth) {
		ent->client->pers.megaTime = 0_ms;
		return;
	}

	ent->client->pers.megaTime -= FRAME_TIME_S;

	if (ent->client->pers.megaTime <= 0_ms) {
		ent->health--;

		if (ent->health > ent->maxHealth)
			ent->client->pers.megaTime = 1000_ms;
		else
			ent->client->pers.megaTime = 0_ms;
	}
}

/*
=================
LagCompensate

[Paril-KEX] push all players' origins back to match their lag compensation
=================
*/
void LagCompensate(gentity_t* from_player, const Vector3& start, const Vector3& dir) {
	uint32_t currentFrame = gi.ServerFrame();

	// if you need this to fight monsters, you need help
	if (!deathmatch->integer)
		return;
	else if (!g_lagCompensation->integer)
		return;
	// don't need this
	else if (from_player->client->cmd.serverFrame >= currentFrame ||
		(from_player->svFlags & SVF_BOT))
		return;

	int32_t frameDelta = (currentFrame - from_player->client->cmd.serverFrame) + 1;

	for (auto player : active_clients()) {
		// we aren't gonna hit ourselves
		if (player == from_player)
			continue;

		// not enough data, spare them
		if (player->client->lag.numOrigins < frameDelta)
			continue;

		// if they're way outside of cone of vision, they won't be captured in this
		if ((player->s.origin - start).normalized().dot(dir) < 0.75f)
			continue;

		int32_t lagID = (player->client->lag.nextOrigin - 1) - (frameDelta - 1);

		if (lagID < 0)
			lagID = game.maxLagOrigins + lagID;

		if (lagID < 0 || lagID >= player->client->lag.numOrigins) {
			gi.Com_PrintFmt("{}: lag compensation error.\n", __FUNCTION__);
			UnLagCompensate();
			return;
		}

		const Vector3& lagOrigin = (game.lagOrigins + ((player->s.number - 1) * game.maxLagOrigins))[lagID];

		// no way they'd be hit if they aren't in the PVS
		if (!gi.inPVS(lagOrigin, start, false))
			continue;

		// only back up once
		if (!player->client->lag.isCompensated) {
			player->client->lag.isCompensated = true;
			player->client->lag.restoreOrigin = player->s.origin;
		}

		player->s.origin = lagOrigin;

		gi.linkEntity(player);
	}
}

/*
=================
UnLagCompensate

[Paril-KEX] pop everybody's lag compensation values
=================
*/
void UnLagCompensate() {
	for (auto player : active_clients()) {
		if (player->client->lag.isCompensated) {
			player->client->lag.isCompensated = false;
			player->s.origin = player->client->lag.restoreOrigin;
			gi.linkEntity(player);
		}
	}
}

/*
=================
G_SaveLagCompensation

[Paril-KEX] save the current lag compensation value
=================
*/
static inline void G_SaveLagCompensation(gentity_t* ent) {
	(game.lagOrigins + ((ent->s.number - 1) * game.maxLagOrigins))[ent->client->lag.nextOrigin] = ent->s.origin;
	ent->client->lag.nextOrigin = (ent->client->lag.nextOrigin + 1) % game.maxLagOrigins;

	if (ent->client->lag.numOrigins < game.maxLagOrigins)
		ent->client->lag.numOrigins++;
}

/*
=================
Frenzy_ApplyAmmoRegen
=================
*/
static void Frenzy_ApplyAmmoRegen(gentity_t* ent) {
	if (!g_frenzy->integer || InfiniteAmmoOn(nullptr) || !ent || !ent->client)
		return;

	gclient_t* client = ent->client;

	if (!client->frenzyAmmoRegenTime) {
		client->frenzyAmmoRegenTime = level.time;
		return;
	}

	if (client->frenzyAmmoRegenTime > level.time)
		return;

	struct RegenEntry {
		const int weaponBit;  // If zero, always applies
		const int ammoIndex;
		const int amount;
		const AmmoID maxIndex;   // same index as ammo unless special
	};

	static constexpr std::array<RegenEntry, 10> regenTable{ {
		{ IT_WEAPON_SHOTGUN | IT_WEAPON_SSHOTGUN, IT_AMMO_SHELLS,     4,	AmmoID::Shells     },
		{ IT_WEAPON_MACHINEGUN | IT_WEAPON_CHAINGUN, IT_AMMO_BULLETS, 10,	AmmoID::Bullets    },
		{ 0,                        IT_AMMO_GRENADES,   2,					AmmoID::Grenades  },
		{ IT_WEAPON_RLAUNCHER,      IT_AMMO_ROCKETS,    2,					AmmoID::Rockets  },
		{ IT_WEAPON_HYPERBLASTER | IT_WEAPON_BFG | IT_WEAPON_IONRIPPER | IT_WEAPON_PLASMAGUN | IT_WEAPON_PLASMABEAM, IT_AMMO_CELLS, 8, AmmoID::Cells },
		{ IT_WEAPON_RAILGUN,        IT_AMMO_SLUGS,      1,					AmmoID::Slugs    },
		{ IT_WEAPON_PHALANX,        IT_AMMO_MAGSLUG,    2,					AmmoID::MagSlugs  },
		{ IT_WEAPON_ETF_RIFLE,      IT_AMMO_FLECHETTES,10,					AmmoID::Flechettes },
		{ IT_WEAPON_PROXLAUNCHER,   IT_AMMO_PROX,       1,					AmmoID::ProxMines     },
		{ IT_WEAPON_DISRUPTOR,      IT_AMMO_ROUNDS,     1,					AmmoID::Rounds },
	} };

	for (const auto& entry : regenTable) {
		if (entry.weaponBit == 0 || (client->pers.inventory[entry.weaponBit] != 0)) {
			int& ammo = client->pers.inventory[entry.ammoIndex];
			const int max = client->pers.ammoMax[static_cast<int>(entry.maxIndex)];

			ammo += entry.amount;
			if (ammo > max)
				ammo = max;
		}
	}

	client->frenzyAmmoRegenTime = level.time + 2000_ms;
}

/*
=================
PlayQueuedAwardSound
=================
*/
static void PlayQueuedAwardSound(gentity_t* ent) {
	auto* cl = ent->client;
	auto& queue = cl->pers.awardQueue;

	if (queue.queueSize <= 0 || level.time < queue.nextPlayTime ||
		level.time < cl->pers.announcerNextTime)
		return;

	int index = queue.playIndex;
	if (index >= queue.queueSize)
		return;

	// Play sound
	if (queue.soundIndex[index] > 0) {
		gi.localSound(
			ent,
			static_cast<soundchan_t>(CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX),
			queue.soundIndex[index],
			1.0f,
			ATTN_NONE,
			0.0f,
			0
		);
	}

	const auto medal = queue.medal[index];
	if (medal != PlayerMedal::None) {
		cl->pers.medalType = medal;
		cl->pers.medalTime = level.time;
		cl->pers.medalDisplayCount =
			queue.count[index] > 0 ? queue.count[index] : 1;
	}

	// Schedule next play
	queue.nextPlayTime = level.time + 1800_ms; // delay between awards
	cl->pers.announcerNextTime = std::max(cl->pers.announcerNextTime, queue.nextPlayTime);

	// Shift queue
	queue.playIndex++;
	if (queue.playIndex >= queue.queueSize) {
		queue.queueSize = 0;
		queue.playIndex = 0;
	}
}


/*
=================
ClientEndServerFrame

Called for each player at the end of the server frame
and right after spawning
=================
*/
[[maybe_unused]] static int scorelimit = -1;
void ClientEndServerFrame(gentity_t* ent) {
	// no player exists yet (load game)
	if (!ent->client->pers.spawned && !level.mapSelector.voteStartTime && !IsUiMenuOpen(ent->client))
		return;

	float bobTime = 0, bobTimeRun = 0;
	gentity_t* e = ent; // eyecam or follow targeting can redirect here if needed

	currentPlayer = e;
	currentClient = e->client;

	// check fog changes
	P_ForceFogTransition(ent, false);

	// check goals
	G_PlayerNotifyGoal(ent);

	if (!level.intermission.time)
		PlayQueuedAwardSound(ent);

	// vampiric damage expiration
	// don't expire if only 1 player in the match
	if (g_vampiric_damage->integer && ClientIsPlaying(ent->client) && !CombatIsDisabled() && (ent->health > g_vampiric_exp_min->integer)) {
		if (level.pop.num_playing_clients > 1 && level.time > ent->client->vampiricExpireTime) {
			int quantity = floor((ent->health - 1) / ent->maxHealth) + 1;
			ent->health -= quantity;
			ent->client->vampiricExpireTime = level.time + 1_sec;
			if (ent->health <= 0) {
				G_AdjustPlayerScore(ent->client, -1, Game::Is(GameType::TeamDeathmatch) || Game::Is(GameType::Domination), -1);

				player_die(ent, ent, ent, 1, vec3_origin, { ModID::Expiration, true });
				if (!ent->client->eliminated)
					return;
			}
		}
	}

	//
	// If the origin or velocity have changed since ClientThink(),
	// update the pmove values.  This will happen when the client
	// is pushed by a bmodel or kicked by an explosion.
	//
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	//
	currentClient->ps.pmove.origin = ent->s.origin;
	currentClient->ps.pmove.velocity = ent->velocity;

	//
	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	//
	if (level.intermission.time || ent->client->awaitingRespawn) {
		if (ent->client->awaitingRespawn || (level.intermission.endOfUnit || level.isN64 || (deathmatch->integer && level.intermission.time))) {
			currentClient->ps.screenBlend[3] = currentClient->ps.damageBlend[3] = 0;
			currentClient->ps.gunIndex = 0;
		}
		SetStats(ent);
		SetCoopStats(ent);

		if (deathmatch->integer) {
			UpdateCgameUiMenus(ent);
		}

		// if the scoreboard is up, update it if a client leaves
		if (deathmatch->integer && ent->client->showScores && ent->client->menu.updateTime) {
			DeathmatchScoreboardMessage(ent, ent->enemy);
			gi.unicast(ent, false);
			ent->client->menu.updateTime = 0_ms;
		}

		/*freeze*/
		if (FreezeTag_IsActive() && ent->client->eliminated) {	// || level.framenum & 8) {
			ent->s.effects |= EF_COLOR_SHELL;
			ent->s.renderFX |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
		}
		/*freeze*/

		return;
	}

	if (deathmatch->integer) {
		int limit = GT_ScoreLimit();
		if (!ent->client->ps.stats[STAT_SCORELIMIT] || limit != strtoul(gi.get_configString(CONFIG_STORY), nullptr, 10)) {
			ent->client->ps.stats[STAT_SCORELIMIT] = CONFIG_STORY;
			gi.configString(CONFIG_STORY, limit ? G_Fmt("{}", limit).data() : "");
		}
	}

	// mega health
	P_RunMegaHealth(ent);

	// auto doc tech
	Tech_ApplyAutoDoc(ent);

	// wor: weapons frenzy ammo regen
	Frenzy_ApplyAmmoRegen(ent);

	AngleVectors(ent->client->vAngle, forward, right, up);

	// burn from lava, etc
	P_WorldEffects();

	//
	// set model angles from view angles so other things in
	// the world can tell which direction you are looking
	//
	if (ent->client->vAngle[PITCH] > 180)
		ent->s.angles[PITCH] = (-360 + ent->client->vAngle[PITCH]) / 3;
	else
		ent->s.angles[PITCH] = ent->client->vAngle[PITCH] / 3;

	ent->s.angles[YAW] = ent->client->vAngle[YAW];
	ent->s.angles[ROLL] = 0;
	// [Paril-KEX] cl_rollhack
	ent->s.angles[ROLL] = -P_CalcRoll(ent->s.angles, ent->velocity) * 4;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	xySpeed = sqrt(ent->velocity[_X] * ent->velocity[_X] + ent->velocity[_Y] * ent->velocity[_Y]);

	if (xySpeed < 5) {
		bobMove = 0;
		currentClient->feedback.bobTime = 0; // start at beginning of cycle again
	}
	else if (ent->groundEntity) { // so bobbing only cycles when on ground
		if (xySpeed > 210)
			bobMove = gi.frameTimeMs / 400.f;
		else if (xySpeed > 100)
			bobMove = gi.frameTimeMs / 800.f;
		else
			bobMove = gi.frameTimeMs / 1600.f;
	}

	bobTime = (currentClient->feedback.bobTime += bobMove);
	bobTimeRun = bobTime;

	if ((currentClient->ps.pmove.pmFlags & PMF_DUCKED) && ent->groundEntity)
		bobTime *= 4;

	bobCycle = (int)bobTime;
	bobCycleRun = (int)bobTimeRun;
	bobFracSin = std::fabs(std::sinf(bobTime * PIf));

	// apply all the damage taken this frame
	P_DamageFeedback(e);

	// determine the view offsets
	G_CalcViewOffset(e);

	// determine the gun offsets
	G_CalcGunOffset(e);

	// determine the full screen color blend
	// must be after viewOffset, so eye contents can be
	// accurately determined
	G_CalcBlend(e);

	// chase cam stuff
	if (!ClientIsPlaying(ent->client) || ent->client->eliminated) {
		SetSpectatorStats(ent);

		if (ent->client->follow.target) {
			ent->client->ps.screenBlend = ent->client->follow.target->client->ps.screenBlend;
			ent->client->ps.damageBlend = ent->client->follow.target->client->ps.damageBlend;

			ent->s.effects = ent->client->follow.target->s.effects;
			ent->s.renderFX = ent->client->follow.target->s.renderFX;
		}
	}
	else {
		SetStats(ent);
	}

	CheckFollowStats(ent);

	SetCoopStats(ent);

	ClientSetEvent(ent);
	ForwardFollowFootsteps(ent);

	ClientSetEffects(e);

	ClientSetSound(e);

	PlayerSetFrame(ent);

	ent->client->oldVelocity = ent->velocity;
	ent->client->oldViewAngles = ent->client->ps.viewAngles;
	ent->client->oldGroundEntity = ent->groundEntity;

	/*
	================
	Unified UI update
	- Menus update at a steady, fast cadence.
	- Scoreboard updates only when no menu is visible, on a slower cadence.
	================
	*/

	if (deathmatch->integer) {
		UpdateCgameUiMenus(ent);
	}

	// SCOREBOARD
	if (ent->client->showScores) {
		if (ent->client->menu.updateTime <= level.time) {
			DeathmatchScoreboardMessage(ent, ent->enemy);
			gi.unicast(ent, false);
			ent->client->menu.updateTime = level.time + 3_sec;
		}
	}

	if ((ent->svFlags & SVF_BOT) != 0) {
		Bot_EndFrame(ent);
	}

	P_AssignClientSkinNum(ent);

	if (deathmatch->integer)
		G_SaveLagCompensation(ent);

	Compass_Update(ent, false);

	// [Paril-KEX] in coop, if player collision is enabled and
	// we are currently in no-player-collision mode, check if
	// it's safe.
	if (CooperativeModeOn() && G_ShouldPlayersCollide(false) && !(ent->clipMask & CONTENTS_PLAYER) && ent->takeDamage) {
		bool clipped_player = false;

		for (auto player : active_clients()) {
			if (player == ent)
				continue;

			trace_t clip = gi.clip(player, ent->s.origin, ent->mins, ent->maxs, ent->s.origin, CONTENTS_MONSTER | CONTENTS_PLAYER);

			if (clip.startSolid || clip.allSolid) {
				clipped_player = true;
				break;
			}
		}

		// safe!
		if (!clipped_player)
			ent->clipMask |= CONTENTS_PLAYER;
	}
}
