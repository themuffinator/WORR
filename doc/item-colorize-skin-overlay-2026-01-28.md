Item colorize skin overlay (Jan 28, 2026)

Summary
- Item colorize now tints the skin while stripping original hue.
- OpenGL uses a new shader path for an overlay pass with luminance-based tint.
- VKPT now uses luminance-based tinting instead of direct color mixing.

Why
- Users reported no visible change and asked for a color screen on top of the skin.
- Luminance-based tint preserves skin detail (highlights/shadows) without keeping
  the original skin colors.

OpenGL changes
- Added GLS_ITEM_COLORIZE shader bit to force a luminance-tinted overlay.
- Overlay pass now uses the real skin texture (not white) and alpha blends on top.
- Shader path replaces diffuse.rgb with (luminance * tint), using v_color.rgb
  for lighting+item tint, and diffuse.a * v_color.a for alpha.
- Bloom generation is disabled for the overlay pass to avoid duplicate bloom.

VKPT changes
- Item colorize now computes luminance from base_color and mixes toward
  tint * luminance by strength.

Tuning
- Strength remains controlled by cl_colorize_items (0..1).
