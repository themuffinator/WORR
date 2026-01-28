Item colorize early parsing (Jan 28, 2026)

Summary
- Item color configstrings are now parsed before precache completes.
- This prevents the color table from staying empty when configstrings arrive
  during the connection phase.

Why
- Configstrings are sent while cls.state < ca_precached.
- update_configstring previously returned early, skipping item color parsing.
- Result: RF_ITEM_COLORIZE never activated despite valid configstrings.

Implementation
- Moved item color parsing above the precache state guard in
  update_configstring.
