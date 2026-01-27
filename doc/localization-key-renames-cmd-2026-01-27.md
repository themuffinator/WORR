# Localization Key Renames (Cmd) and Cleanup - 2026-01-27

## Summary
- Renamed auto-generated command-system localization keys to human-readable names.
- Removed blank/empty lines from all localization files for consistent formatting.

## Renamed Keys (command system)
- `e_auto_048af56bd6de` -> `cmd_if_valid_operators`
- `e_auto_1191386e1852` -> `cmd_trigger_usage`
- `e_auto_123e4c5a134d` -> `cmd_trigger_none`
- `e_auto_146248f9ee83` -> `cmd_alias_loop`
- `e_auto_147a87479fd1` -> `cmd_alias_clear_all`
- `e_auto_2128c0ac068b` -> `cmd_trigger_match_too_short`
- `e_auto_222e9240889b` -> `cmd_overflow`
- `e_auto_23a15e4dcb24` -> `cmd_not_connected`
- `e_auto_34a43883dfb0` -> `cmd_undefined_period`
- `e_auto_372c92bfdaaf` -> `cmd_line_unmatched_quote`
- `e_auto_3f32e987a006` -> `cmd_trigger_not_found`
- `e_auto_43221e8bf8b1` -> `cmd_options_header`
- `e_auto_433adc0b106f` -> `cmd_option_no_argument`
- `e_auto_4b9d566d5f92` -> `cmd_trigger_list_header`
- `e_auto_4f734c250891` -> `cmd_line_expanded_too_long`
- `e_auto_51d307fee76e` -> `cmd_if_usage`
- `e_auto_55f681b31104` -> `cmd_help_hint`
- `e_auto_5a2290e8949f` -> `cmd_alias_list_header`
- `e_auto_5db1acfedd95` -> `cmd_name_already_cvar_quoted`
- `e_auto_63ae75e00274` -> `cmd_name_already_defined`
- `e_auto_6d0101f3edea` -> `cmd_execing_file`
- `e_auto_7535716ffdff` -> `cmd_usage_prefix`
- `e_auto_7987984782b4` -> `cmd_trigger_removed_count`
- `e_auto_7cd2e8eb4815` -> `cmd_unknown_command`
- `e_auto_8229a49e2a97` -> `cmd_exec_usage`
- `e_auto_822ffbdd85b6` -> `cmd_echo_help`
- `e_auto_8aa0a71a471c` -> `cmd_alias_missing_name`
- `e_auto_8ac24f918f3e` -> `cmd_usage_suffix`
- `e_auto_900d94221b36` -> `cmd_name_already_cvar_with_cmd`
- `e_auto_b47c4588b65d` -> `cmd_line_too_long`
- `e_auto_bab55c887650` -> `cmd_exec_failed`
- `e_auto_bcc0a8583c82` -> `cmd_print_level_invalid`
- `e_auto_c1ac475b26f9` -> `cmd_kv_pair`
- `e_auto_c8135ca9df1b` -> `cmd_complete_usage`
- `e_auto_cc5e9df92c38` -> `cmd_name_already_command`
- `e_auto_cfbfbe758955` -> `cmd_list_count`
- `e_auto_d4de46d59515` -> `cmd_if_unknown_operator`
- `e_auto_d6e935de0e44` -> `cmd_unknown_option`
- `e_auto_e58851d27288` -> `cmd_alias_none`
- `e_auto_e6a1218ed003` -> `cmd_macro_loop`
- `e_auto_ed853522e12d` -> `cmd_name_already_cvar`
- `e_auto_f1a758aff3ef` -> `cmd_macro_list_count`
- `e_auto_f1b83d0e66c9` -> `cmd_wait_runaway`
- `e_auto_f2564956cac4` -> `cmd_missing_argument`
- `e_auto_fc5d90c00b6e` -> `cmd_undefined`

## Files Updated
- `assets/localization/loc_*.txt`
- `src/common/cmd.c`

## Cleanup
- Removed blank lines from all localization files under `assets/localization`.
