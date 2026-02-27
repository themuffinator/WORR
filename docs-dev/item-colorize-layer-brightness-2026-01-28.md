Item colorize layer brightness separation (Jan 28, 2026)

Summary
- Removed brightness boost from the color overlay layer.
- Only the base skin layer is now brightened (and greyscaled) when colorize is active.

OpenGL
- GLS_ITEM_COLORIZE overlay now uses raw luminance (no extra boost).
- GLS_ITEM_COLORIZE_BASE still boosts base luminance (1.20x).

VKPT
- Overlay uses raw luminance; base greyscale keeps the 1.20x boost.
