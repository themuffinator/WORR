Item colorize configstring fallback (Jan 28, 2026)

Summary
- Item color configstrings are now parsed even when cs_remap does not expose
  an itemcolors base index.
- This allows WORR item colorization to work on protocols that don't set
  cl.csr.itemcolors, as long as the server still sends CS_ITEM_COLORS.

Client changes
- update_configstring now parses item color configstrings from either
  cl.csr.itemcolors (preferred) or CS_ITEM_COLORS if the remap value is -1.
- CL_GetItemColorizeColor no longer blocks on cl.csr.itemcolors; it relies on
  cl.item_color_by_model being populated.

Notes
- If a server never sends CS_ITEM_COLORS, this fallback has no effect.
