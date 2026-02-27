Item colorize greyscale base (Jan 28, 2026)

Summary
- When item colorize is active, the base skin is now greyscaled and brightened.
- The color overlay still applies on top, preserving item tint.

OpenGL
- Added GLS_ITEM_COLORIZE_BASE shader bit.
- Base pass converts lit diffuse to luminance and boosts it by 1.20x (clamped).
- Enabled GLS_ITEM_COLORIZE_BASE when RF_ITEM_COLORIZE is set.

VKPT
- Base color is greyscaled and brightened (1.20x) before tint overlay.
