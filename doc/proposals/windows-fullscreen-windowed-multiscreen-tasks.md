# Windows/SDL Fullscreen, Windowed, and Multiscreen Tasks & Guidance

This document consolidates the currently listed tasks and guidance around fullscreen/windowed/multiscreen behavior, Windows 11 Print Screen capture issues, Discord streaming, and performance considerations.

## Scope
- SDL3 fullscreen/windowed/multiscreen improvements.
- Windows 11 Print Screen capture failures (including when the in-game console is visible).
- Discord streaming/capture compatibility.
- Performance considerations for exclusive fullscreen vs. borderless fullscreen.

---

## Tasks: SDL3 Fullscreen/Windowed/Multiscreen

### 1) Fullscreen mode list tied to primary display
**Problem:** SDL3 `get_mode_list` uses the primary display while `set_mode` uses the window display, causing mismatched modes on multi-monitor setups.

**Task:** Add display selection and keep modelist in sync.
1. Add a new archived cvar (e.g., `r_display`) in `src/client/renderer.cpp` to select a display index or name.
2. Update `src/unix/video/sdl.c::get_mode_list` to use `r_display` when set; otherwise use the window’s current display (fallback to primary if no window).
3. Update `src/unix/video/sdl.c::set_mode` to use the same display selection logic so fullscreen switching aligns with the modelist.
4. Optionally expose `r_display` in `src/client/ui/worr.menu` under the Video menu for multiscreen selection.
5. If an invalid display is selected, log a warning and fall back to the primary display.

### 2) Display-change events do not refresh state
**Problem:** Dragging a window to another display does not refresh modelist or display-dependent state.

**Task:** Handle SDL window display changes.
1. Handle `SDL_EVENT_WINDOW_DISPLAY_CHANGED` in `src/unix/video/sdl.c::window_event`.
2. On display change, refresh windowed geometry (`VID_SetGeometry`) and trigger a modelist refresh (`MODE_MODELIST`).
3. Ensure fullscreen transitions after a display change use the updated display selection.
4. Verify `SCR_ModeChanged` and `R_ModeChanged` receive the correct sizes after display switches.

---

## Tasks: Windows 11 Print Screen Capture

### 3) Fullscreen Print Screen tool can’t determine window size
**Problem:** Windows 11 capture tools may fail on exclusive fullscreen because the app owns the display mode.

**Task:** Add borderless fullscreen option.
1. Add a new cvar (e.g., `r_fullscreen_exclusive`) in `src/client/renderer.cpp` to choose `exclusive` or `borderless`.
2. In `src/windows/client.c::Win_SetMode`, implement a borderless fullscreen path that:
   - Uses the monitor bounds for `win.rc`.
   - Sets `WS_POPUP` / topmost styles.
   - Skips `ChangeDisplaySettings` (no exclusive mode switch).
3. Keep exclusive fullscreen as the default to preserve legacy behavior.
4. Ensure `Win_ModeChanged` uses the new bounds and cursor clipping remains correct.

### 4) Print Screen fails when the in-game console is visible
**Problem:** Print Screen fails in both windowed and fullscreen when the in-game console is visible.

**Task:** Investigate and fix console-visible capture failures.
1. Reproduce on Windows 11 and confirm whether the issue is specific to `win32wgl` or `win32egl`.
2. Audit how console visibility affects window focus, input capture, and cursor state:
   - `src/client/console.cpp::toggle_console`
   - `src/client/input.cpp::IN_GetCurrentGrab` / `IN_Activate`
   - `src/windows/client.c::Win_DeAcquireMouse` / `Win_GrabMouse`
3. Add temporary logging to track console visibility, window flags, and capture-related state while Print Screen is used. Remove logs after verification.
4. If the failure correlates with mouse capture/focus, update input/cursor handling to keep capture stable while `KEY_CONSOLE` is active.
5. If the failure is tied to exclusive fullscreen, confirm borderless fullscreen fixes capture even with the console visible.

---

## Guidance: Discord Streaming/Capture

- The codebase currently implements **exclusive fullscreen** via `ChangeDisplaySettings(..., CDS_FULLSCREEN)` and uses `WS_POPUP` in fullscreen.
- Discord (and other capture tools) often fail to capture **exclusive fullscreen** windows, while **borderless fullscreen** typically behaves like a normal window and is easier to capture.
- Borderless fullscreen is therefore expected to **improve Discord streaming compatibility**, especially for Window Capture.
- Actual results can vary depending on Discord capture mode (Game Capture vs. Window Capture), GPU drivers, and overlay settings.

---

## Guidance: Performance (Exclusive vs. Borderless)

- **Exclusive fullscreen** can be marginally faster or more consistent because the app owns the display mode directly.
- **Borderless fullscreen** is usually near‑equivalent on Windows 10/11, but may introduce slight compositor overhead depending on GPU drivers and overlays.
- Practical impact tends to be small on modern systems; borderless is often chosen to maximize capture/streaming compatibility.

---

## Open Questions (for implementation alignment)
- Which Windows 11 capture tool is failing (Snipping Tool, Win+Shift+S, Game Bar, other)?
- Which renderer is active on Windows (`win32wgl` or `win32egl`), and is the issue present on both?
- Does the Print Screen failure occur only with the **full console** or also with the **partial console**?
- Should borderless fullscreen be **opt-in** (cvar) or become the **default** on Windows 11?
