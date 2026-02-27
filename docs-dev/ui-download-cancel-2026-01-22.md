# Download Overlay Cancel + UI Font

## Summary
- Added an Esc cancel hint to the download overlay so the hint bar renders with ui_font.
- Added a dedicated download_cancel command that disconnects while downloads are pending.
- Wired the download_status close action to download_cancel.

## Behavior
- The download overlay uses the UI hint bar to show Esc Cancel even without selectable items.
- Pressing Esc (or right-click) runs download_cancel, which disconnects and clears pending downloads.

## Implementation Notes
- download_cancel is registered in CL_InitDownloads in src/client/download.cpp.
- Menu::BuildDefaultHints special-cases download_status to supply the Esc Cancel hint.
- download_status closeCommand points at download_cancel in src/game/cgame/ui/worr.json.
