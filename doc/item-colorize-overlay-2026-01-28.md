Item colorize overlay pass (Jan 28, 2026)

Summary
- Switched item colorize from lighting modulation to a visible overlay mix.
- OpenGL now draws a second pass for RF_ITEM_COLORIZE to apply the tint.
- Vulkan path tracer now mixes albedo toward the tint for the same effect.

Why
- The previous lighting modulation could be too subtle or invisible in practice.
- An overlay mix makes the tint clearly visible while keeping the base model
  detail readable.

OpenGL implementation
- Base pass is unchanged.
- When RF_ITEM_COLORIZE is set and rgba.a > 0, a second pass is drawn.
- The overlay uses TEXNUM_WHITE and alpha blend (GLS_BLEND_BLEND) so it is a
  "color screen" on top of the base model.
- Tint color is multiplied by the current lighting (color[] from setup_color)
  so the overlay still respects shadows and highlights.
- Overlay alpha = (rgba.a / 255) * entity alpha, so translucent items scale
  the effect correctly.

Vulkan path tracer implementation
- base_color is now mixed toward the tint by strength:
  base_color = mix(base_color, tint, strength)
- This mirrors the OpenGL overlay mix, but in the albedo stage.

Behavior notes
- cl_colorize_items remains the strength control (0..1).
- Item RGB values still come from itemList[].colorize.
