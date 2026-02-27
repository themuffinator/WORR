Item colorize bitflags (Jan 28, 2026)

Summary
- cl_colorize_items is now a bitflag cvar.
- Bit 0 (1): enable item colorize overlay.
- Bit 1 (2): enable item rim light of the same color.
- Value is clamped to [0..3].

Behavior
- Overlay uses the existing item color tint path.
- Rim light spawns a separate RF_RIMLIGHT entity with the same color.
