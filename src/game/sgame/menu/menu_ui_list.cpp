#include "menu_ui_list.hpp"

#include "menu_ui_helpers.hpp"

#include <algorithm>

namespace {

constexpr int kUiListPageSize = 12;

struct UiListEntry {
  std::string label;
  std::string command;
};

constexpr std::array<MapFlagEntry, 10> kMapFlags = {{
    { MAPFLAG_PU, "pu", "Powerups" },
    { MAPFLAG_PA, "pa", "Power Armor" },
    { MAPFLAG_AR, "ar", "Armor" },
    { MAPFLAG_AM, "am", "Ammo" },
    { MAPFLAG_HT, "ht", "Health" },
    { MAPFLAG_BFG, "bfg", "BFG10K" },
    { MAPFLAG_PB, "pb", "Plasma Beam" },
    { MAPFLAG_FD, "fd", "Falling Damage" },
    { MAPFLAG_SD, "sd", "Self Damage" },
    { MAPFLAG_WS, "ws", "Weapons Stay" },
}};

const MapFlagEntry *FindMapFlag(std::string_view code)
{
  for (const auto &entry : kMapFlags) {
    if (code == entry.code)
      return &entry;
  }
  return nullptr;
}

void ToggleMapFlagTri(UiMapFlagState &state, uint16_t mask)
{
  const bool en = (state.enableFlags & mask) != 0;
  const bool dis = (state.disableFlags & mask) != 0;

  if (!en && !dis) {
    state.enableFlags |= mask;
  } else if (en) {
    state.enableFlags &= ~mask;
    state.disableFlags |= mask;
  } else {
    state.disableFlags &= ~mask;
  }
}

std::string MapDisplayName(std::string_view mapName)
{
  const MapEntry *entry = game.mapSystem.GetMapEntry(std::string(mapName));
  if (entry && !entry->longName.empty()) {
    return G_Fmt("{} ({})", entry->longName.c_str(), mapName.data()).data();
  }
  return std::string(mapName);
}

} // namespace

const std::array<MapFlagEntry, 10> &MapFlagEntries()
{
  return kMapFlags;
}

std::string BuildMapFlagSummary(const UiMapFlagState &state)
{
  std::string out;
  for (const auto &f : kMapFlags) {
    const bool en = (state.enableFlags & f.bit) != 0;
    const bool dis = (state.disableFlags & f.bit) != 0;
    if (en) {
      out += "+";
      out += f.code;
      out += " ";
    }
    if (dis) {
      out += "-";
      out += f.code;
      out += " ";
    }
  }
  if (out.empty())
    out = "Default";
  else if (out.back() == ' ')
    out.pop_back();
  return out;
}

std::string BuildMapFlagVoteArg(const UiMapFlagState &state,
                                std::string_view mapName)
{
  std::string arg(mapName);
  for (const auto &f : kMapFlags) {
    const bool en = (state.enableFlags & f.bit) != 0;
    const bool dis = (state.disableFlags & f.bit) != 0;
    if (en) {
      arg += " +";
      arg += f.code;
    }
    if (dis) {
      arg += " -";
      arg += f.code;
    }
  }
  return arg;
}

std::vector<std::string> BuildMapFlagArgs(const UiMapFlagState &state)
{
  std::vector<std::string> args;
  for (const auto &f : kMapFlags) {
    if (state.enableFlags & f.bit)
      args.emplace_back(std::string("+") + f.code);
    if (state.disableFlags & f.bit)
      args.emplace_back(std::string("-") + f.code);
  }
  return args;
}

std::string MapFlagStateLabel(const UiMapFlagState &state,
                              const MapFlagEntry &entry)
{
  const bool en = (state.enableFlags & entry.bit) != 0;
  const bool dis = (state.disableFlags & entry.bit) != 0;
  const char *stateLabel = (!en && !dis) ? "Default" : (en ? "Enabled" : "Disabled");
  return fmt::format("{} [{}]", entry.label, stateLabel);
}

bool ToggleMapFlagByCode(UiMapFlagState &state, std::string_view code)
{
  const MapFlagEntry *entry = FindMapFlag(code);
  if (!entry)
    return false;
  ToggleMapFlagTri(state, entry->bit);
  return true;
}

void UiList_Open(gentity_t *ent, UiListKind kind)
{
  if (!ent || !ent->client)
    return;
  ent->client->ui.list.kind = kind;
  ent->client->ui.list.page = 0;
  UiList_Refresh(ent, true);
}

void UiList_Refresh(gentity_t *ent, bool openMenu)
{
  if (!ent || !ent->client)
    return;

  auto &listState = ent->client->ui.list;
  if (listState.kind == UiListKind::None)
    return;

  std::string title;
  std::string subtitle;
  std::string header;
  UiListEntry extras[2]{};
  bool extraShow[2] = { false, false };
  std::vector<UiListEntry> entries;

  switch (listState.kind) {
  case UiListKind::CallvoteMap: {
    title = "Callvote: Map";
    header.clear();
    const auto &flags = ent->client->ui.callvoteMap;
    extras[0].label = fmt::format("Flags: {}", BuildMapFlagSummary(flags));
    extras[0].command = "worr_callvote_map_flags";
    extras[1].label = "Clear Flags";
    extras[1].command = "worr_callvote_map_clear";
    extraShow[0] = true;
    extraShow[1] = true;

    if (game.mapSystem.mapPool.empty())
      LoadMapPool(ent);

    if (game.mapSystem.mapPool.empty()) {
      entries.push_back({ "No maps available", "" });
      break;
    }

    for (const auto &entry : game.mapSystem.mapPool) {
      const std::string displayName =
          entry.longName.empty() ? entry.filename : entry.longName;
      entries.push_back({ displayName,
                          fmt::format("worr_callvote_map {}", entry.filename) });
    }
    break;
  }
  case UiListKind::CallvoteGametype: {
    title = "Callvote: Gametype";
    for (const auto &mode : GAME_MODES) {
      if (mode.type == GameType::None)
        continue;
      entries.push_back({ std::string(mode.long_name),
                          fmt::format("worr_callvote_gametype {}", mode.short_name) });
    }
    break;
  }
  case UiListKind::CallvoteArena: {
    title = "Callvote: Arena";
    int optionsAdded = 0;
    for (int i = 0; i < level.arenaTotal; ++i) {
      int arenaNum = i + 1;
      if (arenaNum == level.arenaActive)
        continue;
      entries.push_back({ fmt::format("Arena {}", arenaNum),
                          fmt::format("worr_callvote_arena {}", arenaNum) });
      ++optionsAdded;
    }
    if (optionsAdded == 0)
      entries.push_back({ "No other arenas available", "" });
    break;
  }
  case UiListKind::MyMap: {
    title = "MyMap";
    const auto &flags = ent->client->ui.mymap;
    extras[0].label = fmt::format("Flags: {}", BuildMapFlagSummary(flags));
    extras[0].command = "worr_mymap_flags";
    extras[1].label = "Clear Flags";
    extras[1].command = "worr_mymap_clear";
    extraShow[0] = true;
    extraShow[1] = true;

    if (game.mapSystem.mapPool.empty())
      LoadMapPool(ent);

    if (game.mapSystem.mapPool.empty()) {
      entries.push_back({ "No maps available", "" });
      break;
    }

    for (const auto &entry : game.mapSystem.mapPool) {
      const std::string displayName =
          entry.longName.empty() ? entry.filename : entry.longName;
      entries.push_back({ displayName,
                          fmt::format("worr_mymap_queue {}", entry.filename) });
    }
    break;
  }
  case UiListKind::SetupGametype: {
    title = "Match Setup";
    subtitle = "Gametype";
    const auto &setup = ent->client->ui.setup;
    header = fmt::format("Current: {}", setup.gametype);
    if (auto gt = Game::FromString(setup.gametype))
      header = fmt::format("Current: {}", Game::GetInfo(*gt).long_name);

    for (const auto &mode : GAME_MODES) {
      if (mode.type == GameType::None)
        continue;
      entries.push_back({ std::string(mode.long_name),
                          fmt::format("worr_setup_gametype {}", mode.short_name) });
    }
    break;
  }
  case UiListKind::TournamentPick:
  case UiListKind::TournamentBan: {
    title = (listState.kind == UiListKind::TournamentPick) ? "*Pick a Map*"
                                                           : "*Ban a Map*";
    if (!Tournament_IsActive() || game.tournament.vetoComplete) {
      entries.push_back({ "Veto is not active.", "" });
      break;
    }

    std::vector<std::string> maps;
    maps.reserve(game.tournament.mapPool.size());
    for (const auto &map : game.tournament.mapPool) {
      bool selected = false;
      for (const auto &pick : game.tournament.mapPicks) {
        if (_stricmp(pick.c_str(), map.c_str()) == 0) {
          selected = true;
          break;
        }
      }
      if (!selected) {
        for (const auto &ban : game.tournament.mapBans) {
          if (_stricmp(ban.c_str(), map.c_str()) == 0) {
            selected = true;
            break;
          }
        }
      }
      if (!selected)
        maps.push_back(map);
    }

    if (maps.empty()) {
      entries.push_back({ "No maps remain to pick or ban.", "" });
      break;
    }

    for (const auto &map : maps) {
      entries.push_back({ MapDisplayName(map),
                          fmt::format("{} {}",
                                      listState.kind == UiListKind::TournamentPick
                                          ? "worr_tourney_pick"
                                          : "worr_tourney_ban",
                                      map) });
    }
    break;
  }
  case UiListKind::TournamentReplay: {
    title = "*Replay Tournament Game*";
    if (!Tournament_IsActive() || game.tournament.mapOrder.empty()) {
      subtitle = "Replay is available once";
      header = "the map order is locked.";
      break;
    }

    int gameNum = 1;
    for (const auto &map : game.tournament.mapOrder) {
      entries.push_back({ fmt::format("Replay game {}: {}", gameNum, MapDisplayName(map)),
                          fmt::format("worr_tourney_replay_confirm {}", gameNum) });
      ++gameNum;
    }
    break;
  }
  default:
    break;
  }

  const int totalEntries = static_cast<int>(entries.size());
  const int totalPages =
      totalEntries > 0 ? (totalEntries + kUiListPageSize - 1) / kUiListPageSize : 1;
  listState.page = std::clamp(listState.page, 0, std::max(0, totalPages - 1));
  const int startIndex = listState.page * kUiListPageSize;

  MenuUi::UiCommandBuilder cmd(ent);
  cmd.AppendCvar("ui_list_title", title);
  cmd.AppendCvar("ui_list_subtitle", subtitle);
  cmd.AppendCvar("ui_list_header", header);
  cmd.AppendCvar("ui_list_extra_label_0", extras[0].label);
  cmd.AppendCvar("ui_list_extra_cmd_0", extras[0].command);
  cmd.AppendCvar("ui_list_extra_show_0", extraShow[0] ? "1" : "0");
  cmd.AppendCvar("ui_list_extra_label_1", extras[1].label);
  cmd.AppendCvar("ui_list_extra_cmd_1", extras[1].command);
  cmd.AppendCvar("ui_list_extra_show_1", extraShow[1] ? "1" : "0");

  for (int i = 0; i < kUiListPageSize; ++i) {
    const int entryIndex = startIndex + i;
    const bool hasEntry = entryIndex >= 0 && entryIndex < totalEntries;
    const UiListEntry entry = hasEntry ? entries[entryIndex] : UiListEntry{};

    cmd.AppendCvar(fmt::format("ui_list_item_label_{}", i).c_str(), entry.label);
    cmd.AppendCvar(fmt::format("ui_list_item_cmd_{}", i).c_str(), entry.command);
    cmd.AppendCvar(fmt::format("ui_list_item_show_{}", i).c_str(), hasEntry ? "1" : "0");
  }

  if (totalPages > 1)
    cmd.AppendCvar("ui_list_page_label",
                   fmt::format("Page {}/{}", listState.page + 1, totalPages));
  else
    cmd.AppendCvar("ui_list_page_label", "");
  cmd.AppendCvar("ui_list_has_prev", listState.page > 0 ? "1" : "0");
  cmd.AppendCvar("ui_list_has_next",
                 listState.page + 1 < totalPages ? "1" : "0");

  if (openMenu)
    cmd.AppendCommand("pushmenu ui_list");
  cmd.Flush();
}

void UiList_Next(gentity_t *ent)
{
  if (!ent || !ent->client)
    return;
  ent->client->ui.list.page++;
  UiList_Refresh(ent, false);
}

void UiList_Prev(gentity_t *ent)
{
  if (!ent || !ent->client)
    return;
  ent->client->ui.list.page--;
  UiList_Refresh(ent, false);
}
