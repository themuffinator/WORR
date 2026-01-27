/*
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "client/font.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "client/client.h"
#include "common/common.h"
#include "common/files.h"
#include "common/utils.h"
#include "common/zone.h"
#include "shared/shared.h"

#if USE_SDL3_TTF
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#endif
#if USE_HARFBUZZ
#include <hb.h>
#include <hb-ot.h>
#endif

enum font_kind_t { FONT_LEGACY, FONT_KFONT, FONT_TTF };
enum font_metric_source_t { FONT_METRIC_NONE, FONT_METRIC_TTF, FONT_METRIC_HB };

struct font_atlas_page_t {
  qhandle_t handle = 0;
  int width = 0;
  int height = 0;
  int next_x = 0;
  int next_y = 0;
  int row_height = 0;
  byte *pixels = nullptr;
  bool dirty = false;
};

struct font_glyph_t {
  int page = -1;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  float bearing_x = 0.0f;
  float bearing_y = 0.0f;
  float offset_x = 0.0f;
  float offset_y = 0.0f;
  int advance = 0;
  bool oversized = false;
  bool metrics_valid = false;
  int metric_source = FONT_METRIC_NONE;
};

#if USE_HARFBUZZ
struct font_shaped_text_t {
  std::string text;
  std::vector<color_t> colors;
  bool use_colors = false;
};
#endif

struct kfont_glyph_t {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

struct font_ttf_t {
#if USE_SDL3_TTF
  TTF_Font *font = nullptr;
  void *data = nullptr;
  int data_size = 0;
  int pixel_height = 0;
  int ascent = 0;
  int line_skip = 0;
  int baseline = 0;
  int extent = 0;
  int rendered_ascent = 0;
  int rendered_descent = 0;
  int rendered_extent = 0;
  bool rendered_metrics_valid = false;
  int fixed_advance_units = 0;
  std::unordered_map<uint32_t, font_glyph_t> glyphs;
  std::list<uint32_t> lru;
  std::unordered_map<uint32_t, std::list<uint32_t>::iterator> lru_index;
  std::unordered_map<uint32_t, font_glyph_t> glyph_indices;
  std::list<uint32_t> index_lru;
  std::unordered_map<uint32_t, std::list<uint32_t>::iterator> index_lru_index;
  std::vector<font_atlas_page_t> pages;
#if USE_HARFBUZZ
  hb_blob_t *hb_blob = nullptr;
  hb_face_t *hb_face = nullptr;
  hb_font_t *hb_font = nullptr;
#endif
#endif
};

struct font_kfont_t {
  qhandle_t pic = 0;
  int tex_w = 0;
  int tex_h = 0;
  float inv_w = 0.0f;
  float inv_h = 0.0f;
  int line_height = 0;
  std::unordered_map<uint32_t, kfont_glyph_t> glyphs;
};

struct font_s {
  font_kind_t kind = FONT_LEGACY;
  int id = 0;
  int virtual_line_height = CONCHAR_HEIGHT;
  float pixel_scale = 1.0f;
  float unit_scale = 1.0f;
  int fixed_advance = 0;
  float letter_spacing = 0.0f;
  qhandle_t legacy_handle = 0;
  font_ttf_t ttf;
  font_kfont_t kfont;
  font_t *fallback_kfont = nullptr;
  bool registered = false;
};

static std::vector<font_t *> g_fonts;
static int g_font_seq = 0;
#if USE_SDL3_TTF
static bool g_ttf_ready = false;
#endif
static cvar_t *cl_debug_fonts = nullptr;
static cvar_t *cl_font_glyph_cache_size = nullptr;
static cvar_t *cl_font_ttf_hinting = nullptr;
static cvar_t *cl_font_ttf_supersample = nullptr;
static cvar_t *cl_font_ttf_metric_mode = nullptr;
static cvar_t *cl_font_debug_draw = nullptr;
static cvar_t *cl_font_debug_dump = nullptr;
static cvar_t *cl_font_debug_match = nullptr;
static cvar_t *cl_font_debug_metrics = nullptr;
static cvar_t *cl_font_ttf_hb_snap = nullptr;
static const float k_font_scale_boost = 1.5f;
static const int k_font_debug_draw_lines = 1;
static const int k_font_debug_draw_bounds = 2;
static const int k_font_debug_draw_advances = 4;
static const int k_font_debug_draw_origins = 8;
static const int k_font_debug_draw_outline = 16;

static float font_draw_scale(const font_t *font, int scale) {
  if (!font)
    return 1.0f;

  int draw_scale = scale > 0 ? scale : 1;
  float pixel_scale = font->pixel_scale > 0.0f ? font->pixel_scale : 1.0f;

  return (font->unit_scale * (float)draw_scale * k_font_scale_boost) /
         pixel_scale;
}

#if USE_SDL3_TTF
static const int k_atlas_size = 1024;
static const int k_atlas_padding = 1;
#endif

static const char *font_safe_str(const char *value) {
  return (value && *value) ? value : "<null>";
}

static bool font_debug_enabled(void) {
  if (!cl_debug_fonts)
    cl_debug_fonts = Cvar_Get("cl_debug_fonts", "1", 0);
  return cl_debug_fonts && cl_debug_fonts->integer;
}

static void font_debug_printf(const char *fmt, ...) {
  if (!font_debug_enabled())
    return;

  char msg[MAXPRINTMSG];
  va_list args;
  va_start(args, fmt);
  Q_vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  Com_Printf("%s", msg);
}

static int font_glyph_cache_limit(void) {
  if (!cl_font_glyph_cache_size)
    cl_font_glyph_cache_size = Cvar_Get("cl_font_glyph_cache_size", "2000", 0);
  return Cvar_ClampInteger(cl_font_glyph_cache_size, 0, 200000);
}

static int font_debug_draw_flags(void) {
  if (!cl_font_debug_draw)
    cl_font_debug_draw = Cvar_Get("cl_font_debug_draw", "0", 0);
  return cl_font_debug_draw ? cl_font_debug_draw->integer : 0;
}

static bool font_debug_dump_requested(void) {
  if (!cl_font_debug_dump)
    cl_font_debug_dump = Cvar_Get("cl_font_debug_dump", "0", 0);
  return cl_font_debug_dump && cl_font_debug_dump->integer > 0;
}

static bool font_debug_metrics_enabled(void) {
  if (!cl_font_debug_metrics)
    cl_font_debug_metrics = Cvar_Get("cl_font_debug_metrics", "0", 0);
  return cl_font_debug_metrics && cl_font_debug_metrics->integer > 0;
}

static void font_debug_dump_clear(void) {
  if (cl_font_debug_dump)
    Cvar_SetInteger(cl_font_debug_dump, 0, FROM_CODE);
}

static bool font_debug_match(const char *string) {
  if (!cl_font_debug_match)
    cl_font_debug_match = Cvar_Get("cl_font_debug_match", "", 0);
  if (!cl_font_debug_match || !cl_font_debug_match->string ||
      !*cl_font_debug_match->string)
    return true;
  if (!string || !*string)
    return false;
  return strstr(string, cl_font_debug_match->string) != nullptr;
}

static void font_debug_draw_line(int x, int y, int w, color_t color) {
  if (w <= 0)
    return;
  R_DrawFill32(x, y, w, 1, color);
}

static void font_debug_draw_box(int x, int y, int w, int h, color_t color) {
  if (w <= 0 || h <= 0)
    return;
  R_DrawFill32(x, y, w, 1, color);
  R_DrawFill32(x, y + h - 1, w, 1, color);
  R_DrawFill32(x, y, 1, h, color);
  R_DrawFill32(x + w - 1, y, 1, h, color);
}

static void font_debug_draw_point(int x, int y, color_t color) {
  R_DrawFill32(x, y, 1, 1, color);
}

#if USE_SDL3_TTF
static TTF_HintingFlags font_ttf_hinting_mode(void) {
  if (!cl_font_ttf_hinting)
    cl_font_ttf_hinting =
        Cvar_Get("cl_font_ttf_hinting", "1", CVAR_ARCHIVE);

  int mode = Cvar_ClampInteger(cl_font_ttf_hinting, 0, 3);
  switch (mode) {
  case 0:
    return TTF_HINTING_NONE;
  case 2:
    return TTF_HINTING_MONO;
  case 3:
    return TTF_HINTING_NORMAL;
  case 1:
  default:
    return TTF_HINTING_LIGHT;
  }
}

static int font_ttf_supersample(void) {
  if (!cl_font_ttf_supersample)
    cl_font_ttf_supersample =
        Cvar_Get("cl_font_ttf_supersample", "1", CVAR_ARCHIVE);
  return Cvar_ClampInteger(cl_font_ttf_supersample, 1, 4);
}

static int font_ttf_metric_mode(void) {
  if (!cl_font_ttf_metric_mode)
    cl_font_ttf_metric_mode =
        Cvar_Get("cl_font_ttf_metric_mode", "1", CVAR_ARCHIVE);
  return Cvar_ClampInteger(cl_font_ttf_metric_mode, 0, 1);
}

static bool font_ttf_hb_snap_positions(void) {
  if (!cl_font_ttf_hb_snap)
    cl_font_ttf_hb_snap = Cvar_Get("cl_font_ttf_hb_snap", "0", CVAR_ARCHIVE);
  return cl_font_ttf_hb_snap && cl_font_ttf_hb_snap->integer != 0;
}

static bool font_ttf_use_rendered_metrics(const font_t *font) {
  if (!font || font->kind != FONT_TTF)
    return false;
  return font_ttf_metric_mode() != 0 && font->ttf.rendered_metrics_valid;
}

static int font_ttf_metric_extent(const font_t *font) {
  if (!font || font->kind != FONT_TTF)
    return 0;
  if (font_ttf_use_rendered_metrics(font))
    return max(1, font->ttf.rendered_extent);
  return max(1, font->ttf.extent);
}

static int font_ttf_metric_baseline(const font_t *font) {
  if (!font || font->kind != FONT_TTF)
    return 0;
  if (font_ttf_use_rendered_metrics(font))
    return max(1, font->ttf.rendered_ascent);
  return max(1, font->ttf.baseline);
}

static float font_ttf_metric_scale(const font_t *font, int scale) {
  int extent = font_ttf_metric_extent(font);
  if (!font || font->kind != FONT_TTF || extent <= 0)
    return 1.0f;

  int draw_scale = scale > 0 ? scale : 1;
  return ((float)font->virtual_line_height * (float)draw_scale *
          k_font_scale_boost) /
         (float)extent;
}

static float font_ttf_letter_spacing(const font_t *font, int scale) {
  if (!font || font->kind != FONT_TTF || font->letter_spacing <= 0.0f)
    return 0.0f;

  int base_units = font_ttf_metric_extent(font);
  if (base_units <= 0)
    return 0.0f;

  float metric_scale = font_ttf_metric_scale(font, scale);
  return (float)base_units * metric_scale * font->letter_spacing;
}

static bool font_ttf_try_nominal_metrics(const font_t *font,
                                         uint32_t codepoint,
                                         uint32_t glyph_index, int *out_minx,
                                         int *out_maxx, int *out_miny,
                                         int *out_maxy, int *out_advance) {
  if (!font || font->kind != FONT_TTF || !font->ttf.font || !codepoint)
    return false;

#if USE_HARFBUZZ
  if (font->ttf.hb_font) {
    hb_codepoint_t nominal = 0;
    if (!hb_font_get_nominal_glyph(font->ttf.hb_font, codepoint, &nominal))
      return false;
    if (nominal != glyph_index)
      return false;
  }
#else
  (void)glyph_index;
#endif

  int minx = 0;
  int maxx = 0;
  int miny = 0;
  int maxy = 0;
  int advance = 0;
  if (!TTF_GetGlyphMetrics(font->ttf.font, codepoint, &minx, &maxx, &miny,
                           &maxy, &advance)) {
    return false;
  }

  if (out_minx)
    *out_minx = minx;
  if (out_maxx)
    *out_maxx = maxx;
  if (out_miny)
    *out_miny = miny;
  if (out_maxy)
    *out_maxy = maxy;
  if (out_advance)
    *out_advance = advance;
  return true;
}

static bool font_update_ttf_rendered_metrics_for_glyph(font_t *font,
                                                       const font_glyph_t &glyph) {
  if (!font || font->kind != FONT_TTF || glyph.h <= 0)
    return false;

  float top = glyph.bearing_y + glyph.offset_y;
  float bottom = top - (float)glyph.h;
  int ascent = max(1, (int)ceilf(top));
  int descent = max(0, (int)ceilf(-bottom));
  int extent = ascent + descent;
  if (extent <= 0)
    return false;

  if (!font->ttf.rendered_metrics_valid) {
    font->ttf.rendered_ascent = ascent;
    font->ttf.rendered_descent = descent;
    font->ttf.rendered_extent = extent;
    font->ttf.rendered_metrics_valid = true;
    return true;
  }

  bool changed = false;
  if (ascent > font->ttf.rendered_ascent) {
    font->ttf.rendered_ascent = ascent;
    changed = true;
  }
  if (descent > font->ttf.rendered_descent) {
    font->ttf.rendered_descent = descent;
    changed = true;
  }
  if (changed) {
    font->ttf.rendered_extent =
        font->ttf.rendered_ascent + font->ttf.rendered_descent;
  }

  return changed;
}
#endif

static const char *font_kind_name(font_kind_t kind) {
  switch (kind) {
  case FONT_LEGACY:
    return "legacy";
  case FONT_KFONT:
    return "kfont";
  case FONT_TTF:
    return "ttf";
  default:
    return "unknown";
  }
}

static inline bool font_ext_is(const char *path, const char *ext) {
  if (!path || !*path || !ext || !*ext)
    return false;
  const char *file_ext = COM_FileExtension(path);
  if (!file_ext || !*file_ext)
    return false;
  if (*file_ext == '.')
    ++file_ext;
  if (*ext == '.')
    ++ext;
  return Q_strcasecmp(file_ext, ext) == 0;
}

static color_t font_resolve_color(int flags, color_t color);
static void font_draw_legacy_glyph_at(const font_t *font, uint32_t codepoint,
                                      int x, int y, int scale, int flags,
                                      color_t color);
static bool font_draw_kfont_glyph_at(const font_t *font, uint32_t codepoint,
                                     int x, int y, int scale, int flags,
                                     color_t color);

#if USE_SDL3_TTF
static font_glyph_t *font_get_ttf_glyph(font_t *font, uint32_t codepoint,
                                        bool *out_created);
static font_glyph_t *font_get_ttf_glyph_index(font_t *font,
                                              uint32_t glyph_index,
                                              uint32_t nominal_codepoint,
                                              bool *out_created);
static uint32_t font_read_codepoint(const char **src, size_t *remaining);
static uint32_t font_codepoint_from_slice(const char *text, size_t length,
                                          bool *out_singleton);
static bool font_draw_ttf_glyph_index_cached(const font_t *font,
                                             const font_glyph_t *glyph,
                                             float draw_x, float draw_y,
                                             float glyph_scale, int scale,
                                             int flags, color_t color);

static void font_touch_ttf_glyph(font_t *font, uint32_t codepoint) {
  if (!font || font->kind != FONT_TTF)
    return;

  auto &lru = font->ttf.lru;
  auto &index = font->ttf.lru_index;
  auto it = index.find(codepoint);
  if (it != index.end())
    lru.erase(it->second);

  lru.push_front(codepoint);
  index[codepoint] = lru.begin();
}

static void font_remove_ttf_glyph(font_t *font, uint32_t codepoint) {
  if (!font)
    return;

  auto &lru = font->ttf.lru;
  auto &index = font->ttf.lru_index;
  auto it = index.find(codepoint);
  if (it != index.end()) {
    lru.erase(it->second);
    index.erase(it);
  }

  font->ttf.glyphs.erase(codepoint);
}

static void font_touch_ttf_glyph_index(font_t *font, uint32_t glyph_index) {
  if (!font || font->kind != FONT_TTF)
    return;

  auto &lru = font->ttf.index_lru;
  auto &index = font->ttf.index_lru_index;
  auto it = index.find(glyph_index);
  if (it != index.end())
    lru.erase(it->second);

  lru.push_front(glyph_index);
  index[glyph_index] = lru.begin();
}

static void font_remove_ttf_glyph_index(font_t *font, uint32_t glyph_index) {
  if (!font)
    return;

  auto &lru = font->ttf.index_lru;
  auto &index = font->ttf.index_lru_index;
  auto it = index.find(glyph_index);
  if (it != index.end()) {
    lru.erase(it->second);
    index.erase(it);
  }

  font->ttf.glyph_indices.erase(glyph_index);
}

static void font_trim_ttf_cache(font_t *font, size_t limit) {
  if (!font)
    return;

  auto &lru = font->ttf.lru;
  while (font->ttf.glyphs.size() > limit && !lru.empty()) {
    uint32_t victim = lru.back();
    font_remove_ttf_glyph(font, victim);
  }
}

static void font_trim_ttf_index_cache(font_t *font, size_t limit) {
  if (!font)
    return;

  auto &lru = font->ttf.index_lru;
  while (font->ttf.glyph_indices.size() > limit && !lru.empty()) {
    uint32_t victim = lru.back();
    font_remove_ttf_glyph_index(font, victim);
  }
}

static bool font_try_reuse_ttf_slot(font_t *font, int w, int h,
                                    font_glyph_t *out_slot) {
  if (!font || !out_slot || w <= 0 || h <= 0)
    return false;

  int limit = font_glyph_cache_limit();
  if (limit <= 0 || font->ttf.glyphs.size() < static_cast<size_t>(limit))
    return false;

  for (auto it = font->ttf.lru.rbegin(); it != font->ttf.lru.rend(); ++it) {
    auto glyph_it = font->ttf.glyphs.find(*it);
    if (glyph_it == font->ttf.glyphs.end())
      continue;

    const font_glyph_t &candidate = glyph_it->second;
    if (candidate.page < 0 || candidate.w < w || candidate.h < h)
      continue;

    *out_slot = candidate;
    font_remove_ttf_glyph(font, *it);
    return true;
  }

  return false;
}

static bool font_try_reuse_ttf_index_slot(font_t *font, int w, int h,
                                          font_glyph_t *out_slot) {
  if (!font || !out_slot || w <= 0 || h <= 0)
    return false;

  int limit = font_glyph_cache_limit();
  if (limit <= 0 ||
      font->ttf.glyph_indices.size() < static_cast<size_t>(limit))
    return false;

  for (auto it = font->ttf.index_lru.rbegin(); it != font->ttf.index_lru.rend();
       ++it) {
    auto glyph_it = font->ttf.glyph_indices.find(*it);
    if (glyph_it == font->ttf.glyph_indices.end())
      continue;

    const font_glyph_t &candidate = glyph_it->second;
    if (candidate.page < 0 || candidate.w < w || candidate.h < h)
      continue;

    *out_slot = candidate;
    font_remove_ttf_glyph_index(font, *it);
    return true;
  }

  return false;
}

static void font_clear_atlas_rect(font_atlas_page_t *page, int x, int y, int w,
                                  int h) {
  if (!page || !page->pixels || w <= 0 || h <= 0)
    return;

  int stride = page->width * 4;
  byte *dst = page->pixels + ((y * page->width) + x) * 4;
  for (int row = 0; row < h; ++row) {
    byte *row_ptr = dst + row * stride;
    for (int col = 0; col < w; ++col) {
      byte *px = row_ptr + col * 4;
      px[0] = 255;
      px[1] = 255;
      px[2] = 255;
      px[3] = 0;
    }
  }
}

#if USE_SDL3_TTF
static void font_copy_glyph_pixels(byte *dst, int dst_stride, const byte *src,
                                   int src_stride, int w, int h,
                                   bool alpha_only) {
  if (!dst || !src || w <= 0 || h <= 0)
    return;

  if (!alpha_only) {
    for (int row = 0; row < h; ++row)
      memcpy(dst + row * dst_stride, src + row * src_stride, w * 4);
    return;
  }

  for (int row = 0; row < h; ++row) {
    byte *dst_row = dst + row * dst_stride;
    const byte *src_row = src + row * src_stride;
    for (int col = 0; col < w; ++col) {
      const byte *s = src_row + col * 4;
      byte *d = dst_row + col * 4;
      d[0] = 255;
      d[1] = 255;
      d[2] = 255;
      d[3] = s[3];
    }
  }
}
#endif

static void font_mark_page_dirty(font_atlas_page_t *page) {
  if (page)
    page->dirty = true;
}

static bool font_flush_ttf_page(font_atlas_page_t *page) {
  if (!page || !page->dirty || !page->handle || !page->pixels)
    return false;

  if (!R_UpdateImageRGBA(page->handle, page->width, page->height, page->pixels))
    return false;

  page->dirty = false;
  return true;
}

static void font_flush_ttf_pages(font_t *font) {
  if (!font || font->kind != FONT_TTF)
    return;

  for (auto &page : font->ttf.pages) {
    if (page.dirty)
      font_flush_ttf_page(&page);
  }
}

static float font_get_ttf_kerning(const font_t *font, uint32_t prev,
                                  uint32_t current, int scale) {
  if (!font || font->kind != FONT_TTF || !font->ttf.font)
    return 0.0f;
  if (!prev || !current || font->fixed_advance > 0)
    return 0.0f;

  int kerning = 0;
  if (!TTF_GetGlyphKerning(font->ttf.font, prev, current, &kerning))
    return 0.0f;

  float glyph_scale = font_ttf_metric_scale(font, scale);
  return (float)kerning * glyph_scale;
}

static bool font_get_ttf_advance_units(const font_t *font, uint32_t codepoint,
                                       int *out_advance) {
  if (!font || font->kind != FONT_TTF || !font->ttf.font || !out_advance)
    return false;

  auto it = font->ttf.glyphs.find(codepoint);
  if (it != font->ttf.glyphs.end()) {
    if (it->second.oversized)
      return false;
    *out_advance = it->second.advance;
    return true;
  }

  if (!TTF_FontHasGlyph(font->ttf.font, codepoint))
    return false;

  int minx = 0;
  int maxx = 0;
  int miny = 0;
  int maxy = 0;
  int advance = 0;
  if (!TTF_GetGlyphMetrics(font->ttf.font, codepoint, &minx, &maxx, &miny,
                           &maxy, &advance))
    return false;

  *out_advance = advance;
  return true;
}

static float font_get_ttf_advance_scaled(const font_t *font, int advance_units,
                                         int scale) {
  if (!font || font->kind != FONT_TTF)
    return 0.0f;

  if (font->fixed_advance > 0) {
    float glyph_scale = font_ttf_metric_scale(font, scale);
    if (font->ttf.fixed_advance_units > 0) {
      float advance = (float)font->ttf.fixed_advance_units * glyph_scale;
      return advance < 0.0f ? 0.0f : advance;
    }
    int draw_scale = scale > 0 ? scale : 1;
    return (float)font->fixed_advance * (float)draw_scale * k_font_scale_boost;
  }

  float glyph_scale = font_ttf_metric_scale(font, scale);
  float advance = (float)advance_units * glyph_scale;
  return advance < 0.0f ? 0.0f : advance;
}

static void font_preload_ascii(font_t *font) {
  if (!font || font->kind != FONT_TTF)
    return;

#if USE_HARFBUZZ
  if (font->ttf.hb_font) {
    for (uint32_t ch = 32; ch < 127; ++ch) {
      hb_codepoint_t glyph_index = 0;
      if (hb_font_get_nominal_glyph(font->ttf.hb_font, ch, &glyph_index))
        font_get_ttf_glyph_index(font, glyph_index, ch, nullptr);
    }
    return;
  }
#endif

  for (uint32_t ch = 32; ch < 127; ++ch)
    font_get_ttf_glyph(font, ch, nullptr);
}
#endif

#if USE_HARFBUZZ
static void font_build_shaped_text(font_shaped_text_t *out, const char *string,
                                   size_t max_chars, int flags,
                                   color_t base_color) {
  if (!out) {
    return;
  }

  out->text.clear();
  out->colors.clear();
  out->use_colors = false;

  if (!string || !*string || !max_chars)
    return;

  (void)flags;

  bool use_color_codes = Com_HasColorEscape(string, max_chars);
  out->use_colors = use_color_codes;

  size_t remaining = max_chars;
  const char *s = string;
  color_t current = base_color;
  while (remaining && *s) {
    if (use_color_codes) {
      color_t parsed;
      if (Com_ParseColorEscape(&s, &remaining, base_color, &parsed)) {
        current = parsed;
        continue;
      }
    }

    const char *prev = s;
    uint32_t codepoint = font_read_codepoint(&s, &remaining);
    if (!codepoint)
      break;

    size_t consumed = static_cast<size_t>(s - prev);
    if (!consumed)
      break;

    out->text.append(prev, consumed);
    if (use_color_codes) {
      for (size_t i = 0; i < consumed; ++i)
        out->colors.push_back(current);
    }
  }

  if (use_color_codes && out->colors.size() < out->text.size())
    out->colors.resize(out->text.size(), current);
}

#if USE_SDL3_TTF
static void font_trim_shaped_text(font_shaped_text_t *shaped, int flags) {
  if (!shaped)
    return;
  if (flags & UI_MULTILINE)
    return;
  size_t newline = shaped->text.find('\n');
  if (newline == std::string::npos)
    return;
  shaped->text.resize(newline);
  if (shaped->use_colors && shaped->colors.size() > newline)
    shaped->colors.resize(newline);
}

static void font_collect_line_breaks(const std::string &text,
                                     std::vector<size_t> &breaks) {
  breaks.clear();
  size_t pos = text.find('\n');
  while (pos != std::string::npos) {
    breaks.push_back(pos);
    pos = text.find('\n', pos + 1);
  }
}

#if USE_HARFBUZZ
static hb_buffer_t *font_shape_harfbuzz_line(const font_t *font,
                                             const char *text, size_t length) {
  if (!font || font->kind != FONT_TTF || !font->ttf.hb_font || !text || !length)
    return nullptr;

  hb_buffer_t *buffer = hb_buffer_create();
  if (!buffer)
    return nullptr;

  int len = length > static_cast<size_t>(INT_MAX)
                ? INT_MAX
                : static_cast<int>(length);
  hb_buffer_set_cluster_level(buffer,
                              HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
  hb_buffer_add_utf8(buffer, text, len, 0, len);
  hb_buffer_guess_segment_properties(buffer);
  hb_shape(font->ttf.hb_font, buffer, nullptr, 0);
  return buffer;
}
#endif

static bool font_draw_ttf_glyph_region_cached(const font_t *font,
                                              const font_glyph_t *glyph,
                                              float draw_x, float draw_y,
                                              float draw_w, float draw_h,
                                              int scale, int flags,
                                              color_t color,
                                              const SDL_Rect *src) {
  if (!font || font->kind != FONT_TTF || !glyph)
    return false;
  if (glyph->oversized)
    return false;
  if (glyph->page < 0 || glyph->page >= (int)font->ttf.pages.size())
    return false;

  const font_atlas_page_t &page = font->ttf.pages[glyph->page];
  if (!page.handle)
    return false;

  int src_x = glyph->x;
  int src_y = glyph->y;
  int src_w = glyph->w;
  int src_h = glyph->h;
  if (src) {
    src_x += src->x;
    src_y += src->y;
    src_w = src->w;
    src_h = src->h;
  }

  if (src_w <= 0 || src_h <= 0)
    return false;

  int w = max(1, (int)ceilf(draw_w));
  int h = max(1, (int)ceilf(draw_h));
  int dx = (int)floorf(draw_x);
  int dy = (int)floorf(draw_y);

  float s1 = (float)src_x / (float)page.width;
  float t1 = (float)src_y / (float)page.height;
  float s2 = (float)(src_x + src_w) / (float)page.width;
  float t2 = (float)(src_y + src_h) / (float)page.height;

  if (flags & UI_DROPSHADOW) {
    int shadow = max(1, scale);
    color_t black = COLOR_A(color.a);
    R_DrawStretchSubPic(dx + shadow, dy + shadow, w, h, s1, t1, s2, t2, black,
                        page.handle);
  }

  R_DrawStretchSubPic(dx, dy, w, h, s1, t1, s2, t2, color, page.handle);
  return true;
}
#endif

static int font_draw_string_hb(font_t *font, int x, int y, int scale, int flags,
                               size_t max_chars, const char *string,
                               color_t color) {
#if !USE_SDL3_TTF || !USE_HARFBUZZ
  return x;
#else
  if (!font || font->kind != FONT_TTF || !font->ttf.hb_font || !string ||
      !*string)
    return x;

  int draw_scale = scale > 0 ? scale : 1;
  float line_height = (float)Font_LineHeight(font, draw_scale);
  float pixel_spacing =
      font->pixel_scale > 0.0f ? (1.0f / font->pixel_scale) : 0.0f;
  float line_step = line_height + pixel_spacing;
  float hb_scale = font_ttf_metric_scale(font, draw_scale);
  float ttf_spacing = font_ttf_letter_spacing(font, draw_scale);
  bool snap_positions = font_ttf_hb_snap_positions();
  float baseline_offset = (float)font_ttf_metric_baseline(font) * hb_scale;
  bool debug_match = font_debug_match(string);
  int debug_flags = debug_match ? font_debug_draw_flags() : 0;
  bool dump_metrics = debug_match && font_debug_dump_requested();

  if (dump_metrics) {
    Com_Printf("FontDebug(HB): font_id=%d scale=%d line_height=%.2f "
               "baseline=%d extent=%d rendered=%d/%d metric_mode=%d snap=%d\n",
               font->id, draw_scale, line_height,
               font->ttf.baseline, font->ttf.extent,
               font->ttf.rendered_ascent, font->ttf.rendered_extent,
               font_ttf_metric_mode(), snap_positions ? 1 : 0);
  }

  int draw_flags = flags;
  bool use_color_codes = Com_HasColorEscape(string, max_chars);
  color_t base_color = color;
  if (use_color_codes) {
    base_color = font_resolve_color(flags, color);
    draw_flags &= ~(UI_ALTCOLOR | UI_XORCOLOR);
  }

  color_t draw_color = font_resolve_color(flags, color);
  if (use_color_codes)
    draw_color = base_color;

  font_shaped_text_t shaped;
  font_build_shaped_text(&shaped, string, max_chars, flags, base_color);
  font_trim_shaped_text(&shaped, flags);
  if (shaped.text.empty())
    return x;

  std::vector<size_t> line_breaks;
  font_collect_line_breaks(shaped.text, line_breaks);

  size_t line_start = 0;
  size_t break_index = 0;
  size_t next_break =
      line_breaks.empty() ? std::string::npos : line_breaks[break_index];
  int line_index = 0;
  float last_x = (float)x;

  while (line_start <= shaped.text.size()) {
    size_t line_end =
        next_break == std::string::npos ? shaped.text.size() : next_break;
    size_t line_len = line_end > line_start ? line_end - line_start : 0;

    float baseline_y =
        (float)y + baseline_offset + (float)line_index * line_step;
    float line_y = (float)y + (float)line_index * line_step;

    float pen_x = (float)x;
    last_x = pen_x;

    if (line_len > 0) {
      hb_buffer_t *buffer = font_shape_harfbuzz_line(
          font, shaped.text.data() + line_start, line_len);
      if (buffer) {
        unsigned int glyph_count = 0;
        hb_glyph_info_t *infos =
            hb_buffer_get_glyph_infos(buffer, &glyph_count);
        hb_glyph_position_t *positions =
            hb_buffer_get_glyph_positions(buffer, &glyph_count);

        std::vector<uint32_t> nominal_codepoints;
        nominal_codepoints.resize(glyph_count, 0);
        for (unsigned int i = 0; i < glyph_count; ++i) {
          size_t cluster = infos[i].cluster;
          size_t next_cluster =
              (i + 1 < glyph_count) ? infos[i + 1].cluster : line_len;
          if (cluster < line_len) {
            if (next_cluster > line_len)
              next_cluster = line_len;
            size_t slice_len =
                next_cluster > cluster ? (next_cluster - cluster) : 0;
            bool singleton = false;
            uint32_t codepoint = font_codepoint_from_slice(
                shaped.text.data() + line_start + cluster, slice_len,
                &singleton);
            if (singleton)
              nominal_codepoints[i] = codepoint;
          }
          font_get_ttf_glyph_index(font, infos[i].codepoint,
                                   nominal_codepoints[i], nullptr);
        }
        font_flush_ttf_pages(font);

        bool first_in_line = true;
        for (unsigned int i = 0; i < glyph_count; ++i) {
          float advance = (float)positions[i].x_advance / 64.0f;
          float advance_px = advance * hb_scale;
          if (snap_positions)
            advance_px = (float)Q_rint(advance_px);
          if (!first_in_line && ttf_spacing != 0.0f && advance > 0.0f)
            pen_x += ttf_spacing;
          if (snap_positions)
            pen_x = (float)Q_rint(pen_x);
          first_in_line = false;

          size_t color_index = line_start + (size_t)infos[i].cluster;
          color_t glyph_color = draw_color;
          if (shaped.use_colors && color_index < shaped.colors.size())
            glyph_color = shaped.colors[color_index];

          uint32_t nominal_codepoint =
              i < nominal_codepoints.size() ? nominal_codepoints[i] : 0;
          font_glyph_t *cached = font_get_ttf_glyph_index(
              font, infos[i].codepoint, nominal_codepoint, nullptr);
          if (cached && cached->oversized)
            cached = nullptr;
          if (cached && cached->page >= 0 &&
              cached->page < (int)font->ttf.pages.size()) {
            font_atlas_page_t &page = font->ttf.pages[cached->page];
            if (page.dirty)
              font_flush_ttf_page(&page);
          }

          float bearing_x = 0.0f;
          float bearing_y = 0.0f;
          float offset_x = 0.0f;
          float offset_y = 0.0f;
          if (cached && cached->metrics_valid) {
            bearing_x = cached->bearing_x;
            bearing_y = cached->bearing_y;
            offset_x = cached->offset_x;
            offset_y = cached->offset_y;
          } else {
            hb_glyph_extents_t extents;
            if (hb_font_get_glyph_extents(font->ttf.hb_font,
                                          infos[i].codepoint, &extents)) {
              bearing_x = (float)extents.x_bearing / 64.0f;
              bearing_y = (float)extents.y_bearing / 64.0f;
              float hb_height = (float)extents.height / 64.0f;
              float hb_miny = bearing_y + hb_height;
              if (cached && cached->h > 0)
                offset_y = (hb_miny + (float)cached->h) - bearing_y;
            }
          }

          float x_offset = (float)positions[i].x_offset / 64.0f;
          float y_offset = (float)positions[i].y_offset / 64.0f;
          if (snap_positions) {
            bearing_x = (float)Q_rint(bearing_x);
            bearing_y = (float)Q_rint(bearing_y);
            offset_x = (float)Q_rint(offset_x);
            offset_y = (float)Q_rint(offset_y);
            x_offset = (float)Q_rint(x_offset);
            y_offset = (float)Q_rint(y_offset);
          }
          float draw_x =
              pen_x + (x_offset + bearing_x + offset_x) * hb_scale;
          float draw_y =
              baseline_y - (y_offset + bearing_y + offset_y) * hb_scale;

          bool drawn = false;
          if (cached) {
            float draw_w = (float)cached->w * hb_scale;
            float draw_h = (float)cached->h * hb_scale;
            drawn = font_draw_ttf_glyph_region_cached(
                font, cached, draw_x, draw_y, draw_w, draw_h, draw_scale,
                draw_flags, glyph_color, nullptr);
            if (debug_flags & k_font_debug_draw_bounds) {
              int bx = (int)floorf(draw_x);
              int by = (int)floorf(draw_y);
              int bw = max(1, (int)ceilf(draw_w));
              int bh = max(1, (int)ceilf(draw_h));
              font_debug_draw_box(bx, by, bw, bh,
                                  COLOR_RGBA(255, 0, 255, 160));
            }
          }

          if (!drawn) {
            uint32_t fallback_cp = '?';
            if (color_index < shaped.text.size()) {
              const char *fallback_ptr = shaped.text.data() + color_index;
              size_t fallback_remaining = shaped.text.size() - color_index;
              fallback_cp =
                  font_read_codepoint(&fallback_ptr, &fallback_remaining);
            }
            if (!fallback_cp)
              fallback_cp = '?';

            if (font->fallback_kfont) {
              if (font->fallback_kfont->kind == FONT_KFONT) {
                font_draw_kfont_glyph_at(font->fallback_kfont, fallback_cp,
                                         Q_rint(draw_x), Q_rint(line_y),
                                         draw_scale, draw_flags, glyph_color);
              } else if (font->fallback_kfont->kind == FONT_LEGACY) {
                font_draw_legacy_glyph_at(font->fallback_kfont, fallback_cp,
                                          Q_rint(draw_x), Q_rint(line_y),
                                          draw_scale, draw_flags, glyph_color);
              }
            } else if (font->legacy_handle) {
              font_draw_legacy_glyph_at(font, fallback_cp, Q_rint(draw_x),
                                        Q_rint(line_y), draw_scale, draw_flags,
                                        glyph_color);
            }
          }

          if (debug_flags & k_font_debug_draw_advances) {
            int ax = (int)floorf(pen_x);
            int ay = (int)floorf(line_y);
            int ah = max(1, (int)ceilf(line_height));
            R_DrawFill32(ax, ay, 1, ah, COLOR_RGBA(255, 255, 0, 160));
          }
          if (debug_flags & k_font_debug_draw_origins) {
            font_debug_draw_point((int)floorf(pen_x),
                                  (int)floorf(baseline_y),
                                  COLOR_RGBA(0, 255, 255, 200));
          }
          if (dump_metrics) {
            const char *src = "none";
            if (cached) {
              if (cached->metric_source == FONT_METRIC_TTF)
                src = "ttf";
              else if (cached->metric_source == FONT_METRIC_HB)
                src = "hb";
            }
            Com_Printf(
                "FontDebug(HB): i=%u glyph=%u adv=%.2f adv_px=%.2f pen=%.2f "
                "bearing=(%.2f,%.2f) offset=(%.2f,%.2f) bmp=%dx%d src=%s "
                "draw=(%.2f,%.2f)\n",
                i, infos[i].codepoint, advance, advance_px, pen_x, bearing_x,
                bearing_y, offset_x, offset_y, cached ? cached->w : 0,
                cached ? cached->h : 0, src, draw_x, draw_y);
            if (font_debug_metrics_enabled()) {
              if (nominal_codepoint) {
                int minx = 0;
                int maxx = 0;
                int miny = 0;
                int maxy = 0;
                int adv = 0;
                if (font_ttf_try_nominal_metrics(
                        font, nominal_codepoint, infos[i].codepoint, &minx,
                        &maxx, &miny, &maxy, &adv)) {
                  Com_Printf("FontDebug(HB): ttf cp=%u minx=%d maxx=%d "
                             "miny=%d maxy=%d adv=%d\n",
                             nominal_codepoint, minx, maxx, miny, maxy, adv);
                }
              }
              hb_glyph_extents_t extents;
              if (hb_font_get_glyph_extents(font->ttf.hb_font,
                                            infos[i].codepoint, &extents)) {
                Com_Printf(
                    "FontDebug(HB): hb extents=(%.2f,%.2f,%.2f,%.2f) pos=(%.2f,"
                    "%.2f) off=(%.2f,%.2f)\n",
                    (float)extents.x_bearing / 64.0f,
                    (float)extents.y_bearing / 64.0f,
                    (float)extents.width / 64.0f,
                    (float)extents.height / 64.0f,
                    (float)positions[i].x_advance / 64.0f,
                    (float)positions[i].y_advance / 64.0f,
                    (float)positions[i].x_offset / 64.0f,
                    (float)positions[i].y_offset / 64.0f);
              }
            }
          }

          pen_x += advance_px;
          last_x = pen_x;
        }

        hb_buffer_destroy(buffer);
      }
    }

    if (debug_flags & k_font_debug_draw_lines) {
      int line_width = max(1, (int)ceilf(last_x - (float)x));
      int top_y = (int)floorf(line_y);
      int base_y = (int)floorf(baseline_y);
      int bottom_y = top_y + max(1, (int)ceilf(line_height)) - 1;
      font_debug_draw_line(x, top_y, line_width,
                           COLOR_RGBA(0, 128, 255, 180));
      font_debug_draw_line(x, base_y, line_width,
                           COLOR_RGBA(0, 255, 0, 180));
      font_debug_draw_line(x, bottom_y, line_width,
                           COLOR_RGBA(255, 0, 0, 180));
    }
    if (debug_flags & k_font_debug_draw_outline) {
      int line_width = max(1, (int)ceilf(last_x - (float)x));
      float outline_top =
          baseline_y - (float)font->ttf.baseline * hb_scale;
      float outline_bottom = outline_top +
                             (float)font->ttf.extent * hb_scale;
      int top_y = (int)floorf(outline_top);
      int bottom_y = (int)floorf(outline_bottom);
      font_debug_draw_line(x, top_y, line_width,
                           COLOR_RGBA(128, 0, 255, 180));
      font_debug_draw_line(x, bottom_y, line_width,
                           COLOR_RGBA(255, 128, 0, 180));
    }

    if (next_break == std::string::npos)
      break;

    line_start = line_end + 1;
    ++line_index;
    ++break_index;
    next_break = break_index < line_breaks.size()
                     ? line_breaks[break_index]
                     : std::string::npos;
  }

  if (dump_metrics)
    font_debug_dump_clear();

  return Q_rint(last_x);
#endif
}

static int font_measure_string_hb(const font_t *font, int scale, int flags,
                                  size_t max_chars, const char *string,
                                  int *out_height) {
  if (out_height)
    *out_height = 0;
#if !USE_SDL3_TTF || !USE_HARFBUZZ
  return 0;
#else
  if (!font || font->kind != FONT_TTF || !font->ttf.hb_font || !string ||
      !*string)
    return 0;

  int draw_scale = scale > 0 ? scale : 1;
  float line_height = (float)Font_LineHeight(font, draw_scale);
  float pixel_spacing =
      font->pixel_scale > 0.0f ? (1.0f / font->pixel_scale) : 0.0f;
  float line_step = line_height + pixel_spacing;
  float hb_scale = font_ttf_metric_scale(font, draw_scale);
  float ttf_spacing = font_ttf_letter_spacing(font, draw_scale);
  bool snap_positions = font_ttf_hb_snap_positions();

  font_shaped_text_t shaped;
  color_t base_color = font_resolve_color(flags, COLOR_WHITE);
  font_build_shaped_text(&shaped, string, max_chars, flags, base_color);
  font_trim_shaped_text(&shaped, flags);
  if (shaped.text.empty())
    return 0;

  std::vector<size_t> line_breaks;
  font_collect_line_breaks(shaped.text, line_breaks);

  size_t line_start = 0;
  size_t break_index = 0;
  size_t next_break =
      line_breaks.empty() ? std::string::npos : line_breaks[break_index];
  float max_width = 0.0f;

  while (line_start <= shaped.text.size()) {
    size_t line_end =
        next_break == std::string::npos ? shaped.text.size() : next_break;
    size_t line_len = line_end > line_start ? line_end - line_start : 0;
    float width = 0.0f;

    if (line_len > 0) {
      hb_buffer_t *buffer = font_shape_harfbuzz_line(
          font, shaped.text.data() + line_start, line_len);
      if (buffer) {
        unsigned int glyph_count = 0;
        hb_glyph_position_t *positions =
            hb_buffer_get_glyph_positions(buffer, &glyph_count);

        bool first_in_line = true;
        for (unsigned int i = 0; i < glyph_count; ++i) {
          float advance = (float)positions[i].x_advance / 64.0f;
          float advance_px = advance * hb_scale;
          if (snap_positions)
            advance_px = (float)Q_rint(advance_px);
          if (!first_in_line && ttf_spacing != 0.0f && advance > 0.0f)
            width += ttf_spacing;
          if (snap_positions)
            width = (float)Q_rint(width);
          first_in_line = false;
          width += advance_px;
        }

        hb_buffer_destroy(buffer);
      }
    }

    if (width > max_width)
      max_width = width;

    if (next_break == std::string::npos)
      break;

    line_start = line_end + 1;
    ++break_index;
    next_break = break_index < line_breaks.size()
                     ? line_breaks[break_index]
                     : std::string::npos;
  }

  if (out_height) {
    int lines = 1;
    if (!line_breaks.empty())
      lines = (int)line_breaks.size() + 1;
    *out_height = Q_rint((float)lines * line_step);
  }

  return max(0, Q_rint(max_width));
#endif
}
#endif

static uint32_t font_read_codepoint(const char **src, size_t *remaining) {
  const unsigned char *text = reinterpret_cast<const unsigned char *>(*src);

  if (!remaining || !*remaining || !*text)
    return 0;

  uint8_t first = text[0];
  if (first < 0x80) {
    *src = reinterpret_cast<const char *>(text + 1);
    (*remaining)--;
    return first;
  }

  int bytes = 7 - Q_log2(first ^ 255);
  if (bytes < 2 || bytes > 4 || static_cast<size_t>(bytes) > *remaining) {
    *src = reinterpret_cast<const char *>(text + 1);
    (*remaining)--;
    return first;
  }

  uint32_t code = first & (127 >> bytes);
  for (int i = 1; i < bytes; ++i) {
    uint8_t cont = text[i];
    if ((cont & 0xC0) != 0x80) {
      *src = reinterpret_cast<const char *>(text + 1);
      (*remaining)--;
      return first;
    }
    code = (code << 6) | (cont & 63);
  }

  *src = reinterpret_cast<const char *>(text + bytes);
  *remaining -= bytes;

  if (code > UNICODE_MAX)
    return first;
  if (code >= 0xD800 && code <= 0xDFFF)
    return first;
  if ((bytes == 2 && code < 0x80) || (bytes == 3 && code < 0x800) ||
      (bytes == 4 && code < 0x10000))
    return first;

  return code;
}

static uint32_t font_codepoint_from_slice(const char *text, size_t length,
                                          bool *out_singleton) {
  if (out_singleton)
    *out_singleton = false;
  if (!text || length == 0)
    return 0;

  const char *ptr = text;
  size_t remaining = length;
  uint32_t codepoint = font_read_codepoint(&ptr, &remaining);
  if (out_singleton)
    *out_singleton = (codepoint != 0 && remaining == 0);
  return codepoint;
}

static color_t font_resolve_color(int flags, color_t color) {
  if (flags & (UI_ALTCOLOR | UI_XORCOLOR)) {
    color_t alt = COLOR_RGB(255, 255, 0);
    alt.a = color.a;
    return alt;
  }
  return color;
}

static bool font_load_kfont(font_t *font, const char *filename) {
  if (!font || !filename || !*filename)
    return false;

  font_debug_printf("Font: loading kfont \"%s\"\n", font_safe_str(filename));

  char *buffer = nullptr;
  if (FS_LoadFile(filename, (void **)&buffer) < 0) {
    font_debug_printf("Font: kfont load failed \"%s\": %s\n",
                      font_safe_str(filename), Com_GetLastError());
    return false;
  }

  const char *data = buffer;
  char texture_path[MAX_QPATH];
  texture_path[0] = '\0';
  while (true) {
    const char *token = COM_Parse(&data);
    if (!*token)
      break;

    if (!strcmp(token, "texture")) {
      token = COM_Parse(&data);
      Q_strlcpy(texture_path, token, sizeof(texture_path));
      font->kfont.pic = R_RegisterFont(va("/%s", token));
      font_debug_printf("Font: kfont texture \"%s\" handle=%d\n",
                        font_safe_str(texture_path), font->kfont.pic);
    } else if (!strcmp(token, "unicode")) {
    } else if (!strcmp(token, "mapchar")) {
      COM_Parse(&data); // "{"

      while (true) {
        token = COM_Parse(&data);
        if (!strcmp(token, "}"))
          break;

        uint32_t codepoint = strtoul(token, NULL, 10);
        uint32_t x = strtoul(COM_Parse(&data), NULL, 10);
        uint32_t y = strtoul(COM_Parse(&data), NULL, 10);
        uint32_t w = strtoul(COM_Parse(&data), NULL, 10);
        uint32_t h = strtoul(COM_Parse(&data), NULL, 10);
        COM_Parse(&data); // skip

        kfont_glyph_t glyph;
        glyph.x = (int)x;
        glyph.y = (int)y;
        glyph.w = (int)w;
        glyph.h = (int)h;
        font->kfont.glyphs[codepoint] = glyph;
        font->kfont.line_height = max(font->kfont.line_height, (int)h);
      }
    }
  }

  FS_FreeFile(buffer);

  if (!font->kfont.pic)
    return false;

  R_GetPicSize(&font->kfont.tex_w, &font->kfont.tex_h, font->kfont.pic);
  if (font->kfont.tex_w <= 0 || font->kfont.tex_h <= 0)
    return false;

  font->kfont.inv_w = 1.0f / (float)font->kfont.tex_w;
  font->kfont.inv_h = 1.0f / (float)font->kfont.tex_h;
  if (font->kfont.line_height <= 0)
    font->kfont.line_height = CONCHAR_HEIGHT;

  font_debug_printf(
      "Font: kfont loaded \"%s\" glyphs=%zu line_height=%d texture=\"%s\"\n",
      font_safe_str(filename), font->kfont.glyphs.size(),
      font->kfont.line_height, font_safe_str(texture_path));

  return true;
}

static qhandle_t font_register_legacy(const char *path, const char *fallback) {
  font_debug_printf("Font: legacy register path=\"%s\" fallback=\"%s\"\n",
                    font_safe_str(path), font_safe_str(fallback));
  qhandle_t handle = 0;
  if (path && *path) {
    handle = R_RegisterFont(path);
    font_debug_printf("Font: legacy handle for \"%s\" = %d\n",
                      font_safe_str(path), handle);
  }
  if (!handle && fallback && *fallback &&
      (!path || Q_strcasecmp(path, fallback))) {
    handle = R_RegisterFont(fallback);
    font_debug_printf("Font: legacy fallback handle for \"%s\" = %d\n",
                      font_safe_str(fallback), handle);
  }
  return handle;
}

#if USE_SDL3_TTF
static font_atlas_page_t font_create_atlas_page(int font_id, int page_index) {
  font_atlas_page_t page;
  page.width = k_atlas_size;
  page.height = k_atlas_size;
  page.pixels = (byte *)Z_Malloc(page.width * page.height * 4);
  font_clear_atlas_rect(&page, 0, 0, page.width, page.height);

  byte *upload = (byte *)Z_Malloc(page.width * page.height * 4);
  memcpy(upload, page.pixels, page.width * page.height * 4);

  page.handle = R_RegisterRawImage(
      va("fonts/_ttf_%d_%d", font_id, page_index), page.width, page.height,
      upload, IT_FONT,
      static_cast<imageflags_t>(IF_PERMANENT | IF_TRANSPARENT |
                                IF_NO_COLOR_ADJUST));
  if (!page.handle) {
    Z_Free(upload);
    font_debug_printf(
        "Font: atlas page registration failed for font=%d page=%d\n", font_id,
        page_index);
  }
  page.next_x = k_atlas_padding;
  page.next_y = k_atlas_padding;
  page.row_height = 0;
  page.dirty = false;
  return page;
}

static void font_compute_ttf_metrics(font_t *font) {
  if (!font || !font->ttf.font)
    return;

  int ascent = font->ttf.ascent > 0 ? font->ttf.ascent
                                    : TTF_GetFontAscent(font->ttf.font);
  int descent = TTF_GetFontDescent(font->ttf.font);
  int extent = ascent + abs(descent);
  if (extent <= 0)
    extent = max(font->ttf.line_skip, 1);

  font->ttf.baseline = ascent > 0 ? ascent : max(font->ttf.line_skip, 1);
  font->ttf.extent = extent;
  font->ttf.rendered_ascent = 0;
  font->ttf.rendered_descent = 0;
  font->ttf.rendered_extent = 0;
  font->ttf.rendered_metrics_valid = false;
}

static void font_compute_ttf_rendered_metrics(font_t *font) {
  if (!font || font->kind != FONT_TTF)
    return;

  float max_top = 0.0f;
  float max_bottom = 0.0f;
  int samples = 0;

#if USE_HARFBUZZ
  const bool use_indices = font->ttf.hb_font != nullptr;
#else
  const bool use_indices = false;
#endif

  if (use_indices) {
    for (const auto &entry : font->ttf.glyph_indices) {
      const font_glyph_t &glyph = entry.second;
      if (glyph.h <= 0 || !glyph.metrics_valid)
        continue;
      float top = glyph.bearing_y + glyph.offset_y;
      float bottom = top - (float)glyph.h;
      max_top = max(max_top, top);
      max_bottom = max(max_bottom, -bottom);
      ++samples;
    }
  } else {
    for (const auto &entry : font->ttf.glyphs) {
      const font_glyph_t &glyph = entry.second;
      if (glyph.h <= 0 || !glyph.metrics_valid)
        continue;
      float top = glyph.bearing_y + glyph.offset_y;
      float bottom = top - (float)glyph.h;
      max_top = max(max_top, top);
      max_bottom = max(max_bottom, -bottom);
      ++samples;
    }
  }

  if (samples <= 0) {
    font->ttf.rendered_metrics_valid = false;
    return;
  }

  int ascent = max(1, (int)ceilf(max_top));
  int descent = max(0, (int)ceilf(max_bottom));
  int extent = ascent + descent;
  if (extent <= 0) {
    font->ttf.rendered_metrics_valid = false;
    return;
  }

  font->ttf.rendered_ascent = ascent;
  font->ttf.rendered_descent = descent;
  font->ttf.rendered_extent = extent;
  font->ttf.rendered_metrics_valid = true;
}

static void font_update_ttf_fixed_advance(font_t *font) {
  if (!font || font->kind != FONT_TTF || font->fixed_advance <= 0)
    return;
  if (font->ttf.fixed_advance_units <= 0)
    return;

  int extent = font_ttf_metric_extent(font);
  if (extent <= 0)
    return;

  float advance_scale = (float)font->virtual_line_height / (float)extent;
  font->fixed_advance =
      max(1, Q_rint((float)font->ttf.fixed_advance_units * advance_scale));
}

#if USE_HARFBUZZ
static void font_destroy_harfbuzz(font_t *font) {
  if (!font)
    return;

  if (font->ttf.hb_font) {
    hb_font_destroy(font->ttf.hb_font);
    font->ttf.hb_font = nullptr;
  }
  if (font->ttf.hb_face) {
    hb_face_destroy(font->ttf.hb_face);
    font->ttf.hb_face = nullptr;
  }
  if (font->ttf.hb_blob) {
    hb_blob_destroy(font->ttf.hb_blob);
    font->ttf.hb_blob = nullptr;
  }
}

static void font_init_harfbuzz(font_t *font) {
  if (!font || !font->ttf.data || font->ttf.data_size <= 0 ||
      font->ttf.pixel_height <= 0) {
    return;
  }

  font_destroy_harfbuzz(font);

  hb_blob_t *blob = hb_blob_create(
      static_cast<const char *>(font->ttf.data), font->ttf.data_size,
      HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  if (!blob)
    return;

  hb_face_t *face = hb_face_create(blob, 0);
  if (!face) {
    hb_blob_destroy(blob);
    return;
  }

  hb_font_t *hb_font = hb_font_create(face);
  if (!hb_font) {
    hb_face_destroy(face);
    hb_blob_destroy(blob);
    return;
  }

  hb_ot_font_set_funcs(hb_font);
  hb_font_set_scale(hb_font, font->ttf.pixel_height * 64,
                    font->ttf.pixel_height * 64);
  hb_font_set_ppem(hb_font, font->ttf.pixel_height, font->ttf.pixel_height);

  font->ttf.hb_blob = blob;
  font->ttf.hb_face = face;
  font->ttf.hb_font = hb_font;
}
#endif

static font_glyph_t *font_get_ttf_glyph(font_t *font, uint32_t codepoint,
                                        bool *out_created) {
  if (out_created)
    *out_created = false;

  if (!font || font->kind != FONT_TTF)
    return nullptr;

  auto it = font->ttf.glyphs.find(codepoint);
  if (it != font->ttf.glyphs.end()) {
    font_touch_ttf_glyph(font, codepoint);
    return &it->second;
  }

  if (!font->ttf.font)
    return nullptr;

  if (!TTF_FontHasGlyph(font->ttf.font, codepoint))
    return nullptr;

  int minx = 0;
  int maxx = 0;
  int miny = 0;
  int maxy = 0;
  int advance = 0;
  if (!TTF_GetGlyphMetrics(font->ttf.font, codepoint, &minx, &maxx, &miny,
                           &maxy, &advance))
    return nullptr;

  font_glyph_t glyph;
  glyph.bearing_x = (float)minx;
  glyph.bearing_y = (float)maxy;
  glyph.advance = advance;
  glyph.metrics_valid = true;
  glyph.metric_source = FONT_METRIC_TTF;

  SDL_Color white = {255, 255, 255, 255};
  SDL_Surface *surface =
      TTF_RenderGlyph_Blended(font->ttf.font, codepoint, white);
  if (!surface) {
    glyph.w = max(0, maxx - minx);
    glyph.h = max(0, maxy - miny);
    if (glyph.h > 0)
      // Compensate for trimmed bitmap height relative to outline metrics.
      glyph.offset_y = (float)(miny + glyph.h - maxy);
    if (glyph.metrics_valid &&
        font_update_ttf_rendered_metrics_for_glyph(font, glyph)) {
      font_update_ttf_fixed_advance(font);
    }
    font->ttf.glyphs[codepoint] = glyph;
    font_touch_ttf_glyph(font, codepoint);
    if (out_created)
      *out_created = true;
    return &font->ttf.glyphs[codepoint];
  }

  const SDL_PixelFormat desired_format = SDL_PIXELFORMAT_RGBA32;
  if (surface->format != desired_format) {
    SDL_Surface *converted = SDL_ConvertSurface(surface, desired_format);
    if (!converted) {
      font_debug_printf("Font: SDL_ConvertSurface failed codepoint=%u: %s\n",
                        codepoint, SDL_GetError());
      SDL_DestroySurface(surface);
      if (glyph.metrics_valid &&
          font_update_ttf_rendered_metrics_for_glyph(font, glyph)) {
        font_update_ttf_fixed_advance(font);
      }
      font->ttf.glyphs[codepoint] = glyph;
      font_touch_ttf_glyph(font, codepoint);
      if (out_created)
        *out_created = true;
      return &font->ttf.glyphs[codepoint];
    }
    SDL_DestroySurface(surface);
    surface = converted;
  }

  if (!surface) {
    if (glyph.metrics_valid &&
        font_update_ttf_rendered_metrics_for_glyph(font, glyph)) {
      font_update_ttf_fixed_advance(font);
    }
    font->ttf.glyphs[codepoint] = glyph;
    font_touch_ttf_glyph(font, codepoint);
    if (out_created)
      *out_created = true;
    return &font->ttf.glyphs[codepoint];
  }

  if (!SDL_LockSurface(surface)) {
    font_debug_printf("Font: SDL_LockSurface failed codepoint=%u: %s\n",
                      codepoint, SDL_GetError());
    SDL_DestroySurface(surface);
    if (glyph.metrics_valid &&
        font_update_ttf_rendered_metrics_for_glyph(font, glyph)) {
      font_update_ttf_fixed_advance(font);
    }
    font->ttf.glyphs[codepoint] = glyph;
    font_touch_ttf_glyph(font, codepoint);
    if (out_created)
      *out_created = true;
    return &font->ttf.glyphs[codepoint];
  }

  glyph.w = surface->w;
  glyph.h = surface->h;
  if (glyph.h > 0)
    // Compensate for trimmed bitmap height relative to outline metrics.
    glyph.offset_y = (float)(miny + glyph.h - maxy);

  if (glyph.w > 0 && glyph.h > 0) {
    int max_dim = k_atlas_size - (k_atlas_padding * 2);
    if (glyph.w > max_dim || glyph.h > max_dim) {
      glyph.oversized = true;
      font_debug_printf(
          "Font: glyph %u too large for atlas %dx%d (w=%d h=%d)\n",
          codepoint, k_atlas_size, k_atlas_size, glyph.w, glyph.h);
    }
  }

  if (glyph.w > 0 && glyph.h > 0 && !glyph.oversized) {
    font_atlas_page_t *page = nullptr;
    int page_index = -1;
    font_glyph_t reuse_slot;

    int limit = font_glyph_cache_limit();
    bool reused = false;
    if (limit > 0 && font->ttf.glyphs.size() >= static_cast<size_t>(limit)) {
      reused = font_try_reuse_ttf_slot(font, glyph.w, glyph.h, &reuse_slot);
      if (!reused)
        font_trim_ttf_cache(font, static_cast<size_t>(limit - 1));
    }

    if (reused) {
      page_index = reuse_slot.page;
      if (page_index >= 0 && page_index < (int)font->ttf.pages.size()) {
        page = &font->ttf.pages[page_index];
        glyph.page = page_index;
        glyph.x = reuse_slot.x;
        glyph.y = reuse_slot.y;
        font_clear_atlas_rect(page, reuse_slot.x, reuse_slot.y, reuse_slot.w,
                              reuse_slot.h);
      } else {
        reused = false;
      }
    }

    if (!reused) {
      for (size_t i = 0; i < font->ttf.pages.size(); ++i) {
        font_atlas_page_t &p = font->ttf.pages[i];

        if (p.next_x + glyph.w + k_atlas_padding > p.width) {
          p.next_x = k_atlas_padding;
          p.next_y += p.row_height + k_atlas_padding;
          p.row_height = 0;
        }

        if (p.next_y + glyph.h + k_atlas_padding <= p.height) {
          page = &p;
          page_index = (int)i;
          break;
        }
      }

      if (!page) {
        font->ttf.pages.push_back(
            font_create_atlas_page(font->id, (int)font->ttf.pages.size()));
        page = &font->ttf.pages.back();
        page_index = (int)font->ttf.pages.size() - 1;
      }

      if (page) {
        glyph.page = page_index;
        glyph.x = page->next_x;
        glyph.y = page->next_y;
      }
    }

    if (page) {
      byte *dst = page->pixels + ((glyph.y * page->width) + glyph.x) * 4;
      const byte *src = static_cast<const byte *>(surface->pixels);
      int dst_stride = page->width * 4;
      int src_stride = surface->pitch;

      font_copy_glyph_pixels(dst, dst_stride, src, src_stride, glyph.w,
                             glyph.h, true);

      if (!reused) {
        page->next_x += glyph.w + k_atlas_padding;
        page->row_height = max(page->row_height, glyph.h);
      }

      font_mark_page_dirty(page);
    }
  }

  SDL_UnlockSurface(surface);
  SDL_DestroySurface(surface);

  if (glyph.metrics_valid &&
      font_update_ttf_rendered_metrics_for_glyph(font, glyph)) {
    font_update_ttf_fixed_advance(font);
  }
  font->ttf.glyphs[codepoint] = glyph;
  font_touch_ttf_glyph(font, codepoint);
  if (out_created)
    *out_created = true;
  return &font->ttf.glyphs[codepoint];
}

static font_glyph_t *font_get_ttf_glyph_index(font_t *font,
                                              uint32_t glyph_index,
                                              uint32_t nominal_codepoint,
                                              bool *out_created) {
  if (out_created)
    *out_created = false;

  if (!font || font->kind != FONT_TTF)
    return nullptr;

  auto it = font->ttf.glyph_indices.find(glyph_index);
  if (it != font->ttf.glyph_indices.end()) {
    font_touch_ttf_glyph_index(font, glyph_index);
    return &it->second;
  }

  if (!font->ttf.font)
    return nullptr;

  font_glyph_t glyph;
  int nominal_minx = 0;
  int nominal_maxx = 0;
  int nominal_miny = 0;
  int nominal_maxy = 0;
  int nominal_advance = 0;
  bool nominal_metrics = false;

  TTF_ImageType image_type = TTF_IMAGE_INVALID;
  SDL_Surface *surface =
      TTF_GetGlyphImageForIndex(font->ttf.font, glyph_index, &image_type);
  if (!surface) {
    font->ttf.glyph_indices[glyph_index] = glyph;
    font_touch_ttf_glyph_index(font, glyph_index);
    if (out_created)
      *out_created = true;
    return &font->ttf.glyph_indices[glyph_index];
  }

  const SDL_PixelFormat desired_format = SDL_PIXELFORMAT_RGBA32;
  if (surface->format != desired_format) {
    SDL_Surface *converted = SDL_ConvertSurface(surface, desired_format);
    if (!converted) {
      font_debug_printf(
          "Font: SDL_ConvertSurface failed glyph_index=%u: %s\n",
          glyph_index, SDL_GetError());
      SDL_DestroySurface(surface);
      font->ttf.glyph_indices[glyph_index] = glyph;
      font_touch_ttf_glyph_index(font, glyph_index);
      if (out_created)
        *out_created = true;
      return &font->ttf.glyph_indices[glyph_index];
    }
    SDL_DestroySurface(surface);
    surface = converted;
  }

  if (!surface) {
    font->ttf.glyph_indices[glyph_index] = glyph;
    font_touch_ttf_glyph_index(font, glyph_index);
    if (out_created)
      *out_created = true;
    return &font->ttf.glyph_indices[glyph_index];
  }

  if (!SDL_LockSurface(surface)) {
    font_debug_printf("Font: SDL_LockSurface failed glyph_index=%u: %s\n",
                      glyph_index, SDL_GetError());
    SDL_DestroySurface(surface);
    font->ttf.glyph_indices[glyph_index] = glyph;
    font_touch_ttf_glyph_index(font, glyph_index);
    if (out_created)
      *out_created = true;
    return &font->ttf.glyph_indices[glyph_index];
  }

  glyph.w = surface->w;
  glyph.h = surface->h;

#if USE_HARFBUZZ
  if (nominal_codepoint) {
    nominal_metrics = font_ttf_try_nominal_metrics(
        font, nominal_codepoint, glyph_index, &nominal_minx, &nominal_maxx,
        &nominal_miny, &nominal_maxy, &nominal_advance);
  }
#endif

  if (nominal_metrics) {
    glyph.bearing_x = (float)nominal_minx;
    glyph.bearing_y = (float)nominal_maxy;
    glyph.advance = nominal_advance;
    if (glyph.h > 0)
      glyph.offset_y =
          (float)(nominal_miny + glyph.h - nominal_maxy);
    glyph.metrics_valid = true;
    glyph.metric_source = FONT_METRIC_TTF;
  }

#if USE_HARFBUZZ
  if (!nominal_metrics && font->ttf.hb_font) {
    hb_glyph_extents_t extents;
    if (hb_font_get_glyph_extents(font->ttf.hb_font, glyph_index, &extents)) {
      float bearing_x = (float)extents.x_bearing / 64.0f;
      float bearing_y = (float)extents.y_bearing / 64.0f;
      float hb_height = (float)extents.height / 64.0f;
      glyph.bearing_x = bearing_x;
      glyph.bearing_y = bearing_y;
      float hb_miny = glyph.bearing_y + hb_height;
      if (glyph.h > 0)
        glyph.offset_y = (hb_miny + (float)glyph.h) - glyph.bearing_y;
      glyph.metrics_valid = true;
      glyph.metric_source = FONT_METRIC_HB;
    }
  }
#endif

  if (glyph.w > 0 && glyph.h > 0) {
    int max_dim = k_atlas_size - (k_atlas_padding * 2);
    if (glyph.w > max_dim || glyph.h > max_dim) {
      glyph.oversized = true;
      font_debug_printf(
          "Font: glyph index %u too large for atlas %dx%d (w=%d h=%d)\n",
          glyph_index, k_atlas_size, k_atlas_size, glyph.w, glyph.h);
    }
  }

  if (glyph.w > 0 && glyph.h > 0 && !glyph.oversized) {
    font_atlas_page_t *page = nullptr;
    int page_index = -1;
    font_glyph_t reuse_slot;

    int limit = font_glyph_cache_limit();
    bool reused = false;
    if (limit > 0 &&
        font->ttf.glyph_indices.size() >= static_cast<size_t>(limit)) {
      reused = font_try_reuse_ttf_index_slot(font, glyph.w, glyph.h,
                                             &reuse_slot);
      if (!reused)
        font_trim_ttf_index_cache(font, static_cast<size_t>(limit - 1));
    }

    if (reused) {
      page_index = reuse_slot.page;
      if (page_index >= 0 && page_index < (int)font->ttf.pages.size()) {
        page = &font->ttf.pages[page_index];
        glyph.page = page_index;
        glyph.x = reuse_slot.x;
        glyph.y = reuse_slot.y;
        font_clear_atlas_rect(page, reuse_slot.x, reuse_slot.y, reuse_slot.w,
                              reuse_slot.h);
      } else {
        reused = false;
      }
    }

    if (!reused) {
      for (size_t i = 0; i < font->ttf.pages.size(); ++i) {
        font_atlas_page_t &p = font->ttf.pages[i];

        if (p.next_x + glyph.w + k_atlas_padding > p.width) {
          p.next_x = k_atlas_padding;
          p.next_y += p.row_height + k_atlas_padding;
          p.row_height = 0;
        }

        if (p.next_y + glyph.h + k_atlas_padding <= p.height) {
          page = &p;
          page_index = (int)i;
          break;
        }
      }

      if (!page) {
        font->ttf.pages.push_back(
            font_create_atlas_page(font->id, (int)font->ttf.pages.size()));
        page = &font->ttf.pages.back();
        page_index = (int)font->ttf.pages.size() - 1;
      }

      if (page) {
        glyph.page = page_index;
        glyph.x = page->next_x;
        glyph.y = page->next_y;
      }
    }

    if (page) {
      byte *dst = page->pixels + ((glyph.y * page->width) + glyph.x) * 4;
      const byte *src = static_cast<const byte *>(surface->pixels);
      int dst_stride = page->width * 4;
      int src_stride = surface->pitch;
      bool alpha_only = true;
#if defined(TTF_IMAGE_COLOR)
      if (image_type == TTF_IMAGE_COLOR)
        alpha_only = false;
#endif

      font_copy_glyph_pixels(dst, dst_stride, src, src_stride, glyph.w,
                             glyph.h, alpha_only);

      if (!reused) {
        page->next_x += glyph.w + k_atlas_padding;
        page->row_height = max(page->row_height, glyph.h);
      }

      font_mark_page_dirty(page);
    }
  }

  SDL_UnlockSurface(surface);
  SDL_DestroySurface(surface);

  if (glyph.metrics_valid &&
      font_update_ttf_rendered_metrics_for_glyph(font, glyph)) {
    font_update_ttf_fixed_advance(font);
  }
  font->ttf.glyph_indices[glyph_index] = glyph;
  font_touch_ttf_glyph_index(font, glyph_index);
  if (out_created)
    *out_created = true;
  return &font->ttf.glyph_indices[glyph_index];
}
#endif

static font_t *font_load_internal(const char *path, int virtual_line_height,
                                  float pixel_scale, int fixed_advance,
                                  const char *fallback_kfont,
                                  const char *fallback_legacy,
                                  bool register_font) {
  if (virtual_line_height <= 0)
    virtual_line_height = CONCHAR_HEIGHT;

  font_debug_printf("Font: load request path=\"%s\" line_height=%d "
                    "pixel_scale=%.3f fixed_advance=%d\n",
                    font_safe_str(path), virtual_line_height, pixel_scale,
                    fixed_advance);
  font_debug_printf("Font: fallbacks kfont=\"%s\" legacy=\"%s\"\n",
                    font_safe_str(fallback_kfont),
                    font_safe_str(fallback_legacy));
  if (path && *path) {
    const char *ext = COM_FileExtension(path);
    font_debug_printf("Font: path extension=\"%s\" (ttf=%d otf=%d kfont=%d)\n",
                      font_safe_str(ext), font_ext_is(path, "ttf"),
                      font_ext_is(path, "otf"), font_ext_is(path, "kfont"));
  }

  font_t *font = new font_t();
  font->id = ++g_font_seq;
  font->virtual_line_height = virtual_line_height;
  font->pixel_scale = pixel_scale > 0.0f ? pixel_scale : 1.0f;
  font->fixed_advance = fixed_advance > 0 ? fixed_advance : 0;
  font->registered = register_font;

  if (font_ext_is(path, "ttf") || font_ext_is(path, "otf")) {
#if USE_SDL3_TTF
    int supersample = font_ttf_supersample();
    int pixel_height = max(
        1, Q_rint((float)virtual_line_height * font->pixel_scale *
                  (float)supersample));
    font_debug_printf("Font: trying TTF \"%s\" pixel_height=%d "
                      "supersample=%d\n",
                      font_safe_str(path), pixel_height, supersample);
    void *data = nullptr;
    int len = FS_LoadFile(path, &data);
    if (len > 0 && data) {
      SDL_IOStream *io = SDL_IOFromMem(data, len);
      if (io) {
        TTF_Font *ttf = TTF_OpenFontIO(io, true, pixel_height);
        if (ttf) {
          TTF_SetFontHinting(ttf, font_ttf_hinting_mode());
          TTF_SetFontKerning(ttf, 1);
          font->kind = FONT_TTF;
          font->ttf.font = ttf;
          font->ttf.data = data;
          font->ttf.data_size = len;
          font->ttf.pixel_height = pixel_height;
          font->ttf.ascent = TTF_GetFontAscent(ttf);
          font->ttf.line_skip = TTF_GetFontLineSkip(ttf);
          if (font->ttf.line_skip <= 0)
            font->ttf.line_skip = max(font->ttf.ascent, 1);
          font_compute_ttf_metrics(font);
          #if USE_HARFBUZZ
          font_init_harfbuzz(font);
          #endif
          font->unit_scale = (float)(virtual_line_height * font->pixel_scale) /
                             (float)font->ttf.extent;
          if (font->fixed_advance > 0) {
            int advance = 0;
            if (font->ttf.extent > 0 &&
                TTF_GetGlyphMetrics(ttf, 'M', nullptr, nullptr, nullptr,
                                    nullptr, &advance) &&
                advance > 0) {
              font->ttf.fixed_advance_units = advance;
              float advance_scale =
                  (float)virtual_line_height / (float)font->ttf.extent;
              font->fixed_advance =
                  max(1, Q_rint((float)advance * advance_scale));
            }
          }
          font->ttf.pages.push_back(font_create_atlas_page(font->id, 0));
          font_preload_ascii(font);
          font_compute_ttf_rendered_metrics(font);
          font_update_ttf_fixed_advance(font);
          font_flush_ttf_pages(font);
          font_debug_printf("Font: TTF loaded \"%s\" ascent=%d line_skip=%d "
                            "extent=%d baseline=%d rendered=%d/%d "
                            "metric_mode=%d unit_scale=%.3f\n",
                            font_safe_str(path), font->ttf.ascent,
                            font->ttf.line_skip, font->ttf.extent,
                            font->ttf.baseline, font->ttf.rendered_ascent,
                            font->ttf.rendered_extent, font_ttf_metric_mode(),
                            font->unit_scale);
        } else {
          font_debug_printf("Font: TTF_OpenFontIO failed \"%s\": %s\n",
                            font_safe_str(path), SDL_GetError());
        }
      } else {
        font_debug_printf("Font: SDL_IOFromMem failed \"%s\"\n",
                          font_safe_str(path));
      }
      if (font->kind != FONT_TTF)
        FS_FreeFile(data);
    } else {
      font_debug_printf("Font: FS_LoadFile failed \"%s\": %s\n",
                        font_safe_str(path), Com_GetLastError());
    }
#else
    font_debug_printf("Font: SDL3_ttf disabled, skipping \"%s\"\n",
                      font_safe_str(path));
#endif
  }

  if (font->kind == FONT_TTF) {
    font_debug_printf("Font: using TTF \"%s\"\n", font_safe_str(path));
    if (fallback_kfont && *fallback_kfont) {
      font->fallback_kfont =
          font_load_internal(fallback_kfont, virtual_line_height, pixel_scale,
                             fixed_advance, nullptr, fallback_legacy, false);
      if (font->fallback_kfont) {
        font_debug_printf("Font: TTF fallback kfont \"%s\" loaded\n",
                          font_safe_str(fallback_kfont));
      } else {
        font_debug_printf("Font: TTF fallback kfont \"%s\" failed\n",
                          font_safe_str(fallback_kfont));
      }
    }
    font->legacy_handle =
        font_register_legacy(fallback_legacy, fallback_legacy);
  } else if (font_ext_is(path, "ttf") || font_ext_is(path, "otf")) {
    font_debug_printf("Font: TTF failed, trying kfont fallback \"%s\"\n",
                      font_safe_str(fallback_kfont));
    if (fallback_kfont && *fallback_kfont) {
      font->kind = FONT_KFONT;
      if (font_load_kfont(font, fallback_kfont)) {
        font->unit_scale = (float)(virtual_line_height * font->pixel_scale) /
                           (float)font->kfont.line_height;
        font->legacy_handle =
            font_register_legacy(fallback_legacy, fallback_legacy);
        goto loaded;
      }
      font->kfont.glyphs.clear();
      font->kfont.pic = 0;
      font->kfont.tex_w = 0;
      font->kfont.tex_h = 0;
      font->kfont.inv_w = 0.0f;
      font->kfont.inv_h = 0.0f;
      font->kfont.line_height = 0;
    }

    font->kind = FONT_LEGACY;
    font->legacy_handle =
        font_register_legacy(fallback_legacy, fallback_legacy);
    if (!font->legacy_handle) {
      delete font;
      return nullptr;
    }
    font->unit_scale = (float)(virtual_line_height * font->pixel_scale) /
                       (float)CONCHAR_HEIGHT;
  } else if (font_ext_is(path, "kfont")) {
    font->kind = FONT_KFONT;
    if (!font_load_kfont(font, path)) {
      font->kfont.glyphs.clear();
      font->kfont.pic = 0;
      font->kfont.tex_w = 0;
      font->kfont.tex_h = 0;
      font->kfont.inv_w = 0.0f;
      font->kfont.inv_h = 0.0f;
      font->kfont.line_height = 0;

      font->kind = FONT_LEGACY;
      font->legacy_handle =
          font_register_legacy(fallback_legacy, fallback_legacy);
      if (!font->legacy_handle) {
        delete font;
        return nullptr;
      }
      font->unit_scale = (float)(virtual_line_height * font->pixel_scale) /
                         (float)CONCHAR_HEIGHT;
    } else {
      font->unit_scale = (float)(virtual_line_height * font->pixel_scale) /
                         (float)font->kfont.line_height;
      font->legacy_handle =
          font_register_legacy(fallback_legacy, fallback_legacy);
    }
  } else {
    font_debug_printf("Font: falling back to legacy path \"%s\"\n",
                      font_safe_str(path));
    font->kind = FONT_LEGACY;
    font->legacy_handle = font_register_legacy(path, fallback_legacy);
    if (!font->legacy_handle) {
      delete font;
      return nullptr;
    }
    font->unit_scale = (float)(virtual_line_height * font->pixel_scale) /
                       (float)CONCHAR_HEIGHT;
  }

loaded:

  font_debug_printf(
      "Font: load complete kind=%s path=\"%s\" legacy_handle=%d\n",
      font_kind_name(font->kind), font_safe_str(path), font->legacy_handle);
  if (register_font)
    g_fonts.push_back(font);

  return font;
}

void Font_Init(void) {
  (void)font_debug_enabled();
  (void)font_glyph_cache_limit();
#if USE_SDL3_TTF
  (void)font_ttf_hinting_mode();
  (void)font_ttf_supersample();
  (void)font_ttf_metric_mode();
  (void)font_ttf_hb_snap_positions();
#endif
#if USE_SDL3_TTF
  if (!g_ttf_ready) {
    if (!TTF_Init()) {
      Com_WPrintf("TTF_Init failed: %s\n", SDL_GetError());
      font_debug_printf("Font: SDL3_ttf init failed\n");
    } else {
      g_ttf_ready = true;
      font_debug_printf("Font: SDL3_ttf initialized\n");
    }
  }
#else
  font_debug_printf("Font: SDL3_ttf not available, TTF fonts will fallback\n");
#endif
}

void Font_Shutdown(void) {
  for (font_t *font : g_fonts)
    Font_Free(font);
  g_fonts.clear();

#if USE_SDL3_TTF
  if (g_ttf_ready) {
    TTF_Quit();
    g_ttf_ready = false;
  }
#endif
}

font_t *Font_Load(const char *path, int virtual_line_height, float pixel_scale,
                  int fixed_advance, const char *fallback_kfont,
                  const char *fallback_legacy) {
  return font_load_internal(path, virtual_line_height, pixel_scale,
                            fixed_advance, fallback_kfont, fallback_legacy,
                            true);
}

void Font_Free(font_t *font) {
  if (!font)
    return;

  if (font->registered) {
    auto it = std::find(g_fonts.begin(), g_fonts.end(), font);
    if (it != g_fonts.end())
      g_fonts.erase(it);
  }

#if USE_SDL3_TTF
  if (font->kind == FONT_TTF) {
    for (auto &page : font->ttf.pages) {
      if (page.handle)
        R_UnregisterImage(page.handle);
      if (page.pixels)
        Z_Free(page.pixels);
    }
    font->ttf.pages.clear();
    font->ttf.glyphs.clear();
    font->ttf.lru.clear();
    font->ttf.lru_index.clear();
    font->ttf.glyph_indices.clear();
    font->ttf.index_lru.clear();
    font->ttf.index_lru_index.clear();
#if USE_HARFBUZZ
    font_destroy_harfbuzz(font);
#endif
    if (font->ttf.font)
      TTF_CloseFont(font->ttf.font);
    if (font->ttf.data)
      FS_FreeFile(font->ttf.data);
  }
#endif

  if (font->fallback_kfont) {
    Font_Free(font->fallback_kfont);
    font->fallback_kfont = nullptr;
  }

  delete font;
}

void Font_SetLetterSpacing(font_t *font, float spacing) {
  if (!font)
    return;
  font->letter_spacing = spacing > 0.0f ? spacing : 0.0f;
}

static int font_advance_for_codepoint(const font_t *font, uint32_t codepoint,
                                      int scale) {
  if (!font || !codepoint)
    return 0;

  int draw_scale = scale > 0 ? scale : 1;
  if (font->fixed_advance > 0) {
#if USE_SDL3_TTF
    if (font->kind == FONT_TTF && font->ttf.fixed_advance_units > 0) {
      float glyph_scale = font_ttf_metric_scale(font, draw_scale);
      return max(1, Q_rint((float)font->ttf.fixed_advance_units * glyph_scale));
    }
#endif
    return max(1, Q_rint((float)font->fixed_advance * (float)draw_scale *
                         k_font_scale_boost));
  }

  if (font->kind == FONT_LEGACY) {
    float legacy_scale = font_draw_scale(font, draw_scale);
    return max(1, Q_rint(CONCHAR_WIDTH * legacy_scale));
  }

  if (font->kind == FONT_KFONT) {
    auto it = font->kfont.glyphs.find(codepoint);
    if (it == font->kfont.glyphs.end())
      return 0;
    float glyph_scale = font_draw_scale(font, draw_scale);
    return max(1, Q_rint((float)it->second.w * glyph_scale));
  }

#if USE_SDL3_TTF
  if (font->kind == FONT_TTF) {
    int advance_units = 0;
    if (!font_get_ttf_advance_units(font, codepoint, &advance_units))
      return 0;
    float glyph_scale = font_ttf_metric_scale(font, draw_scale);
    return max(1, Q_rint((float)advance_units * glyph_scale));
  }
#endif

  return 0;
}

static bool font_draw_legacy_glyph(const font_t *font, uint32_t codepoint,
                                   int *x, int y, int scale, int flags,
                                   color_t color) {
  if (!font || !font->legacy_handle)
    return false;

  int draw_scale = scale > 0 ? scale : 1;
  float legacy_scale = font_draw_scale(font, draw_scale);
  int w = max(1, Q_rint(CONCHAR_WIDTH * legacy_scale));
  int h = max(1, Q_rint(CONCHAR_HEIGHT * legacy_scale));
  int ch = codepoint <= 255 ? (int)codepoint : '?';

  R_DrawStretchChar(*x, y, w, h, flags, ch, color, font->legacy_handle);

  int advance = font->fixed_advance > 0
                    ? max(1, Q_rint((float)font->fixed_advance *
                                    (float)draw_scale * k_font_scale_boost))
                    : w;
  *x += advance;
  return true;
}

static bool font_draw_kfont_glyph(const font_t *font, uint32_t codepoint,
                                  int *x, int y, int scale, int flags,
                                  color_t color) {
  if (!font || font->kind != FONT_KFONT || !font->kfont.pic)
    return false;

  auto it = font->kfont.glyphs.find(codepoint);
  if (it == font->kfont.glyphs.end())
    return false;

  const kfont_glyph_t &glyph = it->second;
  float glyph_scale = font_draw_scale(font, scale);
  int w = max(1, Q_rint((float)glyph.w * glyph_scale));
  int h = max(1, Q_rint((float)glyph.h * glyph_scale));

  float s1 = (float)glyph.x * font->kfont.inv_w;
  float t1 = (float)glyph.y * font->kfont.inv_h;
  float s2 = (float)(glyph.x + glyph.w) * font->kfont.inv_w;
  float t2 = (float)(glyph.y + glyph.h) * font->kfont.inv_h;

  if (flags & UI_DROPSHADOW) {
    int shadow = max(1, scale);
    color_t black = COLOR_A(color.a);
    R_DrawStretchSubPic(*x + shadow, y + shadow, w, h, s1, t1, s2, t2, black,
                        font->kfont.pic);
  }

  R_DrawStretchSubPic(*x, y, w, h, s1, t1, s2, t2, color, font->kfont.pic);

  int advance = font->fixed_advance > 0
                    ? max(1, Q_rint((float)font->fixed_advance * (float)scale *
                                    k_font_scale_boost))
                    : w;
  *x += advance;
  return true;
}

[[maybe_unused]] static void font_draw_legacy_glyph_at(const font_t *font,
                                                       uint32_t codepoint,
                                                       int x, int y, int scale,
                                                       int flags,
                                                       color_t color) {
  if (!font || !font->legacy_handle)
    return;

  int draw_scale = scale > 0 ? scale : 1;
  float legacy_scale = font_draw_scale(font, draw_scale);
  int w = max(1, Q_rint(CONCHAR_WIDTH * legacy_scale));
  int h = max(1, Q_rint(CONCHAR_HEIGHT * legacy_scale));
  int ch = codepoint <= 255 ? (int)codepoint : '?';

  R_DrawStretchChar(x, y, w, h, flags, ch, color, font->legacy_handle);
}

[[maybe_unused]] static bool font_draw_kfont_glyph_at(const font_t *font,
                                                      uint32_t codepoint, int x,
                                                      int y, int scale,
                                                      int flags,
                                                      color_t color) {
  if (!font || font->kind != FONT_KFONT || !font->kfont.pic)
    return false;

  auto it = font->kfont.glyphs.find(codepoint);
  if (it == font->kfont.glyphs.end())
    return false;

  const kfont_glyph_t &glyph = it->second;
  float glyph_scale = font_draw_scale(font, scale);
  int w = max(1, Q_rint((float)glyph.w * glyph_scale));
  int h = max(1, Q_rint((float)glyph.h * glyph_scale));

  float s1 = (float)glyph.x * font->kfont.inv_w;
  float t1 = (float)glyph.y * font->kfont.inv_h;
  float s2 = (float)(glyph.x + glyph.w) * font->kfont.inv_w;
  float t2 = (float)(glyph.y + glyph.h) * font->kfont.inv_h;

  if (flags & UI_DROPSHADOW) {
    int shadow = max(1, scale);
    color_t black = COLOR_A(color.a);
    R_DrawStretchSubPic(x + shadow, y + shadow, w, h, s1, t1, s2, t2, black,
                        font->kfont.pic);
  }

  R_DrawStretchSubPic(x, y, w, h, s1, t1, s2, t2, color, font->kfont.pic);
  return true;
}

#if USE_SDL3_TTF
static bool font_draw_ttf_glyph_cached(const font_t *font,
                                       const font_glyph_t *glyph, float *x,
                                       int y, int scale, int flags,
                                       color_t color) {
  if (!font || font->kind != FONT_TTF || !glyph)
    return false;
  if (glyph->oversized)
    return false;

  float glyph_scale = font_ttf_metric_scale(font, scale);
  float advance = font_get_ttf_advance_scaled(font, glyph->advance, scale);

  if (glyph->page < 0 || glyph->page >= (int)font->ttf.pages.size() ||
      glyph->w <= 0 || glyph->h <= 0) {
    *x += advance;
    return true;
  }

  const font_atlas_page_t &page = font->ttf.pages[glyph->page];
  if (!page.handle) {
    *x += advance;
    return true;
  }
  float base_x = *x;
  float baseline_y =
      (float)y + (float)font_ttf_metric_baseline(font) * glyph_scale;
  float draw_xf =
      base_x + (glyph->bearing_x + glyph->offset_x) * glyph_scale;
  float draw_yf =
      baseline_y - (glyph->bearing_y + glyph->offset_y) * glyph_scale;
  int draw_x = (int)floorf(draw_xf);
  int draw_y = (int)floorf(draw_yf);
  int w = max(1, (int)ceilf((float)glyph->w * glyph_scale));
  int h = max(1, (int)ceilf((float)glyph->h * glyph_scale));

  float s1 = (float)glyph->x / (float)page.width;
  float t1 = (float)glyph->y / (float)page.height;
  float s2 = (float)(glyph->x + glyph->w) / (float)page.width;
  float t2 = (float)(glyph->y + glyph->h) / (float)page.height;

  if (flags & UI_DROPSHADOW) {
    int shadow = max(1, scale);
    color_t black = COLOR_A(color.a);
    R_DrawStretchSubPic(draw_x + shadow, draw_y + shadow, w, h, s1, t1, s2, t2,
                        black, page.handle);
  }

  R_DrawStretchSubPic(draw_x, draw_y, w, h, s1, t1, s2, t2, color, page.handle);

  *x += advance;
  return true;
}

[[maybe_unused]] static bool font_draw_ttf_glyph_index_cached(
    const font_t *font, const font_glyph_t *glyph, float draw_x, float draw_y,
    float glyph_scale, int scale, int flags, color_t color) {
  if (!font || font->kind != FONT_TTF || !glyph)
    return false;
  if (glyph->oversized)
    return false;

  if (glyph->page < 0 || glyph->page >= (int)font->ttf.pages.size() ||
      glyph->w <= 0 || glyph->h <= 0)
    return false;

  const font_atlas_page_t &page = font->ttf.pages[glyph->page];
  if (!page.handle)
    return false;

  int w = max(1, (int)ceilf((float)glyph->w * glyph_scale));
  int h = max(1, (int)ceilf((float)glyph->h * glyph_scale));
  int dx = (int)floorf(draw_x);
  int dy = (int)floorf(draw_y);

  float s1 = (float)glyph->x / (float)page.width;
  float t1 = (float)glyph->y / (float)page.height;
  float s2 = (float)(glyph->x + glyph->w) / (float)page.width;
  float t2 = (float)(glyph->y + glyph->h) / (float)page.height;

  if (flags & UI_DROPSHADOW) {
    int shadow = max(1, scale);
    color_t black = COLOR_A(color.a);
    R_DrawStretchSubPic(dx + shadow, dy + shadow, w, h, s1, t1, s2, t2, black,
                        page.handle);
  }

  R_DrawStretchSubPic(dx, dy, w, h, s1, t1, s2, t2, color, page.handle);
  return true;
}
#endif

#if USE_SDL3_TTF
static void font_prepare_ttf_glyphs(font_t *font, size_t max_chars,
                                    const char *string, int flags) {
  if (!font || font->kind != FONT_TTF || !string || !*string)
    return;

  bool use_color_codes = Com_HasColorEscape(string, max_chars);
  size_t remaining = max_chars;
  const char *s = string;
  while (remaining && *s) {
    if ((flags & UI_MULTILINE) && *s == '\n') {
      ++s;
      --remaining;
      continue;
    }

    if (use_color_codes) {
      if (Com_ParseColorEscape(&s, &remaining, COLOR_WHITE, nullptr))
        continue;
    }

    uint32_t codepoint = font_read_codepoint(&s, &remaining);
    if (!codepoint)
      break;

    font_get_ttf_glyph(font, codepoint, nullptr);
  }
}
#endif

int Font_DrawString(font_t *font, int x, int y, int scale, int flags,
                    size_t max_chars, const char *string, color_t color) {
  if (!font || !string || !*string)
    return x;

#if USE_HARFBUZZ
  if (font->kind == FONT_TTF && font->ttf.hb_font &&
      font->fixed_advance <= 0) {
    return font_draw_string_hb(font, x, y, scale, flags, max_chars, string,
                               color);
  }
#endif

  int draw_scale = scale > 0 ? scale : 1;
  float line_height = (float)Font_LineHeight(font, draw_scale);
  float pixel_spacing =
      font->pixel_scale > 0.0f ? (1.0f / font->pixel_scale) : 0.0f;
  int start_x = x;
  float x_f = (float)x;
#if USE_SDL3_TTF
  bool debug_match = font_debug_match(string);
  int debug_flags = debug_match ? font_debug_draw_flags() : 0;
  bool dump_metrics = debug_match && font_debug_dump_requested();
#else
  int debug_flags = 0;
  bool dump_metrics = false;
#endif
#if USE_SDL3_TTF
  float ttf_spacing = 0.0f;
  if (font->kind == FONT_TTF)
    ttf_spacing = font_ttf_letter_spacing(font, draw_scale);
#endif

#if USE_SDL3_TTF
  if (dump_metrics && font->kind == FONT_TTF) {
    Com_Printf("FontDebug(TTF): font_id=%d scale=%d line_height=%.2f "
               "baseline=%d extent=%d rendered=%d/%d metric_mode=%d\n",
               font->id, draw_scale, line_height,
               font->ttf.baseline, font->ttf.extent,
               font->ttf.rendered_ascent, font->ttf.rendered_extent,
               font_ttf_metric_mode());
  }
#endif

  int draw_flags = flags;
  bool use_color_codes = Com_HasColorEscape(string, max_chars);
  color_t base_color = color;
  if (use_color_codes) {
    base_color = font_resolve_color(flags, color);
    draw_flags &= ~(UI_ALTCOLOR | UI_XORCOLOR);
  }

  color_t draw_color = color;
  if (font->kind != FONT_LEGACY)
    draw_color = font_resolve_color(flags, color);
  if (use_color_codes)
    draw_color = base_color;

#if USE_SDL3_TTF
  if (font->kind == FONT_TTF) {
    font_prepare_ttf_glyphs(font, max_chars, string, flags);
    font_flush_ttf_pages(font);
  }
#endif

#if USE_SDL3_TTF
  uint32_t prev_codepoint = 0;
  bool prev_ttf = false;
#endif

  float line_start_x = (float)start_x;
  float line_last_x = x_f;
  int line_y = y;
  float line_baseline_y = 0.0f;
#if USE_SDL3_TTF
  if (font->kind == FONT_TTF) {
    float glyph_scale = font_ttf_metric_scale(font, draw_scale);
    line_baseline_y =
        (float)line_y + (float)font_ttf_metric_baseline(font) * glyph_scale;
  }
#endif

  size_t remaining = max_chars;
  const char *s = string;
  while (remaining && *s) {
    if ((flags & UI_MULTILINE) && *s == '\n') {
#if USE_SDL3_TTF
      if (font->kind == FONT_TTF &&
          (debug_flags & k_font_debug_draw_lines)) {
        int line_width = max(1, (int)ceilf(line_last_x - line_start_x));
        int top_y = line_y;
        int base_y = (int)floorf(line_baseline_y);
        int bottom_y = top_y + max(1, (int)ceilf(line_height)) - 1;
        font_debug_draw_line((int)floorf(line_start_x), top_y, line_width,
                             COLOR_RGBA(0, 128, 255, 180));
        font_debug_draw_line((int)floorf(line_start_x), base_y, line_width,
                             COLOR_RGBA(0, 255, 0, 180));
        font_debug_draw_line((int)floorf(line_start_x), bottom_y, line_width,
                             COLOR_RGBA(255, 0, 0, 180));
      }
      if (font->kind == FONT_TTF &&
          (debug_flags & k_font_debug_draw_outline)) {
        float glyph_scale = font_ttf_metric_scale(font, draw_scale);
        int line_width = max(1, (int)ceilf(line_last_x - line_start_x));
        float outline_top =
            line_baseline_y - (float)font->ttf.baseline * glyph_scale;
        float outline_bottom =
            outline_top + (float)font->ttf.extent * glyph_scale;
        int top_y = (int)floorf(outline_top);
        int bottom_y = (int)floorf(outline_bottom);
        font_debug_draw_line((int)floorf(line_start_x), top_y, line_width,
                             COLOR_RGBA(128, 0, 255, 180));
        font_debug_draw_line((int)floorf(line_start_x), bottom_y, line_width,
                             COLOR_RGBA(255, 128, 0, 180));
      }
#endif
      x = start_x;
      x_f = (float)start_x;
      y += Q_rint(line_height + pixel_spacing);
      line_start_x = (float)start_x;
      line_last_x = x_f;
      line_y = y;
#if USE_SDL3_TTF
      if (font->kind == FONT_TTF) {
        float glyph_scale = font_ttf_metric_scale(font, draw_scale);
        line_baseline_y =
            (float)line_y +
            (float)font_ttf_metric_baseline(font) * glyph_scale;
      }
#endif
#if USE_SDL3_TTF
      prev_codepoint = 0;
      prev_ttf = false;
#endif
      s++;
      remaining--;
      continue;
    }

    if (use_color_codes) {
      color_t parsed;
      if (Com_ParseColorEscape(&s, &remaining, base_color, &parsed)) {
        draw_color = parsed;
        continue;
      }
    }

    uint32_t codepoint = font_read_codepoint(&s, &remaining);
    if (!codepoint)
      break;

    bool drawn = false;
#if USE_SDL3_TTF
    if (font->kind == FONT_TTF) {
      font_glyph_t *glyph = font_get_ttf_glyph(font, codepoint, nullptr);
      if (glyph && glyph->oversized)
        glyph = nullptr;
      if (glyph) {
        float advance = font_get_ttf_advance_scaled(font, glyph->advance,
                                                    draw_scale);
        if (prev_ttf) {
          float kerning =
              font_get_ttf_kerning(font, prev_codepoint, codepoint, draw_scale);
          if (kerning != 0.0f)
            x_f += kerning;
          if (ttf_spacing != 0.0f && advance > 0.0f)
            x_f += ttf_spacing;
        }
        if (debug_flags & (k_font_debug_draw_bounds | k_font_debug_draw_advances |
                           k_font_debug_draw_origins)) {
          float glyph_scale = font_ttf_metric_scale(font, draw_scale);
          float baseline_y = (float)line_y +
                             (float)font_ttf_metric_baseline(font) * glyph_scale;
          float draw_xf =
              x_f + (glyph->bearing_x + glyph->offset_x) * glyph_scale;
          float draw_yf =
              baseline_y - (glyph->bearing_y + glyph->offset_y) * glyph_scale;
          int draw_x = (int)floorf(draw_xf);
          int draw_y = (int)floorf(draw_yf);
          int w = max(1, (int)ceilf((float)glyph->w * glyph_scale));
          int h = max(1, (int)ceilf((float)glyph->h * glyph_scale));

          if (debug_flags & k_font_debug_draw_bounds) {
            font_debug_draw_box(draw_x, draw_y, w, h,
                                COLOR_RGBA(255, 0, 255, 160));
          }
          if (debug_flags & k_font_debug_draw_advances) {
            int ay = line_y;
            int ah = max(1, (int)ceilf(line_height));
            R_DrawFill32((int)floorf(x_f), ay, 1, ah,
                         COLOR_RGBA(255, 255, 0, 160));
          }
          if (debug_flags & k_font_debug_draw_origins) {
            font_debug_draw_point((int)floorf(x_f),
                                  (int)floorf(baseline_y),
                                  COLOR_RGBA(0, 255, 255, 200));
          }
        }
        if (glyph->page >= 0 && glyph->page < (int)font->ttf.pages.size()) {
          font_atlas_page_t &page = font->ttf.pages[glyph->page];
          if (page.dirty)
            font_flush_ttf_page(&page);
        }
        drawn = font_draw_ttf_glyph_cached(font, glyph, &x_f, y, draw_scale,
                                           draw_flags, draw_color);
        if (drawn) {
          line_last_x = x_f;
          if (dump_metrics) {
            Com_Printf(
                "FontDebug(TTF): cp=%u adv=%d pen=%.2f bearing=(%.2f,%.2f) "
                "offset=(%.2f,%.2f) bmp=%dx%d\n",
                codepoint, glyph->advance, x_f, glyph->bearing_x,
                glyph->bearing_y, glyph->offset_x, glyph->offset_y, glyph->w,
                glyph->h);
            if (font_debug_metrics_enabled() && font->ttf.font) {
              int minx = 0;
              int maxx = 0;
              int miny = 0;
              int maxy = 0;
              int adv = 0;
              if (TTF_GetGlyphMetrics(font->ttf.font, codepoint, &minx, &maxx,
                                      &miny, &maxy, &adv)) {
                Com_Printf("FontDebug(TTF): metrics minx=%d maxx=%d miny=%d "
                           "maxy=%d adv=%d\n",
                           minx, maxx, miny, maxy, adv);
              }
            }
          }
          if (advance > 0.0f)
            prev_codepoint = codepoint;
          prev_ttf = true;
        }
      }
    }
#endif
    if (!drawn && font->kind == FONT_KFONT) {
      drawn = font_draw_kfont_glyph(font, codepoint, &x, y, draw_scale,
                                    draw_flags, draw_color);
#if USE_SDL3_TTF
      prev_codepoint = 0;
      prev_ttf = false;
#endif
    }
    if (!drawn && font->kind == FONT_LEGACY) {
      drawn = font_draw_legacy_glyph(font, codepoint, &x, y, draw_scale,
                                     draw_flags, draw_color);
#if USE_SDL3_TTF
      prev_codepoint = 0;
      prev_ttf = false;
#endif
    }

    if (!drawn && font->fallback_kfont) {
      int fallback_x = font->kind == FONT_TTF ? Q_rint(x_f) : x;
      drawn = font_draw_kfont_glyph(font->fallback_kfont, codepoint,
                                    &fallback_x, y, draw_scale, draw_flags,
                                    draw_color);
      x = fallback_x;
      x_f = (float)x;
#if USE_SDL3_TTF
      prev_codepoint = 0;
      prev_ttf = false;
#endif
    }

    if (!drawn) {
      int fallback_x = font->kind == FONT_TTF ? Q_rint(x_f) : x;
      font_draw_legacy_glyph(font, codepoint, &fallback_x, y, draw_scale,
                             draw_flags, draw_color);
      x = fallback_x;
      x_f = (float)x;
#if USE_SDL3_TTF
      prev_codepoint = 0;
      prev_ttf = false;
#endif
    }
  }

#if USE_SDL3_TTF
  if (font->kind == FONT_TTF &&
      (debug_flags & k_font_debug_draw_lines)) {
    int line_width = max(1, (int)ceilf(line_last_x - line_start_x));
    int top_y = line_y;
    int base_y = (int)floorf(line_baseline_y);
    int bottom_y = top_y + max(1, (int)ceilf(line_height)) - 1;
    font_debug_draw_line((int)floorf(line_start_x), top_y, line_width,
                         COLOR_RGBA(0, 128, 255, 180));
    font_debug_draw_line((int)floorf(line_start_x), base_y, line_width,
                         COLOR_RGBA(0, 255, 0, 180));
    font_debug_draw_line((int)floorf(line_start_x), bottom_y, line_width,
                         COLOR_RGBA(255, 0, 0, 180));
  }
  if (font->kind == FONT_TTF &&
      (debug_flags & k_font_debug_draw_outline)) {
    float glyph_scale = font_ttf_metric_scale(font, draw_scale);
    int line_width = max(1, (int)ceilf(line_last_x - line_start_x));
    float outline_top =
        line_baseline_y - (float)font->ttf.baseline * glyph_scale;
    float outline_bottom =
        outline_top + (float)font->ttf.extent * glyph_scale;
    int top_y = (int)floorf(outline_top);
    int bottom_y = (int)floorf(outline_bottom);
    font_debug_draw_line((int)floorf(line_start_x), top_y, line_width,
                         COLOR_RGBA(128, 0, 255, 180));
    font_debug_draw_line((int)floorf(line_start_x), bottom_y, line_width,
                         COLOR_RGBA(255, 128, 0, 180));
  }
  if (dump_metrics)
    font_debug_dump_clear();
#endif

  return font->kind == FONT_TTF ? Q_rint(x_f) : x;
}

int Font_MeasureString(const font_t *font, int scale, int flags,
                       size_t max_chars, const char *string, int *out_height) {
  if (!font || !string || !*string) {
    if (out_height)
      *out_height = 0;
    return 0;
  }

#if USE_HARFBUZZ
  if (font->kind == FONT_TTF && font->ttf.hb_font &&
      font->fixed_advance <= 0) {
    return font_measure_string_hb(font, scale, flags, max_chars, string,
                                  out_height);
  }
#endif

  int draw_scale = scale > 0 ? scale : 1;
  float line_height = (float)Font_LineHeight(font, draw_scale);
  float pixel_spacing =
      font->pixel_scale > 0.0f ? (1.0f / font->pixel_scale) : 0.0f;
  float width = 0.0f;
  float max_width = 0.0f;
  int lines = 1;

#if USE_SDL3_TTF
  float ttf_spacing = 0.0f;
  if (font->kind == FONT_TTF)
    ttf_spacing = font_ttf_letter_spacing(font, draw_scale);
  uint32_t prev_codepoint = 0;
  bool prev_ttf = false;
#endif

  bool use_color_codes = Com_HasColorEscape(string, max_chars);
  size_t remaining = max_chars;
  const char *s = string;
  while (remaining && *s) {
    if ((flags & UI_MULTILINE) && *s == '\n') {
      max_width = max(max_width, width);
      width = 0.0f;
      lines++;
#if USE_SDL3_TTF
      prev_codepoint = 0;
      prev_ttf = false;
#endif
      s++;
      remaining--;
      continue;
    }

    if (use_color_codes) {
      if (Com_ParseColorEscape(&s, &remaining, COLOR_WHITE, nullptr))
        continue;
    }

    uint32_t codepoint = font_read_codepoint(&s, &remaining);
    if (!codepoint)
      break;

    float advance = 0.0f;
    bool is_ttf = false;
#if USE_SDL3_TTF
    if (font->kind == FONT_TTF) {
      int advance_units = 0;
      if (font_get_ttf_advance_units(font, codepoint, &advance_units)) {
        advance = font_get_ttf_advance_scaled(font, advance_units, draw_scale);
        is_ttf = true;
        if (prev_ttf) {
          float kerning =
              font_get_ttf_kerning(font, prev_codepoint, codepoint, draw_scale);
          if (kerning != 0.0f)
            width += kerning;
          if (ttf_spacing != 0.0f && advance > 0.0f)
            width += ttf_spacing;
        }
      }
    }
#endif
    if (!is_ttf) {
      int fallback_advance = 0;
      if (font->fallback_kfont)
        fallback_advance =
            font_advance_for_codepoint(font->fallback_kfont, codepoint,
                                       draw_scale);
      if (!fallback_advance && font->legacy_handle)
        fallback_advance =
            max(1, Q_rint(CONCHAR_WIDTH * font_draw_scale(font, draw_scale)));
      advance = (float)fallback_advance;
    }

    width += advance;

#if USE_SDL3_TTF
    if (is_ttf) {
      if (advance > 0.0f)
        prev_codepoint = codepoint;
      prev_ttf = true;
    } else {
      prev_codepoint = 0;
      prev_ttf = false;
    }
#endif
  }

  max_width = max(max_width, width);
  if (out_height)
    *out_height =
        Q_rint((float)lines * line_height + (lines - 1) * pixel_spacing);

  return max(0, Q_rint(max_width));
}

int Font_LineHeight(const font_t *font, int scale) {
  if (!font)
    return CONCHAR_HEIGHT * max(scale, 1);

  int draw_scale = scale > 0 ? scale : 1;
  return max(1, Q_rint((float)font->virtual_line_height * (float)draw_scale *
                       k_font_scale_boost));
}

bool Font_IsLegacy(const font_t *font) {
  return font && font->kind == FONT_LEGACY;
}

qhandle_t Font_LegacyHandle(const font_t *font) {
  return font ? font->legacy_handle : 0;
}
