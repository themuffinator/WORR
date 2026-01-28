Item colorize base rgbgen identity (Jan 28, 2026)

Summary
- When item colorize is active, the base skin now renders with rgbgen identity
  (no lighting modulation).
- The color overlay still applies using luminance * item tint.

OpenGL
- GLS_ITEM_COLORIZE_BASE now skips diffuse *= v_color instead of modifying color.

VKPT
- Removed the base brightening override to keep the base texture unchanged.
