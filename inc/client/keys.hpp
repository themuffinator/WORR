/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "../shared/types.hpp"

//
// these are the key numbers that should be passed to Key_Event
//
#define K_BACKSPACE     8
#define K_TAB           9
#define K_ENTER         13
#define K_PAUSE         19
#define K_ESCAPE        27
#define K_SPACE         32
#define K_DEL           127

// normal keys should be passed as lowercased ascii
#define K_ASCIIFIRST    32
#define K_ASCIILAST     127

#define K_UPARROW       128
#define K_DOWNARROW     129
#define K_LEFTARROW     130
#define K_RIGHTARROW    131

#define K_ALT           132
#define K_CTRL          133
#define K_SHIFT         134
#define K_F1            135
#define K_F2            136
#define K_F3            137
#define K_F4            138
#define K_F5            139
#define K_F6            140
#define K_F7            141
#define K_F8            142
#define K_F9            143
#define K_F10           144
#define K_F11           145
#define K_F12           146
#define K_INS           147
#define K_PGDN          148
#define K_PGUP          149
#define K_HOME          150
#define K_END           151

#define K_102ND         152

#define K_NUMLOCK       153
#define K_CAPSLOCK      154
#define K_SCROLLOCK     155
#define K_LWINKEY       156
#define K_RWINKEY       157
#define K_MENU          158
#define K_PRINTSCREEN   159

#define K_KP_HOME       160
#define K_KP_UPARROW    161
#define K_KP_PGUP       162
#define K_KP_LEFTARROW  163
#define K_KP_5          164
#define K_KP_RIGHTARROW 165
#define K_KP_END        166
#define K_KP_DOWNARROW  167
#define K_KP_PGDN       168
#define K_KP_ENTER      169
#define K_KP_INS        170
#define K_KP_DEL        171
#define K_KP_SLASH      172
#define K_KP_MINUS      173
#define K_KP_PLUS       174
#define K_KP_MULTIPLY   175

// these come paired with legacy K_ALT/K_CTRL/K_SHIFT events
#define K_LALT          180
#define K_RALT          181
#define K_LCTRL         182
#define K_RCTRL         183
#define K_LSHIFT        184
#define K_RSHIFT        185

// mouse buttons generate virtual keys
#define K_MOUSEFIRST    200
#define K_MOUSE1        200
#define K_MOUSE2        201
#define K_MOUSE3        202
#define K_MOUSE4        203
#define K_MOUSE5        204
#define K_MOUSE6        205
#define K_MOUSE7        206
#define K_MOUSE8        207

// mouse wheel generates virtual keys
#define K_MWHEELDOWN    210
#define K_MWHEELUP      211
#define K_MWHEELRIGHT   212
#define K_MWHEELLEFT    213
#define K_MOUSELAST     213

// gamepad and joystick buttons/axes
#define K_PAD_FIRST         214
#define K_PAD_A             214
#define K_PAD_B             215
#define K_PAD_X             216
#define K_PAD_Y             217
#define K_PAD_BACK          218
#define K_PAD_GUIDE         219
#define K_PAD_START         220
#define K_PAD_LSTICK        221
#define K_PAD_RSTICK        222
#define K_PAD_LSHOULDER     223
#define K_PAD_RSHOULDER     224
#define K_PAD_DPAD_UP       225
#define K_PAD_DPAD_DOWN     226
#define K_PAD_DPAD_LEFT     227
#define K_PAD_DPAD_RIGHT    228
#define K_PAD_LTRIGGER      229
#define K_PAD_RTRIGGER      230
#define K_PAD_LSTICK_UP     231
#define K_PAD_LSTICK_DOWN   232
#define K_PAD_LSTICK_LEFT   233
#define K_PAD_LSTICK_RIGHT  234
#define K_PAD_RSTICK_UP     235
#define K_PAD_RSTICK_DOWN   236
#define K_PAD_RSTICK_LEFT   237
#define K_PAD_RSTICK_RIGHT  238
#define K_PAD_PADDLE1       239
#define K_PAD_PADDLE2       240
#define K_PAD_PADDLE3       241
#define K_PAD_PADDLE4       242
#define K_PAD_TOUCHPAD      243
#define K_PAD_MISC1         244
#define K_PAD_MISC2         245
#define K_PAD_MISC3         246
#define K_PAD_MISC4         247
#define K_PAD_MISC5         248
#define K_PAD_MISC6         249
#define K_PAD_MISC7         250
#define K_PAD_MISC8         251
#define K_PAD_LAST          251

typedef enum {
    KEY_GAME    = 0,
    KEY_CONSOLE = BIT(0),
    KEY_MESSAGE = BIT(1),
    KEY_MENU    = BIT(2)
} keydest_t;

constexpr keydest_t Key_FromMask(int mask)
{
    return static_cast<keydest_t>(mask);
}

typedef bool (*keywaitcb_t)(void *arg, int key);

void    Key_Init(void);

void    Key_Event(unsigned key, bool down, unsigned time);
void    Key_Event2(unsigned key, bool down, unsigned time);
void    Key_CharEvent(int key);

bool        Key_GetOverstrikeMode(void);
void        Key_SetOverstrikeMode(bool overstrike);
keydest_t   Key_GetDest(void);
void        Key_SetDest(keydest_t dest);

int         Key_IsDown(int key);
int         Key_AnyKeyDown(void);
void        Key_ClearStates(void);

const char  *Key_KeynumToString(int keynum);
int     Key_StringToKeynum(const char *str);
void    Key_SetBinding(int keynum, const char *binding);
const char  *Key_GetBinding(const char *binding);
int     Key_EnumBindings(int key, const char *binding);
void    Key_WriteBindings(qhandle_t f);

void    Key_WaitKey(keywaitcb_t wait, void *arg);
