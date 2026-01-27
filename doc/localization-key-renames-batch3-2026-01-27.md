# Localization Key Renames (Batch 3) - 2026-01-27

## Summary
Renamed 50 additional auto-generated localization keys in client/main, console,
keys, and locs subsystems, updating all localization files and code references.

## Renamed Keys (50 total)
### Client main
- `e_auto_47660594f0e7` -> `cl_status_response_from`
- `e_auto_4769270aeefb` -> `cl_reconnect_no_server`
- `e_auto_4df11c3d1cb7` -> `cl_status_usage`
- `e_auto_52fe84e67c1a` -> `cl_following_server`
- `e_auto_5514076c935f` -> `cl_requesting_connection`
- `e_auto_5978ab1f0a4c` -> `cl_writeconfig_help`
- `e_auto_5b394e95bd0d` -> `cl_status_table_header`
- `e_auto_63acd6f01865` -> `cl_anticheat_loading`
- `e_auto_6997f57b8941` -> `cl_write_open_failed`
- `e_auto_6ee119dba76f` -> `cl_connect_second_arg_ignored`
- `e_auto_6eed87d4d3e2` -> `cl_reconnecting`
- `e_auto_733d45d943d1` -> `cl_vid_restart_manual_ignored`
- `e_auto_73e61d85e709` -> `cl_load_failed`
- `e_auto_7c1aa98fac25` -> `cl_passive_listen_started`
- `e_auto_7e5843912eda` -> `cl_ignore_filter_removed_count`
- `e_auto_86647dcd1eca` -> `cl_passive_connect_received`
- `e_auto_901124f8c0c4` -> `cl_missing_filename`
- `e_auto_94387398dcf4` -> `cl_filter_oversize_line`
- `e_auto_9449d59c2dfa` -> `cl_value_inexact_warning`
- `e_auto_99880e5d83b2` -> `cl_vid_restart_auto_info`
- `e_auto_9d2d506fbca0` -> `cl_anticheat_connect_without`
- `e_auto_9d3ce8989acb` -> `cl_ignore_filter_match_too_short`
- `e_auto_a27b63711677` -> `cl_invalid_protocol`
- `e_auto_a74af888a687` -> `cl_follow_no_address`
- `e_auto_a883e09d37b0` -> `cl_ignore_filters_header`
- `e_auto_b5648fb19f60` -> `cl_anticheat_loaded`
- `e_auto_bd726af48aa6` -> `cl_ignore_filter_entry`
- `e_auto_c82fb443bf35` -> `cl_dump_program_written`
- `e_auto_c859496e563d` -> `cl_write_complete`
- `e_auto_cd41ea86bba9` -> `cl_changing_map`
- `e_auto_cdd0cd072e5b` -> `cl_sound_usage`
- `e_auto_cf3a55754bf2` -> `cl_rcon_requires_address`
- `e_auto_d47c2c22870e` -> `cl_dump_requires_level`
- `e_auto_db2474c829a0` -> `cl_dump_no_target`
- `e_auto_db968b79e17c` -> `cl_connected_to_server`
- `e_auto_e9f660ec33af` -> `cl_warning_disable_hint`
- `e_auto_eed59e38c05c` -> `cl_filename_usage`
- `e_auto_f21c677009ba` -> `cl_ignored_stufftext`
- `e_auto_fb76d1529fd3` -> `cl_command_usage`

### Console
- `e_auto_5d5e456dd309` -> `cl_chat_requires_level`
- `e_auto_72168c60abd6` -> `con_dumped_to_file`
- `e_auto_8b3a3eed2c7e` -> `cl_rcon_usage`
- `e_auto_c4616e5ce1be` -> `con_invalid_value`

### Key binding
- `e_auto_4c61facc074a` -> `key_invalid`
- `e_auto_8f37cefa77a7` -> `key_bind_usage`
- `e_auto_dc34814563e2` -> `key_not_bound`
- `e_auto_ed1789c79286` -> `key_unbind_usage`

### Locations
- `e_auto_56b7f769dcc1` -> `locs_requires_level`
- `e_auto_d506e504b1d3` -> `locs_usage`
- `e_auto_a15ec000714d` -> `locs_no_locations`

## Files Updated
- `assets/localization/loc_*.txt`
- `src/client/main.cpp`
- `src/client/console.cpp`
- `src/client/keys.cpp`
- `src/client/locs.cpp`
- `src/client/sound/dma.cpp`
- `src/client/sound/main.cpp`
- `src/client/ui/script.cpp`
- `src/client/ui/servers.cpp`
- `src/game/cgame/ui/ui_page_servers.cpp`
- `src/game/cgame/cg_ui_sys.cpp`
- `src/game/cgame/ui/ui_json.cpp`
- `src/game/cgame/ui/ui_mapdb.cpp`
- `src/game/cgame/ui/ui_page_player.cpp`

