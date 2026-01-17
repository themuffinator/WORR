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

static const cgame_ui_export_t ui_exports = {
    CGAME_UI_API_VERSION,
    UI_Init,
    UI_Shutdown,
    UI_ModeChanged,
    UI_KeyEvent,
    UI_CharEvent,
    UI_Draw,
    UI_OpenMenu,
    UI_Frame,
    UI_StatusEvent,
    UI_ErrorEvent,
    UI_MouseEvent,
    UI_IsTransparent,
};

extern "C" const cgame_ui_export_t *CG_GetUIAPI(void)
{
    return &ui_exports;
}
