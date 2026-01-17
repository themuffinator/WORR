#pragma once

// Utility scope guard used by CheckDMExitRules to track whether any
// grace-based endmatch condition fired during the current frame. When the
// scope ends without a condition being marked active, it automatically resets
// the grace timer so future violations receive a full grace window.
template<typename TimeT>
class EndmatchGraceScope {
public:
	/*
	=============
	EndmatchGraceScope

	Initializes the scope guard with the timer reference and the value used to
	reset it when no conditions fire.
	=============
	*/
	EndmatchGraceScope(TimeT& timer, TimeT zeroValue)
		: timer(timer)
		, zeroValue(zeroValue) {
	}

	/*
	=============
	MarkConditionActive

	Marks that a grace-based condition triggered during this scope lifetime.
	=============
	*/
	void MarkConditionActive() {
		active = true;
	}

	/*
	=============
	ConditionWasActive

	Indicates whether any condition was marked active during the scope.
	=============
	*/
	[[nodiscard]] bool ConditionWasActive() const {
		return active;
	}

	/*
	=============
	~EndmatchGraceScope

	Resets the timer to its zero value if no condition was marked active.
	=============
	*/
	~EndmatchGraceScope() {
		if (!active && timer)
			timer = zeroValue;
	}

private:
	TimeT& timer;
	TimeT zeroValue;
	bool active = false;
};
