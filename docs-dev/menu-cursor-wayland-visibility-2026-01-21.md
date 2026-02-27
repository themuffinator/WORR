Wayland menu cursor visibility (2026-01-21)

Summary
- Hide the Wayland system cursor while menus are active so only the in-game cursor is visible.
- Ensure KEY_MENU checks in the Wayland backend use the game keydest flags, not Linux input codes.

Details
- Moved the KEY_MENU undef closer to the linux input header to prevent macro conflicts and keep Key_GetDest checks correct.
- Updated the Wayland cursor setter to hide the system cursor when menus are active or the mouse is grabbed.
