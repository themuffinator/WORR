#pragma once

#include <string>

#include "client_session_results.hpp"

struct local_game_import_t;
struct gentity_t;
struct gclient_t;
struct usercmd_t;
struct LevelLocals;
struct GameLocals;

namespace worr::server::client {

/*
=================
ClientSessionService

Abstract interface describing the operations that manage a client's session
lifecycle. The eventual implementation will move the responsibilities that
live in src/server/player/p_client.cpp behind this seam.
=================
*/
class ClientSessionService {
	public:
	virtual ~ClientSessionService() = default;

virtual bool ClientConnect(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
gentity_t* ent, char* userInfo, const char* socialID, bool isBot) = 0;
virtual void ClientBegin(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
gentity_t* ent) = 0;
virtual void ClientUserinfoChanged(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
gentity_t* ent, const char* userInfo) = 0;
virtual DisconnectResult ClientDisconnect(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
gentity_t* ent) = 0;
virtual void ClientThink(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
gentity_t* ent, usercmd_t* cmd) = 0;
virtual void ClientBeginServerFrame(local_game_import_t& gi, GameLocals& game, LevelLocals& level,
gentity_t* ent) = 0;
};

} // namespace worr::server::client
