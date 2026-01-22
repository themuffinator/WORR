Font Debug Logging and Legacy Fallback Path

Overview
- Added a `cl_debugFonts` cvar (default `1`) to print font detection and load
  details during startup and reloads.
- Updated the legacy fallback font image to `conchars.png` so the fallback
  goes through the normal `pics/` lookup path (pak0.pak).

Debug Output
- Prints every font load request (path, line height, pixel scale, fallbacks).
- Logs extension detection, TTF initialization, and SDL3_ttf load success/fail.
- Logs kfont parsing results (texture path, glyph count, line height).
- Logs legacy registration handles for both primary and fallback paths.

Paths
- Fallback legacy font image now uses `conchars.png` for console, UI, and
  screen font fallbacks.
