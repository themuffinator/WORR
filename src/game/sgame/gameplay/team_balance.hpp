#pragma once

#include "../g_local.hpp"
#include <array>

/*
=============
CollectStackedTeamClients

Collect the client indices for the specified team until the buffer is full.
=============
*/
	inline size_t CollectStackedTeamClients(Team stack_team, std::array<int, MAX_CLIENTS_KEX>& indexBuffer) {
		if (!game.clients)
			return 0;

		size_t count = 0;
		for (auto ec : active_clients()) {
			if (ec->client->sess.team != stack_team)
				continue;
			if (count >= indexBuffer.size())
				break;
			indexBuffer[count] = ec->client - game.clients;
			count++;
		}

		return count;
	}
