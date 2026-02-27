Item colorize fullbright overlay (Jan 28, 2026)

Summary
- Colorization overlay now ignores lighting so it stays bright in dark areas.
- Overlay tint uses the raw item color (not multiplied by lightpoint).
- Dynamic lights are disabled for the overlay pass.

OpenGL
- Overlay color uses tint directly.
- GLS_DYNAMIC_LIGHTS is stripped from overlay state.
