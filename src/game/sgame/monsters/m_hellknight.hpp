#pragma once

/*
===============
Hell Knight enums
===============
*/
enum {
	// stand 1..9
	FRAME_hk_stand1, FRAME_hk_stand2, FRAME_hk_stand3, FRAME_hk_stand4, FRAME_hk_stand5, FRAME_hk_stand6, FRAME_hk_stand7, FRAME_hk_stand8, FRAME_hk_stand9,
	// walk 1..20
	FRAME_hk_walk1, FRAME_hk_walk2, FRAME_hk_walk3, FRAME_hk_walk4, FRAME_hk_walk5, FRAME_hk_walk6, FRAME_hk_walk7, FRAME_hk_walk8, FRAME_hk_walk9, FRAME_hk_walk10,
	FRAME_hk_walk11, FRAME_hk_walk12, FRAME_hk_walk13, FRAME_hk_walk14, FRAME_hk_walk15, FRAME_hk_walk16, FRAME_hk_walk17, FRAME_hk_walk18, FRAME_hk_walk19, FRAME_hk_walk20,
	// run 1..8
	FRAME_hk_run1, FRAME_hk_run2, FRAME_hk_run3, FRAME_hk_run4, FRAME_hk_run5, FRAME_hk_run6, FRAME_hk_run7, FRAME_hk_run8,
	// pain 1..5
	FRAME_hk_pain1, FRAME_hk_pain2, FRAME_hk_pain3, FRAME_hk_pain4, FRAME_hk_pain5,
	// death a 1..11
	FRAME_hk_deatha1, FRAME_hk_deatha2, FRAME_hk_deatha3, FRAME_hk_deatha4, FRAME_hk_deatha5, FRAME_hk_deatha6, FRAME_hk_deatha7, FRAME_hk_deatha8, FRAME_hk_deatha9, FRAME_hk_deatha10, FRAME_hk_deatha11,
	// death b 1..9
	FRAME_hk_deathb1, FRAME_hk_deathb2, FRAME_hk_deathb3, FRAME_hk_deathb4, FRAME_hk_deathb5, FRAME_hk_deathb6, FRAME_hk_deathb7, FRAME_hk_deathb8, FRAME_hk_deathb9,
	// magic a 1..14
	FRAME_magica1, FRAME_magica2, FRAME_magica3, FRAME_magica4, FRAME_magica5, FRAME_magica6, FRAME_magica7, FRAME_magica8, FRAME_magica9, FRAME_magica10, FRAME_magica11, FRAME_magica12, FRAME_magica13, FRAME_magica14,
	// magic b 1..13
	FRAME_magicb1, FRAME_magicb2, FRAME_magicb3, FRAME_magicb4, FRAME_magicb5, FRAME_magicb6, FRAME_magicb7, FRAME_magicb8, FRAME_magicb9, FRAME_magicb10, FRAME_magicb11, FRAME_magicb12, FRAME_magicb13,
	// magic c 1..11
	FRAME_magicc1, FRAME_magicc2, FRAME_magicc3, FRAME_magicc4, FRAME_magicc5, FRAME_magicc6, FRAME_magicc7, FRAME_magicc8, FRAME_magicc9, FRAME_magicc10, FRAME_magicc11,
	// melee charge 1..16
	FRAME_char_a1, FRAME_char_a2, FRAME_char_a3, FRAME_char_a4, FRAME_char_a5, FRAME_char_a6, FRAME_char_a7, FRAME_char_a8, FRAME_char_a9, FRAME_char_a10, FRAME_char_a11, FRAME_char_a12, FRAME_char_a13, FRAME_char_a14, FRAME_char_a15, FRAME_char_a16,
	// melee slice 1..10
	FRAME_slice1, FRAME_slice2, FRAME_slice3, FRAME_slice4, FRAME_slice5, FRAME_slice6, FRAME_slice7, FRAME_slice8, FRAME_slice9, FRAME_slice10,
	// melee smash 1..11
	FRAME_smash1, FRAME_smash2, FRAME_smash3, FRAME_smash4, FRAME_smash5, FRAME_smash6, FRAME_smash7, FRAME_smash8, FRAME_smash9, FRAME_smash10, FRAME_smash11
};

void SP_monster_hell_knight(gentity_t* self);