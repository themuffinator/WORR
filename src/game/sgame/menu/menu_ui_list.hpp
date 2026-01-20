#pragma once

#include "../g_local.hpp"

#include <array>
#include <string>
#include <string_view>
#include <vector>

struct MapFlagEntry {
  uint16_t bit;
  const char *code;
  const char *label;
};

const std::array<MapFlagEntry, 10> &MapFlagEntries();

std::string BuildMapFlagSummary(const UiMapFlagState &state);
std::string BuildMapFlagVoteArg(const UiMapFlagState &state, std::string_view mapName);
std::vector<std::string> BuildMapFlagArgs(const UiMapFlagState &state);
std::string MapFlagStateLabel(const UiMapFlagState &state, const MapFlagEntry &entry);
bool ToggleMapFlagByCode(UiMapFlagState &state, std::string_view code);

void UiList_Open(gentity_t *ent, UiListKind kind);
void UiList_Refresh(gentity_t *ent, bool openMenu);
void UiList_Next(gentity_t *ent);
void UiList_Prev(gentity_t *ent);
