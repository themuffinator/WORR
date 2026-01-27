# Localization Key Renames (Map Voting/Pool) - 2026-01-27

## Summary
Renamed additional auto-generated localization keys in the map voting / map pool
system to descriptive names and updated all code references and localization
files accordingly.

## Renamed Keys
- `g_sgame_auto_535dd80f8240` -> `match_map_vote_failed_random_select`
- `g_sgame_auto_92b75a8a3a53` -> `match_map_vote_failed_no_maps_prefix`
- `g_sgame_auto_0f9ab3ba943c` -> `admin_map_selection_failed_prefix`
- `g_sgame_auto_12ef9f85f0bd` -> `match_map_vote_unavailable_restart`
- `g_sgame_auto_d0cdf8fb0240` -> `match_map_vote_unavailable_next`
- `g_sgame_auto_e076d979db35` -> `match_map_vote_started`
- `g_sgame_auto_1369af5fa8ea` -> `match_map_vote_cast`
- `g_sgame_auto_19ede381a411` -> `match_map_vote_majority`
- `g_sgame_auto_1ecd89958d33` -> `mappool_load_summary_prefix`
- `g_sgame_auto_8f788aea4ba0` -> `mappool_removed_requests_prefix`
- `g_sgame_auto_af5d04591c5b` -> `mappool_skip_entry`
- `g_sgame_auto_a863201cae91` -> `mappool_file_open_failed`
- `g_sgame_auto_895252feff84` -> `mappool_json_parse_failed`
- `g_sgame_auto_ea3a23fdef34` -> `mappool_json_missing_maps_array`
- `g_sgame_auto_ed23801259d6` -> `mappool_file_not_found`
- `g_sgame_auto_1f442d9cde11` -> `mapcycle_marked_cycleable_summary`
- `g_sgame_auto_aba33e33156a` -> `mapcycle_invalid_cycle_file`
- `g_sgame_auto_c7f9001a3958` -> `mapcycle_file_open_failed`
- `g_sgame_auto_c23f4c0c976c` -> `maplist_filter_match_summary`
- `g_sgame_auto_bf21a9e8fbc5` -> `format_passthrough`
- `g_sgame_auto_adc83b19e793` -> `format_blank_line`

## Files Updated
- `assets/localization/loc_*.txt` (all languages)
- `src/game/sgame/gameplay/g_map_manager.cpp`
- `src/game/sgame/commands/command_client.cpp`
- `src/game/sgame/client/client_session_service_impl.cpp`
- `src/game/sgame/match/tournament.cpp`
- `src/game/sgame/gameplay/g_main.cpp`
- `src/game/sgame/gameplay/g_utilities.cpp`
- `src/game/sgame/gameplay/g_func.cpp`
- `src/game/sgame/gameplay/g_target.cpp`
