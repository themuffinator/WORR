# Bold Name Visibility Fixes (2026-01-25)

## Summary
- Increase bold weight for chat speaker names and obituary killer/victim names so it is visibly distinct.
- Avoid multiplying drop shadows by disabling shadow on the additional bold passes.

## Bold Rendering Approach
- Use a multi-pass horizontal offset (2px at scale 1, otherwise `scale`) to create a thicker stroke.
- Reserve additional horizontal space so following text does not overlap the bolded name.
- First pass keeps drop shadow; extra passes draw without shadow to avoid heavy black blur.

## Files Touched
- `src/client/screen.cpp`: stronger faux-bold for notify chat names.
- `src/game/cgame/cg_draw.cpp`: stronger faux-bold for notify names and obituary killer/victim names, with spacing adjustments.

## Compatibility Notes
- Client-only rendering changes; no protocol, demo, or server changes.

## Testing
- Not run.
