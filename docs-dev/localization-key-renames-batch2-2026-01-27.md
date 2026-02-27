# Localization Key Renames (Batch 2) - 2026-01-27

## Summary
Renamed 50 additional auto-generated localization keys in UI/client/sound paths
and removed blank lines from localization files to keep formatting consistent.

## Renamed Keys (50 total)
### UI (cgame) keys
- `cg_auto_0f3fc5b0f530` -> `ui_bad_server_port`
- `cg_auto_35af01e118a2` -> `ui_bad_server_address`
- `cg_auto_38449524bdfb` -> `ui_unknown_menu_feeder`
- `cg_auto_43221e8bf8b1` -> `ui_cmd_options_header`
- `cg_auto_433adc0b106f` -> `ui_cmd_option_no_argument`
- `cg_auto_4f734c250891` -> `ui_cmd_expanded_line_too_long`
- `cg_auto_55f681b31104` -> `ui_cmd_help_hint`
- `cg_auto_713f13864237` -> `ui_pushmenu_max_depth`
- `cg_auto_7535716ffdff` -> `ui_cmd_usage_prefix`
- `cg_auto_8ac24f918f3e` -> `ui_cmd_usage_suffix`
- `cg_auto_9064e9b262c3` -> `ui_mapdb_unknown_type`
- `cg_auto_9f78db570fe8` -> `ui_http_fetch_not_supported`
- `cg_auto_ad494789521e` -> `ui_menu_not_found`
- `cg_auto_bf1a859155a3` -> `ui_json_load_failed`
- `cg_auto_bf69489cd5dd` -> `ui_mapdb_bad_level`
- `cg_auto_d6e935de0e44` -> `ui_cmd_unknown_option`
- `cg_auto_e13d46568ae9` -> `ui_cmd_menu_usage`
- `cg_auto_e6ba1369ba86` -> `ui_mapdb_bad_episode`
- `cg_auto_f0e9a2b48b82` -> `ui_player_models_not_found`
- `cg_auto_f2564956cac4` -> `ui_cmd_missing_argument`
- `cg_auto_f8b0044bcfd3` -> `ui_invalid_master_url_ignored`

### Client/sound keys
- `e_auto_001763d30d22` -> `s_dma_speed`
- `e_auto_078045195028` -> `cl_userinfo_header`
- `e_auto_09cfc56c5ba5` -> `cl_passive_listen_disabled`
- `e_auto_0ceb51b7d6b5` -> `cl_ignore_filters_none`
- `e_auto_0e4356fb2cf6` -> `s_painted_stats`
- `e_auto_0f3fc5b0f530` -> `cl_bad_server_port`
- `e_auto_11f0de8c20e7` -> `cl_bad_server_address`
- `e_auto_167b4e384169` -> `ui_script_unknown_state`
- `e_auto_198a2158c500` -> `cl_server_cmd_usage`
- `e_auto_1ae0c0071632` -> `cl_loadskins_requires_level`
- `e_auto_1d7c8dd8349d` -> `cl_anticheat_required_missing`
- `e_auto_2097b2791be0` -> `ui_script_command_usage`
- `e_auto_248efcc4ac89` -> `cl_location_entry`
- `e_auto_255c3fbcc0f4` -> `cl_requesting_challenge`
- `e_auto_28119ee774cb` -> `s_issue_line`
- `e_auto_2b03ba9a75d8` -> `ui_script_dir_usage`
- `e_auto_2b6a11d27c8c` -> `cl_location_added`
- `e_auto_2bc25acf4b25` -> `s_separator_line`
- `e_auto_2d29e918127d` -> `cl_ignore_filter_not_found`
- `e_auto_2d5bd94651b7` -> `cl_shutdown_recursive`
- `e_auto_313aa18385ba` -> `ui_script_invalid_width`
- `e_auto_3240af950c4e` -> `cl_error_writing_file`
- `e_auto_35af01e118a2` -> `cl_bad_server_address_detail`
- `e_auto_38e74046af44` -> `cl_reconnect_loopback_blocked`
- `e_auto_3995db0e9377` -> `cl_bad_address`
- `e_auto_3cc5fe0a3654` -> `cl_rcon_password_required`
- `e_auto_3d6cf9654db2` -> `ui_script_cvar_filter_usage`
- `e_auto_3e5f335bfb5c` -> `ui_script_menu_missing_end`
- `e_auto_41788ae1f142` -> `s_placeholder_entry`

## Files Updated
- `assets/localization/loc_*.txt`
- `src/game/cgame/ui/ui_page_servers.cpp`
- `src/game/cgame/ui/cg_ui_sys.cpp`
- `src/game/cgame/ui/ui_json.cpp`
- `src/game/cgame/ui/ui_mapdb.cpp`
- `src/game/cgame/ui/ui_page_player.cpp`
- `src/client/ui/servers.cpp`
- `src/client/ui/script.cpp`
- `src/client/main.cpp`
- `src/client/console.cpp`
- `src/client/ascii.cpp`
- `src/client/locs.cpp`
- `src/client/sound/dma.cpp`
- `src/client/sound/main.cpp`

## Cleanup
- Removed blank/empty lines from all localization files under `assets/localization`.
