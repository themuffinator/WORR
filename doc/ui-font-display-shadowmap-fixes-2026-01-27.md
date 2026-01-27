## UI font reload on mode changes

Display mode changes can recreate the GL context without a full renderer
restart. The cgame UI font atlas was cached across these changes and was not
reloaded, so the menu font degraded into blocky glyphs after leaving the video
menu. The UI font path now invalidates cached font state on mode change so the
atlas is rebuilt with the new context.

## Display slider range corrections

The video menu sliders used reversed ranges (`min > max`) for gamma and
texture quality. Mouse-driven sliders clamp with `Q_clipf`, so reversed ranges
lock the value at the minimum. The ranges were normalized to ascending order
with positive steps in both the JSON and legacy menu definitions.

## Shadowmap framebuffer state restore

Shadow map rendering binds dedicated FBOs. To avoid leaking framebuffer draw
state into the main pass (which can cause scrambled output on some drivers),
the shadow passes now force the draw/read buffer to `GL_COLOR_ATTACHMENT0`
while rendering, then restore the appropriate draw/read target after returning
to the main framebuffer.

## Files touched

- `inc/client/ui_font.h`
- `src/client/ui_font.cpp`
- `src/client/screen.cpp`
- `src/game/cgame/ui/worr.json`
- `src/client/ui/worr.menu`
- `src/rend_gl/main.c`
