#pragma once

#include <optional>
#include <utility>

// Generic helper for applying match-state transitions to any context that
// exposes the Quake-style level fields. This allows both production code and
// tests to exercise the same transition logic without depending on the
// enormous game headers.
template<typename Context>
struct MatchStateTransition {
	using StateType = decltype(std::declval<Context>().matchState);
	using TimeType = decltype(std::declval<Context>().matchStateTimer);
	using WarmupType = decltype(std::declval<Context>().warmupState);

	StateType state{};
	TimeType timer{};
	std::optional<WarmupType> warmup{};
	std::optional<TimeType> warmupNotice{};
	std::optional<bool> prepareToFight{};
};

// Applies the supplied transition to the provided context, updating only the
// fields that are explicitly specified. This mirrors the behaviour in
// g_match_state.cpp but is testable in isolation.
template<typename Context>
void ApplyMatchState(Context& context, const MatchStateTransition<Context>& transition) {
	context.matchState = transition.state;
	context.matchStateTimer = transition.timer;

	if (transition.warmup)
		context.warmupState = *transition.warmup;

	if (transition.warmupNotice)
		context.warmupNoticeTime = *transition.warmupNotice;

	if (transition.prepareToFight)
		context.prepare_to_fight = *transition.prepareToFight;
}

