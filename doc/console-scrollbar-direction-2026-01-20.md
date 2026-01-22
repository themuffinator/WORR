# Console Scrollbar Direction Fix (2026-01-20)

## Summary
- Fix the console scrollbar thumb direction so it tracks scroll position
  correctly: newest content maps to the bottom, older content maps upward.

## Behavior
- When `con.display == con.current` (latest output), the thumb now sits at the
  bottom of the track.
- When scrolled up into history, the thumb moves upward as expected.

## Files Updated
- `src/client/console.cpp`
