# Cgame Player Preview and Bind Icons

## Player Setup Preview
- The preview viewport is anchored to the right of the menu entries and vertically
  centered against the menu content block.
- The model is larger via a tighter FOV and closer preview origin, with a faster
  idle rotation.
- Weapon preview models are sourced from `players/<model>` `w_*.md2` and
  `weapon.md2` assets (sorted); `uis.weaponModel` is still used as a preferred
  default or fallback.

## Keybind Widget Icons
- Keybind rows are 1.5x the normal line height so icon height can scale to match.
- Icon textures draw without tint; fallback keycaps keep UI colors for readability.
- Primary and secondary bindings render side by side when both are present.
- Bind icon lookup uses:
  - `gfx/controller/keyboard/<code>.png` for keyboard keys
  - `gfx/controller/mouse/f000X.png` for mouse buttons/wheel
  - `gfx/controller/generic/fXXXX.png` for gamepad keys (`keynum - K_GAMEPAD_FIRST`)
