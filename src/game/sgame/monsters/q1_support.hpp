#pragma once

#include "../g_local.hpp"

// Shared helpers for Quake 1 monster behaviours.
bool TryRandomTeleportPosition(gentity_t* self, float radius);
void CheckTeleportReturn(gentity_t* self);
void Q1BossExplode(gentity_t* self);

[[nodiscard]] gentity_t* fire_lavaball(gentity_t* self, const Vector3& start, const Vector3& dir,
	int damage, int speed, float damageRadius, int radiusDamage);
void fire_vorepod(gentity_t* self, const Vector3& start, const Vector3& dir, int damage, int speed,
	float damageRadius, int radiusDamage, float turnFraction, int skin);
[[nodiscard]] gentity_t* fire_flame(gentity_t* self, const Vector3& start, const Vector3& dir, int damage, int speed,
        ModID mod = ModID::IonRipper);
void fire_gib(gentity_t* self, const Vector3& start, const Vector3& aimDir, int damage, int speed,
	float rightAdjust, float upAdjust);
void fire_plasmaball(gentity_t* self, const Vector3& start, const Vector3& dir, int damage, int speed, float damageRadius);
void fire_lightning(gentity_t* self, const Vector3& start, const Vector3& dir, int damage, int speed, Effect effect);
