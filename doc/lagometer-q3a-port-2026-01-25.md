# Q3A Lagometer Port (cg_lagometer)

## Summary
- Added Quake III-style lagometer graph (frame interpolation/extrapolation + snapshot ping/drop) to the client HUD.
- Uses existing Q2 timing and net data to avoid protocol changes.

## Implementation Details
- Ring buffers: 128 samples for frame offset (`cl.time - cl.servertime`) and snapshot ping/drop data.
- Snapshot sampling: piggybacks on `SCR_LagSample`, uses `cls.netchan.dropped` to emit drop samples and `FF_SUPPRESSED` to tag rate-delayed snapshots (yellow).
- Frame sampling: recorded once per rendered frame in `SCR_DrawActive` to match render timing.
- Rendering: 48x48 graph drawn in `SCR_DrawNet`, with color scheme aligned to Q3A (blue interpolation, yellow extrapolation, green ping, yellow rate delayed, red drops). Shows `snc` when prediction is disabled and ping text when not in demo playback.
- Positioning still honors `cl_lag_x`/`cl_lag_y` (negative values align to bottom-right).
- Lagometer is suppressed for local servers (`sv_running`) to match Q3A behavior; legacy `scr_lag_draw` alias remains usable if needed.

## Cvars
- New `cg_lagometer` (default `1`, archive) controls the Q3A-style lagometer.
- Legacy `scr_lag_draw` alias remains available and is used when `cg_lagometer` is disabled.

## UI
- Updated screen options menus to toggle `cg_lagometer` (label: "lagometer").

## Compatibility
- No protocol changes; relies on existing client history, netchan drop counts, and frame flags.
- Legacy servers/demos remain unaffected.
