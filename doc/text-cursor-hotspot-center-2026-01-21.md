Text cursor hotspot center (2026-01-21)

Summary
- Restore the text cursor hotspot to its centered position while keeping the normal cursor anchored at the top-left.

Details
- Legacy and cgame menu cursor draws now offset the text cursor by half its size so the hotspot remains centered.
- Normal cursor continues to use a top-left hotspot so pointer coordinates match the image origin.
