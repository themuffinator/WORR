# SDL3 Gamepad Support

## Overview
- Added SDL3 gamepad subsystem initialization, hotplug handling, and input event
  processing for SDL-based clients.
- Introduced explicit gamepad key names to match Quake II Rerelease `default.cfg`
  bindings (e.g. `a_button`, `left_trigger`, `DPAD_UP`).
- Implemented analog stick movement and look handling with per-stick deadzones,
  plus trigger thresholding to emit digital `left_trigger` / `right_trigger`
  key events.
- Added optional gamepad bind icon resolution that maps key names to
  `/gfx/controller/gamepad/<key>.png` when assets are present.

## Gamepad Button Mapping
SDL gamepad buttons map to the following key names (case-insensitive):
- `SDL_GAMEPAD_BUTTON_SOUTH` -> `A_BUTTON`
- `SDL_GAMEPAD_BUTTON_EAST` -> `B_BUTTON`
- `SDL_GAMEPAD_BUTTON_WEST` -> `X_BUTTON`
- `SDL_GAMEPAD_BUTTON_NORTH` -> `Y_BUTTON`
- `SDL_GAMEPAD_BUTTON_BACK` -> `BACK_BUTTON`
- `SDL_GAMEPAD_BUTTON_GUIDE` -> `GUIDE_BUTTON`
- `SDL_GAMEPAD_BUTTON_START` -> `START_BUTTON`
- `SDL_GAMEPAD_BUTTON_LEFT_STICK` -> `LEFT_STICK`
- `SDL_GAMEPAD_BUTTON_RIGHT_STICK` -> `RIGHT_STICK`
- `SDL_GAMEPAD_BUTTON_LEFT_SHOULDER` -> `LEFT_SHOULDER`
- `SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER` -> `RIGHT_SHOULDER`
- `SDL_GAMEPAD_BUTTON_DPAD_UP` -> `DPAD_UP`
- `SDL_GAMEPAD_BUTTON_DPAD_DOWN` -> `DPAD_DOWN`
- `SDL_GAMEPAD_BUTTON_DPAD_LEFT` -> `DPAD_LEFT`
- `SDL_GAMEPAD_BUTTON_DPAD_RIGHT` -> `DPAD_RIGHT`
- `SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1` -> `RIGHT_PADDLE1`
- `SDL_GAMEPAD_BUTTON_LEFT_PADDLE1` -> `LEFT_PADDLE1`
- `SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2` -> `RIGHT_PADDLE2`
- `SDL_GAMEPAD_BUTTON_LEFT_PADDLE2` -> `LEFT_PADDLE2`
- `SDL_GAMEPAD_BUTTON_TOUCHPAD` -> `TOUCHPAD`
- `SDL_GAMEPAD_BUTTON_MISC1`..`MISC6` -> `MISC1`..`MISC6`

Triggers are axes (`SDL_GAMEPAD_AXIS_LEFT_TRIGGER` /
`SDL_GAMEPAD_AXIS_RIGHT_TRIGGER`) and emit `LEFT_TRIGGER` / `RIGHT_TRIGGER`
key events when their normalized value crosses `in_gamepad_trigger_threshold`.

## Analog Stick Behavior
- Left stick controls forward/side movement using `cl_forwardspeed` and
  `cl_sidespeed`, honoring `cl_run` / `+speed` for doubling.
- Right stick controls view yaw/pitch (degrees/sec) using `in_gamepad_yaw` and
  `in_gamepad_pitch` scaled by `in_gamepad_look_sensitivity`.
- When the weapon/inventory wheel is open, the right stick feeds
  `Wheel_Input()` instead of rotating the view.

## Menu/Console Navigation
When the menu or console is active, gamepad navigation is mapped as follows:
- `DPAD_*` -> Arrow keys
- `A_BUTTON` -> `Enter`
- `B_BUTTON` -> `Escape`

This preserves normal bindings in-game while enabling controller navigation in
UI contexts.

## New/Updated Cvars
All are archived (`CVAR_ARCHIVE`) unless noted.
- `in_gamepad` (1): Master enable for gamepad input.
- `in_gamepad_deadzone` (0.2): Radial deadzone for the left stick.
- `in_gamepad_look_deadzone` (0.15): Radial deadzone for the right stick.
- `in_gamepad_trigger_threshold` (0.2): Trigger press threshold (0..1) for
  emitting digital trigger key events.
- `in_gamepad_yaw` (140): Yaw speed in degrees/sec at full right stick deflection.
- `in_gamepad_pitch` (150): Pitch speed in degrees/sec at full right stick deflection.
- `in_gamepad_look_sensitivity` (1.0): Multiplier for right stick look speed.
- `in_gamepad_invert_y` (0): Invert the right stick Y axis when set to 1.
- `in_gamepad_wheel_speed` (1200): Right stick speed used to drive wheel input.

## Files Touched
- `inc/client/keys.h`: Added gamepad keycodes and ranges.
- `src/client/keys.c`: Registered new key names and UI navigation mappings.
- `inc/client/input.h`: Added gamepad axis enum and input entry points.
- `src/client/input.c`: Added axis handling, trigger thresholds, analog movement,
  and look integration.
- `src/client/client.h`: Added `gamepadmove` accumulator.
- `src/unix/video/sdl.c`: SDL3 gamepad initialization, hotplug, and event processing.
- `src/client/screen.c`: Optional gamepad bind icon lookup path support.
