# Font Implementation Analysis

**Date:** 2026-01-20  
**Updated:** 2026-01-20 22:20  
**Status:** ✅ Implemented  
**Scope:** `src/client/font.cpp`, `src/client/ui_font.cpp`, renderer integration

---

## Critical Issue: Double Scaling Bug

> [!CAUTION]
> The `unit_scale` formula includes `pixel_scale`, but the renderer's `R_SetScale()` ALSO applies scaling. This causes **double scaling** - glyphs are drawn at incorrect sizes.

### How the Renderer's Scale System Works

The renderer uses `R_SetScale(scale)` to set up orthographic projection:

```cpp
void R_SetScale(float scale) {
    GL_Ortho(0, virtual_width * scale, virtual_height * scale, 0, -1, 1);
}
```

| Screen Resolution | Virtual Size | R_SetScale | Ortho Projection |
|-------------------|--------------|------------|------------------|
| 640×480 | 640×480 | 1.0 | 640×480 |
| 1280×960 | 640×480 | 2.0 | 1280×960 |
| 1920×1080 | 640×360 | 3.0 | 1920×1080 |

**Key insight:** When `R_SetScale(2.0)` is active, the coordinate system is 1280×960. One "virtual unit" in the code directly maps to one screen pixel. **The renderer has already applied the pixel_scale transformation!**

### Current (Broken) unit_scale Formula

```cpp
// Line 856-857
unit_scale = (virtual_line_height * pixel_scale) / extent;
// Example: (8 * 2.0) / 14 = 1.143
```

### The Double Scaling Problem

**Step 1: Font atlas created**
- `pixel_height = 8 * 2.0 = 16px` → font opened at 16px
- `extent = 14` (measured height)

**Step 2: Drawing (with R_SetScale(2.0) active)**
```cpp
glyph_scale = unit_scale * draw_scale = 1.143 * 1 = 1.143

// For a glyph with h=14 (atlas pixels):
h_output = 14 * 1.143 = 16 screen units

// The renderer is in 1280×960 space (R_SetScale(2.0))
// So 16 screen units = 16 screen pixels
// But we wanted 8 virtual units × 2.0 scale = 16 screen pixels... 
```

Wait, let me trace this more carefully with the actual R_SetScale values:

**At 1280×960:**
```
UI_Resize:
  base_scale_int = 2
  uis.scale = R_ClampScale(ui_scale=0) = 2.0/2.0 = 1.0 (!)
  
Hmm, R_ClampScale returns base_scale_int / ui_scale_int
With ui_scale=0, ui_scale_int = base_scale_int * 1.0 = 2
So R_ClampScale = 2 / 2 = 1.0
```

Wait, let me re-read R_ClampScale:

```cpp
float R_ClampScale(cvar_t *var) {
    int base_scale_int = get_base_scale_int();  // = 2
    int ui_scale_int = get_ui_scale_int(base_scale_int, var);  // depends on var
    return (float)base_scale_int / (float)ui_scale_int;
}

static int get_ui_scale_int(int base_scale_int, cvar_t *var) {
    float extra_scale = 1.0f;
    if (var && var->value)
        extra_scale = Cvar_ClampValue(var, 0.25f, 10.0f);
    int ui_scale_int = (int)((float)base_scale_int * extra_scale);
    if (ui_scale_int < 1)
        ui_scale_int = 1;
    return ui_scale_int;
}
```

With `ui_scale=0` (default), `var->value=0`, so `extra_scale=1.0`:
```
ui_scale_int = base_scale_int * 1.0 = 2
R_ClampScale = 2 / 2 = 1.0
uis.scale = 1.0
```

So `R_SetScale(1.0)` → Ortho(0, 640, 480, 0, -1, 1)

**UI is drawing in 640×480 virtual coordinates!**

Now the font:
```
UI_FontCalcPixelScale:
  pixel_scale = base_scale_int / ui_draw_scale = 2 / 1.0 = 2.0

Font_Load(..., virtual_line_height=8, pixel_scale=2.0):
  pixel_height = 8 * 2.0 = 16
  extent = 14
  unit_scale = (8 * 2.0) / 14 = 1.143
```

**Drawing with R_SetScale(1.0) active (640×480 space):**
```cpp
glyph_scale = 1.143 * 1 = 1.143
h_output = 14 * 1.143 = 16 virtual units

// But the screen is 640×480! We're drawing a 16-unit tall glyph in a 480-unit tall screen.
// That's 16/480 = 3.3% of screen height for one character!
// Expected: 8/480 = 1.7% of screen height
```

**The glyph is being drawn at 2× the intended size in virtual coordinates!**

### Root Cause Confirmed

The font's `unit_scale` includes `pixel_scale`:
```cpp
unit_scale = (virtual_line_height * pixel_scale) / extent
```

But `pixel_scale` is already factored into the atlas (font opened at 16px instead of 8px). The scaling from atlas to virtual should NOT include `pixel_scale` again.

### The Correct Formula

```cpp
// unit_scale should convert atlas pixels to virtual pixels
// Atlas is at pixel_height = virtual_line_height * pixel_scale
// We want output in virtual units (not physical)
// So: unit_scale = virtual_line_height / extent

unit_scale = (float)virtual_line_height / (float)extent;
// Example: 8 / 14 = 0.571

// Now a 14px atlas glyph → 14 * 0.571 = 8 virtual units ✓
```

### Summary of Required Changes

**Line 856-857 (TTF):**
```cpp
// FROM:
font->unit_scale = (float)(virtual_line_height * font->pixel_scale) / (float)font->ttf.extent;
// TO:
font->unit_scale = (float)virtual_line_height / (float)font->ttf.extent;
```

**Line 904-905 (kfont fallback after TTF fail):**
```cpp
// FROM:
font->unit_scale = (float)(virtual_line_height * font->pixel_scale) / (float)font->kfont.line_height;
// TO:
font->unit_scale = (float)virtual_line_height / (float)font->kfont.line_height;
```

**Line 924-925 (legacy fallback after kfont fail):**
```cpp
// FROM:
font->unit_scale = (float)(virtual_line_height * font->pixel_scale) / (float)CONCHAR_HEIGHT;
// TO:
font->unit_scale = (float)virtual_line_height / (float)CONCHAR_HEIGHT;
```

**Line 943-944 (legacy after explicit kfont fail):**
```cpp
// FROM:
font->unit_scale = (float)(virtual_line_height * font->pixel_scale) / (float)CONCHAR_HEIGHT;
// TO:
font->unit_scale = (float)virtual_line_height / (float)CONCHAR_HEIGHT;
```

**Line 946-947 (kfont success):**
```cpp
// FROM:
font->unit_scale = (float)(virtual_line_height * font->pixel_scale) / (float)font->kfont.line_height;
// TO:
font->unit_scale = (float)virtual_line_height / (float)font->kfont.line_height;
```

**Line 958-959 (legacy path):**
```cpp
// FROM:
font->unit_scale = (float)(virtual_line_height * font->pixel_scale) / (float)CONCHAR_HEIGHT;
// TO:
font->unit_scale = (float)virtual_line_height / (float)CONCHAR_HEIGHT;
```

---

## Secondary Issues

### Kfont Has No High-DPI Atlas

For kfont and legacy fonts, there's no high-DPI atlas - they're fixed-resolution bitmaps. At high DPI, these will appear blurry. The only solution is to use TTF fonts for crisp rendering.

### pixel_scale Is Still Needed for TTF

The `pixel_scale` IS correctly used for:
- Opening the TTF at higher resolution: `pixel_height = virtual_line_height * pixel_scale` ✓

Just NOT for `unit_scale`, which converts atlas→virtual, not atlas→physical.

---

## Testing Plan

After implementing fixes:

1. **Visual verification** at 640×480, 1280×960, 1920×1080, 4K
2. **Character size check**: An 8px virtual char should be:
   - 8 screen pixels at 640×480
   - 16 screen pixels at 1280×960 (2× scale)
   - 24 screen pixels at 1920×1080 (3× scale)
3. **Alignment test**: TTF, kfont, and legacy should render at same virtual size
4. **ui_scale cvar test**: Verify `ui_scale 2` doubles UI size correctly
