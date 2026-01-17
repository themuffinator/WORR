# Client Notification Area

## Overview
The client HUD now renders a bottom-left notification area that combines player chat and match announcements under the WORR banner. The area is anchored to the HUD safe zone and stays as low as possible without overlapping the HUD.

## Placement and Layout
- Anchor: bottom-left of the HUD safe zone.
- Stack: newest messages render closest to the bottom, pushing older lines upward.
- Message box: rendered at the bottom of the area, with notifications directly above.
- Safe zone: both the message list and input box are offset by `scr_safe_zone`.

## Visibility Rules
- Max visible notifications: 4 during normal play.
- Max visible notifications: 8 while message mode is active (`messagemode`, `messagemode2`).
- Timeout: 4 seconds per notification, fading out over the last 200 ms.

## Content Sources
- Player messages: `PRINT_CHAT` messages added when `cl_chat_notify` is enabled.
- Match announcements: `PRINT_HIGH` / `PRINT_TTS` messages added when `cl_cgame_notify` is enabled.

## Interaction (Message Mode)
- Mouse is released while message mode is active to allow UI interaction.
- Mouse wheel scrolls the notification history smoothly.
- Scrollbar supports click and drag for scrolling.
- Clicking in the input box positions the caret within the current message.

## Cvars and Compatibility Notes
- `scr_chathud` now defaults to `1` to enable the new notification area by default.
- `scr_chathud_lines`, `scr_chathud_time`, `scr_chathud_x`, and `scr_chathud_y` are no longer used for layout or timing; the notification area uses fixed rules above.
- Legacy console notify overlays are no longer drawn in-game; the HUD area replaces them.
