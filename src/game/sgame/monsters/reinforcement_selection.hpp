// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

/*
=============
ReinforcementSelectionConfig

Configuration block controlling how reinforcement candidates are prioritized.
=============
*/
struct ReinforcementSelectionConfig {
	uint32_t base_weight;
	bool prefer_least_used;
};

constexpr ReinforcementSelectionConfig reinforcement_selection_defaults { 1, true };

/*
=============
M_SelectReinforcementIndex

Pick the next reinforcement index using a round-robin cursor and historical spawn counts.
=============
*/
inline uint8_t M_SelectReinforcementIndex(uint32_t *usage_counts, uint32_t count_size, uint32_t &cursor,
		const std::vector<uint8_t> &available, const ReinforcementSelectionConfig &config = reinforcement_selection_defaults) {
	if (!available.size() || !count_size)
	return 255;

	uint32_t min_count = UINT32_MAX;

	if (config.prefer_least_used) {
		for (uint8_t candidate : available) {
			uint32_t usage = (usage_counts && candidate < count_size) ? usage_counts[candidate] : 0;
			min_count = std::min(min_count, usage);
		}
	}

	for (uint32_t step = 0; step < count_size; step++) {
		uint32_t candidate = (cursor + step) % count_size;
		if (std::find(available.begin(), available.end(), candidate) == available.end())
		continue;

		uint32_t usage = (usage_counts && candidate < count_size) ? usage_counts[candidate] : 0;

		if (!config.prefer_least_used || usage == min_count) {
			uint8_t chosen = static_cast<uint8_t>(candidate);
			if (usage_counts && chosen < count_size)
			usage_counts[chosen] += config.base_weight;

			cursor = (candidate + 1) % count_size;
			return chosen;
		}
	}

	uint8_t chosen = available.front();

	if (usage_counts && chosen < count_size)
	usage_counts[chosen] += config.base_weight;

	cursor = (static_cast<uint32_t>(chosen) + 1) % count_size;

	return chosen;
}
