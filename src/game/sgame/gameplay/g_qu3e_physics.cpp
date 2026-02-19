// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_qu3e_physics.hpp"

#include "../g_local.hpp"

#include "../third_party/qu3e/q3.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace {

enum class tracked_role_t : uint8_t {
  DynamicGib,
  DynamicBarrel,
  KinematicPusher
};

struct tracked_body_t {
  q3Body *body = nullptr;
  tracked_role_t role = tracked_role_t::KinematicPusher;
};

std::unique_ptr<q3Scene> g_qu3e_scene;
std::unordered_map<int, tracked_body_t> g_qu3e_bodies;
float g_qu3e_step_seconds = 0.0f;

constexpr float k_gib_default_mass = 8.0f;
constexpr float k_barrel_default_mass = 50.0f;
constexpr float k_min_extent = 0.5f;
constexpr float k_min_density = 0.01f;
constexpr float k_max_density = 50.0f;

[[nodiscard]] inline q3Vec3 ToQ3Vec(const Vector3 &v) {
  return q3Vec3(v.x, v.y, v.z);
}

[[nodiscard]] inline Vector3 FromQ3Vec(const q3Vec3 &v) {
  return Vector3(v.x, v.y, v.z);
}

[[nodiscard]] inline bool IsDynamicRole(tracked_role_t role) {
  return role == tracked_role_t::DynamicGib ||
         role == tracked_role_t::DynamicBarrel;
}

[[nodiscard]] bool IsSolidPusherCandidate(const gentity_t *ent) {
  if (!ent || !ent->inUse || !ent->className) {
    return false;
  }

  if (ent->solid == SOLID_NOT || ent->solid == SOLID_TRIGGER) {
    return false;
  }

  if (ent->client) {
    return ent->health > 0;
  }

  if (ent->svFlags & SVF_MONSTER) {
    return ent->health > 0;
  }

  return false;
}

[[nodiscard]] std::optional<tracked_role_t>
DetermineTrackedRole(const gentity_t *ent) {
  if (!ent || !ent->inUse || !ent->className) {
    return std::nullopt;
  }

  if (sg_phys_qu3e_gibs && sg_phys_qu3e_gibs->integer &&
      !std::strcmp(ent->className, "gib")) {
    return tracked_role_t::DynamicGib;
  }

  if (sg_phys_qu3e_barrels && sg_phys_qu3e_barrels->integer &&
      !std::strcmp(ent->className, "misc_explobox")) {
    return tracked_role_t::DynamicBarrel;
  }

  if (sg_phys_qu3e_barrels && sg_phys_qu3e_barrels->integer &&
      IsSolidPusherCandidate(ent)) {
    return tracked_role_t::KinematicPusher;
  }

  return std::nullopt;
}

[[nodiscard]] Vector3 ComputeHalfExtents(const gentity_t *ent,
                                         tracked_role_t role) {
  Vector3 half = (ent->maxs - ent->mins) * 0.5f;
  if (half.x >= k_min_extent && half.y >= k_min_extent && half.z >= k_min_extent) {
    return half;
  }

  const float scale = ent->s.scale > 0.0f ? ent->s.scale : 1.0f;

  switch (role) {
  case tracked_role_t::DynamicBarrel:
    return Vector3(16.0f, 16.0f, 20.0f) * scale;
  case tracked_role_t::DynamicGib:
    return Vector3(2.5f, 2.5f, 2.5f) * scale;
  case tracked_role_t::KinematicPusher:
  default:
    return Vector3(16.0f, 16.0f, 24.0f) * scale;
  }
}

[[nodiscard]] float ComputeDensity(const gentity_t *ent, const Vector3 &half,
                                   tracked_role_t role) {
  if (!IsDynamicRole(role)) {
    return 1.0f;
  }

  float desired_mass = static_cast<float>(ent->mass);
  if (desired_mass <= 0.0f) {
    desired_mass =
        role == tracked_role_t::DynamicBarrel ? k_barrel_default_mass
                                              : k_gib_default_mass;
  }

  const float full_x = std::max(half.x * 2.0f, 1.0f);
  const float full_y = std::max(half.y * 2.0f, 1.0f);
  const float full_z = std::max(half.z * 2.0f, 1.0f);
  const float volume = std::max(full_x * full_y * full_z, 1.0f);
  const float density = desired_mass / volume;

  return std::clamp(density, k_min_density, k_max_density);
}

void ResetScene(float step_seconds) {
  if (step_seconds <= 0.0f) {
    step_seconds = 1.0f / 60.0f;
  }

  g_qu3e_scene = std::make_unique<q3Scene>(
      step_seconds, q3Vec3(0.0f, 0.0f, 0.0f), 10);
  g_qu3e_scene->SetEnableFriction(true);
  g_qu3e_bodies.clear();
  g_qu3e_step_seconds = step_seconds;
}

void RemoveBody(int ent_num) {
  auto it = g_qu3e_bodies.find(ent_num);
  if (it == g_qu3e_bodies.end()) {
    return;
  }

  if (g_qu3e_scene && it->second.body) {
    g_qu3e_scene->RemoveBody(it->second.body);
  }

  g_qu3e_bodies.erase(it);
}

[[nodiscard]] q3Body *CreateBodyForEntity(gentity_t *ent, tracked_role_t role) {
  if (!g_qu3e_scene) {
    return nullptr;
  }

  q3BodyDef body_def;
  body_def.bodyType = IsDynamicRole(role) ? eDynamicBody : eKinematicBody;
  body_def.position = ToQ3Vec(ent->s.origin);
  body_def.gravityScale = 0.0f;
  body_def.linearDamping =
      role == tracked_role_t::DynamicBarrel ? 0.35f : 0.12f;
  body_def.angularDamping =
      role == tracked_role_t::DynamicBarrel ? 0.8f : 0.2f;
  body_def.allowSleep = IsDynamicRole(role);
  body_def.awake = true;
  body_def.userData = ent;

  q3Body *body = g_qu3e_scene->CreateBody(body_def);
  if (!body) {
    return nullptr;
  }

  q3BoxDef box_def;
  q3Transform local_space;
  q3Identity(local_space);

  const Vector3 half = ComputeHalfExtents(ent, role);
  const Vector3 center = (ent->mins + ent->maxs) * 0.5f;

  local_space.position = ToQ3Vec(center);
  box_def.Set(local_space, ToQ3Vec(half));
  box_def.SetDensity(ComputeDensity(ent, half, role));

  switch (role) {
  case tracked_role_t::DynamicBarrel:
    box_def.SetFriction(0.85f);
    box_def.SetRestitution(0.08f);
    break;
  case tracked_role_t::DynamicGib:
    box_def.SetFriction(0.45f);
    box_def.SetRestitution(0.25f);
    break;
  case tracked_role_t::KinematicPusher:
  default:
    box_def.SetFriction(0.9f);
    box_def.SetRestitution(0.0f);
    break;
  }

  body->AddBox(box_def);
  return body;
}

void SyncBodyFromEntity(q3Body *body, const gentity_t *ent, tracked_role_t role) {
  body->SetTransform(ToQ3Vec(ent->s.origin));
  body->SetLinearVelocity(ToQ3Vec(ent->velocity));

  if (IsDynamicRole(role)) {
    body->SetAngularVelocity(ToQ3Vec(ent->aVelocity * DEG2RAD(1.0f)));
  } else {
    body->SetAngularVelocity(q3Vec3(0.0f, 0.0f, 0.0f));
  }

  body->SetToAwake();
}

void SyncEntityFromBody(gentity_t *ent, const q3Body *body) {
  const float blend =
      std::clamp(sg_phys_qu3e_velocity_blend->value, 0.0f, 1.0f);
  if (blend <= 0.0f) {
    return;
  }

  const Vector3 linear = FromQ3Vec(body->GetLinearVelocity());
  const Vector3 angular = FromQ3Vec(body->GetAngularVelocity()) * RAD2DEG(1.0f);

  if (std::isfinite(linear.x) && std::isfinite(linear.y) &&
      std::isfinite(linear.z)) {
    ent->velocity += (linear - ent->velocity) * blend;
  }

  if (std::isfinite(angular.x) && std::isfinite(angular.y) &&
      std::isfinite(angular.z)) {
    ent->aVelocity += (angular - ent->aVelocity) * blend;
  }
}

} // namespace

void SG_QU3EPhysics_Init() {
  const float step_seconds = std::max(gi.frameTimeSec, 1.0f / 60.0f);
  ResetScene(step_seconds);
}

void SG_QU3EPhysics_Shutdown() {
  g_qu3e_bodies.clear();
  g_qu3e_scene.reset();
  g_qu3e_step_seconds = 0.0f;
}

void SG_QU3EPhysics_RunFrame() {
  if (!sg_phys_qu3e_enable || !sg_phys_qu3e_enable->integer ||
      globals.numEntities <= 0) {
    if (!g_qu3e_bodies.empty()) {
      for (const auto &entry : g_qu3e_bodies) {
        if (g_qu3e_scene && entry.second.body) {
          g_qu3e_scene->RemoveBody(entry.second.body);
        }
      }
      g_qu3e_bodies.clear();
    }
    return;
  }

  const float frame_seconds = std::max(gi.frameTimeSec, 1.0f / 60.0f);
  if (!g_qu3e_scene ||
      std::fabs(g_qu3e_step_seconds - frame_seconds) > 0.0001f) {
    ResetScene(frame_seconds);
  }

  std::vector<uint8_t> active(static_cast<size_t>(globals.numEntities), 0);

  for (int ent_num = 0; ent_num < globals.numEntities; ++ent_num) {
    gentity_t *ent = &g_entities[ent_num];
    const auto role_opt = DetermineTrackedRole(ent);
    if (!role_opt.has_value()) {
      continue;
    }

    const tracked_role_t role = *role_opt;
    active[static_cast<size_t>(ent_num)] = 1;

    auto it = g_qu3e_bodies.find(ent_num);
    if (it == g_qu3e_bodies.end()) {
      q3Body *body = CreateBodyForEntity(ent, role);
      if (!body) {
        continue;
      }

      tracked_body_t tracked;
      tracked.body = body;
      tracked.role = role;
      it = g_qu3e_bodies.emplace(ent_num, tracked).first;
    } else if (it->second.role != role) {
      RemoveBody(ent_num);

      q3Body *body = CreateBodyForEntity(ent, role);
      if (!body) {
        continue;
      }

      tracked_body_t tracked;
      tracked.body = body;
      tracked.role = role;
      it = g_qu3e_bodies.emplace(ent_num, tracked).first;
    }

    if (it != g_qu3e_bodies.end() && it->second.body) {
      SyncBodyFromEntity(it->second.body, ent, it->second.role);
    }
  }

  for (auto it = g_qu3e_bodies.begin(); it != g_qu3e_bodies.end();) {
    const int ent_num = it->first;
    const bool out_of_bounds =
        ent_num < 0 || ent_num >= globals.numEntities ||
        ent_num >= static_cast<int>(active.size());
    const bool inactive =
        !out_of_bounds && active[static_cast<size_t>(ent_num)] == 0;

    if (out_of_bounds || inactive) {
      if (g_qu3e_scene && it->second.body) {
        g_qu3e_scene->RemoveBody(it->second.body);
      }
      it = g_qu3e_bodies.erase(it);
      continue;
    }

    ++it;
  }

  if (g_qu3e_bodies.empty()) {
    return;
  }

  g_qu3e_scene->Step();

  for (const auto &entry : g_qu3e_bodies) {
    if (!IsDynamicRole(entry.second.role)) {
      continue;
    }

    const int ent_num = entry.first;
    if (ent_num < 0 || ent_num >= globals.numEntities) {
      continue;
    }

    gentity_t *ent = &g_entities[ent_num];
    if (!ent->inUse) {
      continue;
    }

    SyncEntityFromBody(ent, entry.second.body);
  }
}

bool SG_QU3EPhysics_HandleBarrelTouch(gentity_t *barrel, gentity_t *other) {
  if (!barrel || !other || !barrel->inUse || !other->inUse) {
    return false;
  }

  if (!sg_phys_qu3e_enable || !sg_phys_qu3e_enable->integer ||
      !sg_phys_qu3e_barrels || !sg_phys_qu3e_barrels->integer) {
    return false;
  }

  if (!barrel->className || std::strcmp(barrel->className, "misc_explobox")) {
    return false;
  }

  auto it = g_qu3e_bodies.find(barrel->s.number);
  if (it == g_qu3e_bodies.end() || !it->second.body ||
      it->second.role != tracked_role_t::DynamicBarrel) {
    return false;
  }

  Vector3 push_dir = barrel->s.origin - other->s.origin;
  push_dir.z = 0.0f;

  if (push_dir.lengthSquared() < 0.0001f) {
    push_dir = other->velocity;
    push_dir.z = 0.0f;
  }

  if (push_dir.lengthSquared() < 0.0001f) {
    return true;
  }

  push_dir.normalize();

  float ratio = 1.0f;
  if (barrel->mass > 0 && other->mass > 0) {
    ratio = static_cast<float>(other->mass) / static_cast<float>(barrel->mass);
  }
  ratio = std::clamp(ratio, 0.25f, 4.0f);

  const float relative_speed =
      std::max((other->velocity - barrel->velocity).length(), 120.0f);
  const float delta_v = std::clamp(relative_speed * 0.25f * ratio, 15.0f, 220.0f);
  const float body_mass = std::max(it->second.body->GetMass(), 1.0f);

  const Vector3 impulse = push_dir * (body_mass * delta_v);
  it->second.body->ApplyLinearImpulse(ToQ3Vec(impulse));
  it->second.body->SetToAwake();

  // Add immediate response so barrel movement is visible without a full-frame delay.
  barrel->velocity += push_dir * (delta_v * 0.15f);
  return true;
}
