Item colorize without greyscale (Jan 28, 2026)

Summary
- Removed greyscale conversion on the base skin layer when item colorize is active.
- Base layer is now only brightened while preserving the original skin colors.

OpenGL
- GLS_ITEM_COLORIZE_BASE now boosts diffuse.rgb by 1.20x (clamped) instead of
  converting to luminance.

VKPT
- Base color is brightened (1.20x, clamped) without greyscale conversion.
