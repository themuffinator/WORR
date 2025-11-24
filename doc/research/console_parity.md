# Quake III Arena startup console vs. WORR WinConsole

## Quake III Arena behaviors (win_syscon.c)
- **Rendering/layout:** Uses a dedicated GUI window with fixed-size edit controls: a multiline read-only buffer with scroll bar plus Clear/Copy/Quit buttons and blue background with yellow text; error box uses gray background with blinking red/black text. Visible states: hidden, normal, or minimized controlled by `visLevel`.【F:refs/quake3/win_syscon.c†L44-L475】
- **Input/navigation:** Input line is a simple edit control; pressing Enter appends the line to internal text and sends it for processing. No explicit keybinds for history, word movement, or other navigation beyond what the edit control provides by default.【F:refs/quake3/win_syscon.c†L260-L290】
- **Color handling:** Quake color escape sequences are stripped before writing to the buffer; output is otherwise plain text with CR-LF normalization.【F:refs/quake3/win_syscon.c†L493-L574】
- **History/persistence:** Console text is stored only in the buffer; there is no saved history file or dedicated in-console history navigation APIs.【F:refs/quake3/win_syscon.c†L260-L492】
- **Resize handling:** Window uses absolute positions and does not process `WM_SIZE`; controls stay at their initial sizes.【F:refs/quake3/win_syscon.c†L295-L428】
- **Scrollback:** The buffer scrolls automatically to the bottom on show and when appending, and it trims content by replacing the full selection once total characters exceed 0x7fff.【F:refs/quake3/win_syscon.c†L454-L574】

## WORR WinConsole behaviors (src/windows/system.cpp)
- **Rendering/layout:** Uses the native console buffer; renders its own prompt line and updates on resize events. Supports explicit console font selection and color attributes per message.【F:src/windows/system.cpp†L330-L505】【F:src/windows/system.cpp†L507-L519】【F:src/windows/system.cpp†L1220-L1254】
- **Input/navigation:** Implements rich readline-style navigation: cursor movement, word jumps, deletion variants, history traversal, incremental search, and command completion with Tab/backslash toggle.【F:src/windows/system.cpp†L521-L945】
- **Color handling:** Applies Quake-style color indices to Win32 console attributes when ready, falling back to plain output when not initialized.【F:src/windows/system.cpp†L1220-L1254】
- **History/persistence:** Loads/saves history to a file when enabled and keeps prompt state in `sys_con`.【F:src/windows/system.cpp†L918-L945】【F:src/windows/system.cpp†L1263-L1277】
- **Resize handling:** Listens for `WINDOW_BUFFER_SIZE_EVENT` to recompute prompt width and logs the resize.【F:src/windows/system.cpp†L507-L519】
- **Scrollback:** Adjusts the console window to scroll by line, by page, or to the buffer edges; scrollback is tied to the console buffer rather than truncation.【F:src/windows/system.cpp†L946-L1000】【F:src/windows/system.cpp†L977-L1000】

## Parity gaps / requirements
1. **GUI affordances:** If matching Quake III’s startup console, add optional GUI shell with copy/clear/quit controls, static error box, and background/text theming akin to the yellow-on-blue palette; current WinConsole only uses the terminal buffer.【F:refs/quake3/win_syscon.c†L44-L195】【F:src/windows/system.cpp†L330-L505】
2. **Visibility states:** Support explicit hidden/normal/minimized states set via a `visLevel` equivalent so startup console can be minimized without closing, mirroring `Sys_ShowConsole`.【F:refs/quake3/win_syscon.c†L445-L475】
3. **Color escape stripping option:** Provide a mode to strip Quake color escapes when rendering to the Windows console to mirror Quake III’s plain-text buffer behavior, or map escapes to colors consistently during startup logging.【F:refs/quake3/win_syscon.c†L493-L574】【F:src/windows/system.cpp†L1220-L1254】
4. **Scrollback trimming:** Implement optional buffer-length limiting (e.g., reset/replace selection when total characters exceed a threshold) to match Quake III’s overflow behavior instead of relying solely on console buffer size.【F:refs/quake3/win_syscon.c†L560-L574】【F:src/windows/system.cpp†L946-L1000】
5. **Error state UI:** Add ability to swap the input line for a dedicated error box when fatal text is set, echoing `Sys_SetErrorText` UI semantics.【F:refs/quake3/win_syscon.c†L576-L596】
6. **Fixed-layout fallback:** Offer non-resizable fixed-position layout for startup scenarios if parity with Quake III’s non-resizing window is desired; today the console reacts to buffer resizes instead.【F:refs/quake3/win_syscon.c†L295-L428】【F:src/windows/system.cpp†L507-L519】
