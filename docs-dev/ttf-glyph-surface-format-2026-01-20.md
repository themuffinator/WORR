# SDL3_ttf Glyph Surface Fix (2026-01-20)

## Problem
- TTF glyphs were rendered as opaque rectangles with tinted backgrounds.
- The glyphs also appeared vertically odd because the pixel data was being
  interpreted incorrectly.

## Cause
- SDL3_ttf returns glyph surfaces in byte-order formats (RGBA32). The code
  was forcing `SDL_PIXELFORMAT_RGBA8888` (packed format), which swaps channel
  meaning on little-endian platforms. This caused the alpha channel to be read
  from the wrong byte, making the background opaque.
- The glyph surfaces were accessed without locking, which can be invalid for
  certain surface storage modes.

## Fix
- Convert glyph surfaces to `SDL_PIXELFORMAT_RGBA32` so byte order is RGBA
  for `glTexImage2D(..., GL_RGBA, ...)` and Vulkan `R8G8B8A8`.
- Lock/unlock the surface before copying pixel data into the font atlas.

## Result
- Correct alpha masks (no colored rectangles).
- Proper glyph placement once alpha is respected during blending.
