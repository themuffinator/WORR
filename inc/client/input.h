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

#ifdef __cplusplus
extern "C" {
#endif

//
// input.h -- external (non-keyboard) input devices
//

void IN_Frame(void);
void IN_Activate(void);
void IN_WarpMouse(int x, int y);

typedef enum {
    IN_GAMEPAD_AXIS_LEFTX,
    IN_GAMEPAD_AXIS_LEFTY,
    IN_GAMEPAD_AXIS_RIGHTX,
    IN_GAMEPAD_AXIS_RIGHTY,
    IN_GAMEPAD_AXIS_LEFT_TRIGGER,
    IN_GAMEPAD_AXIS_RIGHT_TRIGGER,
    IN_GAMEPAD_AXIS_COUNT
} in_gamepad_axis_t;

void IN_GamepadAxisEvent(in_gamepad_axis_t axis, int value, unsigned time);
void IN_GamepadReset(unsigned time);
bool IN_GamepadEnabled(void);

#ifdef __cplusplus
}
#endif
