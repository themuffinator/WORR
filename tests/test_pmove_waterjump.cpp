#include "common/game3_pmove/waterjump.hpp"
#include "shared/shared.hpp"

#include <cmath>
#include <cstdio>

/*
=============
ApproximatelyEqual

Compares floats within epsilon.
=============
*/
static bool ApproximatelyEqual(float expected, float actual, float epsilon)
{
	return std::fabs(expected - actual) <= epsilon;
}

/*
=============
ExpectImpulse

Asserts the impulse matches expected value.
=============
*/
static bool ExpectImpulse(int contents, float frametime, float expected, const char *label)
{
	float	impulse = PM_CalcWaterJumpImpulse(frametime, contents);

	if (!ApproximatelyEqual(expected, impulse, 0.0001f)) {
		std::printf("%s: expected %.4f got %.4f\n", label, expected, impulse);
		return false;
	}

	return true;
}

/*
=============
main
=============
*/
int main()
{
	bool ok = true;

	ok &= ExpectImpulse(CONTENTS_WATER, 0.016f, 1.6f, "water 16ms");
	ok &= ExpectImpulse(CONTENTS_WATER, 0.1f, 10.0f, "water 100ms");
	ok &= ExpectImpulse(CONTENTS_SLIME, 0.05f, 4.0f, "slime 50ms");
	ok &= ExpectImpulse(CONTENTS_LAVA, 0.033f, 1.65f, "lava 33ms");
	ok &= ExpectImpulse(CONTENTS_WATER, 0.0f, 0.0f, "no frametime");

	return ok ? 0 : 1;
}
