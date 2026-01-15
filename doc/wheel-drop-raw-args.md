# Wheel Drop Raw Args Fix

## Root Cause
The server exposes `gi.args()` as `Cmd_RawArgs` (not `Cmd_Args`). When the wheel
sent `drop "<name>"`, the raw args string retained the quotes. `FindItem()` uses
the pickup name verbatim, so it failed to match `"Railgun"` or `"Slugs"`.

## Fix
- Wheel drop commands now send `drop <name>` without quotes.
- Raw args therefore contain the plain pickup name (including spaces), which
  matches `FindItem()` as expected.
