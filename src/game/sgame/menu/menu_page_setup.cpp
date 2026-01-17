/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

menu_page_setup.cpp (Menu Page - Match Setup) This file implements a multi-page
wizard-style menu for setting up a custom match. It guides the user through
selecting a gametype, modifiers, player count, and other options. Key
Responsibilities: - State Management: Uses a `MatchSetupState` struct to hold
the user's selections as they navigate through the different setup pages. -
Wizard Flow: Each menu page (e.g., `OpenSetupGametypeMenu`,
`OpenSetupModifierMenu`) handles one aspect of the setup and then calls the next
function in the sequence, creating a step-by-step setup process. - Finalization:
The final step in the wizard calls `FinishMatchSetup`, which applies the chosen
settings to the server and closes the menu.*/

#include "../g_local.hpp"

extern void OpenJoinMenu(gentity_t *ent);

/*
===============
MatchSetupState
===============
*/
struct MatchSetupState {
  std::string format = "regular";
  std::string gametype = "ffa";
  std::string modifier = "standard";
  int maxPlayers = 8;
  std::string length = "standard";
  std::string type = "standard";
  std::string bestOf = "bo1";
};

static void OpenSetupGametypeMenu(gentity_t *ent, MatchSetupState *state);

namespace {
bool IsTeamBasedGametype(std::string_view gt);
bool IsOneVOneGametype(std::string_view gt);

constexpr std::array<std::string_view, 4> kFormatKeys = {
    {"regular", "practice", "marathon", "tournament"}};

constexpr std::array<std::string_view, 5> kModifierKeys = {
    {"standard", "instagib", "vampiric", "frenzy", "gravity_lotto"}};

constexpr std::array<std::string_view, 4> kLengthKeys = {
    {"short", "standard", "long", "endurance"}};

constexpr std::array<std::string_view, 4> kTypeKeys = {
    {"casual", "standard", "competitive", "tournament"}};

constexpr std::array<std::string_view, 5> kBestOfKeys = {
    {"bo1", "bo3", "bo5", "bo7", "bo9"}};

template <size_t N>
bool IsSelectionAllowed(std::string_view value,
                        const std::array<std::string_view, N> &allowed) {
  for (const auto &candidate : allowed) {
    if (candidate == value)
      return true;
  }
  return false;
}

template <size_t N>
std::string NormalizeSelection(std::string_view value,
                               std::string_view fallback,
                               const std::array<std::string_view, N> &allowed) {
  if (IsSelectionAllowed(value, allowed))
    return std::string(value);
  return std::string(fallback);
}

constexpr std::array<int, 4> kMatchLengthSmallMinutes = {5, 10, 15, 30};
constexpr std::array<int, 4> kMatchLengthLargeMinutes = {10, 20, 30, 40};

constexpr std::array<int, 4> kMatchTypeScoreFree = {30, 40, 40, 50};
constexpr std::array<int, 4> kMatchTypeMercyFree = {20, 30, 0, 0};
constexpr std::array<int, 4> kMatchTypeScoreTeamFrag = {50, 100, 0, 0};
constexpr std::array<int, 4> kMatchTypeMercyTeamFrag = {30, 50, 50, 0};
constexpr std::array<int, 4> kMatchTypeScoreTeamCapture = {5, 8, 8, 8};
constexpr std::array<int, 4> kMatchTypeRoundTeam = {5, 8, 8, 8};
constexpr std::array<int, 4> kMatchTypeMercyOneVOne = {10, 20, 20, 0};
constexpr std::array<int, 4> kMatchTypeWeaponTeam = {15, 25, 25, 25};
constexpr std::array<int, 4> kMatchTypeWeaponFree = {5, 8, 8, 8};

int MatchLengthIndex(std::string_view length) {
  if (length == "short")
    return 0;
  if (length == "long")
    return 2;
  if (length == "endurance")
    return 3;
  return 1;
}

int MatchTypeIndex(std::string_view type) {
  if (type == "casual")
    return 0;
  if (type == "competitive")
    return 2;
  if (type == "tournament")
    return 3;
  return 1;
}

bool UsesRoundLimit(std::string_view gt) {
  if (auto type = Game::FromString(gt))
    return HasFlag(Game::GetInfo(*type).flags, GameFlags::Rounds);
  return false;
}

bool UsesCaptureLimit(std::string_view gt) {
  if (auto type = Game::FromString(gt))
    return *type == GameType::CaptureTheFlag || *type == GameType::ProBall;
  return false;
}

int MatchLengthMinutes(std::string_view length, std::string_view gametype,
                       int maxPlayers) {
  const bool oneVOne = IsOneVOneGametype(gametype);
  const bool teamBased = IsTeamBasedGametype(gametype);
  const bool smallTeams = teamBased && maxPlayers > 0 && maxPlayers <= 4;
  const bool useSmallTable = oneVOne || !teamBased || smallTeams;
  const size_t index = static_cast<size_t>(MatchLengthIndex(length));
  return useSmallTable ? kMatchLengthSmallMinutes[index]
                       : kMatchLengthLargeMinutes[index];
}

std::string GetGametypeLabel(std::string_view key) {
  if (auto gt = Game::FromString(key))
    return std::string(Game::GetInfo(*gt).long_name);
  return std::string(key);
}

std::string GetFormatLabel(std::string_view key) {
  if (key == "practice")
    return "Practice";
  if (key == "marathon")
    return "Marathon";
  if (key == "tournament")
    return "Tournament";
  return "Regular";
}

std::string GetModifierLabel(std::string_view key) {
  if (key == "instagib")
    return "InstaGib";
  if (key == "vampiric")
    return "Vampiric Damage";
  if (key == "frenzy")
    return "Frenzy";
  if (key == "gravity_lotto")
    return "Gravity Lotto";
  return "Standard";
}

std::string GetLengthLabel(std::string_view key) {
  if (key == "short")
    return "Short";
  if (key == "long")
    return "Long";
  if (key == "endurance")
    return "Endurance";
  return "Standard";
}

std::string GetTypeLabel(std::string_view key) {
  if (key == "casual")
    return "Casual";
  if (key == "competitive")
    return "Competitive";
  if (key == "tournament")
    return "Tournament";
  return "Standard";
}

std::string GetBestOfLabel(std::string_view key) {
  if (key == "bo3")
    return "BO3";
  if (key == "bo5")
    return "BO5";
  if (key == "bo7")
    return "BO7";
  if (key == "bo9")
    return "BO9";
  return "BO1";
}

std::string GetCurrentModifierKey() {
  if (g_gravity_lotto && g_gravity_lotto->integer)
    return "gravity_lotto";
  if (g_instaGib && g_instaGib->integer)
    return "instagib";
  if (g_vampiric_damage && g_vampiric_damage->integer)
    return "vampiric";
  if (g_frenzy && g_frenzy->integer)
    return "frenzy";
  return "standard";
}

void AddSetupHeading(MenuBuilder &builder, std::string_view title,
                     const std::string &current) {
  builder.add("Match Setup", MenuAlign::Center);
  builder.add(std::string(title), MenuAlign::Center);
  if (!current.empty())
    builder.add(G_Fmt("Current: {}", current).data(), MenuAlign::Center);
  builder.spacer();
}

void InitializeMatchSetupState(MatchSetupState &state) {
  if (Game::GetCurrentType() == GameType::None) {
    state.gametype = "ffa";
  } else {
    state.gametype = std::string(Game::GetCurrentInfo().short_name);
  }
  state.modifier =
      NormalizeSelection(GetCurrentModifierKey(), "standard", kModifierKeys);

  const bool marathonEnabled =
      (marathon && marathon->integer) ||
      (g_marathon_timelimit && g_marathon_timelimit->value > 0.0f) ||
      (g_marathon_scorelimit && g_marathon_scorelimit->integer > 0);

  if (g_practice && g_practice->integer) {
    state.format = "practice";
  } else if (marathonEnabled) {
    state.format = "marathon";
  } else {
    state.format = "regular";
  }
  state.format = NormalizeSelection(state.format, "regular", kFormatKeys);

  if (maxplayers && maxplayers->integer > 0)
    state.maxPlayers = maxplayers->integer;

  if (match_setup_length)
    state.length =
        NormalizeSelection(match_setup_length->string, "standard", kLengthKeys);
  if (match_setup_type)
    state.type =
        NormalizeSelection(match_setup_type->string, "standard", kTypeKeys);
  if (match_setup_bestof)
    state.bestOf =
        NormalizeSelection(match_setup_bestof->string, "bo1", kBestOfKeys);
}

void ApplyMatchLength(std::string_view length, std::string_view gametype,
                      int maxPlayers) {
  const int minutes = MatchLengthMinutes(length, gametype, maxPlayers);
  gi.cvarSet("timelimit", G_Fmt("{}", minutes).data());
}

void ApplyMatchType(std::string_view type, std::string_view gametype) {
  const bool readyUp = (type == "competitive" || type == "tournament");
  const bool lock = (type == "tournament");
  gi.cvarSet("warmup_do_ready_up", readyUp ? "1" : "0");
  gi.cvarSet("match_lock", lock ? "1" : "0");

  const size_t typeIndex = static_cast<size_t>(MatchTypeIndex(type));
  const bool oneVOne = IsOneVOneGametype(gametype);
  const bool teamBased = IsTeamBasedGametype(gametype);
  const bool free = !oneVOne && !teamBased;

  const int weaponRespawn =
      (oneVOne || teamBased) ? kMatchTypeWeaponTeam[typeIndex]
                             : kMatchTypeWeaponFree[typeIndex];
  gi.cvarSet("g_weapon_respawn_time", G_Fmt("{}", weaponRespawn).data());

  const bool usesRounds = UsesRoundLimit(gametype);
  const bool usesCapture = UsesCaptureLimit(gametype);

  if (oneVOne) {
    if (usesRounds)
      gi.cvarSet("roundlimit", "0");
    else
      gi.cvarSet("fraglimit", "0");
    gi.cvarSet("mercylimit",
               G_Fmt("{}", kMatchTypeMercyOneVOne[typeIndex]).data());
    return;
  }

  if (free) {
    const int scoreLimit = kMatchTypeScoreFree[typeIndex];
    if (usesRounds)
      gi.cvarSet("roundlimit", G_Fmt("{}", scoreLimit).data());
    else
      gi.cvarSet("fraglimit", G_Fmt("{}", scoreLimit).data());
    gi.cvarSet("mercylimit",
               G_Fmt("{}", kMatchTypeMercyFree[typeIndex]).data());
    return;
  }

  if (teamBased && usesRounds) {
    gi.cvarSet("roundlimit",
               G_Fmt("{}", kMatchTypeRoundTeam[typeIndex]).data());
    gi.cvarSet("mercylimit", "0");
    return;
  }

  if (teamBased && usesCapture) {
    gi.cvarSet("capturelimit",
               G_Fmt("{}", kMatchTypeScoreTeamCapture[typeIndex]).data());
    gi.cvarSet("mercylimit", "0");
    return;
  }

  if (teamBased) {
    gi.cvarSet("fraglimit",
               G_Fmt("{}", kMatchTypeScoreTeamFrag[typeIndex]).data());
    gi.cvarSet("mercylimit",
               G_Fmt("{}", kMatchTypeMercyTeamFrag[typeIndex]).data());
  }
}

void ApplyMatchFormat(std::string_view format) {
  const bool practice = (format == "practice");
  const bool marathonEnabled = (format == "marathon");

  if (g_practice)
    gi.cvarSet("g_practice", practice ? "1" : "0");
  if (marathon)
    gi.cvarSet("marathon", marathonEnabled ? "1" : "0");
}

bool ApplyModifiers(std::string_view modifier) {
  const bool wantInsta = (modifier == "instagib");
  const bool wantVampiric = (modifier == "vampiric");
  const bool wantFrenzy = (modifier == "frenzy");
  const bool wantGravity = (modifier == "gravity_lotto");

  const int prevInsta = g_instaGib ? g_instaGib->integer : 0;
  const int prevFrenzy = g_frenzy ? g_frenzy->integer : 0;
  const int prevQuad = g_quadhog ? g_quadhog->integer : 0;
  const int prevNade = g_nadeFest ? g_nadeFest->integer : 0;
  const int prevGravity = g_gravity_lotto ? g_gravity_lotto->integer : 0;

  const int nextInsta = wantInsta ? 1 : 0;
  const int nextFrenzy = wantFrenzy ? 1 : 0;
  const int nextGravity = wantGravity ? 1 : 0;

  bool latchedChanged = (prevInsta != nextInsta) || (prevFrenzy != nextFrenzy);
  latchedChanged |= (prevQuad != 0) || (prevNade != 0);

  gi.cvarSet("g_instaGib", nextInsta ? "1" : "0");
  gi.cvarSet("g_vampiric_damage", wantVampiric ? "1" : "0");
  gi.cvarSet("g_frenzy", nextFrenzy ? "1" : "0");
  gi.cvarSet("g_quadhog", "0");
  gi.cvarSet("g_nadeFest", "0");
  gi.cvarSet("g_gravity_lotto", nextGravity ? "1" : "0");

  if (nextGravity && prevGravity != nextGravity)
    ApplyGravityLotto();

  return latchedChanged;
}

// Available player count options
constexpr std::array<int, 9> kPlayerCountOptions = {2,  4,  6,  8,  12,
                                                    16, 24, 32, 64};

// Get max players cap from maxclients cvar
int GetMaxClientsLimit() {
  cvar_t *maxclients = gi.cvar("maxclients", "8", CVAR_NOFLAGS);
  return maxclients ? maxclients->integer : 64;
}

// Check if gametype is team-based
bool IsTeamBasedGametype(std::string_view gt) {
  const auto type = Game::FromString(gt);
  if (!type)
    return false;
  return HasFlag(Game::GetInfo(*type).flags, GameFlags::Teams);
}

// Check if gametype is 1v1 (duel or gauntlet)
bool IsOneVOneGametype(std::string_view gt) {
  const auto type = Game::FromString(gt);
  if (!type)
    return false;
  return HasFlag(Game::GetInfo(*type).flags, GameFlags::OneVOne);
}

// Get default player count for gametype
int GetDefaultPlayerCount(std::string_view gt) {
  if (IsOneVOneGametype(gt))
    return 2;
  if (IsTeamBasedGametype(gt))
    return 8;
  return 12; // FFA and other non-team modes
}
} // namespace

static void FinishMatchSetup(gentity_t *ent, MatchSetupState *state) {
  state->format = NormalizeSelection(state->format, "regular", kFormatKeys);
  state->modifier =
      NormalizeSelection(state->modifier, "standard", kModifierKeys);
  state->length = NormalizeSelection(state->length, "standard", kLengthKeys);
  state->type = NormalizeSelection(state->type, "standard", kTypeKeys);
  state->bestOf = NormalizeSelection(state->bestOf, "bo1", kBestOfKeys);

  bool gametypeChanged = false;
  if (auto gt = Game::FromString(state->gametype)) {
    if (*gt != Game::GetCurrentType()) {
      ChangeGametype(*gt);
      gametypeChanged = true;
    }
  }

  if (IsOneVOneGametype(state->gametype))
    state->maxPlayers = 2;

  ApplyMatchFormat(state->format);
  const bool latchedChanged = ApplyModifiers(state->modifier);
  ApplyMatchLength(state->length, state->gametype, state->maxPlayers);
  ApplyMatchType(state->type, state->gametype);

  gi.cvarSet("maxplayers", G_Fmt("{}", state->maxPlayers).data());

  if (match_setup_length)
    gi.cvarSet("match_setup_length", state->length.c_str());
  if (match_setup_type)
    gi.cvarSet("match_setup_type", state->type.c_str());
  if (match_setup_bestof)
    gi.cvarSet("match_setup_bestof", state->bestOf.c_str());

  if (latchedChanged && !gametypeChanged && level.mapName[0]) {
    gi.AddCommandString(G_Fmt("gamemap {}\n", level.mapName.data()).data());
  }

  gi.Com_PrintFmt("Match setup complete: format={} gametype={} modifier={} "
                  "players={} length={} type={} bestof={}\n",
                  state->format.c_str(), state->gametype.c_str(),
                  state->modifier.c_str(), state->maxPlayers,
                  state->length.c_str(), state->type.c_str(),
                  state->bestOf.c_str());

  gi.TagFree(state);
  MenuSystem::Close(ent);

  if (ent && ent->client && ent->client->initialMenu.frozen)
    OpenJoinMenu(ent);
}

/*
===============
OpenSetupBestOfMenu
===============
*/
static void OpenSetupBestOfMenu(gentity_t *ent, MatchSetupState *state) {
  MenuBuilder b;
  AddSetupHeading(b, "Best Of", GetBestOfLabel(state->bestOf));
  // BO3 is a common default for tournaments
  b.add("BO1", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->bestOf = "bo1";
    FinishMatchSetup(e, state);
  });
  b.addDefault("BO3", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->bestOf = "bo3";
    FinishMatchSetup(e, state);
  });
  b.add("BO5", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->bestOf = "bo5";
    FinishMatchSetup(e, state);
  });
  b.add("BO7", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->bestOf = "bo7";
    FinishMatchSetup(e, state);
  });
  b.add("BO9", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->bestOf = "bo9";
    FinishMatchSetup(e, state);
  });
  MenuSystem::Open(ent, b.build());
}

/*
===============
OpenSetupMatchTypeMenu
===============
*/
static void OpenSetupMatchTypeMenu(gentity_t *ent, MatchSetupState *state) {
  MenuBuilder b;
  AddSetupHeading(b, "Match Type", GetTypeLabel(state->type));
  // Order: casual, standard (default), competitive, tournament
  b.add("Casual", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->type = "casual";
    FinishMatchSetup(e, state);
  });
  b.addDefault("Standard", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->type = "standard";
    FinishMatchSetup(e, state);
  });
  b.add("Competitive", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->type = "competitive";
    FinishMatchSetup(e, state);
  });
  b.add("Tournament", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->type = "tournament";
    // Best Of menu for all gametypes in tournament mode
    OpenSetupBestOfMenu(e, state);
  });
  MenuSystem::Open(ent, b.build());
}

/*
===============
OpenSetupMatchLengthMenu
===============
*/
static void OpenSetupMatchLengthMenu(gentity_t *ent, MatchSetupState *state) {
  MenuBuilder b;
  AddSetupHeading(b, "Match Length", GetLengthLabel(state->length));

  // Items in ascending order: Short, Standard (default), Long, Endurance
  b.add("Short", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->length = "short";
    OpenSetupMatchTypeMenu(e, state);
  });
  b.addDefault("Standard", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->length = "standard";
    OpenSetupMatchTypeMenu(e, state);
  });
  b.add("Long", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->length = "long";
    OpenSetupMatchTypeMenu(e, state);
  });
  b.add("Endurance", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->length = "endurance";
    OpenSetupMatchTypeMenu(e, state);
  });
  MenuSystem::Open(ent, b.build());
}

/*
===============
OpenSetupMaxPlayersMenu
===============
*/
static void OpenSetupMaxPlayersMenu(gentity_t *ent, MatchSetupState *state) {
  const int maxClientsLimit = GetMaxClientsLimit();
  const int defaultPlayers = GetDefaultPlayerCount(state->gametype);
  const int currentPlayers = state->maxPlayers;

  MenuBuilder b;
  AddSetupHeading(b, "Max Players", G_Fmt("{}", currentPlayers).data());

  int defaultIndex = -1;
  int itemIndex = 0;

  for (int count : kPlayerCountOptions) {
    // Skip options that exceed maxclients
    if (count > maxClientsLimit)
      continue;

    const bool isDefault = (count == defaultPlayers);
    const bool isCurrent = (count == currentPlayers);

    if (isDefault || isCurrent) {
      // Use addDefault to mark with * and set as selected
      b.addDefault(G_Fmt("{}", count).data(), MenuAlign::Left,
                   [=](gentity_t *e, Menu &m) {
                     state->maxPlayers = count;
                     OpenSetupGametypeMenu(e, state);
                   });
      if (defaultIndex < 0)
        defaultIndex = itemIndex;
    } else {
      b.add(G_Fmt("{}", count).data(), MenuAlign::Left,
            [=](gentity_t *e, Menu &m) {
              state->maxPlayers = count;
              OpenSetupGametypeMenu(e, state);
            });
    }
    itemIndex++;
  }

  // If we found a default, use selectDefault to ensure it's initially selected
  if (defaultIndex >= 0)
    b.selectDefault(defaultIndex);

  MenuSystem::Open(ent, b.build());
}

/*
===============
OpenSetupModifierMenu
===============
*/
static void OpenSetupModifierMenu(gentity_t *ent, MatchSetupState *state) {
  MenuBuilder b;
  AddSetupHeading(b, "Modifiers", GetModifierLabel(state->modifier));
  // Standard is the default - mark it with * and select it
  b.addDefault("Standard", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->modifier = "standard";
    OpenSetupMatchLengthMenu(e, state);
  });
  b.add("InstaGib", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->modifier = "instagib";
    OpenSetupMatchLengthMenu(e, state);
  });
  b.add("Vampiric Damage", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->modifier = "vampiric";
    OpenSetupMatchLengthMenu(e, state);
  });
  b.add("Frenzy", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->modifier = "frenzy";
    OpenSetupMatchLengthMenu(e, state);
  });
  b.add("Gravity Lotto", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->modifier = "gravity_lotto";
    OpenSetupMatchLengthMenu(e, state);
  });
  MenuSystem::Open(ent, b.build());
}

/*
===============
OpenSetupGametypeMenu
===============
*/
static void OpenSetupGametypeMenu(gentity_t *ent, MatchSetupState *state) {
  MenuBuilder b;
  AddSetupHeading(b, "Gametype", GetGametypeLabel(state->gametype));

  // List all gametypes in g_gametype order with FFA as default
  for (const auto &mode : GAME_MODES) {
    if (mode.type == GameType::None)
      continue;
    const std::string label(mode.long_name);
    const std::string value(mode.short_name);
    const bool isFFA = (mode.type == GameType::FreeForAll);

    if (isFFA) {
      // FFA is the default selection
      b.addDefault(label, MenuAlign::Left, [=](gentity_t *e, Menu &m) {
        state->gametype = value;
        if (IsOneVOneGametype(state->gametype))
          state->maxPlayers = 2;
        OpenSetupModifierMenu(e, state);
      });
    } else {
      b.add(label, MenuAlign::Left, [=](gentity_t *e, Menu &m) {
        state->gametype = value;
        if (IsOneVOneGametype(state->gametype))
          state->maxPlayers = 2;
        OpenSetupModifierMenu(e, state);
      });
    }
  }
  MenuSystem::Open(ent, b.build());
}

/*
===============
OpenSetupMatchFormatMenu
===============
*/
static void OpenSetupMatchFormatMenu(gentity_t *ent, MatchSetupState *state) {
  MenuBuilder b;
  AddSetupHeading(b, "Match Format", GetFormatLabel(state->format));
  const bool tourneyAvailable = Tournament_ConfigIsValid();
  b.addDefault("Regular", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->format = "regular";
    OpenSetupMaxPlayersMenu(e, state);
  });
  b.add("Practice", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->format = "practice";
    OpenSetupMaxPlayersMenu(e, state);
  });
  b.add("Marathon", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
    state->format = "marathon";
    OpenSetupMaxPlayersMenu(e, state);
  });
  if (tourneyAvailable) {
    b.add("Tournament", MenuAlign::Left, [=](gentity_t *e, Menu &m) {
      std::string error;
      if (!Tournament_LoadConfig({}, &error)) {
        if (!error.empty()) {
          gi.LocClient_Print(e, PRINT_HIGH, "Tournament load failed: {}\n",
                             error.c_str());
        }
        return;
      }
      gi.TagFree(state);
      MenuSystem::Close(e);
      if (e && e->client && e->client->initialMenu.frozen)
        OpenJoinMenu(e);
    });
  }
  MenuSystem::Open(ent, b.build());
}

/*
===============
OpenSetupWelcomeMenu
===============
*/
void OpenSetupWelcomeMenu(gentity_t *ent) {
  auto *state = static_cast<MatchSetupState *>(
      gi.TagMalloc(sizeof(MatchSetupState), TAG_LEVEL));
  new (state) MatchSetupState();
  InitializeMatchSetupState(*state);

  MenuBuilder b;
  // Line 1: Welcome to
  b.add("Welcome to", MenuAlign::Center);
  // Line 2: Product name and version
  b.add(G_Fmt("{} v{}", worr::version::kGameTitle, worr::version::kGameVersion)
            .data(),
        MenuAlign::Center);
  // Line 3: blank
  b.spacer();
  // Line 4: Match Setup (default selection)
  b.addDefault("Match Setup", MenuAlign::Left,
               [=](gentity_t *e, Menu &m) {
                 OpenSetupMatchFormatMenu(e, state);
               });
  // Line 5: Skip
  b.add("Skip", MenuAlign::Left, [state](gentity_t *e, Menu &m) {
    gi.TagFree(state);
    MenuSystem::Close(e);
    if (e && e->client && e->client->initialMenu.frozen)
      OpenJoinMenu(e);
  });
  MenuSystem::Open(ent, b.build());
}
