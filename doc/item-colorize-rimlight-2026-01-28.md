Item colorize rimlight + brightness (Jan 28, 2026)

Summary
- cl_colorize_items now supports values up to 2.0.
- Values in [0..1] keep the standard item color overlay.
- Values in (1..2] add a rim light of the same item color with strength
  (cl_colorize_items - 1.0).
- Item color overlay is brightened by scaling luminance up to 1.35x.

Client behavior
- Item colorize strength is clamped to 0..2.
- Overlay alpha uses min(value, 1.0).
- Rim light uses max(value - 1.0, 0.0) and follows entity alpha.

Renderer behavior
- GLSL item colorize overlay uses luminance * 1.35 (clamped to 1.0) before tint.
- VKPT applies the same luminance boost.
