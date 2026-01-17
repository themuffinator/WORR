# Console Notify Suppression

## Summary
Client-side HUD notifies drawn by the cgame now respect `con_notifytime` and
`con_notifylines`. When either is set to `0`, top-left console notifications
are disabled and the internal list is cleared.

## Behavior
- Effective notify lines are clamped by `scr_maxlines`, `MAX_NOTIFY`, and
  `con_notifylines`.
- If `con_notifytime` or `con_notifylines` are `0`, no notify lines are added
  or drawn by the cgame layer.
- Obituary placement uses the effective notify line count, so it no longer
  leaves empty space when notifications are disabled.
