# SDL_ttf / FreeType Vertical Alignment & Missing Descenders

## Correct vertical alignment equation (SDL_ttf / FreeType metrics)

SDL_ttf’s `TTF_GetGlyphMetrics()` gives you glyph bounds **relative to the baseline**:

- `maxy` = distance from baseline **up to the top** of the glyph (usually positive)
- `miny` = distance from baseline **down to the bottom** of the glyph (usually negative for descenders)

If you’re positioning a line by its **top-left** at `(x0, y0)`:

```c
int ascent = TTF_GetFontAscent(font);        // baseline offset from top
int minx, maxx, miny, maxy, advance;
TTF_GetGlyphMetrics(font, cp, &minx,&maxx,&miny,&maxy,&advance);

// baseline in screen pixels
float baseline_y = y0 + ascent * s;

// top-left of glyph bitmap in screen pixels
float glyph_x = pen_x + minx * s;
float glyph_y = baseline_y - maxy * s;      // key equation
```

**One-liner:**

> glyph_top = line_top + ascent − glyph_maxy (scaled)

---

## What’s going wrong (why descenders are missing)

In the screenshot, **all descenders vanish** (`g p q y j`). This almost never means a baseline math bug.  
It strongly indicates **alpha cutoff / alpha test rendering**.

At small sizes, FreeType rasterizes many descender pixels with **low coverage alpha**.  
If your renderer treats the glyph atlas as a **binary mask** (alpha test, discard < 0.5), those pixels are dropped.

This kind of behavior is typical in Quake-derived render paths where fonts are rendered via a special “font” image type that implies alpha testing instead of blending.

Result:
- Main letter bodies survive
- Thin / antialiased descender rows disappear completely

---

## How to fix it (in priority order)

### 1. Render TTF text with proper alpha blending

For TTF glyph atlases:

- **Disable alpha test / discard**
- **Enable blending**
- Use straight alpha blending

```c
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

If shader-based:
- Do **not** `discard` based on alpha for text

If you have legacy bitmap fonts:
- Keep their old path
- Add a separate render state or image type for TTF fonts

---

### 2. Treat glyph textures as alpha-only masks

For maximum robustness:

- Store coverage in **A**
- Set **RGB = 255** (or ignore RGB in shader)

Fragment logic:

```glsl
out.rgb = vertexColor.rgb;
out.a   = vertexColor.a * tex.a;
```

This avoids issues with straight vs premultiplied alpha coming out of SDL_ttf.

---

### 3. Supersample glyphs (especially if cutoff must remain)

If alpha testing cannot be removed:

- Rasterize fonts at **2×–4×** the target size
- Downscale when rendering (linear filtering)

This converts fractional coverage into solid pixels that survive cutoff.

Example:
- Console font ~8 px high → rasterize at 16–32 px
- Draw scaled down

---

### 4. Adjust hinting mode (secondary tuning)

Current setting:

```c
TTF_SetFontHinting(ttf, TTF_HINTING_LIGHT);
```

Try:

- `TTF_HINTING_MONO` (often increases solid pixels)
- `TTF_HINTING_NONE` (more faithful shapes, blurrier at tiny sizes)

This won’t fix alpha test alone, but can improve survivability.

---

## Rounding bug safety net

When scaling glyphs, rounding **down** can clip the last pixel row.

Rules:
- Positions → `floor`
- Sizes → `ceil`

Example:

```c
int draw_y = (int)floorf(y);
int h      = (int)ceilf(glyph->h * glyph_scale);
```

This prevents losing single-pixel descenders during rasterization.

---

## 2-minute confirmation test

1. Inspect the SDL surface returned for `'p'` or `'g'`
2. Check bottom rows’ alpha values (often < 128)
3. Temporarily disable alpha test and enable blending
4. If descenders instantly appear → diagnosis confirmed

---

## Bottom line

- Baseline math is correct:
  **glyph_y = (line_top + ascent) − maxy**
- The missing descenders are almost certainly caused by **binary alpha rendering**
- Use **alpha blending** for TTF fonts, or supersample aggressively if you can’t

This is a classic legacy-renderer vs modern font rasterization mismatch.
