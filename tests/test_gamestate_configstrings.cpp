/*
=============
test_gamestate_configstrings

Validate configstring span accounting for long entries spanning multiple slots.
=============
*/
#include "common/utils.hpp"
#include "shared/shared.hpp"

#include <array>
#include <cstring>

int main()
{
	cs_remap_t csr{};
	csr.airaccel = CS_AIRACCEL_OLD;
	csr.general = CS_GENERAL_OLD;
	csr.end = MAX_CONFIGSTRINGS_OLD;

	std::array<configstring_t, MAX_CONFIGSTRINGS_OLD> configstrings{};

	size_t long_len = CS_MAX_STRING_LENGTH * 2 + 12;
	char *target = configstrings[CS_STATUSBAR];
	std::memset(target, 'A', long_len);
	target[long_len] = '\0';

	size_t measured = Com_ConfigstringLength(&csr, CS_STATUSBAR, target);
	if (measured != long_len) {
		return 1;
	}

	size_t span = Com_ConfigstringSpan(measured);
	if (span < 3) {
		return 2;
	}

	int sent = 0;
	for (int i = 0; i < static_cast<int>(csr.end); i++) {
		char *string = configstrings[i];
		if (!string[0]) {
			continue;
		}

		size_t len = Com_ConfigstringLength(&csr, i, string);
		sent++;

		size_t consumed = Com_ConfigstringSpan(len);
		if (consumed) {
			i += static_cast<int>(consumed) - 1;
		}
	}

	return sent == 1 ? 0 : 3;
}
