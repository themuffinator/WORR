# Demo/Server Browser List Layout Update

## Goals
- Prevent list backgrounds and status rows from overlapping the hint bar icons.
- Increase list row spacing to track the active UI font line height.

## Changes
- Added per-list row spacing support in `ListWidget`, with row-aware hit testing, scrolling, and draw sizing.
- Centered list text vertically inside each row so larger spacing reads cleanly.
- Demo and server browser lists now compute row spacing from `UI_FontLineHeight(1)`.
- Demo and server browser layouts reserve two text rows for the hint bar to match icon height.

## Notes
- The hint reserve height matches the current hint icon sizing (2x text height) in `UI_DrawHintBar`.
- List spacing defaults to the existing `MLIST_SPACING` when no custom row spacing is set.
