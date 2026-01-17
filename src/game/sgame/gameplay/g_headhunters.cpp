/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

g_headhunters.cpp implementation.*/

#include "../g_local.hpp"
#include "g_headhunters.hpp"

#include <algorithm>
#include <array>
#include <limits>

extern const spawn_temp_t& ED_GetSpawnTemp();

namespace HeadHunters {

	namespace {
		constexpr const char* kHeadModelPath = "models/objects/gibs/skull/tris.md2";
		constexpr const char* kReceptacleModelPath = "models/objects/headhunters/receptacle/tris.md2";
		constexpr GameTime kHeadLifetime = 30_sec;
		constexpr float kHeadHorizontalImpulse = 120.0f;
		constexpr float kHeadVerticalImpulse = 200.0f;
		constexpr GameTime kPickupCooldown = 250_ms;
		constexpr GameTime kDropCooldown = 500_ms;
		constexpr float kSpikeForwardOffset = 16.0f;
		constexpr float kSpikeBaseHeight = 12.0f;
		constexpr float kSpikeStep = 6.0f;
		constexpr size_t kMaxSpikeDisplay = 20;
		constexpr std::array<float, gclient_t::HeadHunterData::MAX_ATTACHMENTS> kAttachmentForwardOffsets{ { 14.0f, 12.0f, 12.0f } };
		constexpr std::array<float, gclient_t::HeadHunterData::MAX_ATTACHMENTS> kAttachmentSideOffsets{ { 0.0f, 10.0f, -10.0f } };
		constexpr std::array<float, gclient_t::HeadHunterData::MAX_ATTACHMENTS> kAttachmentUpOffsets{ { 18.0f, 16.0f, 16.0f } };

		constexpr SpawnFlags SPAWNFLAG_RECEPTACLE_RED = 1_spawnflag;
		constexpr SpawnFlags SPAWNFLAG_RECEPTACLE_BLUE = 2_spawnflag;

		inline LevelLocals::HeadHuntersState& State() {
			return level.headHunters;
		}

		inline bool Active() {
			return Game::Is(GameType::HeadHunters);
		}

		int FindReceptacleIndex(const LevelLocals::HeadHuntersState& state, const gentity_t* ent) {
			if (!ent)
				return -1;
			for (size_t i = 0; i < state.receptacles.size(); ++i) {
				const auto& slot = state.receptacles[i];
				if (slot.ent == ent)
					return static_cast<int>(i);
			}
			return -1;
		}

		void RefreshReceptacleCount(LevelLocals::HeadHuntersState& state) {
			size_t count = 0;
			for (auto& slot : state.receptacles) {
				if (slot.ent && slot.ent->inUse)
					++count;
				else
					slot = {};
			}
			state.receptacleCount = count;
		}

		void CleanupLooseHeads(LevelLocals::HeadHuntersState& state) {
			size_t count = 0;
			for (auto& slot : state.looseHeads) {
				if (slot && slot->inUse) {
					++count;
				}
				else {
					slot = nullptr;
				}
			}
			state.looseHeadCount = count;
		}

		void CleanupSpikeQueue(LevelLocals::HeadHuntersState& state) {
			size_t writeIndex = 0;
			for (size_t i = 0; i < state.spikeCount; ++i) {
				auto& entry = state.spikeQueue[i];
				if (entry.ent && entry.ent->inUse && entry.base && entry.base->inUse &&
					FindReceptacleIndex(state, entry.base) >= 0) {
					state.spikeQueue[writeIndex++] = entry;
				}
				else {
					if (entry.ent && entry.ent->inUse)
						FreeEntity(entry.ent);
				}
			}
			for (size_t i = writeIndex; i < state.spikeCount; ++i)
				state.spikeQueue[i] = {};
			state.spikeCount = writeIndex;
		}

		/*
		=============
		RegisterLooseHead

		Tracks a loose head entity, cleaning up stale entries and handling overflow.
		=============
		*/
		void RegisterLooseHead(gentity_t* ent) {
			auto& state = State();
			CleanupLooseHeads(state);
			for (auto& slot : state.looseHeads) {
				if (!slot) {
					slot = ent;
					CleanupLooseHeads(state);
					return;
				}
			}
			gi.Com_PrintFmt("HeadHunters: loose head overflow detected (max {}). Check map or configuration for excessive drops.\n",
					state.looseHeads.size());
			if (ent && ent->inUse)
				FreeEntity(ent);
		}


		void UnregisterLooseHead(gentity_t* ent) {
			auto& state = State();
			for (auto& slot : state.looseHeads) {
				if (slot == ent) {
					slot = nullptr;
					break;
				}
			}
			CleanupLooseHeads(state);
		}

		void SyncClient(gclient_t* client) {
			if (!client)
				return;
			client->ps.stats[STAT_GAMEPLAY_CARRIED] = client->headhunter.carried;
		}

		Vector3 DropOrigin(const gentity_t* player) {
			Vector3 origin = player->s.origin;
			origin.z += player->viewHeight * 0.5f;
			return origin;
		}

		void ClearAttachmentHeads(gclient_t* client) {
			if (!client)
				return;
			for (gentity_t*& head : client->headhunter.attachments) {
				if (head && head->inUse)
					FreeEntity(head);
				head = nullptr;
			}
		}

		gentity_t* SpawnAttachmentHead(gentity_t* owner) {
			gentity_t* head = Spawn();
			if (!head)
				return nullptr;
			auto& state = State();
			if (!state.headModelIndex)
				state.headModelIndex = gi.modelIndex(kHeadModelPath);
			head->className = "headhunters_carried_head";
			head->solid = SOLID_NOT;
			head->clipMask = CONTENTS_NONE;
			head->moveType = MoveType::None;
			head->touch = nullptr;
			head->think = nullptr;
			head->gravityVector = { 0.0f, 0.0f, -1.0f };
			head->s.effects |= EF_ROTATE;
			head->s.renderFX |= RF_IR_VISIBLE | RF_NO_LOD;
			head->s.modelIndex = state.headModelIndex;
			gi.setModel(head, kHeadModelPath);
			head->owner = owner;
			gi.linkEntity(head);
			return head;
		}

		gentity_t* SpawnSpikeDisplayHead(gentity_t* base) {
			gentity_t* head = Spawn();
			if (!head)
				return nullptr;
			auto& state = State();
			if (!state.headModelIndex)
				state.headModelIndex = gi.modelIndex(kHeadModelPath);
			head->className = "headhunters_spike_head";
			head->solid = SOLID_NOT;
			head->clipMask = CONTENTS_NONE;
			head->moveType = MoveType::None;
			head->touch = nullptr;
			head->think = nullptr;
			head->gravityVector = { 0.0f, 0.0f, -1.0f };
			head->s.effects |= EF_ROTATE;
			head->s.renderFX |= RF_IR_VISIBLE | RF_NO_LOD;
			head->s.modelIndex = state.headModelIndex;
			gi.setModel(head, kHeadModelPath);
			head->owner = base;
			gi.linkEntity(head);
			return head;
		}

		size_t DesiredAttachmentCount(const gclient_t* client) {
			if (!client)
				return 0;
			return std::min<size_t>(client->headhunter.carried, client->headhunter.attachments.size());
		}

		void EnsureAttachmentCount(gentity_t* player, size_t desired) {
			if (!player || !player->client)
				return;
			auto& attachments = player->client->headhunter.attachments;
			for (size_t i = 0; i < attachments.size(); ++i) {
				gentity_t*& head = attachments[i];
				if (i < desired) {
					if (!head || !head->inUse)
						head = SpawnAttachmentHead(player);
				}
				else {
					if (head && head->inUse)
						FreeEntity(head);
					head = nullptr;
				}
			}
		}

		Vector3 AttachmentOffset(const Vector3& forward, const Vector3& right, const Vector3& up, size_t slot) {
			const size_t index = std::min(slot, kAttachmentForwardOffsets.size() - 1);
			return forward * kAttachmentForwardOffsets[index] + right * kAttachmentSideOffsets[index] + up * kAttachmentUpOffsets[index];
		}

		void UpdateAttachmentPositions(gentity_t* player) {
			if (!player || !player->client)
				return;
			gclient_t* client = player->client;
			const size_t desired = DesiredAttachmentCount(client);
			if (!desired)
				return;
			Vector3 forward, right, up;
			AngleVectors(client->vAngle, forward, right, up);
			Vector3 base = player->s.origin;
			base.z += player->viewHeight * 0.6f;
			for (size_t i = 0; i < desired; ++i) {
				gentity_t* head = client->headhunter.attachments[i];
				if (!head || !head->inUse)
					continue;
				const Vector3 offset = AttachmentOffset(forward, right, up, i);
				const Vector3 position = base + offset;
				head->s.origin = position;
				head->s.oldOrigin = position;
				head->s.angles = { 0.0f, client->vAngle.y, 0.0f };
				head->velocity = player->velocity;
				head->aVelocity = { 0.0f, 0.0f, 0.0f };
				gi.linkEntity(head);
			}
		}

		size_t CountSpikeEntriesForBase(const LevelLocals::HeadHuntersState& state, const gentity_t* base) {
			size_t count = 0;
			for (size_t i = 0; i < state.spikeCount; ++i) {
				const auto& entry = state.spikeQueue[i];
				if (entry.base == base)
					++count;
			}
			return count;
		}

		void RemoveSpikeEntry(LevelLocals::HeadHuntersState& state, size_t index) {
			if (index >= state.spikeCount)
				return;
			auto& entry = state.spikeQueue[index];
			if (entry.ent && entry.ent->inUse)
				FreeEntity(entry.ent);
			for (size_t i = index + 1; i < state.spikeCount; ++i)
				state.spikeQueue[i - 1] = state.spikeQueue[i];
			state.spikeQueue[state.spikeCount - 1] = {};
			--state.spikeCount;
		}

		void RemoveOldestSpikeForBase(LevelLocals::HeadHuntersState& state, gentity_t* base) {
			bool hasCandidate = false;
			size_t candidate = 0;
			GameTime bestTime{};
			for (size_t i = 0; i < state.spikeCount; ++i) {
				const auto& entry = state.spikeQueue[i];
				if (entry.base != base)
					continue;
				if (!hasCandidate || entry.nextActivation <= bestTime) {
					bestTime = entry.nextActivation;
					candidate = i;
					hasCandidate = true;
				}
			}
			if (hasCandidate)
				RemoveSpikeEntry(state, candidate);
		}

		Vector3 SpikeSlotPosition(const gentity_t* receptacle, size_t slot) {
			Vector3 forward, right, up;
			AngleVectors(receptacle->s.angles, forward, right, up);
			return receptacle->s.origin + forward * kSpikeForwardOffset + up * (kSpikeBaseHeight + kSpikeStep * static_cast<float>(slot));
		}

		void QueueSpikeHeads(gentity_t* base, size_t count) {
			if (!base || !base->inUse || !count)
				return;
			auto& state = State();
			CleanupSpikeQueue(state);
			for (size_t i = 0; i < count; ++i) {
				while (CountSpikeEntriesForBase(state, base) >= kMaxSpikeDisplay)
					RemoveOldestSpikeForBase(state, base);
				if (state.spikeCount >= state.spikeQueue.size())
					RemoveSpikeEntry(state, 0);
				gentity_t* head = SpawnSpikeDisplayHead(base);
				if (!head)
					break;
				auto& entry = state.spikeQueue[state.spikeCount++];
				entry = {};
				entry.ent = head;
				entry.base = base;
				entry.nextActivation = level.time;
			}
		}

		Team ReceptacleTeam(const gentity_t* ent) {
			if (!ent)
				return Team::None;
			const bool red = ent->spawnFlags.has(SPAWNFLAG_RECEPTACLE_RED);
			const bool blue = ent->spawnFlags.has(SPAWNFLAG_RECEPTACLE_BLUE);
			if (red == blue)
				return Team::None;
			return red ? Team::Red : Team::Blue;
		}

		void AnnounceDeposit(const gentity_t* player, uint8_t heads, int points) {
			if (!player || !player->client || !heads)
				return;
			const char* name = player->client->sess.netName;
			const char* headWord = (heads == 1) ? "head" : "heads";
			const char* pointWord = (points == 1) ? "point" : "points";
			gi.LocBroadcast_Print(PRINT_HIGH, "{} deposits {} {} for {} {}!\n", name, heads, headWord, points, pointWord);
		}

		void AnnounceDrop(const gentity_t* player, const gentity_t* instigator, uint8_t heads) {
			if (!player || !player->client || !heads)
				return;
			const char* victimName = player->client->sess.netName;
			const char* headWord = (heads == 1) ? "head" : "heads";
			if (instigator && instigator->client) {
				gi.LocBroadcast_Print(PRINT_HIGH, "{} knocks {}'s {} {} loose!\n",
					instigator->client->sess.netName, victimName, heads, headWord);
			}
			else {
				gi.LocBroadcast_Print(PRINT_HIGH, "{} drops {} {}!\n", victimName, heads, headWord);
			}
		}

		void EnsureReceptacleBounds(gentity_t* ent) {
			if (!ent)
				return;
			if (ent->model && ent->model[0]) {
				gi.setModel(ent, ent->model);
				return;
			}
			const auto& st = ED_GetSpawnTemp();
			if (!(ent->mins || ent->maxs)) {
				const float radius = (st.radius > 0.0f) ? st.radius : 48.0f;
				const float height = (st.height > 0) ? static_cast<float>(st.height) : 64.0f;
				ent->mins = { -radius, -radius, 0.0f };
				ent->maxs = { radius, radius, height };
			}
			gi.setModel(ent, kReceptacleModelPath);
		}

	} // namespace

	void ClearState() {
		auto& state = State();
		for (size_t i = 0; i < game.maxClients; ++i)
			ClearAttachmentHeads(&game.clients[i]);
		for (gentity_t* ent : state.looseHeads) {
			if (ent && ent->inUse && ent->touch == HandlePickup)
				FreeEntity(ent);
		}
		for (auto& entry : state.spikeQueue) {
			if (entry.ent && entry.ent->inUse)
				FreeEntity(entry.ent);
			entry = {};
		}
		state = {};
	}

	void ResetPlayerState(gclient_t* client) {
		if (!client)
			return;
		ClearAttachmentHeads(client);
		client->headhunter = {};
		SyncClient(client);
	}

	void InitLevel() {
		auto& state = State();
		state.headModelIndex = gi.modelIndex(kHeadModelPath);
		state.receptacleCount = 0;
		state.spikeCount = 0;
		CleanupLooseHeads(state);
		CleanupSpikeQueue(state);
		for (size_t i = 0; i < game.maxClients; ++i)
			ResetPlayerState(&game.clients[i]);
		if (!Active())
			return;
		RefreshReceptacleCount(state);
	}

	void RunFrame() {
		auto& state = State();
		CleanupLooseHeads(state);
		CleanupSpikeQueue(state);
		if (!Active())
			return;
		RefreshReceptacleCount(state);
		for (auto& receptacle : state.receptacles) {
			if (receptacle.ent && !receptacle.ent->inUse)
				receptacle = {};
		}
		RefreshReceptacleCount(state);
for (size_t i = 0; i < game.maxClients; ++i) {
gentity_t* player = &g_entities[i + 1];
if (!player->inUse || !player->client)
continue;
gclient_t* client = player->client;
if (!ClientIsPlaying(client) || client->eliminated) {
ClearAttachmentHeads(client);
client->headhunter.carried = 0;
SyncClient(client);
continue;
}
const size_t desired = DesiredAttachmentCount(client);
			EnsureAttachmentCount(player, desired);
			if (desired)
				UpdateAttachmentPositions(player);
		}
		std::array<size_t, LevelLocals::HeadHuntersState::MAX_RECEPTACLES> perReceptacle{};
		for (size_t i = 0; i < state.spikeCount; ++i) {
			auto& entry = state.spikeQueue[i];
			int index = FindReceptacleIndex(state, entry.base);
			if (index < 0)
				continue;
			const size_t slot = perReceptacle[static_cast<size_t>(index)]++;
			if (entry.ent && entry.ent->inUse) {
				const Vector3 position = SpikeSlotPosition(entry.base, slot);
				entry.ent->s.origin = position;
				entry.ent->s.oldOrigin = position;
				entry.ent->s.angles = entry.base->s.angles;
				gi.linkEntity(entry.ent);
			}
		}
	}

	gentity_t* SpawnGroundHead(const Vector3& origin, const Vector3& velocity, Team team) {
		if (!Active())
			return nullptr;
		gentity_t* head = Spawn();
		if (!head)
			return nullptr;
		head->className = "item_headhunter_head";
		head->mins = { -12.0f, -12.0f, -12.0f };
		head->maxs = { 12.0f, 12.0f, 12.0f };
		head->solid = SOLID_TRIGGER;
		head->clipMask = MASK_SOLID;
		head->moveType = MoveType::Toss;
		head->touch = HandlePickup;
		head->think = FreeEntity;
		head->nextThink = level.time + kHeadLifetime;
		head->spawnFlags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
		head->gravityVector = { 0.0f, 0.0f, -1.0f };
		head->s.effects |= EF_ROTATE;
		head->s.renderFX |= RF_IR_VISIBLE | RF_NO_LOD;
		switch (team) {
		case Team::Red:
			head->s.renderFX |= RF_SHELL_RED;
			break;
		case Team::Blue:
			head->s.renderFX |= RF_SHELL_BLUE;
			break;
		default:
			break;
		}
		head->fteam = team;
		head->s.origin = origin;
		head->velocity = velocity;
		head->aVelocity = { 0.0f, 0.0f, 90.0f };
		head->s.oldOrigin = origin;
		auto& state = State();
		if (!state.headModelIndex)
			state.headModelIndex = gi.modelIndex(kHeadModelPath);
		head->s.modelIndex = state.headModelIndex;
		gi.setModel(head, kHeadModelPath);
		gi.linkEntity(head);
		RegisterLooseHead(head);
		return head;
	}

	void DropHeads(gentity_t* player, gentity_t* instigator) {
		if (!Active())
			return;
		if (!player || !player->client)
			return;
		gclient_t* client = player->client;
		if (client->headhunter.carried == 0)
			return;
		const uint8_t carried = client->headhunter.carried;
		AnnounceDrop(player, instigator, carried);
		ClearAttachmentHeads(client);
		Vector3 baseOrigin = DropOrigin(player);
		const Team team = client->sess.team;
		for (uint8_t i = 0; i < carried; ++i) {
			Vector3 spawnOrigin = baseOrigin;
			spawnOrigin.x += crandom() * 12.0f;
			spawnOrigin.y += crandom() * 12.0f;
			Vector3 tossVelocity = player->velocity;
			tossVelocity.x += crandom() * kHeadHorizontalImpulse;
			tossVelocity.y += crandom() * kHeadHorizontalImpulse;
			tossVelocity.z += kHeadVerticalImpulse + frandom() * 50.0f;
			if (instigator && instigator->client)
				tossVelocity += instigator->client->vForward * 75.0f;
			SpawnGroundHead(spawnOrigin, tossVelocity, team);
		}
		client->headhunter.carried = 0;
		client->headhunter.dropCooldown = level.time + kDropCooldown;
		client->headhunter.pickupCooldown = level.time + kPickupCooldown;
		SyncClient(client);
	}

	void HandlePickup(gentity_t* ent, gentity_t* other, const trace_t&, bool) {
		if (!Active())
			return;
		if (!ent || !other || !other->client)
			return;
		gclient_t* client = other->client;
		if (!ClientIsPlaying(client) || client->eliminated)
			return;
		if (client->headhunter.dropCooldown > level.time)
			return;
		if (client->headhunter.pickupCooldown > level.time)
			return;
		if (client->headhunter.carried >= kMaxCarriedHeads)
			return;
		++client->headhunter.carried;
		client->headhunter.pickupCooldown = level.time + kPickupCooldown;
		SyncClient(client);
		EnsureAttachmentCount(other, DesiredAttachmentCount(client));
		UpdateAttachmentPositions(other);
		gi.sound(other, CHAN_ITEM, gi.soundIndex("items/pkup.wav"), 1.0f, ATTN_NORM, 0);
		UnregisterLooseHead(ent);
		if (ent->inUse)
			FreeEntity(ent);
	}

	void RegisterReceptacle(gentity_t* ent) {
		if (!ent)
			return;
		auto& state = State();
		const Team team = ReceptacleTeam(ent);
		for (auto& slot : state.receptacles) {
			if (slot.ent == ent) {
				slot.team = team;
				RefreshReceptacleCount(state);
				return;
			}
		}
		for (auto& slot : state.receptacles) {
			if (!slot.ent || !slot.ent->inUse) {
				slot = {};
				slot.ent = ent;
				slot.team = team;
				RefreshReceptacleCount(state);
				return;
			}
		}
		gi.Com_PrintFmt("HeadHunters: ignoring {} because the maximum number of receptacles has been reached.\n", *ent);
	}

	void ApplyReceptacleVisuals(gentity_t* ent, Team team) {
		if (!ent)
			return;
		EnsureReceptacleBounds(ent);
		ent->s.renderFX &= ~(RF_SHELL_RED | RF_SHELL_BLUE);
		switch (team) {
		case Team::Red:
			ent->s.renderFX |= RF_SHELL_RED;
			break;
		case Team::Blue:
			ent->s.renderFX |= RF_SHELL_BLUE;
			break;
		default:
			break;
		}
		gi.linkEntity(ent);
	}

	void OnReceptacleTouch(gentity_t* ent, gentity_t* other, const trace_t&, bool) {
		if (!Active())
			return;
		if (!ent || !other || !other->client)
			return;
		gclient_t* client = other->client;
		if (!ClientIsPlaying(client) || client->eliminated)
			return;
		if (client->headhunter.carried == 0)
			return;
		const uint8_t carried = client->headhunter.carried;
		const int points = static_cast<int>((carried * (carried + 1)) / 2);
		if (!ScoringIsDisabled() && level.matchState == MatchState::In_Progress)
			G_AdjustPlayerScore(client, points, false, 0);
		AnnounceDeposit(other, carried, points);
		QueueSpikeHeads(ent, carried);
		client->headhunter.carried = 0;
		client->headhunter.dropCooldown = level.time + kDropCooldown;
		client->headhunter.pickupCooldown = level.time + kPickupCooldown;
		ClearAttachmentHeads(client);
		SyncClient(client);
	}

	void SP_headhunters_receptacle(gentity_t* ent) {
		if (!ent)
			return;
		EnsureReceptacleBounds(ent);
		ent->solid = SOLID_TRIGGER;
		ent->clipMask = MASK_PLAYERSOLID;
		ent->moveType = MoveType::None;
		ent->touch = OnReceptacleTouch;
		gi.linkEntity(ent);
		RegisterReceptacle(ent);
	}

} // namespace HeadHunters
