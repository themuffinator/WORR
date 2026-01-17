// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "../g_local.hpp"

#include <array>

inline constexpr auto DamageBoostTimers = std::to_array<PowerupTimer>({
	PowerupTimer::QuadDamage,
	PowerupTimer::Haste,
	PowerupTimer::DoubleDamage,
});

void Entity_UpdateState(gentity_t* entity);
const gentity_t* FindLocalPlayer();
const gentity_t* FindFirstBot();
const gentity_t* FindFirstMonster();
const gentity_t* FindActorUnderCrosshair(const gentity_t* player);
