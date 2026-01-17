#pragma once

#include <utility>

namespace MatchWarmup {
	template<typename MatchStateT, typename TimeT, typename WarmupStateT>
	inline bool PromoteInitialDelayToWarmup(MatchStateT& matchState,
		TimeT& matchStateTimer,
		const TimeT& currentTime,
		WarmupStateT& warmupState,
		TimeT& warmupNoticeTime,
		const MatchStateT& initialDelay,
		const MatchStateT& warmupDefault,
		const WarmupStateT& defaultWarmupState,
		const TimeT& zeroTime) {
		if (matchState != initialDelay)
			return false;

		if (matchStateTimer > currentTime)
			return false;

		matchState = warmupDefault;
		matchStateTimer = zeroTime;
		warmupState = defaultWarmupState;
		warmupNoticeTime = currentTime;
		return true;
	}
} // namespace MatchWarmup

