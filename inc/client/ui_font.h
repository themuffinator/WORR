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

#include "shared/shared.h"
#include "renderer/renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

void UI_FontInit(void);
void UI_FontShutdown(void);
void UI_FontModeChanged(void);

int UI_FontDrawString(int x, int y, int flags, size_t max_chars,
                      const char *string, color_t color);
int UI_FontMeasureString(int flags, size_t max_chars, const char *string,
                         int *out_height);
int UI_FontLineHeight(int scale);
int UI_FontDrawStringSized(int x, int y, int flags, size_t max_chars,
                            const char *string, color_t color, int size);
int UI_FontMeasureStringSized(int flags, size_t max_chars, const char *string,
                               int *out_height, int size);
int UI_FontLineHeightSized(int size);
qhandle_t UI_FontLegacyHandle(void);

#ifdef __cplusplus
}
#endif
