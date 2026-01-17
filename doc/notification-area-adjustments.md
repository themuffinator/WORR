# Notification Area Adjustments

## Summary
- The notification area now respects safe-zone clamping and stays above the status bar by two icon heights.
- Message mode keeps the full notification buffer available for smooth scrolling instead of dropping expired lines.
- Chat prompt sizing is derived from the actual prompt text so `say` and `say_team` fit cleanly.
- Mouse grab stays active in message mode, with SDL/Wayland handling updated for scroll and drag input.

## Details
Safe zone margins are derived from `scr_safe_zone` and clamped to 0.0-0.5, matching other HUD elements.
The bottom anchor is lifted by two status bar icon heights to avoid overlapping the layout program.
Prompt spacing now uses the prompt string length (with a trailing space), and the skip is clamped to
leave at least one visible input character.
When message mode is active, all buffered notifications (up to 32) are included for scrolling even
if they are older than the standard 4s timeout. Outside message mode, the 4s + 200ms fade still
applies.
Mouse capture stays on during message mode; SDL disables relative mode for message input, and Wayland
updates the virtual cursor on relative motion so the scrollbar and input cursor can be used.
