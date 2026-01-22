/*
Copyright (C) 2003-2006 Andrey Nazarov

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

//
// field.c
//

#include "shared/shared.h"
#include "common/common.h"
#include "common/field.h"
#include "client/client.h"
#include "client/keys.h"
#include "client/video.h"
#include "renderer/renderer.h"

/*
================
IF_Init
================
*/
void IF_Init(inputField_t *field, size_t visibleChars, size_t maxChars)
{
    memset(field, 0, sizeof(*field));
    field->maxChars = min(maxChars, sizeof(field->text) - 1);
    field->visibleChars = min(visibleChars, field->maxChars);
}

/*
================
IF_Clear
================
*/
void IF_Clear(inputField_t *field)
{
    memset(field->text, 0, sizeof(field->text));
    field->cursorPos = 0;
}

/*
================
IF_Replace
================
*/
void IF_Replace(inputField_t *field, const char *text)
{
    if (field->maxChars && text) {
        size_t len = Q_strlcpy(field->text, text, field->maxChars + 1);
        field->cursorPos = min(len, field->maxChars - 1);
    } else {
        field->text[0] = 0;
        field->cursorPos = 0;
    }
}

#if USE_CLIENT

static size_t IF_ByteLength(const inputField_t *field)
{
    return field ? strlen(field->text) : 0;
}

static size_t IF_CursorChars(const inputField_t *field)
{
    if (!field)
        return 0;
    return UTF8_CountChars(field->text, field->cursorPos);
}

static size_t IF_OffsetForChars(const inputField_t *field, size_t chars)
{
    if (!field)
        return 0;
    return UTF8_OffsetForChars(field->text, chars);
}

/*
================
IF_KeyEvent
================
*/
bool IF_KeyEvent(inputField_t *field, int key)
{
    if (!field->maxChars) {
        return false;
    }
    Q_assert(field->cursorPos < field->maxChars);

    if (key == K_DEL && Key_IsDown(K_CTRL)) {
        size_t cursor_chars = IF_CursorChars(field);
        size_t total_chars = UTF8_CountChars(field->text, IF_ByteLength(field));
        size_t chars = cursor_chars;

        // kill leading whitespace
        while (chars < total_chars) {
            size_t offset = IF_OffsetForChars(field, chars);
            unsigned char c = (unsigned char)field->text[offset];
            if (!c || c > 32)
                break;
            chars++;
        }

        // kill this word
        while (chars < total_chars) {
            size_t offset = IF_OffsetForChars(field, chars);
            unsigned char c = (unsigned char)field->text[offset];
            if (!c || c <= 32)
                break;
            chars++;
        }

        size_t end = IF_OffsetForChars(field, chars);
        memmove(field->text + field->cursorPos, field->text + end,
                sizeof(field->text) - end);
        return true;
    }

    if ((key == K_BACKSPACE || key == 'w') && Key_IsDown(K_CTRL)) {
        size_t cursor_chars = IF_CursorChars(field);
        size_t chars = cursor_chars;

        // kill trailing whitespace
        while (chars > 0) {
            size_t offset = IF_OffsetForChars(field, chars - 1);
            unsigned char c = (unsigned char)field->text[offset];
            if (c > 32)
                break;
            chars--;
        }

        // kill this word
        while (chars > 0) {
            size_t offset = IF_OffsetForChars(field, chars - 1);
            unsigned char c = (unsigned char)field->text[offset];
            if (c <= 32)
                break;
            chars--;
        }

        size_t start = IF_OffsetForChars(field, chars);
        memmove(field->text + start, field->text + field->cursorPos,
                sizeof(field->text) - field->cursorPos);
        field->cursorPos = start;
        return true;
    }

    if (key == K_DEL) {
        size_t len = IF_ByteLength(field);
        if (field->cursorPos < len) {
            size_t cursor_chars = IF_CursorChars(field);
            size_t next = IF_OffsetForChars(field, cursor_chars + 1);
            memmove(field->text + field->cursorPos, field->text + next,
                    sizeof(field->text) - next);
        }
        return true;
    }

    if (key == K_BACKSPACE || (key == 'h' && Key_IsDown(K_CTRL))) {
        if (field->cursorPos > 0) {
            size_t cursor_chars = IF_CursorChars(field);
            if (cursor_chars > 0) {
                size_t prev = IF_OffsetForChars(field, cursor_chars - 1);
                memmove(field->text + prev, field->text + field->cursorPos,
                        sizeof(field->text) - field->cursorPos);
                field->cursorPos = prev;
            }
        }
        return true;
    }

    if (key == 'u' && Key_IsDown(K_CTRL)) {
        memmove(field->text, field->text + field->cursorPos,
                sizeof(field->text) - field->cursorPos);
        field->cursorPos = 0;
        return true;
    }

    if (key == 'k' && Key_IsDown(K_CTRL)) {
        field->text[field->cursorPos] = 0;
        return true;
    }

    if (key == 'c' && Key_IsDown(K_CTRL)) {
        if (vid && vid->set_clipboard_data)
            vid->set_clipboard_data(field->text);
        return true;
    }

    if ((key == K_LEFTARROW && Key_IsDown(K_CTRL)) || (key == 'b' && Key_IsDown(K_ALT))) {
        size_t cursor_chars = IF_CursorChars(field);
        size_t chars = cursor_chars;

        while (chars > 0) {
            size_t offset = IF_OffsetForChars(field, chars - 1);
            unsigned char c = (unsigned char)field->text[offset];
            if (c > 32)
                break;
            chars--;
        }
        while (chars > 0) {
            size_t offset = IF_OffsetForChars(field, chars - 1);
            unsigned char c = (unsigned char)field->text[offset];
            if (c <= 32)
                break;
            chars--;
        }

        field->cursorPos = IF_OffsetForChars(field, chars);
        return true;
    }

    if ((key == K_RIGHTARROW && Key_IsDown(K_CTRL)) || (key == 'f' && Key_IsDown(K_ALT))) {
        size_t cursor_chars = IF_CursorChars(field);
        size_t total_chars = UTF8_CountChars(field->text, IF_ByteLength(field));
        size_t chars = cursor_chars;

        while (chars < total_chars) {
            size_t offset = IF_OffsetForChars(field, chars);
            unsigned char c = (unsigned char)field->text[offset];
            if (!c || c > 32)
                break;
            chars++;
        }
        while (chars < total_chars) {
            size_t offset = IF_OffsetForChars(field, chars);
            unsigned char c = (unsigned char)field->text[offset];
            if (!c || c <= 32)
                break;
            chars++;
        }

        field->cursorPos = IF_OffsetForChars(field, chars);
        goto check;
    }

    if (key == K_LEFTARROW || (key == 'b' && Key_IsDown(K_CTRL))) {
        if (field->cursorPos > 0) {
            size_t cursor_chars = IF_CursorChars(field);
            if (cursor_chars > 0)
                field->cursorPos = IF_OffsetForChars(field, cursor_chars - 1);
        }
        return true;
    }

    if (key == K_RIGHTARROW || (key == 'f' && Key_IsDown(K_CTRL))) {
        size_t len = IF_ByteLength(field);
        if (field->cursorPos < len) {
            size_t cursor_chars = IF_CursorChars(field);
            field->cursorPos = IF_OffsetForChars(field, cursor_chars + 1);
        }
        goto check;
    }

    if (key == K_HOME || (key == 'a' && Key_IsDown(K_CTRL))) {
        field->cursorPos = 0;
        return true;
    }

    if (key == K_END || (key == 'e' && Key_IsDown(K_CTRL))) {
        field->cursorPos = IF_ByteLength(field);
        goto check;
    }

    if (key == K_INS) {
        Key_SetOverstrikeMode(Key_GetOverstrikeMode() ^ 1);
        return true;
    }

    return false;

check:
    field->cursorPos = min(field->cursorPos, field->maxChars - 1);
    field->cursorPos = min(field->cursorPos, IF_ByteLength(field));
    return true;
}

/*
================
IF_CharEvent
================
*/
bool IF_CharEvent(inputField_t *field, int key)
{
    if (!field->maxChars) {
        return false;
    }
    Q_assert(field->cursorPos < field->maxChars);

    if (key < 32) {
        return false;   // non printable
    }

    if ((uint32_t)key > UNICODE_MAX) {
        return false;
    }

    char encoded[4];
    size_t encoded_len = UTF8_EncodeCodePoint((uint32_t)key, encoded, sizeof(encoded));
    if (!encoded_len) {
        return false;
    }

    size_t len = IF_ByteLength(field);
    if (field->cursorPos > len)
        field->cursorPos = len;

    if (Key_GetOverstrikeMode() && field->cursorPos < len) {
        size_t cursor_chars = IF_CursorChars(field);
        size_t next = IF_OffsetForChars(field, cursor_chars + 1);
        memmove(field->text + field->cursorPos, field->text + next,
                sizeof(field->text) - next);
        len = IF_ByteLength(field);
    }

    if (len + encoded_len >= field->maxChars) {
        if (field->cursorPos < len) {
            size_t cursor_chars = IF_CursorChars(field);
            size_t next = IF_OffsetForChars(field, cursor_chars + 1);
            memmove(field->text + field->cursorPos, field->text + next,
                    sizeof(field->text) - next);
            len = IF_ByteLength(field);
        }
    }

    if (len + encoded_len >= field->maxChars) {
        return false;
    }

    if (!Key_GetOverstrikeMode()) {
        memmove(field->text + field->cursorPos + encoded_len,
                field->text + field->cursorPos,
                sizeof(field->text) - field->cursorPos - encoded_len);
    }

    memcpy(field->text + field->cursorPos, encoded, encoded_len);
    field->cursorPos += encoded_len;
    field->text[field->maxChars] = 0;

    return true;
}

/*
================
IF_Draw

The input line scrolls horizontally if typing goes beyond the right edge.
Returns x offset of the rightmost character drawn.
================
*/
int IF_Draw(const inputField_t *field, int x, int y, int flags, qhandle_t font)
{
    const char *text = field->text;
    size_t cursorPos = field->cursorPos;
    size_t offset = 0;
    size_t cursor_chars = UTF8_CountChars(field->text, cursorPos);
    int ret;

    if (!field->maxChars || !field->visibleChars) {
        return 0;
    }

    Q_assert(cursorPos < field->maxChars);

    // scroll horizontally (codepoint-aware)
    if (cursor_chars >= field->visibleChars) {
        size_t offset_chars = cursor_chars - (field->visibleChars - 1);
        offset = UTF8_OffsetForChars(text, offset_chars);
        cursorPos = field->cursorPos - offset;
    }

    text += offset;
    size_t draw_len = UTF8_OffsetForChars(text, field->visibleChars);
    size_t cursor_chars_visible = UTF8_CountChars(text, cursorPos);

    // draw text
    ret = R_DrawString(x, y, flags, draw_len, text, COLOR_WHITE, font);

    // draw blinking cursor
    if (flags & UI_DRAWCURSOR && com_localTime & BIT(8)) {
        R_DrawChar(x + cursor_chars_visible * CONCHAR_WIDTH, y, flags,
                   Key_GetOverstrikeMode() ? 11 : '_', COLOR_WHITE, font);
    }

    return ret;
}

#endif
