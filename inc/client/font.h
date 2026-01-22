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

#pragma once

#include <stddef.h>

#include "shared/shared.h"
#include "renderer/renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct font_s font_t;

void Font_Init(void);
void Font_Shutdown(void);

font_t *Font_Load(const char *path, int virtual_line_height, float pixel_scale,
                  int fixed_advance, const char *fallback_kfont,
                  const char *fallback_legacy);
void Font_Free(font_t *font);
void Font_SetLetterSpacing(font_t *font, float spacing);

int Font_DrawString(font_t *font, int x, int y, int scale, int flags,
                    size_t max_chars, const char *string, color_t color);
int Font_MeasureString(const font_t *font, int scale, int flags, size_t max_chars,
                       const char *string, int *out_height);
int Font_LineHeight(const font_t *font, int scale);

bool Font_IsLegacy(const font_t *font);
qhandle_t Font_LegacyHandle(const font_t *font);

#ifdef __cplusplus
}
#endif
