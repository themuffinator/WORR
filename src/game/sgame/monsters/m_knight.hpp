
/*
===============
Knight enums
===============
*/

enum {
        // stand
        FRAME_stand1, FRAME_stand2, FRAME_stand3, FRAME_stand4, FRAME_stand5, FRAME_stand6, FRAME_stand7, FRAME_stand8, FRAME_stand9,

        // run
        FRAME_run1, FRAME_run2, FRAME_run3, FRAME_run4, FRAME_run5, FRAME_run6, FRAME_run7, FRAME_run8,

        // running attack
        FRAME_runattack1, FRAME_runattack2, FRAME_runattack3, FRAME_runattack4, FRAME_runattack5, FRAME_runattack6,
        FRAME_runattack7, FRAME_runattack8, FRAME_runattack9, FRAME_runattack10, FRAME_runattack11,

        // pain variations
        FRAME_pain1, FRAME_pain2, FRAME_pain3,
        FRAME_painb1, FRAME_painb2, FRAME_painb3, FRAME_painb4, FRAME_painb5, FRAME_painb6, FRAME_painb7, FRAME_painb8, FRAME_painb9, FRAME_painb10, FRAME_painb11,

        // melee combo
        FRAME_attackb1, FRAME_attackb2, FRAME_attackb3, FRAME_attackb4, FRAME_attackb5, FRAME_attackb6, FRAME_attackb7, FRAME_attackb8, FRAME_attackb9, FRAME_attackb10,

        // walk
        FRAME_walk1, FRAME_walk2, FRAME_walk3, FRAME_walk4, FRAME_walk5, FRAME_walk6, FRAME_walk7, FRAME_walk8,
        FRAME_walk9, FRAME_walk10, FRAME_walk11, FRAME_walk12, FRAME_walk13, FRAME_walk14, FRAME_walk15, FRAME_walk16,

        // kneel / idle variants
        FRAME_kneel1, FRAME_kneel2, FRAME_kneel3, FRAME_kneel4, FRAME_kneel5,
        FRAME_standing2, FRAME_standing3, FRAME_standing4, FRAME_standing5,

        // deaths
        FRAME_death1, FRAME_death2, FRAME_death3, FRAME_death4, FRAME_death5, FRAME_death6, FRAME_death7, FRAME_death8, FRAME_death9, FRAME_death10,
        FRAME_deathb1, FRAME_deathb2, FRAME_deathb3, FRAME_deathb4, FRAME_deathb5, FRAME_deathb6, FRAME_deathb7, FRAME_deathb8, FRAME_deathb9, FRAME_deathb10, FRAME_deathb11
};

constexpr float MODEL_SCALE = 1.0f;

void SP_monster_knight(gentity_t *self);
