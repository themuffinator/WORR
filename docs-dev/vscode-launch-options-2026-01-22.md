# VSCode launch options update (2026-01-22)

## Why
- Default client launch should start `worr.exe` directly without triggering a build.
- Provide a quick Vulkan renderer toggle without editing configs by hand.

## What changed
- `WORR (client)` now launches `builddir\worr.exe` with no pre-launch build task.
- Added `WORR (client, Vulkan)` which passes `+set r_renderer vulkan` on the command line.
- `WORR (dedicated)` remains available for `worr.ded.exe` launches and still runs the `meson: build` task.

## How to use
- Select `WORR (client)` for a fast, no-build client run.
- Select `WORR (client, Vulkan)` to force the Vulkan renderer for the session.
