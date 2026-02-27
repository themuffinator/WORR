# Download Display TTF Bar Fix (2026-01-25)

## Problem
- The console download bar used legacy high-bit glyphs (0x80-0x83).
- With TTF/OTF console fonts, those bytes can resolve to missing-glyph boxes, so the bar
  shows block placeholders instead of a clean progress indicator.

## Fix
- When the console font is non-legacy, the download bar now uses ASCII glyphs
  (`[`, `=`, `>`, `]`) so TTF/OTF fonts render correctly.
- When a legacy bitmap font is active, the original bar glyphs remain unchanged.

## Impact
- TTF/OTF console fonts display the download bar without placeholder blocks.
- Legacy font appearance is preserved for classic setups.
