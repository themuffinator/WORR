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

#include "ui.hpp"
#include "client/input.hpp"
#include "common/prompt.hpp"

uiStatic_t    uis;

LIST_DECL(ui_menus);

cvar_t    *ui_debug;
static cvar_t    *ui_open;
static cvar_t    *ui_scale;

#define UI_COMPOSITOR_FADE_TIME	200
#define UI_COMPOSITOR_SLIDE_PIXELS	32

typedef enum uiTransitionType_e {
	UI_TRANSITION_NONE,
	UI_TRANSITION_FADE_IN,
	UI_TRANSITION_FADE_OUT,
	UI_TRANSITION_SLIDE_IN,
	UI_TRANSITION_SLIDE_OUT
} uiTransitionType_t;

typedef struct uiLayerState_s {
	menuFrameWork_t *menu;
	float startOpacity;
	float targetOpacity;
	float opacity;
	vec2_t slideStart;
	vec2_t slideTarget;
	vec2_t slide;
	uiTransitionType_t transition;
	unsigned startTime;
	unsigned duration;
	bool modal;
	bool passthrough;
	bool drawBackdrop;
} uiLayerState_t;

typedef struct uiCompositor_s {
	uiLayerState_t layers[MAX_MENU_DEPTH];
	int count;

static uiCompositor_t ui_compositor;


/*
=============
UI_CompositorReset

Clears any cached compositor state when the menu stack is torn down.
=============
*/
static void UI_CompositorReset(void)
{
	memset(&ui_compositor, 0, sizeof(ui_compositor));
}

/*
=============
UI_CompositorLerp

Returns a clamped linear interpolation value for transition curves.
=============
*/
static float UI_CompositorLerp(float from, float to, float frac)
{
	if (frac < 0.0f) {
		frac = 0.0f;
	} else if (frac > 1.0f) {
		frac = 1.0f;
	}

	return from + (to - from) * frac;
}

/*
=============
UI_CompositorFindLayer

Looks for cached layer information for the provided menu.
=============
*/
static uiLayerState_t *UI_CompositorFindLayer(menuFrameWork_t *menu)
{
	for (int i = 0; i < ui_compositor.count; i++) {
		uiLayerState_t *layer = &ui_compositor.layers[i];
		if (layer->menu == menu) {
			return layer;
		}
	}

	return NULL;
}

/*
=============
UI_CompositorSync

Rebuilds the compositor layer list from the stacked menus while
preserving transition progress for existing layers.
=============
*/
static void UI_CompositorSync(void)
{
	uiLayerState_t rebuilt[MAX_MENU_DEPTH];
	int rebuildCount = 0;

	for (int i = 0; i < uis.menuDepth && rebuildCount < MAX_MENU_DEPTH; i++) {
		menuFrameWork_t *menu = uis.layers[i];
		uiLayerState_t *existing = UI_CompositorFindLayer(menu);
		uiLayerState_t *dest = &rebuilt[rebuildCount++];

		if (existing) {
			*dest = *existing;
		} else {
			memset(dest, 0, sizeof(*dest));
			dest->menu = menu;
			dest->startOpacity = 0.0f;
			dest->targetOpacity = menu->opacity > 0.0f ? menu->opacity : 1.0f;
			dest->opacity = dest->startOpacity;
			dest->transition = UI_TRANSITION_FADE_IN;
			dest->startTime = uis.realtime;
			dest->duration = UI_COMPOSITOR_FADE_TIME;
			dest->slideStart[0] = 0.0f;
			dest->slideStart[1] = UI_COMPOSITOR_SLIDE_PIXELS;
			dest->slideTarget[0] = 0.0f;
			dest->slideTarget[1] = 0.0f;
			Vector2Copy(dest->slideStart, dest->slide);
		}

		dest->modal = menu->modal || !menu->allowInputPassthrough;
		dest->passthrough = menu->allowInputPassthrough;
		dest->drawBackdrop = menu->drawsBackdrop || (menu->modal && !menu->transparent);
	}

	ui_compositor.count = rebuildCount;
	for (int i = 0; i < rebuildCount; i++) {
		ui_compositor.layers[i] = rebuilt[i];
	}
}

/*
=============
UI_CompositorUpdateLayer

Applies transition math to a layer prior to drawing.
=============
*/
static void UI_CompositorUpdateLayer(uiLayerState_t *layer)
{
	float frac = 1.0f;

	if (layer->duration) {
		unsigned elapsed = uis.realtime - layer->startTime;
		if (elapsed < layer->duration) {
			frac = static_cast<float>(elapsed) / static_cast<float>(layer->duration);
		}
	}

	switch (layer->transition) {
	case UI_TRANSITION_FADE_IN:
	case UI_TRANSITION_NONE:
		layer->opacity = UI_CompositorLerp(layer->startOpacity, layer->targetOpacity, frac);
		break;
	case UI_TRANSITION_FADE_OUT:
		layer->opacity = UI_CompositorLerp(layer->startOpacity, 0.0f, frac);
		break;
	case UI_TRANSITION_SLIDE_IN:
		Vector2Set(layer->slide,
			UI_CompositorLerp(layer->slideStart[0], layer->slideTarget[0], frac),
			UI_CompositorLerp(layer->slideStart[1], layer->slideTarget[1], frac));
		layer->opacity = UI_CompositorLerp(layer->startOpacity, layer->targetOpacity, frac);
		break;
	case UI_TRANSITION_SLIDE_OUT:
		Vector2Set(layer->slide,
			UI_CompositorLerp(layer->slideStart[0], layer->slideTarget[0], frac),
			UI_CompositorLerp(layer->slideStart[1], layer->slideTarget[1], frac));
		layer->opacity = UI_CompositorLerp(layer->startOpacity, 0.0f, frac);
		break;
	}
}

/*
=============
UI_DrawBackdropForLayer

Draws a dimmed overlay for modal layers.
=============
*/
static void UI_DrawBackdropForLayer(const uiLayerState_t *layer)
{
	if (!layer->drawBackdrop) {
		return;
	}

	int alpha = static_cast<int>(layer->opacity * 160.0f);
	if (alpha < 0) {
		alpha = 0;
	} else if (alpha > 255) {
		alpha = 255;
	}
	R_DrawFill32(Q_rint(layer->slide[0]), Q_rint(layer->slide[1]), uis.width, uis.height,
		ColorSetAlpha(COLOR_BLACK, static_cast<uint8_t>(alpha)));
}


typedef struct uiColorStack_s {
	color_t background;
	color_t normal;
	color_t active;
	color_t selection;
	color_t disabled;
} uiColorStack_t;

/*
=============
UI_CompositorPushOpacity

Modulates UI palette colors to respect a layer's opacity.
=============
*/
static void UI_CompositorPushOpacity(uiColorStack_t *backup, float opacity)
{
	*backup = uis.color;
	if (opacity >= 1.0f) {
		return;
	}

	int scaled = static_cast<int>(opacity * 255.0f);
	if (scaled < 0) {
		scaled = 0;
	} else if (scaled > 255) {
		scaled = 255;
	}
	uis.color.background = ColorSetAlpha(backup->background, static_cast<uint8_t>(scaled));
	uis.color.normal = ColorSetAlpha(backup->normal, static_cast<uint8_t>(scaled));
	uis.color.active = ColorSetAlpha(backup->active, static_cast<uint8_t>(scaled));
	uis.color.selection = ColorSetAlpha(backup->selection, static_cast<uint8_t>(scaled));
	uis.color.disabled = ColorSetAlpha(backup->disabled, static_cast<uint8_t>(scaled));
}

/*
=============
UI_CompositorPopOpacity

Restores the UI palette after a composited layer has been drawn.
=============
*/
static void UI_CompositorPopOpacity(const uiColorStack_t *backup)
{
	uis.color = *backup;
}

/*
=============
UI_UpdateActiveMenuFromStack

Ensures the active menu reference points at the topmost layer.
=============
*/
static void UI_UpdateActiveMenuFromStack(void)
{
	uis.activeMenu = NULL;
	for (int i = uis.menuDepth - 1; i >= 0; i--) {
		if (uis.layers[i]) {
			uis.activeMenu = uis.layers[i];
			break;
		}
	}
}

/*
=============
UI_ApplyMenuDefaults

Initializes modal and opacity defaults for a menu layer.
=============
*/
static void UI_ApplyMenuDefaults(menuFrameWork_t *menu)
{
	if (!menu->modal && !menu->allowInputPassthrough) {
		menu->modal = true;
	}

	if (menu->opacity <= 0.0f || menu->opacity > 1.0f) {
		menu->opacity = 1.0f;
	}

	if (!menu->drawsBackdrop && menu->modal && !menu->transparent) {
		menu->drawsBackdrop = true;
	}
}

// ===========================================================================

/*
=================
UI_PushMenu
=================
*/
void UI_PushMenu(menuFrameWork_t *menu)
{
	int i, j;

	if (!menu) {
		return;
	}

	// if this menu is already present, drop back to that level
	// to avoid stacking menus by hotkeys
	for (i = 0; i < uis.menuDepth; i++) {
		if (uis.layers[i] == menu) {
			break;
		}
	}

	if (i == uis.menuDepth) {
		if (uis.menuDepth >= MAX_MENU_DEPTH) {
			Com_EPrintf("UI_PushMenu: MAX_MENU_DEPTH exceeded\n");
			return;
		}
		uis.layers[uis.menuDepth++] = menu;
	} else {
		for (j = i; j < uis.menuDepth; j++) {
			UI_PopMenu();
		}
		uis.menuDepth = i + 1;
	}

	if (menu->push && !menu->push(menu)) {
		uis.menuDepth--;
		return;
	}

	UI_ApplyMenuDefaults(menu);

	Menu_Init(menu);

	Key_SetDest(Key_FromMask((Key_GetDest() & ~KEY_CONSOLE) | KEY_MENU));

	Con_Close(true);

	if (!uis.activeMenu) {
		// opening menu moves cursor to the nice location
		IN_WarpMouse(menu->mins[0] / uis.scale, menu->mins[1] / uis.scale);

		uis.mouseCoords[0] = menu->mins[0];
		uis.mouseCoords[1] = menu->mins[1];

		uis.entersound = true;
	}

	uis.activeMenu = menu;
	UI_UpdateActiveMenuFromStack();

	UI_DoHitTest();
	UI_CompositorSync();

	if (menu->expose) {
		menu->expose(menu);
	}
}

static void UI_Resize(void)
{
    int i;

    uis.scale = R_ClampScale(ui_scale);
    uis.width = Q_rint(r_config.width * uis.scale);
    uis.height = Q_rint(r_config.height * uis.scale);

    for (i = 0; i < uis.menuDepth; i++) {
        Menu_Init(uis.layers[i]);
    }

    //CL_WarpMouse(0, 0);
}


/*
=================
UI_ForceMenuOff
=================
*/
void UI_ForceMenuOff(void)
{
	menuFrameWork_t *menu;
	int i;

	for (i = 0; i < uis.menuDepth; i++) {
		menu = uis.layers[i];
		if (menu->pop) {
			menu->pop(menu);
		}
	}

	Key_SetDest(Key_FromMask(Key_GetDest() & ~KEY_MENU));
	uis.menuDepth = 0;
	uis.activeMenu = NULL;
	uis.mouseTracker = NULL;
	uis.transparent = false;
	UI_CompositorReset();
	UI_UpdateActiveMenuFromStack();
}

/*
=================
UI_PopMenu
=================
*/
void UI_PopMenu(void)
{
	menuFrameWork_t *menu;

	Q_assert(uis.menuDepth > 0);

	menu = uis.layers[--uis.menuDepth];
	if (menu->pop) {
		menu->pop(menu);
	}

	if (!uis.menuDepth) {
		UI_ForceMenuOff();
		return;
	}

	uis.activeMenu = uis.layers[uis.menuDepth - 1];
	uis.mouseTracker = NULL;
	UI_UpdateActiveMenuFromStack();
	UI_CompositorSync();

	UI_DoHitTest();
}

/*
=================
UI_IsTransparent
=================
*/
bool UI_IsTransparent(void)
{
    if (!(Key_GetDest() & KEY_MENU)) {
        return true;
    }

    if (!uis.activeMenu) {
        return true;
    }

    return uis.activeMenu->transparent;
}

menuFrameWork_t *UI_FindMenu(const char *name)
{
    menuFrameWork_t *menu;

    LIST_FOR_EACH(menuFrameWork_t, menu, &ui_menus, entry) {
        if (!strcmp(menu->name, name)) {
            return menu;
        }
    }

    return NULL;
}

/*
=================
UI_OpenMenu
=================
*/
void UI_OpenMenu(uiMenu_t type)
{
    menuFrameWork_t *menu = NULL;

    if (!uis.initialized) {
        return;
    }

    // close any existing menus
    UI_ForceMenuOff();

    switch (type) {
    case UIMENU_DEFAULT:
        if (ui_open->integer) {
            menu = UI_FindMenu("main");
        }
        break;
    case UIMENU_MAIN:
        menu = UI_FindMenu("main");
        break;
    case UIMENU_GAME:
        menu = UI_FindMenu("game");
        if (!menu) {
            menu = UI_FindMenu("main");
        }
        break;
    case UIMENU_NONE:
        break;
    default:
        Q_assert(!"bad menu");
    }

    UI_PushMenu(menu);
}

//=============================================================================

/*
=================
UI_FormatColumns
=================
*/
void *UI_FormatColumns(int extrasize, ...)
{
    va_list argptr;
    char *buffer, *p;
    int i, j;
    size_t total = 0;
    char *strings[MAX_COLUMNS];
    size_t lengths[MAX_COLUMNS];

    va_start(argptr, extrasize);
    for (i = 0; i < MAX_COLUMNS; i++) {
        if ((p = va_arg(argptr, char *)) == NULL) {
            break;
        }
        strings[i] = p;
        total += lengths[i] = strlen(p) + 1;
    }
    va_end(argptr);

    buffer = UI_Malloc(extrasize + total + 1);
    p = buffer + extrasize;
    for (j = 0; j < i; j++) {
        memcpy(p, strings[j], lengths[j]);
        p += lengths[j];
    }
    *p = 0;

    return buffer;
}

char *UI_GetColumn(char *s, int n)
{
    int i;

    for (i = 0; i < n && *s; i++) {
        s += strlen(s) + 1;
    }

    return s;
}

/*
=================
UI_CursorInRect
=================
*/
bool UI_CursorInRect(const vrect_t *rect)
{
    if (uis.mouseCoords[0] < rect->x) {
        return false;
    }
    if (uis.mouseCoords[0] >= rect->x + rect->width) {
        return false;
    }
    if (uis.mouseCoords[1] < rect->y) {
        return false;
    }
    if (uis.mouseCoords[1] >= rect->y + rect->height) {
        return false;
    }
    return true;
}

// nb: all UI strings are drawn at full alpha
void UI_DrawString(int x, int y, int flags, color_t color, const char *string)
{
    SCR_DrawStringStretch(x, y, 1, flags, MAX_STRING_CHARS, string,
                          ColorSetAlpha(color, static_cast<uint8_t>(255)), uis.fontHandle);
}

// nb: all UI chars are drawn at full alpha
void UI_DrawChar(int x, int y, int flags, color_t color, int ch)
{
    R_DrawChar(x, y, flags, ch, ColorSetAlpha(color, static_cast<uint8_t>(255)), uis.fontHandle);
}

void UI_StringDimensions(vrect_t *rc, int flags, const char *string)
{
    rc->height = SCR_FontLineHeight(1, uis.fontHandle);
    rc->width = SCR_MeasureString(1, flags & ~UI_MULTILINE, MAX_STRING_CHARS, string, uis.fontHandle);

    if ((flags & UI_CENTER) == UI_CENTER) {
        rc->x -= rc->width / 2;
    } else if (flags & UI_RIGHT) {
        rc->x -= rc->width;
    }
}

void UI_DrawRect8(const vrect_t *rc, int border, int c)
{
    R_DrawFill8(rc->x, rc->y, border, rc->height, c);   // left
    R_DrawFill8(rc->x + rc->width - border, rc->y, border, rc->height, c);   // right
    R_DrawFill8(rc->x + border, rc->y, rc->width - border * 2, border, c);   // top
    R_DrawFill8(rc->x + border, rc->y + rc->height - border, rc->width - border * 2, border, c);   // bottom
}

//=============================================================================
/* Menu Subsystem */

/*
=================
UI_DoHitTest
=================
*/
bool UI_DoHitTest(void)
{
menuCommon_t *item = NULL;

	if (!uis.menuDepth) {
		return false;
	}

	for (int i = uis.menuDepth - 1; i >= 0; i--) {
		menuFrameWork_t *menu = uis.layers[i];
		if (uis.mouseTracker && uis.mouseTracker->parent == menu) {
			item = uis.mouseTracker;
		} else {
			item = Menu_HitTest(menu);
		}

		if (item && UI_IsItemSelectable(item)) {
			Menu_MouseMove(item);

			if (!(item->flags & QMF_HASFOCUS)) {
				Menu_SetFocus(item);
			}

			uis.activeMenu = menu;
			return true;
		}

		if (menu->modal) {
			break;
		}
	}

UI_UpdateActiveMenuFromStack();
return false;
}

/*
=============
UI_DispatchKeyToLayers

Routes key presses through the stacked menus honoring modal barriers.
=============
*/
static menuSound_t UI_DispatchKeyToLayers(int key)
{
	menuSound_t sound = QMS_NOTHANDLED;

	for (int i = uis.menuDepth - 1; i >= 0; i--) {
		menuFrameWork_t *menu = uis.layers[i];
		sound = Menu_Keydown(menu, key);
		if (sound != QMS_NOTHANDLED) {
			uis.activeMenu = menu;
			return sound;
		}

		if (menu->modal) {
			return sound;
		}
	}

	UI_UpdateActiveMenuFromStack();
	return sound;
}

/*
=============
UI_DispatchCharToLayers

Routes printable character input with pass-through semantics.
=============
*/
static menuSound_t UI_DispatchCharToLayers(int key)
{
	for (int i = uis.menuDepth - 1; i >= 0; i--) {
		menuFrameWork_t *menu = uis.layers[i];
		menuCommon_t *item = Menu_ItemAtCursor(menu);
		menuSound_t sound = QMS_NOTHANDLED;

		if (item) {
			sound = Menu_CharEvent(item, key);
		}

		if (sound != QMS_NOTHANDLED) {
			uis.activeMenu = menu;
			return sound;
		}

		if (menu->modal) {
			return sound;
		}
	}

	UI_UpdateActiveMenuFromStack();
	return QMS_NOTHANDLED;
}

/*
=================
UI_MouseEvent
=================
*/
void UI_MouseEvent(int x, int y)
{
	x = Q_clip(x, 0, r_config.width - 1);
	y = Q_clip(y, 0, r_config.height - 1);

	uis.mouseCoords[0] = Q_rint(x * uis.scale);
	uis.mouseCoords[1] = Q_rint(y * uis.scale);

	UI_DoHitTest();
}

/*
=================
UI_Draw
=================
*/
void UI_Draw(unsigned realtime)
{
	uiColorStack_t colors;

	uis.realtime = realtime;

	if (!(Key_GetDest() & KEY_MENU)) {
		return;
	}

	if (!uis.menuDepth) {
		return;
	}

	UI_UpdateActiveMenuFromStack();
	UI_CompositorSync();

	R_SetScale(uis.scale);

	for (int i = 0; i < ui_compositor.count; i++) {
		uiLayerState_t *layer = &ui_compositor.layers[i];
		UI_CompositorUpdateLayer(layer);
		UI_DrawBackdropForLayer(layer);
		UI_CompositorPushOpacity(&colors, layer->opacity);
		if (layer->menu->draw) {
			layer->menu->draw(layer->menu);
		} else {
			Menu_Draw(layer->menu);
		}
		UI_CompositorPopOpacity(&colors);
	}

	if (r_config.flags & QVF_FULLSCREEN) {
		R_DrawPic(uis.mouseCoords[0] - uis.cursorWidth / 2,
				uis.mouseCoords[1] - uis.cursorHeight / 2,
				COLOR_WHITE, uis.cursorHandle);
	}

	if (ui_debug->integer) {
		UI_DrawString(uis.width - 4, 4, UI_RIGHT,
				COLOR_WHITE, va("%3i %3i", uis.mouseCoords[0], uis.mouseCoords[1]));
	}

	if (uis.entersound) {
		uis.entersound = false;
		S_StartLocalSound("misc/menu1.wav");
	}

	R_SetScale(1.0f);
}

void UI_StartSound(menuSound_t sound)
{
    switch (sound) {
    case QMS_IN:
        S_StartLocalSound("misc/menu1.wav");
        break;
    case QMS_MOVE:
        S_StartLocalSound("misc/menu2.wav");
        break;
    case QMS_OUT:
        S_StartLocalSound("misc/menu3.wav");
        break;
    case QMS_BEEP:
        S_StartLocalSound("misc/talk1.wav");
        break;
    default:
        break;
    }
}

/*
=================
UI_KeyEvent
=================
*/
void UI_KeyEvent(int key, bool down)
{
	menuSound_t sound;

	if (!uis.menuDepth) {
		return;
	}

	if (!down) {
		if (key == K_MOUSE1) {
			uis.mouseTracker = NULL;
		}
		return;
	}

	sound = UI_DispatchKeyToLayers(key);

	if (sound != QMS_NOTHANDLED) {
		UI_StartSound(sound);
	}
}

/*
=================
UI_CharEvent
=================
*/
void UI_CharEvent(int key)
{
	menuSound_t sound;

	if (!uis.menuDepth) {
		return;
	}

	sound = UI_DispatchCharToLayers(key);
	if (sound != QMS_NOTHANDLED) {
		UI_StartSound(sound);
	}
}

static void UI_Menu_g(genctx_t *ctx)
{
    menuFrameWork_t *menu;

    LIST_FOR_EACH(menuFrameWork_t, menu, &ui_menus, entry)
        Prompt_AddMatch(ctx, menu->name);
}

static void UI_PushMenu_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        UI_Menu_g(ctx);
    }
}

static void UI_PushMenu_f(void)
{
    menuFrameWork_t *menu;
    char *s;

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: %s <menu>\n", Cmd_Argv(0));
        return;
    }
    s = Cmd_Argv(1);
    menu = UI_FindMenu(s);
    if (menu) {
        UI_PushMenu(menu);
    } else {
        Com_Printf("No such menu: %s\n", s);
    }
}

static void UI_PopMenu_f(void)
{
    if (uis.activeMenu) {
        UI_PopMenu();
    }
}


static const cmdreg_t c_ui[] = {
    { "forcemenuoff", UI_ForceMenuOff },
    { "pushmenu", UI_PushMenu_f, UI_PushMenu_c },
    { "popmenu", UI_PopMenu_f },

    { NULL, NULL }
};

static void ui_scale_changed(cvar_t *self)
{
    UI_Resize();
}

void UI_ModeChanged(void)
{
    ui_scale = Cvar_Get("ui_scale", "0", 0);
    ui_scale->changed = ui_scale_changed;
    UI_Resize();
}

static void UI_FreeMenus(void)
{
    menuFrameWork_t *menu, *next;

    LIST_FOR_EACH_SAFE(menuFrameWork_t, menu, next, &ui_menus, entry) {
        if (menu->free) {
            menu->free(menu);
        }
    }
    List_Init(&ui_menus);
}


/*
=================
UI_Init
=================
*/
void UI_Init(void)
{
    Cmd_Register(c_ui);

    ui_debug = Cvar_Get("ui_debug", "0", 0);
    ui_open = Cvar_Get("ui_open", "0", 0);

    UI_ModeChanged();

    uis.fontHandle = SCR_RegisterFontPath("conchars.pcx");
    uis.cursorHandle = R_RegisterPic("ch1");
    R_GetPicSize(&uis.cursorWidth, &uis.cursorHeight, uis.cursorHandle);

    for (int i = 0; i < NUM_CURSOR_FRAMES; i++) {
        uis.bitmapCursors[i] = R_RegisterPic(va("m_cursor%d", i));
    }

    uis.color.background    = ColorRGBA(0,   0,   0,   255);
    uis.color.normal        = ColorRGBA(15,  128, 235, 100);
    uis.color.active        = ColorRGBA(15,  128, 235, 100);
    uis.color.selection     = ColorRGBA(15,  128, 235, 100);
    uis.color.disabled      = ColorRGBA(127, 127, 127, 255);

    strcpy(uis.weaponModel, "w_railgun.md2");

    // load mapdb
    UI_MapDB_Init();

    // load custom menus
    UI_LoadScript();

    // load built-in menus
    M_Menu_PlayerConfig();
    M_Menu_Servers();
    M_Menu_Demos();

    Com_DPrintf("Registered %d menus.\n", List_Count(&ui_menus));

    uis.initialized = true;
}

/*
=================
UI_Shutdown
=================
*/
void UI_Shutdown(void)
{
    if (!uis.initialized) {
        return;
    }
    UI_ForceMenuOff();

    ui_scale->changed = NULL;

    PlayerModel_Free();

    UI_FreeMenus();

    Cmd_Deregister(c_ui);

    memset(&uis, 0, sizeof(uis));

    UI_MapDB_Shutdown();

    Z_LeakTest(TAG_UI);
}
