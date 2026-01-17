// Copyright (c) 2025 WOR
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "../g_local.hpp"

// This header mirrors the Quake 1 Enforcer animation groups.
// Ensure these match your generated model frames for the Enforcer.

enum {
	// stand
	FRAME_stand01,
	FRAME_stand02,
	FRAME_stand03,
	FRAME_stand04,
	FRAME_stand05,
	FRAME_stand06,
	FRAME_stand07,

	// walk (Q1 has 16)
	FRAME_walk01,
	FRAME_walk02,
	FRAME_walk03,
	FRAME_walk04,
	FRAME_walk05,
	FRAME_walk06,
	FRAME_walk07,
	FRAME_walk08,
	FRAME_walk09,
	FRAME_walk10,
	FRAME_walk11,
	FRAME_walk12,
	FRAME_walk13,
	FRAME_walk14,
	FRAME_walk15,
	FRAME_walk16,

	// run (Q1 has 8)
	FRAME_run01,
	FRAME_run02,
	FRAME_run03,
	FRAME_run04,
	FRAME_run05,
	FRAME_run06,
	FRAME_run07,
	FRAME_run08,

	// attack (shooting sequence ~10 frames)
	FRAME_attack01,
	FRAME_attack02,
	FRAME_attack03,
	FRAME_attack04,
	FRAME_attack05,
	FRAME_attack06,
	FRAME_attack07,
	FRAME_attack08,
	FRAME_attack09,
	FRAME_attack10,

	// pain variants
	FRAME_paina01, FRAME_paina02, FRAME_paina03, FRAME_paina04,
	FRAME_painb01, FRAME_painb02, FRAME_painb03, FRAME_painb04, FRAME_painb05,
	FRAME_painc01, FRAME_painc02, FRAME_painc03, FRAME_painc04, FRAME_painc05, FRAME_painc06, FRAME_painc07, FRAME_painc08,
	FRAME_paind01, FRAME_paind02, FRAME_paind03, FRAME_paind04, FRAME_paind05, FRAME_paind06, FRAME_paind07, FRAME_paind08,
	FRAME_paind09, FRAME_paind10, FRAME_paind11, FRAME_paind12, FRAME_paind13, FRAME_paind14, FRAME_paind15, FRAME_paind16,
	FRAME_paind17, FRAME_paind18, FRAME_paind19,

	// death (define at least one full sequence, ~8 frames)
	FRAME_death01,
	FRAME_death02,
	FRAME_death03,
	FRAME_death04,
	FRAME_death05,
	FRAME_death06,
	FRAME_death07,
	FRAME_death08
};

// Model scale for Enforcer (adjust to your model export)
constexpr float MODEL_SCALE = 1.0f;

// Spawn entry points
void SP_monster_enforcer(gentity_t *self);
