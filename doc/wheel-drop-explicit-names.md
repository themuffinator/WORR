# Wheel Drop Explicit Names Fix

## Problem
Wheel drop commands were passing localization tokens (eg. `$item_railgun`) to
the server. The server `drop` command expects the explicit pickup name, so it
rejected those tokens as unknown items.

## Fix
- Added `CL_Wheel_GetItemDropName()` to resolve configstring item names into
  explicit pickup names via `Loc_Localize()` before issuing a drop command.
- Updated weapon and ammo drop validation to require a resolvable explicit name.
- Drop commands now send `drop <pickup name>` for both weapon and ammo paths,
  avoiding the `unknown item` error.
