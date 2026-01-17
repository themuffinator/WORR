#pragma once

#include "client_session_results.hpp"
#include "client_session_service.hpp"

class ClientConfigStore;

namespace worr::server::client {

class ClientStatsService;

class ClientSessionServiceImpl : public ClientSessionService {
	public:
	ClientSessionServiceImpl(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
	ClientConfigStore& configStore, ClientStatsService& statsService);

	bool ClientConnect(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
	gentity_t* ent, char* userInfo, const char* socialID, bool isBot) override;
	void ClientBegin(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
	gentity_t* ent) override;
	void ClientUserinfoChanged(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
	gentity_t* ent, const char* userInfo) override;
	DisconnectResult ClientDisconnect(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
	gentity_t* ent) override;
	void ClientThink(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
	gentity_t* ent, usercmd_t* cmd) override;
	void ClientBeginServerFrame(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
	gentity_t* ent) override;
	ReadyResult OnReadyToggled(gentity_t* ent, bool state, bool toggle);
	void ApplySpawnFlags(gentity_t* ent) const;
	void PrepareSpawnPoint(gentity_t* ent, bool allowElevatorDrop = false,
	void (*dropThink)(gentity_t*) = nullptr) const;

	using ClientBeginServerFrameFreezeHook = bool (*)(gentity_t*);
	void SetClientBeginServerFrameFreezeHookForTests(ClientBeginServerFrameFreezeHook hook);

	private:
		local_game_import_t& gi_;
		[[maybe_unused]] GameLocals& game_;
		LevelLocals& level_;
		ClientConfigStore& configStore_;
		ClientStatsService& statsService_;
		void OnDisconnect(local_game_import_t& gi, gentity_t* ent);
};


void InitializeClientSessionService(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
		ClientConfigStore& configStore, ClientStatsService& statsService);
void InitializeClientSessionService(local_game_import_t& gi, GameLocals& game, LevelLocals& level);
ClientSessionServiceImpl& GetClientSessionService();

} // namespace worr::server::client
