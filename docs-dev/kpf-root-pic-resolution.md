# Rooted Pics and Q2Game.kpf Assets

## Issue
- UI elements such as the weapon wheel load images from `/gfx/` paths.
- These assets live in `Q2Game.kpf` (for example, `gfx/weaponwheel.png`), so
  treating rooted pic paths as `pics/` paths causes `No such file or directory`
  errors and missing UI imagery.

## Change
- Rooted pic paths now stay rooted (no `pics/` prefix) while still allowing the
  default `.pcx` extension to be applied when an explicit extension is missing.
- This keeps `/gfx/*.png` and other rooted assets discoverable in `Q2Game.kpf`
  without impacting `pics/`-relative lookups for standard UI art.

## Impact
- `R_RegisterPic("/gfx/weaponwheel.png")` and `/gfx/wheelbutton.png` resolve
  directly against `Q2Game.kpf` content.
- Rooted pics remain compatible with extension fallback, so `/tags/default`
  can still resolve to `tags/default.png` in base data.
