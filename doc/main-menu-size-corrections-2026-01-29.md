# Main menu plaque/logo sizing + footer hint offset (2026-01-29)

## Overview
- Apply explicit width/height scaling for fixed plaque/logo rects.
- Ensure footer text sits above the hint bar so button hints remain visible.

## Changes
- Fixed plaque/logo drawing now uses R_DrawStretchPic with the JSON rect sizes.
- Footer Y positioning subtracts the hint bar height.
