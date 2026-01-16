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

#include "client/cgame_ui.h"
#include "client/ui.h"

static cgame_ui_export_t ui_exports = {
    .api_version = CGAME_UI_API_VERSION,

    .Init = UI_Init,
    .Shutdown = UI_Shutdown,
    .ModeChanged = UI_ModeChanged,
    .KeyEvent = UI_KeyEvent,
    .CharEvent = UI_CharEvent,
    .Draw = UI_Draw,
    .OpenMenu = UI_OpenMenu,
    .Frame = UI_Frame,
    .StatusEvent = UI_StatusEvent,
    .ErrorEvent = UI_ErrorEvent,
    .MouseEvent = UI_MouseEvent,
    .IsTransparent = UI_IsTransparent,
};

const cgame_ui_export_t *CG_GetUIAPI(void)
{
    return &ui_exports;
}
