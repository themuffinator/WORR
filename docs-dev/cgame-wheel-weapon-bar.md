# Cgame Wheel and Weapon Bar Migration

## Summary
- Weapon wheel and weapon bar logic now live in `src/game/cgame/cg_wheel.cpp` and render from cgame HUD code.
- Engine input routes wheel open/close, weapon cycling, and attack gating through cgame exports.
- Wheel and weapon bar assets are precached in cgame (`/gfx/weaponwheel.png`, `/gfx/wheelbutton.png`, `carousel/selected`).
- Wheel configstrings are parsed in cgame, and engine-side wheel data handling is no longer used.

## Engine Wiring
- `src/client/input.c` now queries `Wheel_AllowAttack` to gate attack input and uses `Wheel_ApplyButtons` + `WeaponBar_Input`
  to annotate command buttons before prediction.
- Mouse input forwards wheel motion via `Wheel_Input` when the wheel is open.
- `Wheel_WeapNext`/`Wheel_WeapPrev` are invoked for rerelease servers with a command fallback when unavailable.
- Command cleanup uses `WeaponBar_ClearInput` and `Wheel_ClearInput` to close out wheel state after send.

## Cgame API Surface
- New imports supply HUD rendering helpers and input hooks:
  `CL_ClientRealTimeUnscaled`, `SCR_DrawCharStretch`, `SCR_WarpMouse`, and optional `SCR_DrawBindIcon`.
- New exports allow the engine to drive the wheel and bar:
  `Wheel_Open`, `Wheel_Close`, `Wheel_Input`, `Wheel_WeapNext`, `Wheel_WeapPrev`, `Wheel_ApplyButtons`,
  `Wheel_ClearInput`, `Wheel_IsOpen`, `Wheel_TimeScale`, `Wheel_AllowAttack`, `Wheel_Update`,
  `WeaponBar_Input`, `WeaponBar_ClearInput`.

## Notes
- `CG_WeaponBar_Draw` and `CG_Wheel_Draw` are called from `CG_DrawHUD`.
- Weapon lock timing is tracked in cgame and enforced by `Wheel_AllowAttack`.
