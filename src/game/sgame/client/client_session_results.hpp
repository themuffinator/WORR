#pragma once

namespace worr::server::client {
	enum class ReadyResult {
		Success,
		NoConditions,
		AlreadySet,
	};

	enum class DisconnectResult {
		Success,
		InvalidEntity,
	};
}
