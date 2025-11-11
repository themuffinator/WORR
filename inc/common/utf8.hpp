#pragma once

#include <cstddef>
#include <cstdint>

/*
=============
utf8_next

Advance the pointer to the next UTF-8 codepoint while updating the remaining byte budget.
Returns the decoded codepoint, '?' for invalid sequences, or 0 on exhaustion.
=============
*/
inline uint32_t utf8_next(const char *&p, size_t &remaining) noexcept
{
	if (!remaining || !p || !*p)
		return 0;

	const unsigned char lead = static_cast<unsigned char>(*p);

	if (lead < 0x80) {
		++p;
		--remaining;
		return lead;
	}

	size_t expected = 0;
	uint32_t codepoint = 0;
	uint32_t min_value = 0;

	if ((lead & 0xE0) == 0xC0) {
		expected = 1;
		codepoint = lead & 0x1F;
		min_value = 0x80;
	} else if ((lead & 0xF0) == 0xE0) {
		expected = 2;
		codepoint = lead & 0x0F;
		min_value = 0x800;
	} else if ((lead & 0xF8) == 0xF0 && lead <= 0xF4) {
		expected = 3;
		codepoint = lead & 0x07;
		min_value = 0x10000;
	} else {
		++p;
		--remaining;
		return '?';
	}

	if (remaining < expected + 1) {
		++p;
		--remaining;
		return '?';
	}

	const char *cursor = p + 1;

	for (size_t i = 0; i < expected; ++i) {
		const unsigned char c = static_cast<unsigned char>(cursor[i]);
		if ((c & 0xC0) != 0x80) {
			++p;
			--remaining;
			return '?';
		}
		codepoint = (codepoint << 6) | (c & 0x3F);
	}

	if (codepoint < min_value || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
		codepoint = '?';
		++p;
		--remaining;
		return codepoint;
	}

	const size_t consumed = expected + 1;
	p += consumed;
	remaining -= consumed;

	return codepoint;
}
