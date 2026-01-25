# Menu Rework Proposal Review Updates (2026-01-22)

## Summary
This document records the updates applied to `doc/proposals/menu_rework.md` after the repo-aware review, focusing on JSON alignment, cvar correctness, and scope decisions.

## Changes Applied
1) Scope and preservation
- Deferred the retro theme requirement for this pass, keeping it as a future hook.
- Clarified that existing in-game flows (`setup_*`, `vote_menu`, `map_selector`, `match_stats`) are preserved and restyled, with a dedicated modal for voting/map selection.

2) JSON alignment
- Corrected condition keys to `showIf`/`enableIf`/`defaultIf` and updated the example usage.
- Updated widget guidance to prefer the existing `dropdown` item type and `switch`/`checkbox` for toggles; noted that overlay dropdown behavior still needs implementation work.

3) Cvar corrections and accuracy
- Replaced `vid_mode` with the actual `r_fullscreen` + `$$r_modelist` modelist behavior.
- Replaced `gl_contrast` with `gl_color_contrast` and added `gl_color_correction` as the gating toggle.
- Clarified that `ui_scale`, `con_scale`, and `cl_scale` should be represented as dropdown/pairs (0 = auto).
- Reframed bloom to use `gl_bloom` with `gl_bloom_intensity`/`gl_bloom_threshold` (optional `gl_bloom_sigma` as advanced).
- Clarified `cl_draw2d` as a selector (0-3), not a binary toggle.

4) Data sources for lists and palettes
- Map vote list now points at `$$com_maplist` or `ui_mapselector_option_*` cvars instead of local `maps/*.bsp`.
- Forced model selection specifies `model/skin` values sourced from a full scan of `players/`.
- Crosshair color is documented as an indexed palette (1-26), not a generic color string.

5) Formatting cleanup
- Replaced section symbol references with "Section" for ASCII consistency.

## Files Updated
- `doc/proposals/menu_rework.md`
