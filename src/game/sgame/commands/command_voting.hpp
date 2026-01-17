#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

struct gentity_t;

namespace Commands {

inline constexpr int32_t kVoteFlag_Map = 1 << 0;
inline constexpr int32_t kVoteFlag_NextMap = 1 << 1;
inline constexpr int32_t kVoteFlag_Restart = 1 << 2;
inline constexpr int32_t kVoteFlag_Gametype = 1 << 3;
inline constexpr int32_t kVoteFlag_Timelimit = 1 << 4;
inline constexpr int32_t kVoteFlag_Scorelimit = 1 << 5;
inline constexpr int32_t kVoteFlag_Shuffle = 1 << 6;
inline constexpr int32_t kVoteFlag_Unlagged = 1 << 7;
inline constexpr int32_t kVoteFlag_Cointoss = 1 << 8;
inline constexpr int32_t kVoteFlag_Random = 1 << 9;
inline constexpr int32_t kVoteFlag_Balance = 1 << 10;
inline constexpr int32_t kVoteFlag_Ruleset = 1 << 11;
inline constexpr int32_t kVoteFlag_Arena = 1 << 12;
inline constexpr int32_t kVoteFlag_Forfeit = 1 << 13;

inline constexpr int32_t kDefaultVoteFlags =
    kVoteFlag_Map | kVoteFlag_NextMap | kVoteFlag_Restart | kVoteFlag_Gametype |
    kVoteFlag_Timelimit | kVoteFlag_Scorelimit | kVoteFlag_Shuffle |
    kVoteFlag_Unlagged | kVoteFlag_Cointoss | kVoteFlag_Random |
    kVoteFlag_Balance | kVoteFlag_Ruleset | kVoteFlag_Arena | kVoteFlag_Forfeit;

struct VoteDefinitionView {
  std::string name;
  int32_t flag;
  bool visibleInMenu;
};

struct VoteLaunchResult {
  bool success = false;
  std::string message;
};

VoteLaunchResult TryLaunchVote(gentity_t *ent, std::string_view voteName,
                               std::string_view voteArg);
std::span<const VoteDefinitionView> GetRegisteredVoteDefinitions();

} // namespace Commands
