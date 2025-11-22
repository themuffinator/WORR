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

#include "ui.hpp"
#include "MenuItem.h"
#include "common/files.hpp"
#include "menu_controls.hpp"

#include <limits.h>
#include <initializer_list>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>
#include <cstring>

class MenuController {
public:
	enum class Action {
		Up,
		Down,
		Left,
		Right,
		Activate,
		Back,
		Primary,
Secondary,
Tertiary,
ClearBinding,
First,
Last,
ScrollUp,
ScrollDown
};

	MenuController();

	menuSound_t HandleKey(menuFrameWork_t *menu, int key);
	void CloseDropdown(menuDropdown_t *d, bool applyHovered);
	void OpenDropdown(menuDropdown_t *d);
	menuDropdown_t *ActiveDropdown(menuFrameWork_t *menu) const;
	void ResetDropdown(menuFrameWork_t *menu);
	bool HasAction(int key, Action action) const;
	std::vector<Action> ActionsForKey(int key) const;

private:
	std::vector<std::pair<int, Action>> bindings;
	menuDropdown_t *activeDropdown;

	void Bind(Action action, std::initializer_list<int> keys);
	menuSound_t HandleDropdownAction(menuFrameWork_t *menu, menuDropdown_t *dropdown, Action action);
	menuSound_t HandleMenuAction(menuFrameWork_t *menu, Action action, int key);
	menuSound_t HandlePointerAction(menuFrameWork_t *menu, Action action, int key);
};

static void Dropdown_Close(menuDropdown_t *d, bool applyHovered);

/*
=============
UI_MenuController

Returns the shared menu input controller.
=============
*/
static MenuController &UI_MenuController()
{
	static MenuController controller;
	return controller;
}

/*
=============
MenuController

Initializes the menu controller with default bindings and no active dropdown.
=============
*/
MenuController::MenuController()
	: activeDropdown(NULL)
{
	Bind(Action::Back, { K_ESCAPE, K_MOUSE2 });
	Bind(Action::Up, { K_KP_UPARROW, K_UPARROW, 'k' });
	Bind(Action::Down, { K_KP_DOWNARROW, K_DOWNARROW, 'j', K_TAB });
Bind(Action::Left, { K_KP_LEFTARROW, K_LEFTARROW, 'h', K_MWHEELDOWN });
Bind(Action::Right, { K_KP_RIGHTARROW, K_RIGHTARROW, 'l', K_MWHEELUP });
	Bind(Action::Activate, { K_SPACE, K_ENTER, K_KP_ENTER });
Bind(Action::Primary, { K_MOUSE1, K_MOUSE3 });
Bind(Action::Secondary, { K_MOUSE2 });
Bind(Action::Tertiary, { });
Bind(Action::ClearBinding, { K_BACKSPACE, K_DEL });
Bind(Action::First, { K_HOME, K_KP_HOME });
Bind(Action::Last, { K_END, K_KP_END });
Bind(Action::ScrollUp, { K_MWHEELUP });
Bind(Action::ScrollDown, { K_MWHEELDOWN });
}

/*
=============
MenuController::Bind

Registers one or more keys for a given action.
=============
*/
void MenuController::Bind(Action action, std::initializer_list<int> keys)
{
	for (int key : keys) {
		bindings.emplace_back(key, action);
	}
}

/*
=============
MenuController::HasAction

Returns whether a key is bound to the provided action.
=============
*/
bool MenuController::HasAction(int key, Action action) const
{
	for (const auto &binding : bindings) {
		if (binding.first == key && binding.second == action) {
			return true;
		}
	}

	return false;
}

/*
=============
MenuController::ActionsForKey

Collects the actions mapped to a key in insertion order.
=============
*/
std::vector<MenuController::Action> MenuController::ActionsForKey(int key) const
{
	std::vector<Action> results;

	for (const auto &binding : bindings) {
		if (binding.first == key) {
			results.push_back(binding.second);
		}
	}

	return results;
}

static bool UI_TestCondition(const uiItemCondition_t *condition)
{
    if (!condition)
        return true;

    const char *currentString = "";
    float currentValue = 0.0f;

    if (condition->cvar) {
        currentString = condition->cvar->string ? condition->cvar->string : "";
        currentValue = condition->cvar->value;
    }

    switch (condition->op) {
    case UI_CONDITION_EQUALS:
        if (!condition->value)
            return currentString[0] == '\0';
        return !strcmp(currentString, condition->value);
    case UI_CONDITION_NOT_EQUALS:
        if (!condition->value)
            return currentString[0] != '\0';
        return strcmp(currentString, condition->value) != 0;
    case UI_CONDITION_GREATER:
        return currentValue > condition->numericValue;
    case UI_CONDITION_GREATER_EQUAL:
        return currentValue >= condition->numericValue;
    case UI_CONDITION_LESS:
        return currentValue < condition->numericValue;
    case UI_CONDITION_LESS_EQUAL:
        return currentValue <= condition->numericValue;
    }

    return false;
}

static bool UI_EvaluateConditionAll(const uiItemCondition_t *conditions)
{
    for (const uiItemCondition_t *it = conditions; it; it = it->next) {
        if (!UI_TestCondition(it))
            return false;
    }
    return true;
}

static bool UI_EvaluateConditionAny(const uiItemCondition_t *conditions)
{
    for (const uiItemCondition_t *it = conditions; it; it = it->next) {
        if (UI_TestCondition(it))
            return true;
    }
    return false;
}

static void Menu_UpdateConditionalState(menuFrameWork_t *menu)
{
	const int itemCount = Menu_ItemCount(menu);

	for (int i = 0; i < itemCount; i++) {
		auto *item = static_cast<menuCommon_t *>(menu->items[i]);
		bool disabled = item->defaultDisabled;

		if (item->conditional) {
			if (item->conditional->enable)
				disabled = !UI_EvaluateConditionAll(item->conditional->enable);
			if (!disabled && item->conditional->disable)
				disabled = UI_EvaluateConditionAny(item->conditional->disable);
		}

		if (disabled)
			item->flags |= QMF_DISABLED;
		else
			item->flags &= ~QMF_DISABLED;
	}

	menuDropdown_t *activeDropdown = UI_MenuController().ActiveDropdown(menu);
	if (activeDropdown) {
		if (activeDropdown->spin.generic.flags & (QMF_DISABLED | QMF_HIDDEN))
			Dropdown_Close(activeDropdown, false);
	}
}

static void Menu_DrawGroups(menuFrameWork_t *menu);
static void Menu_DrawGroups(menuFrameWork_t *menu);
static void Menu_UpdateGroupBounds(menuFrameWork_t *menu);

/*
===================================================================

KEYBIND CONTROL

===================================================================
*/

/*
=================
Keybind_Init
=================
*/
static void Keybind_Init(menuKeybind_t *k)
{
	size_t len;
	uiLayoutRect_t row{};
	uiLayoutSplit_t split{};
	
	Q_assert(k->generic.name);
	
	k->generic.uiFlags &= ~(UI_LEFT | UI_RIGHT);
	
	row.x = UI_Pixels(static_cast<float>(k->generic.x));
	row.y = UI_Pixels(static_cast<float>(k->generic.y));
	row.width = UI_Percent(0.6f);
	row.height = UI_Pixels(static_cast<float>(UI_CharHeight()));
	row.spacing = UI_ColumnPadding();
	
	split = UI_SplitLayoutRow(&row, nullptr, 0.45f);
	
	k->generic.rect = split.label;
	UI_StringDimensions(&k->generic.rect,
		k->generic.uiFlags | UI_RIGHT, k->generic.name);
	
	if (k->altbinding[0]) {
	len = strlen(k->binding) + 4 + strlen(k->altbinding);
	} else if (k->binding[0]) {
	len = strlen(k->binding);
	} else {
	len = 3;
	}
	
	const int fieldExtent = (split.field.x + split.field.width) - k->generic.rect.x;
	if (k->generic.rect.width < fieldExtent) {
	k->generic.rect.width = fieldExtent;
	}
	
	k->generic.rect.width += len * UI_CharWidth();
}

static void Keybind_Push(menuKeybind_t *k)
{
    int key = Key_EnumBindings(0, k->cmd);

    k->altbinding[0] = 0;
    if (key == -1) {
        strcpy(k->binding, "???");
    } else {
        Q_strlcpy(k->binding, Key_KeynumToString(key), sizeof(k->binding));
        key = Key_EnumBindings(key + 1, k->cmd);
        if (key != -1) {
            Q_strlcpy(k->altbinding, Key_KeynumToString(key), sizeof(k->altbinding));
        }
    }
}

static void Keybind_Pop(menuKeybind_t *k)
{
    Key_WaitKey(NULL, NULL);
}

static void Keybind_Update(menuFrameWork_t *menu)
{
    for (int i = 0; i < Menu_ItemCount(menu); i++) {
        void *rawItem = menu->items[i];
        auto *item = static_cast<menuCommon_t *>(rawItem);
        if (item->type == MTYPE_KEYBIND) {
            auto *keybind = static_cast<menuKeybind_t *>(rawItem);
            Keybind_Push(keybind);
            Keybind_Init(keybind);
        }
    }
}

static void Keybind_Remove(const char *cmd)
{
    int key;

    for (key = 0; ; key++) {
        key = Key_EnumBindings(key, cmd);
        if (key == -1) {
            break;
        }
        Key_SetBinding(key, NULL);
    }
}

static bool keybind_cb(void *arg, int key)
{
    auto *k = static_cast<menuKeybind_t *>(arg);
    menuFrameWork_t *menu = k->generic.parent;

    // console key is hardcoded
    if (key == '`') {
        UI_StartSound(QMS_BEEP);
        return false;
    }

    // menu key is hardcoded
    if (key != K_ESCAPE) {
        if (k->altbinding[0]) {
            Keybind_Remove(k->cmd);
        }
        Key_SetBinding(key, k->cmd);
    }

    Keybind_Update(menu);

    menu->keywait = false;
    menu->status = k->generic.status;
    Key_WaitKey(NULL, NULL);

    UI_StartSound(QMS_OUT);
    return false;
}

static menuSound_t Keybind_DoEnter(menuKeybind_t *k)
{
    menuFrameWork_t *menu = k->generic.parent;

    menu->keywait = true;
    menu->status = k->altstatus;
    Key_WaitKey(keybind_cb, k);
    return QMS_IN;
}

static menuSound_t Keybind_Key(menuKeybind_t *k, int key)
{
	menuFrameWork_t *menu = k->generic.parent;

	if (menu->keywait) {
		return QMS_OUT; // never gets there
	}

	MenuController &controller = UI_MenuController();
	if (controller.HasAction(key, MenuController::Action::ClearBinding)) {
		Keybind_Remove(k->cmd);
		Keybind_Update(menu);
		return QMS_IN;
	}

	return QMS_NOTHANDLED;
}


/*
===================================================================

SPIN CONTROL

===================================================================
*/

static void SpinControl_Push(menuSpinControl_t *s)
{
    int val = s->cvar->integer;

    if (val < 0 || val >= s->numItems)
        s->curvalue = -1;
    else
        s->curvalue = val;
}

static void SpinControl_Pop(menuSpinControl_t *s)
{
    if (s->curvalue >= 0 && s->curvalue < s->numItems)
        Cvar_SetInteger(s->cvar, s->curvalue, FROM_MENU);
}

/*
=================
SpinControl_Init
=================
*/
void SpinControl_Init(menuSpinControl_t *s)
{
	    char **n;
	    int    maxLength, length;
	    uiLayoutRect_t row{};
	    uiLayoutSplit_t split{};
	
	    s->generic.uiFlags &= ~(UI_LEFT | UI_RIGHT);
	
	    row.x = UI_Pixels(static_cast<float>(s->generic.x));
	    row.y = UI_Pixels(static_cast<float>(s->generic.y));
	    row.width = UI_Percent(0.6f);
	    row.height = UI_Pixels(static_cast<float>(UI_CharHeight()));
	    row.spacing = UI_ColumnPadding();
	
	    split = UI_SplitLayoutRow(&row, nullptr, 0.45f);
	
	    s->generic.rect = split.label;
	    UI_StringDimensions(&s->generic.rect,
	                        s->generic.uiFlags | UI_RIGHT, s->generic.name);
	
	    maxLength = 0;
	    s->numItems = 0;
	    n = s->itemnames;
	    while (*n) {
	        length = strlen(*n);
	
	        if (maxLength < length) {
	            maxLength = length;
	        }
	        s->numItems++;
	        n++;
	    }
	
	    const int minWidth = (split.field.x + split.field.width) - s->generic.rect.x;
	    if (s->generic.rect.width < minWidth) {
	        s->generic.rect.width = minWidth;
	    }

	    s->generic.rect.width += maxLength * UI_CharWidth();
}

/*
=================
EpisodeControl_Init
=================
*/
static void EpisodeControl_Init(menuEpisodeSelector_t *s)
{
    SpinControl_Init(&s->spin);
}

/*
=================
EpisodeControl_Init
=================
*/
static void UnitControl_Init(menuUnitSelector_t *s)
{
    SpinControl_Init(&s->spin);
}

/*
=================
SpinControl_DoEnter
=================
*/
static int SpinControl_DoEnter(menuSpinControl_t *s)
{
    if (!s->numItems)
        return QMS_BEEP;

    s->curvalue++;

    if (s->curvalue >= s->numItems)
        s->curvalue = 0;

    if (s->generic.change) {
        s->generic.change(&s->generic);
    }

    return QMS_MOVE;
}

/*
=================
SpinControl_DoSlide
=================
*/
static int SpinControl_DoSlide(menuSpinControl_t *s, int dir)
{
    if (!s->numItems)
        return QMS_BEEP;

    s->curvalue += dir;

    if (s->curvalue < 0) {
        s->curvalue = s->numItems - 1;
    } else if (s->curvalue >= s->numItems) {
        s->curvalue = 0;
    }

    if (s->generic.change) {
        s->generic.change(&s->generic);
    }

    return QMS_MOVE;
}

/*
=================
SpinControl_Draw
=================
*/
static void SpinControl_Draw(menuSpinControl_t *s)
{
    const char *name;

    UI_DrawString(s->generic.x + LCOLUMN_OFFSET, s->generic.y,
                  s->generic.uiFlags | UI_RIGHT | UI_ALTCOLOR, COLOR_WHITE, s->generic.name);

    if (s->generic.flags & QMF_HASFOCUS) {
        if ((uis.realtime >> 8) & 1) {
            UI_DrawChar(s->generic.x + RCOLUMN_OFFSET / 2, s->generic.y,
                        s->generic.uiFlags | UI_RIGHT, COLOR_WHITE, 13);
        }
    }

    if (s->curvalue < 0 || s->curvalue >= s->numItems)
        name = "???";
    else
        name = s->itemnames[s->curvalue];

    UI_DrawString(s->generic.x + RCOLUMN_OFFSET, s->generic.y,
                  s->generic.uiFlags, COLOR_WHITE, name);
}

/*
===================================================================

IMAGE SPIN CONTROL

===================================================================
*/

/*
=================
ImageSpinControl_Push
=================
*/
static void ImageSpinControl_Push(menuSpinControl_t *s)
{
}

/*
=================
ImageSpinControl_Pop
=================
*/
void ImageSpinControl_Pop(menuSpinControl_t *s)
{
    if (s->curvalue >= 0 && s->curvalue < s->numItems) {
        size_t path_offset = strlen(s->path) + (strchr(s->filter, '*') - s->filter) + 1;
        const char *file_value = s->itemnames[s->curvalue] + path_offset;
        const char *dot = strchr(file_value, '.');
        size_t file_name_length = dot - file_value;
        Cvar_SetEx(s->cvar->name, va("%.*s", (int)file_name_length, file_value), FROM_MENU);
    }

    FS_FreeList(reinterpret_cast<void **>(s->itemnames));
    s->itemnames = NULL;
}

/*
=================
ImageSpinControl_Free
=================
*/
/*
=================
ImageSpinControl_Draw
=================
*/
static void ImageSpinControl_Draw(menuSpinControl_t *s)
{
    SpinControl_Draw(s);

    bool in_range = !(s->curvalue < 0 || s->curvalue >= s->numItems);

    if (in_range) {
        qhandle_t pic = R_RegisterTempPic(va("/%s", s->itemnames[s->curvalue]));
        int w, h;
        R_GetPicSize(&w, &h, pic);

        R_DrawPic(s->generic.x + LCOLUMN_OFFSET + ((s->generic.width / 2) - (w / 2)), s->generic.y + MENU_SPACING + ((s->generic.height / 2) - (h / 2)), COLOR_WHITE, pic);
    }
}

/*
=================
ImageSpinControl_Init
=================
*/
void ImageSpinControl_Init(menuSpinControl_t *s)
{
    s->numItems = 0;
    s->itemnames = reinterpret_cast<char **>(FS_ListFiles(NULL, va("%s/%s", s->path, s->filter), FS_SEARCH_BYFILTER, &s->numItems));

    SpinControl_Init(s);

    // find the selected value
    const char *val = s->cvar->string;
    size_t val_len = strlen(val);

    s->curvalue = -1;

    size_t path_offset = strlen(s->path) + (strchr(s->filter, '*') - s->filter) + 1;

    for (int i = 0; i < s->numItems; i++) {
        const char *file_value = s->itemnames[i] + path_offset;
        const char *dot = strchr(file_value, '.');
        size_t file_name_length = dot - file_value;

        if (!Q_strncasecmp(val, file_value, max(val_len, file_name_length))) {
            s->curvalue = i;
            break;
        }
    }
}

/*
===================================================================

BITFIELD CONTROL

===================================================================
*/

static void BitField_Push(menuSpinControl_t *s)
{
    const int negate = s->negate ? 1 : 0;

    if (s->cvar->integer & s->mask) {
        s->curvalue = 1 ^ negate;
    } else {
        s->curvalue = 0 ^ negate;
    }
}

static void BitField_Pop(menuSpinControl_t *s)
{
    int val = s->cvar->integer;

    const bool set = (s->curvalue != 0) != s->negate;

    if (set) {
        val |= s->mask;
    } else {
        val &= ~s->mask;
    }
    Cvar_SetInteger(s->cvar, val, FROM_MENU);
}

/*
===================================================================

PAIRS CONTROL

===================================================================
*/

static void Pairs_Push(menuSpinControl_t *s)
{
    int i;

    for (i = 0; i < s->numItems; i++) {
        if (!Q_stricmp(s->itemvalues[i], s->cvar->string)) {
            s->curvalue = i;
            return;
        }
    }

    s->curvalue = -1;
}

static void Pairs_Pop(menuSpinControl_t *s)
{
    if (s->curvalue >= 0 && s->curvalue < s->numItems)
        Cvar_SetByVar(s->cvar, s->itemvalues[s->curvalue], FROM_MENU);
}

/*
===================================================================

STRINGS CONTROL

===================================================================
*/

static void Strings_Push(menuSpinControl_t *s)
{
    int i;

    for (i = 0; i < s->numItems; i++) {
        if (!Q_stricmp(s->itemnames[i], s->cvar->string)) {
            s->curvalue = i;
            return;
        }
    }

    s->curvalue = -1;
}

static void Strings_Pop(menuSpinControl_t *s)
{
    if (s->curvalue >= 0 && s->curvalue < s->numItems)
        Cvar_SetByVar(s->cvar, s->itemnames[s->curvalue], FROM_MENU);
}

/*
===================================================================

TOGGLE CONTROL

===================================================================
*/

static void Toggle_Push(menuSpinControl_t *s)
{
    int val = s->cvar->integer;

    if (val == 0 || val == 1)
        s->curvalue = s->negate ? (1 - val) : val;
    else
        s->curvalue = -1;
}

static void Toggle_Pop(menuSpinControl_t *s)
{
    if (s->curvalue == 0 || s->curvalue == 1) {
        int value = s->negate ? (s->curvalue ? 0 : 1) : s->curvalue;
        Cvar_SetInteger(s->cvar, value, FROM_MENU);
    }
}

/*
===================================================================

CHECKBOX CONTROL

===================================================================
*/

static bool Checkbox_GetState(const menuCheckbox_t *c)
{
    if (!c->cvar)
        return false;

    if (c->useBitmask)
        return (c->cvar->integer & c->mask) != 0;

    if (c->useStrings) {
        if (!c->checkedValue)
            return false;
        return !strcmp(c->cvar->string ? c->cvar->string : "", c->checkedValue);
    }

    bool state = c->cvar->integer != 0;
    return c->negate ? !state : state;
}

static void Checkbox_SetState(menuCheckbox_t *c, bool enable)
{
    if (!c->cvar)
        return;

    if (c->useBitmask) {
        int value = c->cvar->integer;
        if (enable)
            value |= c->mask;
        else
            value &= ~c->mask;
        Cvar_SetInteger(c->cvar, value, FROM_MENU);
        return;
    }

    if (c->useStrings) {
        const char *value = enable ? c->checkedValue : c->uncheckedValue;
        if (value)
            Cvar_SetByVar(c->cvar, value, FROM_MENU);
        return;
    }

    int value = enable ? 1 : 0;
    if (c->negate)
        value = enable ? 0 : 1;
    Cvar_SetInteger(c->cvar, value, FROM_MENU);
}

static menuSound_t Checkbox_Activate(menuCommon_t *item)
{
    auto *checkbox = reinterpret_cast<menuCheckbox_t *>(item);
    bool checked = Checkbox_GetState(checkbox);
    Checkbox_SetState(checkbox, !checked);

    if (item->change)
        item->change(item);

    return QMS_MOVE;
}

static menuSound_t Checkbox_Keydown(menuCommon_t *item, int key)
{
    switch (key) {
    case K_SPACE:
        return Checkbox_Activate(item);
    default:
        return QMS_NOTHANDLED;
    }
}

static menuSound_t Checkbox_Slide(menuCheckbox_t *c, int dir)
{
    if (dir == 0)
        return QMS_NOTHANDLED;

    bool current = Checkbox_GetState(c);
    bool desired = dir > 0;

    if (desired == current)
        return QMS_BEEP;

    Checkbox_SetState(c, desired);

    if (c->generic.change)
        c->generic.change(&c->generic);

    return QMS_MOVE;
}

static void Checkbox_Init(menuCheckbox_t *c)
{
    c->generic.uiFlags &= ~(UI_LEFT | UI_RIGHT);

    c->generic.rect.x = c->generic.x + LCOLUMN_OFFSET;
    c->generic.rect.y = c->generic.y;
    UI_StringDimensions(&c->generic.rect, c->generic.uiFlags | UI_RIGHT, c->generic.name ? c->generic.name : "");

    const int padding = (RCOLUMN_OFFSET - LCOLUMN_OFFSET) + CONCHAR_WIDTH * 4;
    c->generic.rect.width += padding;
    c->generic.rect.height = CONCHAR_HEIGHT;
    c->generic.activate = Checkbox_Activate;
    c->generic.keydown = Checkbox_Keydown;
}

static void Checkbox_Draw(menuCheckbox_t *c)
{
    const bool disabled = (c->generic.flags & QMF_DISABLED) != 0;
    const bool focused = (c->generic.flags & QMF_HASFOCUS) != 0;

    color_t labelColor = disabled ? uis.color.disabled : uis.color.normal;
    color_t valueColor = disabled ? uis.color.disabled : (focused ? uis.color.active : uis.color.selection);

    UI_DrawString(c->generic.x + LCOLUMN_OFFSET, c->generic.y,
                  c->generic.uiFlags | UI_RIGHT | UI_ALTCOLOR, labelColor,
                  c->generic.name ? c->generic.name : "");

    if (focused) {
        if ((uis.realtime >> 8) & 1) {
            UI_DrawChar(c->generic.x + RCOLUMN_OFFSET / 2, c->generic.y,
                        c->generic.uiFlags | UI_RIGHT, valueColor, 13);
        }
    }

    const char *mark = Checkbox_GetState(c) ? "[x]" : "[ ]";
    UI_DrawString(c->generic.x + RCOLUMN_OFFSET, c->generic.y,
                  c->generic.uiFlags | UI_LEFT, valueColor, mark);
}

/*
===================================================================

DROPDOWN CONTROL

===================================================================
*/

static int Dropdown_ValueWidth(const menuDropdown_t *d)
{
    const int minimumWidth = CONCHAR_WIDTH * 10;
    int width = d->spin.generic.rect.width - (RCOLUMN_OFFSET - LCOLUMN_OFFSET);
    if (width < minimumWidth)
        width = minimumWidth;
    return width;
}

static vrect_t Dropdown_ValueRect(const menuDropdown_t *d)
{
    vrect_t rect;
    rect.x = d->spin.generic.x + RCOLUMN_OFFSET;
    rect.y = d->spin.generic.y;
    rect.width = Dropdown_ValueWidth(d);
    rect.height = CONCHAR_HEIGHT;
    return rect;
}

static const char *Dropdown_GetLabel(const menuDropdown_t *d, int index)
{
    if (!d->spin.itemnames || index < 0 || index >= d->spin.numItems)
        return "";
    return d->spin.itemnames[index] ? d->spin.itemnames[index] : "";
}

static const char *Dropdown_GetValue(const menuDropdown_t *d, int index)
{
    if (index < 0 || index >= d->spin.numItems)
        return "";
    if (!d->spin.itemvalues || !d->spin.itemvalues[index])
        return Dropdown_GetLabel(d, index);
    return d->spin.itemvalues[index];
}

static void Dropdown_Commit(menuDropdown_t *d)
{
    menuSpinControl_t *spin = &d->spin;
    if (!spin->cvar || spin->curvalue < 0 || spin->curvalue >= spin->numItems)
        return;

    switch (d->binding) {
    case DROPDOWN_BINDING_LABEL:
        Cvar_SetByVar(spin->cvar, Dropdown_GetLabel(d, spin->curvalue), FROM_MENU);
        break;
    case DROPDOWN_BINDING_VALUE:
        Cvar_SetByVar(spin->cvar, Dropdown_GetValue(d, spin->curvalue), FROM_MENU);
        break;
    case DROPDOWN_BINDING_INDEX:
        Cvar_SetInteger(spin->cvar, spin->curvalue, FROM_MENU);
        break;
    }
}

static void Dropdown_UpdateScroll(menuDropdown_t *d)
{
    if (!d->spin.numItems) {
        d->scrollOffset = 0;
        return;
    }

    const int visible = (std::min)(d->maxVisibleItems, d->spin.numItems);
    if (visible <= 0)
        return;

    if (d->scrollOffset < 0)
        d->scrollOffset = 0;
    if (d->scrollOffset > d->spin.numItems - visible)
        d->scrollOffset = d->spin.numItems - visible;

    int target = d->open ? d->hovered : d->spin.curvalue;
    if (target < 0)
        target = 0;

    if (target < d->scrollOffset)
        d->scrollOffset = target;
    else if (target >= d->scrollOffset + visible)
        d->scrollOffset = target - visible + 1;
}

static void Dropdown_SetSelection(menuDropdown_t *d, int index, bool notify)
{
    menuSpinControl_t *spin = &d->spin;
    if (index < 0 || index >= spin->numItems)
        return;

    if (spin->curvalue == index && !notify)
        return;

    spin->curvalue = index;
    Dropdown_Commit(d);

    if (notify && spin->generic.change)
        spin->generic.change(&spin->generic);

    if (d->open)
        d->hovered = index;

    Dropdown_UpdateScroll(d);
}

static void Dropdown_Close(menuDropdown_t *d, bool applyHovered)
{
UI_MenuController().CloseDropdown(d, applyHovered);
}

static void Dropdown_Open(menuDropdown_t *d)
{
UI_MenuController().OpenDropdown(d);
}

static void Dropdown_DrawList(menuDropdown_t *d)
{
    if (!d->open || d->spin.numItems <= 0)
        return;

    const int visible = (std::min)(d->maxVisibleItems, d->spin.numItems);
    if (visible <= 0)
        return;

    Dropdown_UpdateScroll(d);

    vrect_t valueRect = Dropdown_ValueRect(d);
    const int itemHeight = CONCHAR_HEIGHT;
    const int spacing = 2;

    d->listRect.x = valueRect.x;
    d->listRect.y = valueRect.y + valueRect.height + spacing;
    d->listRect.width = valueRect.width;
    d->listRect.height = visible * itemHeight;

    color_t background = ColorSetAlpha(uis.color.background, static_cast<uint8_t>(220));
    R_DrawFill32(d->listRect.x, d->listRect.y - 1, d->listRect.width, d->listRect.height + 2, background);
    UI_DrawRect8(&d->listRect, 1, 223);

    for (int i = 0; i < visible; i++) {
        int index = d->scrollOffset + i;
        if (index >= d->spin.numItems)
            break;

        int y = d->listRect.y + i * itemHeight;
        bool highlighted = (index == d->hovered);

        if (highlighted) {
            color_t highlight = ColorSetAlpha(uis.color.selection, static_cast<uint8_t>(200));
            R_DrawFill32(d->listRect.x, y, d->listRect.width, itemHeight, highlight);
        }

        color_t textColor;
        if (d->spin.generic.flags & QMF_DISABLED)
            textColor = uis.color.disabled;
        else if (highlighted)
            textColor = uis.color.active;
        else
            textColor = uis.color.normal;

        UI_DrawString(d->listRect.x + CONCHAR_WIDTH, y,
                      UI_LEFT, textColor, Dropdown_GetLabel(d, index));
    }
}

static void Dropdown_Draw(menuDropdown_t *d)
{
    menuCommon_t &generic = d->spin.generic;
    const bool disabled = (generic.flags & QMF_DISABLED) != 0;
    const bool focused = (generic.flags & QMF_HASFOCUS) != 0;

    color_t labelColor = disabled ? uis.color.disabled : uis.color.normal;
    UI_DrawString(generic.x + LCOLUMN_OFFSET, generic.y,
                  generic.uiFlags | UI_RIGHT | UI_ALTCOLOR, labelColor,
                  generic.name ? generic.name : "");

    vrect_t valueRect = Dropdown_ValueRect(d);

    color_t valueColor;
    if (disabled)
        valueColor = uis.color.disabled;
    else if (focused || d->open)
        valueColor = uis.color.active;
    else
        valueColor = uis.color.selection;

    color_t fill = ColorSetAlpha(uis.color.background, static_cast<uint8_t>(200));
    R_DrawFill32(valueRect.x, valueRect.y, valueRect.width, valueRect.height, fill);
    UI_DrawRect8(&valueRect, 1, 223);

    if (focused && !d->open) {
        if ((uis.realtime >> 8) & 1) {
            UI_DrawChar(generic.x + RCOLUMN_OFFSET / 2, generic.y,
                        generic.uiFlags | UI_RIGHT, valueColor, 13);
        }
    }

    const char *label = (d->spin.curvalue >= 0 && d->spin.curvalue < d->spin.numItems)
                            ? Dropdown_GetLabel(d, d->spin.curvalue)
                            : "";

    UI_DrawString(valueRect.x + CONCHAR_WIDTH, valueRect.y,
                  UI_LEFT, valueColor, label);

    const char *caret = d->open ? "^" : "v";
    UI_DrawString(valueRect.x + valueRect.width - CONCHAR_WIDTH * 2, valueRect.y,
                  UI_LEFT, valueColor, caret);

    if (!d->open)
        d->listRect = {};
}

static menuSound_t Dropdown_Activate(menuCommon_t *item)
{
    auto *dropdown = reinterpret_cast<menuDropdown_t *>(item);

    if (dropdown->open) {
        Dropdown_Close(dropdown, dropdown->hovered >= 0);
        return QMS_MOVE;
    }

    if (!(item->flags & QMF_DISABLED)) {
        Dropdown_Open(dropdown);
        Dropdown_UpdateScroll(dropdown);
        return QMS_IN;
    }

return QMS_NOTHANDLED;
}

static menuSound_t Dropdown_Keydown(menuCommon_t *item, int key)
{
	auto *dropdown = reinterpret_cast<menuDropdown_t *>(item);
	MenuController &controller = UI_MenuController();
	std::vector<MenuController::Action> actions = controller.ActionsForKey(key);

	if (dropdown->open) {
		for (MenuController::Action action : actions) {
			switch (action) {
			case MenuController::Action::Back:
			case MenuController::Action::Secondary:
				Dropdown_Close(dropdown, false);
				return QMS_SILENT;
			case MenuController::Action::Activate:
				Dropdown_Close(dropdown, true);
				return QMS_MOVE;
			case MenuController::Action::Up:
			case MenuController::Action::ScrollUp:
				if (dropdown->hovered > 0) {
					dropdown->hovered--;
					Dropdown_UpdateScroll(dropdown);
					return QMS_MOVE;
				}
				return QMS_BEEP;
			case MenuController::Action::Down:
			case MenuController::Action::ScrollDown:
				if (dropdown->hovered < dropdown->spin.numItems - 1) {
					dropdown->hovered++;
					Dropdown_UpdateScroll(dropdown);
					return QMS_MOVE;
				}
				return QMS_BEEP;
			default:
				break;
			}
		}
	} else {
		for (MenuController::Action action : actions) {
			if (action == MenuController::Action::Activate || action == MenuController::Action::Primary)
				return Dropdown_Activate(item);
		}
	}

	return QMS_NOTHANDLED;
}

static menuSound_t Dropdown_MouseMove(menuDropdown_t *d)
{
    if (!d->open)
        return QMS_NOTHANDLED;

    if (d->spin.numItems <= 0)
        return QMS_NOTHANDLED;

    if (!UI_CursorInRect(&d->listRect)) {
        if (d->hovered != -1) {
            d->hovered = -1;
            return QMS_MOVE;
        }
        return QMS_NOTHANDLED;
    }

    int relative = uis.mouseCoords[1] - d->listRect.y;
    if (relative < 0)
        relative = 0;

    int index = d->scrollOffset + relative / CONCHAR_HEIGHT;
    index = Q_clip(index, 0, d->spin.numItems - 1);

    if (index != d->hovered) {
        d->hovered = index;
        return QMS_MOVE;
    }

    return QMS_NOTHANDLED;
}

static menuSound_t Dropdown_Slide(menuDropdown_t *d, int dir)
{
	if (!d->spin.numItems)
		return QMS_BEEP;

	int next = d->spin.curvalue + dir;

	if (next < 0)
		next = d->spin.numItems - 1;
	else if (next >= d->spin.numItems)
		next = 0;

	if (next == d->spin.curvalue)
		return QMS_BEEP;

	Dropdown_SetSelection(d, next, true);
	return QMS_MOVE;
}

/*
=============
MenuController::CloseDropdown

Closes the tracked dropdown and optionally applies the hovered selection.
=============
*/
void MenuController::CloseDropdown(menuDropdown_t *d, bool applyHovered)
{
	if (!d || !d->open)
		return;

	if (applyHovered && d->hovered >= 0 && d->hovered < d->spin.numItems)
		Dropdown_SetSelection(d, d->hovered, true);

	d->open = false;
	d->hovered = -1;
	d->listRect = {};

	if (activeDropdown == d)
		activeDropdown = NULL;
}

/*
=============
MenuController::OpenDropdown

Opens the requested dropdown while closing any other active dropdown.
=============
*/
void MenuController::OpenDropdown(menuDropdown_t *d)
{
	if (!d)
		return;

	if (activeDropdown && activeDropdown != d)
		CloseDropdown(activeDropdown, false);

	d->open = true;
	activeDropdown = d;
	d->hovered = d->spin.curvalue;
	Dropdown_UpdateScroll(d);
}

/*
=============
MenuController::ActiveDropdown

Returns the active dropdown for a menu, if any.
=============
*/
menuDropdown_t *MenuController::ActiveDropdown(menuFrameWork_t *menu) const
{
	if (activeDropdown && activeDropdown->spin.generic.parent == menu)
		return activeDropdown;

	return NULL;
}

/*
=============
MenuController::ResetDropdown

Clears the tracked dropdown when a menu is being discarded.
=============
*/
void MenuController::ResetDropdown(menuFrameWork_t *menu)
{
	if (activeDropdown && activeDropdown->spin.generic.parent == menu)
		activeDropdown = NULL;
}

/*
=============
MenuController::HandleDropdownAction

Applies input actions while a dropdown list is open.
=============
*/
menuSound_t MenuController::HandleDropdownAction(menuFrameWork_t *menu, menuDropdown_t *dropdown, Action action)
{
	if (!dropdown || !dropdown->open || dropdown->spin.generic.parent != menu)
		return QMS_NOTHANDLED;

	switch (action) {
	case Action::Back:
	case Action::Secondary:
		CloseDropdown(dropdown, false);
		return QMS_SILENT;
	case Action::Activate:
		CloseDropdown(dropdown, true);
		return QMS_MOVE;
	case Action::Up:
	case Action::ScrollUp:
		if (dropdown->hovered > 0) {
			dropdown->hovered--;
			Dropdown_UpdateScroll(dropdown);
			return QMS_MOVE;
		}
		return QMS_BEEP;
case Action::Down:
case Action::ScrollDown:
if (dropdown->hovered < dropdown->spin.numItems - 1) {
dropdown->hovered++;
Dropdown_UpdateScroll(dropdown);
return QMS_MOVE;
}
return QMS_BEEP;
case Action::First:
dropdown->hovered = 0;
Dropdown_UpdateScroll(dropdown);
return QMS_MOVE;
case Action::Last:
dropdown->hovered = dropdown->spin.numItems - 1;
Dropdown_UpdateScroll(dropdown);
return QMS_MOVE;
case Action::Primary:
if (UI_CursorInRect(&dropdown->listRect)) {
if (dropdown->hovered >= 0 && dropdown->hovered < dropdown->spin.numItems)
Dropdown_SetSelection(dropdown, dropdown->hovered, true);
			CloseDropdown(dropdown, false);
			return QMS_MOVE;
		}

		CloseDropdown(dropdown, false);
		return QMS_SILENT;
	default:
		break;
	}

	return QMS_NOTHANDLED;
}

/*
=============
MenuController::HandlePointerAction

Routes pointer-style activation actions through hit-testing.
=============
*/
menuSound_t MenuController::HandlePointerAction(menuFrameWork_t *menu, Action action, int key)
{
	if (action != Action::Primary && action != Action::Tertiary)
		return QMS_NOTHANDLED;

	menuCommon_t *item = Menu_HitTest(menu);
	if (!item)
		return QMS_NOTHANDLED;

	if (!(item->flags & QMF_HASFOCUS))
		return QMS_NOTHANDLED;

	if (MenuItem *wrapped = Menu_FindCppItem(menu, item)) {
		MenuItem::MenuEvent event{};
		event.type = MenuItem::MenuEvent::Type::Pointer;
		event.key = key;
		event.x = uis.mouseCoords[0];
		event.y = uis.mouseCoords[1];

		if (wrapped->HandleEvent(event)) {
			return QMS_IN;
		}
	}

	return Menu_SelectItem(menu);
}

/*
=============
MenuController::HandleMenuAction

Executes default menu-level actions.
=============
*/
menuSound_t MenuController::HandleMenuAction(menuFrameWork_t *menu, Action action, int key)
{
	switch (action) {
	case Action::Back:
		UI_PopMenu();
		return QMS_OUT;
	case Action::Up:
		return Menu_AdjustCursor(menu, -1);
	case Action::Down:
		return Menu_AdjustCursor(menu, 1);
	case Action::Left:
		return Menu_SlideItem(menu, -1);
	case Action::Right:
		return Menu_SlideItem(menu, 1);
	case Action::Activate:
		return Menu_SelectItem(menu);
	case Action::Primary:
	case Action::Tertiary:
		return HandlePointerAction(menu, action, key);
	default:
		break;
	}

	return QMS_NOTHANDLED;
}

/*
=============
MenuController::HandleKey

Converts a raw key into actions and dispatches them.
=============
*/
menuSound_t MenuController::HandleKey(menuFrameWork_t *menu, int key)
{
	std::vector<Action> actions = ActionsForKey(key);
	menuDropdown_t *dropdown = ActiveDropdown(menu);

	if (dropdown && dropdown->open) {
		for (Action action : actions) {
			menuSound_t sound = HandleDropdownAction(menu, dropdown, action);
			if (sound != QMS_NOTHANDLED)
				return sound;
		}
	}

	for (Action action : actions) {
		menuSound_t sound = HandleMenuAction(menu, action, key);
		if (sound != QMS_NOTHANDLED)
			return sound;
	}

	return QMS_NOTHANDLED;
}

static void Dropdown_Init(menuDropdown_t *d)
{
    SpinControl_Init(&d->spin);

    d->spin.generic.activate = Dropdown_Activate;
    d->spin.generic.keydown = Dropdown_Keydown;
    d->hovered = -1;
    d->scrollOffset = 0;
    d->open = false;
    d->listRect = {};

    // ensure enough width for dropdown visuals
    vrect_t rect = Dropdown_ValueRect(d);
    d->spin.generic.rect.width = (rect.x + rect.width) - (d->spin.generic.x + LCOLUMN_OFFSET);
    d->spin.generic.rect.height = CONCHAR_HEIGHT;
}

static void Dropdown_Push(menuDropdown_t *d)
{
    menuSpinControl_t *spin = &d->spin;
    spin->curvalue = -1;

    if (!spin->cvar)
        return;

    switch (d->binding) {
    case DROPDOWN_BINDING_LABEL: {
        const char *value = spin->cvar->string ? spin->cvar->string : "";
        for (int i = 0; i < spin->numItems; i++) {
            if (!Q_stricmp(Dropdown_GetLabel(d, i), value)) {
                spin->curvalue = i;
                break;
            }
        }
        break;
    }
    case DROPDOWN_BINDING_VALUE: {
        const char *value = spin->cvar->string ? spin->cvar->string : "";
        for (int i = 0; i < spin->numItems; i++) {
            if (!Q_stricmp(Dropdown_GetValue(d, i), value)) {
                spin->curvalue = i;
                break;
            }
        }
        break;
    }
    case DROPDOWN_BINDING_INDEX: {
        int value = spin->cvar->integer;
        if (value >= 0 && value < spin->numItems)
            spin->curvalue = value;
        break;
    }
    }

    if (spin->curvalue < 0 && spin->numItems)
        spin->curvalue = 0;

    Dropdown_UpdateScroll(d);
}

static void Dropdown_Pop(menuDropdown_t *d)
{
    Dropdown_Close(d, false);
    Dropdown_Commit(d);
}

/*
===================================================================

RADIO BUTTON CONTROL

===================================================================
*/

static bool RadioButton_IsSelected(const menuRadioButton_t *r)
{
    if (!r->cvar || !r->value)
        return false;

    const char *current = r->cvar->string ? r->cvar->string : "";
    return !Q_stricmp(current, r->value);
}

static void RadioButton_Select(menuRadioButton_t *r)
{
    if (!r->cvar || !r->value)
        return;

    if (RadioButton_IsSelected(r))
        return;

    Cvar_SetByVar(r->cvar, r->value, FROM_MENU);

    if (r->generic.change)
        r->generic.change(&r->generic);
}

static menuSound_t RadioButton_Activate(menuCommon_t *item)
{
    auto *radio = reinterpret_cast<menuRadioButton_t *>(item);

    if (item->flags & QMF_DISABLED)
        return QMS_BEEP;

    RadioButton_Select(radio);
    return QMS_MOVE;
}

static menuSound_t RadioButton_Keydown(menuCommon_t *item, int key)
{
    switch (key) {
    case K_SPACE:
        return RadioButton_Activate(item);
    default:
        return QMS_NOTHANDLED;
    }
}

static void RadioButton_Init(menuRadioButton_t *r)
{
    r->generic.uiFlags &= ~(UI_LEFT | UI_RIGHT);

    r->generic.rect.x = r->generic.x + LCOLUMN_OFFSET;
    r->generic.rect.y = r->generic.y;
    UI_StringDimensions(&r->generic.rect,
                        r->generic.uiFlags | UI_RIGHT, r->generic.name ? r->generic.name : "");

    const int padding = (RCOLUMN_OFFSET - LCOLUMN_OFFSET) + CONCHAR_WIDTH * 4;
    r->generic.rect.width += padding;
    r->generic.rect.height = CONCHAR_HEIGHT;

    r->generic.activate = RadioButton_Activate;
    r->generic.keydown = RadioButton_Keydown;
}

static void RadioButton_Draw(menuRadioButton_t *r)
{
    const bool disabled = (r->generic.flags & QMF_DISABLED) != 0;
    const bool focused = (r->generic.flags & QMF_HASFOCUS) != 0;
    const bool selected = RadioButton_IsSelected(r);

    color_t labelColor = disabled ? uis.color.disabled : uis.color.normal;
    color_t valueColor = disabled ? uis.color.disabled : (selected ? uis.color.active : uis.color.selection);

    UI_DrawString(r->generic.x + LCOLUMN_OFFSET, r->generic.y,
                  r->generic.uiFlags | UI_RIGHT | UI_ALTCOLOR, labelColor,
                  r->generic.name ? r->generic.name : "");

    if (focused) {
        if ((uis.realtime >> 8) & 1) {
            UI_DrawChar(r->generic.x + RCOLUMN_OFFSET / 2, r->generic.y,
                        r->generic.uiFlags | UI_RIGHT, valueColor, 13);
        }
    }

    const char *mark = selected ? "(o)" : "( )";
    UI_DrawString(r->generic.x + RCOLUMN_OFFSET, r->generic.y,
                  r->generic.uiFlags | UI_LEFT, valueColor, mark);
}

// Episode selector

static void Episode_Push(menuEpisodeSelector_t *s)
{
    s->spin.curvalue = 0;
}

static void Episode_Pop(menuEpisodeSelector_t *s)
{
    if (s->spin.curvalue >= 0 && s->spin.curvalue < s->spin.numItems)
        Cvar_SetInteger(s->spin.cvar, s->spin.curvalue, FROM_MENU);
}

// Unit selector

static void Unit_Push(menuUnitSelector_t *s)
{
    s->spin.curvalue = 0;
}

static void Unit_Pop(menuUnitSelector_t *s)
{
    if (s->spin.curvalue >= 0 && s->spin.curvalue < s->spin.numItems)
        Cvar_SetInteger(s->spin.cvar, s->itemindices[s->spin.curvalue], FROM_MENU);
}

/*
===================================================================

LIST CONTROL

===================================================================
*/

/*
=================
MenuList_ValidatePrestep
=================
*/
static void MenuList_ValidatePrestep(menuList_t *l)
{
    if (l->prestep > l->numItems - l->maxItems) {
        l->prestep = l->numItems - l->maxItems;
    }
    if (l->prestep < 0) {
        l->prestep = 0;
    }
}

static void MenuList_AdjustPrestep(menuList_t *l)
{
    if (l->numItems > l->maxItems && l->curvalue > 0) {
        if (l->prestep > l->curvalue) {
            l->prestep = l->curvalue;
        } else if (l->prestep < l->curvalue - l->maxItems + 1) {
            l->prestep = l->curvalue - l->maxItems + 1;
        }
    } else {
        l->prestep = 0;
    }
}

/*
=================
MenuList_Init
=================
*/
void MenuList_Init(menuList_t *l)
{
    int        height;
    int        i;

    height = l->generic.height;
    if (l->mlFlags & MLF_HEADER) {
        height -= MLIST_SPACING;
    }

    l->maxItems = height / MLIST_SPACING;

    //clamp(l->curvalue, 0, l->numItems - 1);

    MenuList_ValidatePrestep(l);

    l->generic.rect.x = l->generic.x;
    l->generic.rect.y = l->generic.y;

    l->generic.rect.width = 0;
    for (i = 0; i < l->numcolumns; i++) {
        l->generic.rect.width += l->columns[i].width;
    }

    if (l->mlFlags & MLF_SCROLLBAR) {
        l->generic.rect.width += MLIST_SCROLLBAR_WIDTH;
    }

    l->generic.rect.height = l->generic.height;

    if (l->sortdir && l->sort) {
        l->sort(l);
    }
}

/*
=================
MenuList_SetValue
=================
*/
void MenuList_SetValue(menuList_t *l, int value)
{
    if (value > l->numItems - 1)
        value = l->numItems - 1;
    if (value < 0)
        value = 0;

    if (value != l->curvalue) {
        l->curvalue = value;
        if (l->generic.change) {
            l->generic.change(&l->generic);
        }
    }

    MenuList_AdjustPrestep(l);
}

static menuSound_t MenuList_SetColumn(menuList_t *l, int value)
{
    if (l->sortcol == value) {
        l->sortdir = -l->sortdir;
    } else {
        l->sortcol = value;
        l->sortdir = 1;
    }
    if (l->sort) {
        l->sort(l);
        MenuList_AdjustPrestep(l);
    }
    return QMS_SILENT;
}

// finds a visible column by number, with numeration starting at 1
static menuSound_t MenuList_FindColumn(menuList_t *l, int rel)
{
    int i, j;

    if (!l->sortdir)
        return QMS_NOTHANDLED;

    for (i = 0, j = 0; i < l->numcolumns; i++) {
        if (!l->columns[i].width)
            continue;

        if (++j == rel)
            return MenuList_SetColumn(l, i);
    }

    return QMS_NOTHANDLED;
}

static menuSound_t MenuList_PrevColumn(menuList_t *l)
{
    int col;

    if (!l->sortdir || !l->numcolumns) {
        return QMS_NOTHANDLED;
    }

    col = l->sortcol;
    if (col < 0)
        return MenuList_FindColumn(l, 1);

    do {
        if (col == 0) {
            col = l->numcolumns - 1;
        } else {
            col--;
        }
        if (col == l->sortcol) {
            return QMS_SILENT;
        }
    } while (!l->columns[col].width);

    return MenuList_SetColumn(l, col);
}

static menuSound_t MenuList_NextColumn(menuList_t *l)
{
    int col;

    if (!l->sortdir || !l->numcolumns) {
        return QMS_NOTHANDLED;
    }

    col = l->sortcol;
    if (col < 0)
        return MenuList_FindColumn(l, 1);

    do {
        if (col == l->numcolumns - 1) {
            col = 0;
        } else {
            col++;
        }
        if (col == l->sortcol) {
            return QMS_SILENT;
        }
    } while (!l->columns[col].width);

    return MenuList_SetColumn(l, col);
}

/*
=================
MenuList_Click
=================
*/
static menuSound_t MenuList_Click(menuList_t *l)
{
    int i, j;
    vrect_t rect;

    if (!l->items) {
        return QMS_SILENT;
    }

    // click on scroll bar
    if ((l->mlFlags & MLF_SCROLLBAR) && l->numItems > l->maxItems) {
        int x = l->generic.rect.x + l->generic.rect.width - MLIST_SCROLLBAR_WIDTH;
        int y = l->generic.rect.y + MLIST_SPACING;
        int h = l->generic.height;
        int barHeight, pageHeight, prestepHeight;
        float pageFrac, prestepFrac;

        if (l->mlFlags & MLF_HEADER) {
            y += MLIST_SPACING;
            h -= MLIST_SPACING;
        }

        barHeight = h - MLIST_SPACING * 2;
        pageFrac = (float)l->maxItems / l->numItems;
        prestepFrac = (float)l->prestep / l->numItems;

        pageHeight = Q_rint(barHeight * pageFrac);
        prestepHeight = Q_rint(barHeight * prestepFrac);

        // click above thumb
        rect.x = x;
        rect.y = y;
        rect.width = MLIST_SCROLLBAR_WIDTH;
        rect.height = prestepHeight;
        if (UI_CursorInRect(&rect)) {
            l->prestep -= l->maxItems;
            MenuList_ValidatePrestep(l);
            return QMS_MOVE;
        }

        // click on thumb
        rect.y = y + prestepHeight;
        rect.height = pageHeight;
        if (UI_CursorInRect(&rect)) {
            l->drag_y = uis.mouseCoords[1] - rect.y;
            uis.mouseTracker = &l->generic;
            return QMS_SILENT;
        }

        // click below thumb
        rect.y = y + prestepHeight + pageHeight;
        rect.height = barHeight - prestepHeight - pageHeight;
        if (UI_CursorInRect(&rect)) {
            l->prestep += l->maxItems;
            MenuList_ValidatePrestep(l);
            return QMS_MOVE;
        }

        // click above scrollbar
        rect.y = y - MLIST_SPACING;
        rect.height = MLIST_SPACING;
        if (UI_CursorInRect(&rect)) {
            l->prestep--;
            MenuList_ValidatePrestep(l);
            return QMS_MOVE;
        }

        // click below scrollbar
        rect.y = l->generic.rect.y + l->generic.height - MLIST_SPACING;
        rect.height = MLIST_SPACING;
        if (UI_CursorInRect(&rect)) {
            l->prestep++;
            MenuList_ValidatePrestep(l);
            return QMS_MOVE;
        }
    }

    rect.x = l->generic.rect.x;
    rect.y = l->generic.rect.y;
    rect.width = l->generic.rect.width;
    rect.height = MLIST_SPACING;

    if (l->mlFlags & MLF_SCROLLBAR) {
        rect.width -= MLIST_SCROLLBAR_WIDTH;
    }

    // click on header
    if (l->mlFlags & MLF_HEADER) {
        if (l->sortdir && UI_CursorInRect(&rect)) {
            for (j = 0; j < l->numcolumns; j++) {
                if (!l->columns[j].width) {
                    continue;
                }
                rect.width = l->columns[j].width;
                if (UI_CursorInRect(&rect)) {
                    return MenuList_SetColumn(l, j);
                }
                rect.x += rect.width;
            }
            return QMS_SILENT;
        }
        rect.y += MLIST_SPACING;
    }

    // click on item
    j = min(l->numItems, l->prestep + l->maxItems);
    for (i = l->prestep; i < j; i++) {
        if (UI_CursorInRect(&rect)) {
            if (l->curvalue == i && uis.realtime -
                l->clickTime < DOUBLE_CLICK_DELAY) {
                if (l->generic.activate) {
                    return l->generic.activate(&l->generic);
                }
                return QMS_SILENT;
            }
            l->clickTime = uis.realtime;
            l->curvalue = i;
            if (l->generic.change) {
                return l->generic.change(&l->generic);
            }
            return QMS_SILENT;
        }
        rect.y += MLIST_SPACING;
    }

    return QMS_SILENT;
}

/*
=================
MenuList_Key
=================
*/
static menuSound_t MenuList_Key(menuList_t *l, int key)
{
    //int i;

    if (!l->items) {
        return QMS_NOTHANDLED;
    }

    if (Key_IsDown(K_ALT) && Q_isdigit(key)) {
        return MenuList_FindColumn(l, key - '0');
    }

#if 0
    if (key > 32 && key < 127) {
        if (uis.realtime > l->scratchTime + 1300) {
            l->scratchCount = 0;
            l->scratchTime = uis.realtime;
        }

        if (l->scratchCount >= sizeof(l->scratch) - 1) {
            return QMS_NOTHANDLED;
        }

        l->scratch[l->scratchCount++] = key;
        l->scratch[l->scratchCount] = 0;

        //l->scratchTime = uis.realtime;

        if (!Q_stricmpn(UI_GetColumn((char *)l->items[l->curvalue] + l->extrasize, l->sortcol),
                        l->scratch, l->scratchCount)) {
            return QMS_NOTHANDLED;
        }

        for (i = 0; i < l->numItems; i++) {
            if (!Q_stricmpn(UI_GetColumn((char *)l->items[i] + l->extrasize, l->sortcol), l->scratch, l->scratchCount)) {
                MenuList_SetValue(l, i);
                return QMS_SILENT;
            }
            i++;
        }

        return QMS_NOTHANDLED;
    }

    l->scratchCount = 0;
#endif

    switch (key) {
    case K_LEFTARROW:
    case 'h':
        return MenuList_PrevColumn(l);

    case K_RIGHTARROW:
    case 'l':
        return MenuList_NextColumn(l);

    case K_UPARROW:
    case K_KP_UPARROW:
    case 'k':
        if (l->curvalue < 0) {
            goto home;
        }
        if (l->curvalue > 0) {
            l->curvalue--;
            if (l->generic.change) {
                l->generic.change(&l->generic);
            }
            MenuList_AdjustPrestep(l);
            return QMS_MOVE;
        }
        return QMS_BEEP;

    case K_DOWNARROW:
    case K_KP_DOWNARROW:
    case 'j':
        if (l->curvalue < 0) {
            goto home;
        }
        if (l->curvalue < l->numItems - 1) {
            l->curvalue++;
            if (l->generic.change) {
                l->generic.change(&l->generic);
            }
            MenuList_AdjustPrestep(l);
            return QMS_MOVE;
        }
        return QMS_BEEP;

    case K_HOME:
    case K_KP_HOME:
    home:
        l->prestep = 0;
        l->curvalue = 0;
        if (l->generic.change) {
            l->generic.change(&l->generic);
        }
        return QMS_MOVE;

    case K_END:
    case K_KP_END:
        if (!l->numItems) {
            goto home;
        }
        if (l->numItems > l->maxItems) {
            l->prestep = l->numItems - l->maxItems;
        }
        l->curvalue = l->numItems - 1;
        if (l->generic.change) {
            l->generic.change(&l->generic);
        }
        return QMS_MOVE;

    case K_MWHEELUP:
        if (Key_IsDown(K_CTRL)) {
            l->prestep -= 4;
        } else {
            l->prestep -= 2;
        }
        MenuList_ValidatePrestep(l);
        return QMS_SILENT;

    case K_MWHEELDOWN:
        if (Key_IsDown(K_CTRL)) {
            l->prestep += 4;
        } else {
            l->prestep += 2;
        }
        MenuList_ValidatePrestep(l);
        return QMS_SILENT;

    case K_PGUP:
    case K_KP_PGUP:
        if (l->curvalue < 0) {
            goto home;
        }
        if (l->curvalue > 0) {
            l->curvalue -= l->maxItems - 1;
            if (l->curvalue < 0) {
                l->curvalue = 0;
            }
            if (l->generic.change) {
                l->generic.change(&l->generic);
            }
            MenuList_AdjustPrestep(l);
            return QMS_MOVE;
        }
        return QMS_BEEP;

    case K_PGDN:
    case K_KP_PGDN:
        if (l->curvalue < 0) {
            goto home;
        }
        if (l->curvalue < l->numItems - 1) {
            l->curvalue += l->maxItems - 1;
            if (l->curvalue > l->numItems - 1) {
                l->curvalue = l->numItems - 1;
            }
            if (l->generic.change) {
                l->generic.change(&l->generic);
            }
            MenuList_AdjustPrestep(l);
            return QMS_MOVE;
        }
        return QMS_BEEP;

    case K_MOUSE1:
    //case K_MOUSE2:
    //case K_MOUSE3:
        return MenuList_Click(l);
    }

    return QMS_NOTHANDLED;
}

static menuSound_t MenuList_MouseMove(menuList_t *l)
{
    int y, h, barHeight;

    if (uis.mouseTracker != &l->generic)
        return QMS_NOTHANDLED;

    y = l->generic.y + MLIST_SPACING;
    h = l->generic.height;

    if (l->mlFlags & MLF_HEADER) {
        y += MLIST_SPACING;
        h -= MLIST_SPACING;
    }

    barHeight = h - MLIST_SPACING * 2;
    if (barHeight > 0) {
        l->prestep = (uis.mouseCoords[1] - y - l->drag_y) * l->numItems / barHeight;
        MenuList_ValidatePrestep(l);
    }

    return QMS_SILENT;
}

/*
=================
MenuList_DrawString
=================
*/
static void MenuList_DrawString(int x, int y, int flags,
                                menuListColumn_t *column,
                                color_t color, const char *string)
{
    clipRect_t rc;

    rc.left = x;
    rc.right = x + column->width - 1;
    rc.top = y + 1;
    rc.bottom = y + CONCHAR_HEIGHT + 1;

    if ((column->uiFlags & UI_CENTER) == UI_CENTER) {
        x += column->width / 2 - 1;
    } else if (column->uiFlags & UI_RIGHT) {
        x += column->width - MLIST_PRESTEP;
    } else {
        x += MLIST_PRESTEP;
    }

    R_SetClipRect(&rc);
    UI_DrawString(x, y + 1, column->uiFlags | flags, color, string);
    R_SetClipRect(NULL);
}

/*
=================
MenuList_Draw
=================
*/
static void MenuList_Draw(menuList_t *l)
{
    char *s;
    int x, y, xx, yy;
    int i, j, k;
    int width, height;
    float pageFrac, prestepFrac;
    int barHeight;

    x = l->generic.rect.x;
    y = l->generic.rect.y;
    width = l->generic.rect.width;
    height = l->generic.rect.height;

    // draw header
    if (l->mlFlags & MLF_HEADER) {
        xx = x;
        for (j = 0; j < l->numcolumns; j++) {
            int flags = UI_ALTCOLOR;
            color_t color = uis.color.normal;

            if (!l->columns[j].width) {
                continue;
            }

            if (l->sortcol == j && l->sortdir) {
                flags = 0;
                if (l->generic.flags & QMF_HASFOCUS) {
                    color = uis.color.active;
                }
            }
            R_DrawFill32(xx, y, l->columns[j].width - 1,
                         MLIST_SPACING - 1, color);

            if (l->columns[j].name) {
                MenuList_DrawString(xx, y, flags,
                                    &l->columns[j], COLOR_WHITE, l->columns[j].name);
            }
            xx += l->columns[j].width;
        }
        y += MLIST_SPACING;
        height -= MLIST_SPACING;
    }

    if (l->mlFlags & MLF_SCROLLBAR) {
        barHeight = height - MLIST_SPACING * 2;
        yy = y + MLIST_SPACING;

        // draw scrollbar background
        R_DrawFill32(x + width - MLIST_SCROLLBAR_WIDTH, yy,
                     MLIST_SCROLLBAR_WIDTH - 1, barHeight,
                     uis.color.normal);

        if (l->numItems > l->maxItems) {
            pageFrac = (float)l->maxItems / l->numItems;
            prestepFrac = (float)l->prestep / l->numItems;
        } else {
            pageFrac = 1;
            prestepFrac = 0;
        }

        // draw scrollbar thumb
        R_DrawFill32(x + width - MLIST_SCROLLBAR_WIDTH,
                     yy + Q_rint(barHeight * prestepFrac),
                     MLIST_SCROLLBAR_WIDTH - 1,
                     Q_rint(barHeight * pageFrac),
                     uis.color.selection);
    }

    // draw background
    xx = x;
    for (j = 0; j < l->numcolumns; j++) {
        color_t color = uis.color.normal;

        if (!l->columns[j].width) {
            continue;
        }

        if (l->sortcol == j && l->sortdir) {
            if (l->generic.flags & QMF_HASFOCUS) {
                color = uis.color.active;
            }
        }
        R_DrawFill32(xx, y, l->columns[j].width - 1,
                     height, color);

        xx += l->columns[j].width;
    }

    yy = y;
    k = min(l->numItems, l->prestep + l->maxItems);
    color_t color = COLOR_WHITE;

    for (i = l->prestep; i < k; i++) {
        // draw selection
        if (!(l->generic.flags & QMF_DISABLED) && i == l->curvalue) {
            xx = x;
            for (j = 0; j < l->numcolumns; j++) {
                if (!l->columns[j].width) {
                    continue;
                }
                R_DrawFill32(xx, yy, l->columns[j].width - 1,
                             MLIST_SPACING, uis.color.selection);
                xx += l->columns[j].width;
            }
        }

        // draw contents
        s = (char *)l->items[i] + l->extrasize;

        if (l->mlFlags & MLF_COLOR) {
            color = ColorU32(*((uint32_t *)(s - 4)));
        }

        xx = x;
        for (j = 0; j < l->numcolumns; j++) {
            if (!*s) {
                break;
            }

            if (l->columns[j].width) {
                MenuList_DrawString(xx, yy, 0, &l->columns[j], color, s);
                xx += l->columns[j].width;
            }
            s += strlen(s) + 1;
        }

        yy += MLIST_SPACING;
    }
}

void MenuList_Sort(menuList_t *l, int offset, int (*cmpfunc)(const void *, const void *))
{
    void *n;
    int i;

    if (!l->items)
        return;

    if (offset >= l->numItems)
        return;

    if (l->sortcol < 0 || l->sortcol >= l->numcolumns)
        return;

    if (l->curvalue < 0 || l->curvalue >= l->numItems)
        n = NULL;
    else
        n = l->items[l->curvalue];

    qsort(l->items + offset, l->numItems - offset, sizeof(char *), cmpfunc);

    for (i = 0; i < l->numItems; i++) {
        if (l->items[i] == n) {
            l->curvalue = i;
            break;
        }
    }
}

/*
===================================================================

SEPARATOR CONTROL

===================================================================
*/

/*
=================
Separator_Init
=================
*/
static void Separator_Init(menuSeparator_t *s)
{
    s->generic.rect.x = s->generic.rect.y = 999999;
    s->generic.rect.width = s->generic.rect.height = -999999;
}

/*
=================
Separator_Draw
=================
*/
static void Separator_Draw(menuSeparator_t *s)
{
    if (s->generic.name)
        UI_DrawString(s->generic.x, s->generic.y, UI_RIGHT, COLOR_WHITE, s->generic.name);
}

/*
===================================================================

MISC

===================================================================
*/

/*
=================
Common_DoEnter
=================
*/
static menuSound_t Common_DoEnter(menuCommon_t *item)
{
	if (item->activate) {
		menuSound_t sound = item->activate(item);
		if (sound != QMS_NOTHANDLED) {
			return sound;
		}
	}

	return QMS_IN;
}

/*
=============
Menu_MakeActivateCallback

Bridges legacy activation hooks to the C++ menu item callbacks.
=============
*/
static MenuItem::Callback Menu_MakeActivateCallback(menuCommon_t *item)
{
	return [item](MenuItem &)
	{
		Common_DoEnter(item);
	};
}

/*
=============
Menu_MakeTextureHandle

Creates a shared_ptr-backed texture handle for C++ menu items.
=============
*/
static MenuItem::TextureHandle Menu_MakeTextureHandle(qhandle_t handle)
{
	if (!handle)
	{
		return nullptr;
	}

	return std::make_shared<qhandle_t>(handle);
}

/*
=============
Menu_SyncLegacyToCpp

Copies mutable legacy widget state back into its MenuItem wrapper so the C++
rendering and input layers retain user edits.
=============
*/
static void Menu_SyncLegacyToCpp(menuCommon_t *item, MenuItem &wrapped)
{
	switch (item->type)
	{
	case MTYPE_FIELD:
	{
		auto *field = dynamic_cast<FieldItem *>(&wrapped);
		auto *legacy = reinterpret_cast<menuField_t *>(item);

		if (field && legacy)
		{
			field->SetValue(legacy->field.text);
		}
		break;
	}
	case MTYPE_SLIDER:
	{
		auto *slider = dynamic_cast<SliderItem *>(&wrapped);
		auto *legacy = reinterpret_cast<menuSlider_t *>(item);

		if (slider && legacy)
		{
			slider->SetRange(legacy->minvalue, legacy->maxvalue);
			slider->SetStep(legacy->step);
			slider->SetValue(legacy->curvalue);
		}
		break;
	}
	default:
		break;
	}
}

/*
=============
Menu_ItemCount

Returns the number of legacy items attached to a menu.
=============
*/
static int Menu_ItemCount(const menuFrameWork_t *menu)
{
	if (!menu)
		return 0;

	return static_cast<int>(menu->items.size());
}

/*
=============
Menu_EmplaceCppItem

Builds and stores a C++ wrapper for a legacy menu widget.
=============
*/
static void Menu_EmplaceCppItem(menuFrameWork_t *menu, menuCommon_t *item)
{
	std::unique_ptr<MenuItem> wrapped = Menu_BuildMenuItem(item);

	if (wrapped)
		menu->itemsCpp.emplace_back(std::move(wrapped));
}

/*
=============
Menu_BuildMenuItem

Instantiates the modern MenuItem subclass for a legacy menu widget.
=============
*/
static std::unique_ptr<MenuItem> Menu_BuildMenuItem(menuCommon_t *item)
{
	switch (item->type)
	{
	case MTYPE_ACTION:
	{
		auto *action = static_cast<menuAction_t *>(item);
		bool disabled = (action->generic.flags & (QMF_GRAYED | QMF_DISABLED)) != 0;
		return std::make_unique<ActionItem>(action->generic.name ? action->generic.name : "",
			action->generic.x,
			action->generic.y,
			action->generic.uiFlags,
			Menu_MakeActivateCallback(item),
			nullptr,
			disabled);
	}
	case MTYPE_STATIC:
	{
		auto *s = static_cast<menuStatic_t *>(item);
		color_t color = (s->generic.flags & QMF_CUSTOM_COLOR) ? s->generic.color : COLOR_WHITE;
		return std::make_unique<StaticItem>(s->generic.name ? s->generic.name : "",
			s->generic.x,
			s->generic.y,
			s->generic.uiFlags,
			nullptr,
			color);
	}
	case MTYPE_BITMAP:
	{
		auto *bitmap = static_cast<menuBitmap_t *>(item);
		return std::make_unique<BitmapItem>(bitmap->generic.name ? bitmap->generic.name : "",
			bitmap->generic.x,
			bitmap->generic.y,
			bitmap->generic.width,
			bitmap->generic.height,
			Menu_MakeTextureHandle(bitmap->pics[0]),
			Menu_MakeTextureHandle(bitmap->pics[1]),
			Menu_MakeActivateCallback(item));
	}
	case MTYPE_FIELD:
	{
		auto *field = static_cast<menuField_t *>(item);
		return std::make_unique<FieldItem>(field->generic.name ? field->generic.name : "",
			field->generic.x,
			field->generic.y,
			field->generic.uiFlags,
			field->field.text,
			field->field.visibleChars,
			field->field.maxChars,
			[field](MenuItem &)
			{
				if (field->generic.change)
					field->generic.change(&field->generic);
			},
			nullptr);
	}
	case MTYPE_SLIDER:
	{
		auto *slider = static_cast<menuSlider_t *>(item);
		return std::make_unique<SliderItem>(slider->generic.name ? slider->generic.name : "",
			slider->generic.x,
			slider->generic.y,
			slider->generic.uiFlags,
			slider->minvalue,
			slider->maxvalue,
			slider->step,
			slider->curvalue,
			[slider](MenuItem &)
			{
				if (slider->generic.change)
					slider->generic.change(&slider->generic);
			},
			nullptr);
	}
default:
	return nullptr;
}
}

/*
=============
Menu_FindCppItem

Finds the MenuItem wrapper associated with a legacy menu entry.
=============
*/
static MenuItem *Menu_FindCppItem(menuFrameWork_t *menu, const menuCommon_t *item)
{
	if (!menu)
		return nullptr;

	const size_t itemCount = std::min<size_t>(menu->itemsCpp.size(), menu->items.size());

	for (size_t i = 0; i < itemCount; i++) {
		if (menu->items[i] == item)
			return menu->itemsCpp[i].get();
	}

	return nullptr;
}
/*
=================
Menu_AddItem
=================
*/
void Menu_AddItem(menuFrameWork_t *menu, void *item)
{
	Q_assert(Menu_ItemCount(menu) < MAX_MENU_ITEMS);

	menu->items.push_back(item);
	static_cast<menuCommon_t *>(item)->parent = menu;

	Menu_EmplaceCppItem(menu, static_cast<menuCommon_t *>(item));
}

static void UI_ClearBounds(int mins[2], int maxs[2])
{
    mins[0] = mins[1] = 9999;
    maxs[0] = maxs[1] = -9999;
}

static void UI_AddRectToBounds(const vrect_t *rc, int mins[2], int maxs[2])
{
    if (mins[0] > rc->x) {
        mins[0] = rc->x;
    } else if (maxs[0] < rc->x + rc->width) {
        maxs[0] = rc->x + rc->width;
    }

    if (mins[1] > rc->y) {
        mins[1] = rc->y;
    } else if (maxs[1] < rc->y + rc->height) {
        maxs[1] = rc->y + rc->height;
    }
}

void Menu_Init(menuFrameWork_t *menu)
{
    int focus = 0;

    menu->y1 = 0;
    menu->y2 = uis.height;

    if (!menu->size) {
        menu->size = Menu_Size;
    }
    menu->size(menu);

    for (int i = 0; i < Menu_ItemCount(menu); i++) {
        void *rawItem = menu->items[i];
        auto *item = static_cast<menuCommon_t *>(rawItem);

        focus |= item->flags & QMF_HASFOCUS;
        switch (item->type) {
        case MTYPE_FIELD:
            Field_Init(static_cast<menuField_t *>(rawItem));
            break;
        case MTYPE_SLIDER:
            Slider_Init(static_cast<menuSlider_t *>(rawItem));
            break;
        case MTYPE_LIST:
            MenuList_Init(static_cast<menuList_t *>(rawItem));
            break;
        case MTYPE_SPINCONTROL:
        case MTYPE_BITFIELD:
        case MTYPE_PAIRS:
        case MTYPE_VALUES:
        case MTYPE_STRINGS:
        case MTYPE_TOGGLE:
            SpinControl_Init(static_cast<menuSpinControl_t *>(rawItem));
            break;
        case MTYPE_EPISODE:
            EpisodeControl_Init(static_cast<menuEpisodeSelector_t *>(rawItem));
            break;
        case MTYPE_UNIT:
            UnitControl_Init(static_cast<menuUnitSelector_t *>(rawItem));
            break;
        case MTYPE_IMAGESPINCONTROL:
            ImageSpinControl_Init(static_cast<menuSpinControl_t *>(rawItem));
            break;
        case MTYPE_CHECKBOX:
            Checkbox_Init(reinterpret_cast<menuCheckbox_t *>(rawItem));
            break;
        case MTYPE_DROPDOWN:
            Dropdown_Init(reinterpret_cast<menuDropdown_t *>(rawItem));
            break;
        case MTYPE_RADIO:
            RadioButton_Init(reinterpret_cast<menuRadioButton_t *>(rawItem));
            break;
        case MTYPE_ACTION:
        case MTYPE_SAVEGAME:
        case MTYPE_LOADGAME:
            Action_Init(static_cast<menuAction_t *>(rawItem));
            break;
        case MTYPE_SEPARATOR:
            Separator_Init(static_cast<menuSeparator_t *>(rawItem));
            break;
        case MTYPE_STATIC:
            Static_Init(static_cast<menuStatic_t *>(rawItem));
            break;
        case MTYPE_KEYBIND:
            Keybind_Init(static_cast<menuKeybind_t *>(rawItem));
            break;
        case MTYPE_BITMAP:
            Bitmap_Init(static_cast<menuBitmap_t *>(rawItem));
            break;
        default:
            Q_assert(!"unknown item type");
        }
    }

    // set focus to the first item by default
    if (!focus && Menu_ItemCount(menu)) {
        auto *item = static_cast<menuCommon_t *>(menu->items[0]);
        item->flags |= QMF_HASFOCUS;
        if (item->status) {
            menu->status = item->status;
        }
    }

    Menu_UpdateGroupBounds(menu);

    // calc menu bounding box
    UI_ClearBounds(menu->mins, menu->maxs);

    for (int i = 0; i < Menu_ItemCount(menu); i++) {
        auto *item = static_cast<menuCommon_t *>(menu->items[i]);
        UI_AddRectToBounds(&item->rect, menu->mins, menu->maxs);
    }

	if (!menu->groups.empty()) {
		for (int i = 0; i < menu->numGroups; i++) {
			uiItemGroup_t *group = menu->groups[i].get();
			if (!group || !group->active)
				continue;
			if (group->rect.width <= 0 || group->rect.height <= 0)
				continue;
			UI_AddRectToBounds(&group->rect, menu->mins, menu->maxs);
		}
	}

    // expand
    menu->mins[0] -= MENU_SPACING;
    menu->mins[1] -= MENU_SPACING;
    menu->maxs[0] += MENU_SPACING;
    menu->maxs[1] += MENU_SPACING;

    // clamp
    if (menu->mins[0] < 0) menu->mins[0] = 0;
    if (menu->mins[1] < 0) menu->mins[1] = 0;
    if (menu->maxs[0] > uis.width) menu->maxs[0] = uis.width;
    if (menu->maxs[1] > uis.height) menu->maxs[1] = uis.height;
}

void Menu_Size(menuFrameWork_t *menu)
{
    int x, y, w;
    int h = 0;
    int widest = -1;
    uiItemGroup_t *currentGroup = NULL;

	if (!menu->groups.empty()) {
		for (int i = 0; i < menu->numGroups; i++) {
			uiItemGroup_t *group = menu->groups[i].get();
			if (!group)
				continue;
			group->active = false;
			group->contentTop = 0;
			group->contentBottom = 0;
			group->headerY = 0;
			group->baseX = 0;
			group->rect = {};
			group->headerRect = {};
		}
	}

    // count visible items including groups
    for (int i = 0; i < Menu_ItemCount(menu); i++) {
        auto *item = static_cast<menuCommon_t *>(menu->items[i]);
        if (item->flags & QMF_HIDDEN)
            continue;

        uiItemGroup_t *group = item->group;
        if (group && !group->hasItems)
            group = NULL;

        if (group != currentGroup) {
            if (currentGroup)
                h += currentGroup->padding;
            if (group) {
                group->active = true;
                h += group->headerHeight;
                h += group->padding;
            }
            currentGroup = group;
        }

        if (item->type == MTYPE_BITMAP) {
            h += GENERIC_SPACING(item->height);
            if (widest < item->width)
                widest = item->width;
        } else if (item->type == MTYPE_IMAGESPINCONTROL) {
            h += GENERIC_SPACING(item->height);
        } else {
            h += MENU_SPACING;
        }
    }

    if (currentGroup)
        h += currentGroup->padding;

    // account for banner
    if (menu->banner)
        h += GENERIC_SPACING(menu->banner_rc.height);

    // set menu top/bottom
    if (menu->compact) {
        menu->y1 = (uis.height - h) / 2 - MENU_SPACING;
        menu->y2 = (uis.height + h) / 2 + MENU_SPACING;
    } else {
        menu->y1 = 0;
        menu->y2 = uis.height;
    }

    // set menu horizontal base
    if (widest == -1) {
        x = uis.width / 2;
    } else {
        // if menu has bitmaps, it is expected to have plaque and logo
        // align them horizontally to avoid going off screen on small resolution
        w = widest + CURSOR_WIDTH;
        if (menu->plaque_rc.width > menu->logo_rc.width)
            w += menu->plaque_rc.width;
        else
            w += menu->logo_rc.width;
        x = (uis.width + w) / 2 - widest;
    }

    // set menu vertical base
    y = (uis.height - h) / 2;

	if (!menu->groups.empty()) {
		for (int i = 0; i < menu->numGroups; i++) {
			uiItemGroup_t *group = menu->groups[i].get();
			if (!group)
				continue;
			group->contentTop = 0;
			group->contentBottom = 0;
		}
	}

    // banner is horizontally centered and
    // positioned on top of all menu items
    if (menu->banner) {
        menu->banner_rc.x = (uis.width - menu->banner_rc.width) / 2;
        menu->banner_rc.y = y;
        y += GENERIC_SPACING(menu->banner_rc.height);
    }

    // plaque and logo are vertically centered and
    // positioned to the left of bitmaps and cursor
    h = 0;
    if (menu->plaque)
        h += menu->plaque_rc.height;
    if (menu->logo)
        h += menu->logo_rc.height + 5;

    if (menu->plaque) {
        menu->plaque_rc.x = x - CURSOR_WIDTH - menu->plaque_rc.width;
        menu->plaque_rc.y = (uis.height - h) / 2;
    }

    if (menu->logo) {
        menu->logo_rc.x = x - CURSOR_WIDTH - menu->logo_rc.width;
        menu->logo_rc.y = (uis.height + h) / 2 - menu->logo_rc.height;
    }

    // align items
    currentGroup = NULL;
    for (int i = 0; i < Menu_ItemCount(menu); i++) {
        auto *item = static_cast<menuCommon_t *>(menu->items[i]);
        if (item->flags & QMF_HIDDEN)
            continue;

        uiItemGroup_t *group = item->group;
        if (group && !group->hasItems)
            group = NULL;

        if (group != currentGroup) {
            if (currentGroup)
                y += currentGroup->padding;
            if (group) {
                group->headerY = y;
                group->baseX = x + group->indent;
                if (group->headerHeight > 0)
                    y += group->headerHeight;
                y += group->padding;
                group->contentTop = y;
            }
            currentGroup = group;
        }

        item->x = group ? x + group->indent : x;
        item->y = y;

        if (item->type == MTYPE_BITMAP) {
            y += GENERIC_SPACING(item->height);
        } else if (item->type == MTYPE_IMAGESPINCONTROL) {
            y += GENERIC_SPACING(item->height);
        } else {
            y += MENU_SPACING;
        }

        if (group)
            group->contentBottom = y;
    }

    if (currentGroup)
        y += currentGroup->padding;
}

static void Menu_UpdateGroupBounds(menuFrameWork_t *menu)
{
	if (menu->groups.empty())
		return;

	for (int i = 0; i < menu->numGroups; i++) {
		uiItemGroup_t *group = menu->groups[i].get();
		if (!group)
			continue;

		if (!group->active) {
			group->rect = {};
			group->headerRect = {};
			continue;
		}

		int minX = INT_MAX;
		int maxX = INT_MIN;
		int minY = INT_MAX;
		int maxY = INT_MIN;

		for (int j = 0; j < Menu_ItemCount(menu); j++) {
			auto *item = static_cast<menuCommon_t *>(menu->items[j]);
			if (item->flags & QMF_HIDDEN)
				continue;
			if (item->group != group)
				continue;

			const vrect_t &rect = item->rect;
            if (minX > rect.x)
                minX = rect.x;
            if (maxX < rect.x + rect.width)
                maxX = rect.x + rect.width;
            if (minY > rect.y)
                minY = rect.y;
            if (maxY < rect.y + rect.height)
                maxY = rect.y + rect.height;
        }

        if (minX == INT_MAX || maxX == INT_MIN) {
            group->active = false;
            group->rect = {};
            group->headerRect = {};
            continue;
        }

        int top = group->headerHeight > 0 ? group->headerY : group->contentTop - group->padding;
        int bottom = group->contentBottom + group->padding;
        if (bottom < top)
            bottom = top + group->headerHeight;

        group->rect.x = minX - group->padding;
        group->rect.y = top;
        group->rect.width = (maxX - minX) + group->padding * 2;
        group->rect.height = bottom - top;

        if (group->rect.width < CONCHAR_WIDTH)
            group->rect.width = CONCHAR_WIDTH;
        if (group->rect.height < group->headerHeight)
            group->rect.height = group->headerHeight;

        group->headerRect.x = group->baseX;
        group->headerRect.y = group->headerY;
        group->headerRect.width = group->rect.width;
        group->headerRect.height = group->headerHeight;
    }
}

static void Menu_DrawGroups(menuFrameWork_t *menu)
{
	if (menu->groups.empty())
		return;

	for (int i = 0; i < menu->numGroups; i++) {
		uiItemGroup_t *group = menu->groups[i].get();
		if (!group || !group->active)
			continue;

		if (group->rect.width <= 0 || group->rect.height <= 0)
			continue;

		color_t background = group->hasBackground ? group->background : ColorSetAlpha(uis.color.background, static_cast<uint8_t>(160));
		R_DrawFill32(group->rect.x, group->rect.y, group->rect.width, group->rect.height, background);

		if (group->border) {
			color_t border = ColorSetAlpha(uis.color.selection, static_cast<uint8_t>(200));
			R_DrawFill32(group->rect.x, group->rect.y, group->rect.width, 1, border);
			R_DrawFill32(group->rect.x, group->rect.y + group->rect.height - 1, group->rect.width, 1, border);
			R_DrawFill32(group->rect.x, group->rect.y, 1, group->rect.height, border);
            R_DrawFill32(group->rect.x + group->rect.width - 1, group->rect.y, 1, group->rect.height, border);
        }

        if (group->label && group->headerHeight > 0) {
            color_t labelColor = uis.color.normal;
            UI_DrawString(group->baseX + LCOLUMN_OFFSET, group->headerRect.y,
                          UI_LEFT | UI_ALTCOLOR, labelColor, group->label);
        }
    }
}

menuCommon_t *Menu_ItemAtCursor(menuFrameWork_t *m)
{
	menuCommon_t *item;
	int i;

	const size_t itemCount = std::min<size_t>(m->itemsCpp.size(), m->items.size());

	for (size_t cppIndex = 0; cppIndex < itemCount; cppIndex++) {
		item = static_cast<menuCommon_t *>(m->items[cppIndex]);
		MenuItem *wrapped = m->itemsCpp[cppIndex].get();

		if (wrapped && wrapped->HasFocus()) {
			if (!(item->flags & QMF_HASFOCUS)) {
				Menu_SetFocus(item);
			}

			return item;
		}
	}

	for (i = 0; i < Menu_ItemCount(m); i++) {
		item = static_cast<menuCommon_t *>(m->items[i]);
		if (item->flags & QMF_HASFOCUS) {
			MenuItem *wrapped = Menu_FindCppItem(m, item);

			if (wrapped && !wrapped->HasFocus()) {
				wrapped->SetFocus(true);
			}

			return item;
		}
	}

	return NULL;
}

/*
=============
Menu_SetFocus

Synchronizes focus flags across legacy items and their C++ wrappers.
=============
*/
void Menu_SetFocus(menuCommon_t *focus)
{
	menuFrameWork_t *menu;
	menuCommon_t *item;
	int i;

	if (focus->flags & QMF_HASFOCUS) {
		return;
	}

	menu = focus->parent;

	menuDropdown_t *activeDropdown = UI_MenuController().ActiveDropdown(menu);
	if (activeDropdown && &activeDropdown->spin.generic != focus) {
		Dropdown_Close(activeDropdown, false);
	}

	for (i = 0; i < Menu_ItemCount(menu); i++) {
		item = static_cast<menuCommon_t *>(menu->items[i]);
		MenuItem *wrapped = Menu_FindCppItem(menu, item);

		if (item == focus) {
			item->flags |= QMF_HASFOCUS;
			if (item->focus) {
				item->focus(item, true);
			} else if (item->status) {
				menu->status = item->status;
			}
		} else if (item->flags & QMF_HASFOCUS) {
			item->flags &= ~QMF_HASFOCUS;
			if (item->focus) {
				item->focus(item, false);
			} else if (menu->status == item->status
				   && menu->status != focus->status) {
				menu->status = NULL;
			}
		}

		if (wrapped) {
			wrapped->SetFocus((item->flags & QMF_HASFOCUS) != 0);
		}
	}

}

/*
=================
Menu_AdjustCursor

This function takes the given menu, the direction, and attempts
to adjust the menu's cursor so that it's at the next available
slot.
=================
*/
menuSound_t Menu_AdjustCursor(menuFrameWork_t *m, int dir)
{
	menuCommon_t *item;
	int cursor, pos;
	int i;
	bool foundFocus;

	if (!Menu_ItemCount(m)) {
	return QMS_NOTHANDLED;
	}

	pos = 0;
	foundFocus = false;
	const size_t itemCount = std::min<size_t>(m->itemsCpp.size(), m->items.size());

	for (size_t cppIndex = 0; cppIndex < itemCount; cppIndex++) {
		item = static_cast<menuCommon_t *>(m->items[cppIndex]);
		MenuItem *wrapped = m->itemsCpp[cppIndex].get();

		if (wrapped && wrapped->HasFocus()) {
			pos = static_cast<int>(cppIndex);
			foundFocus = true;
			break;
		}
	}

	if (!foundFocus) {
		for (i = 0; i < Menu_ItemCount(m); i++) {
			item = static_cast<menuCommon_t *>(m->items[i]);

			if (item->flags & QMF_HASFOCUS) {
				pos = i;
				foundFocus = true;
				break;
			}
		}
	}

	/*
	** crawl in the direction indicated until we find a valid spot
	*/
	cursor = pos;
	if (dir == 1) {
		do {
			cursor++;
			if (cursor >= Menu_ItemCount(m))
				cursor = 0;

			item = static_cast<menuCommon_t *>(m->items[cursor]);
			if (UI_IsItemSelectable(item))
				break;
		} while (cursor != pos);
	} else {
		do {
			cursor--;
			if (cursor < 0)
				cursor = Menu_ItemCount(m) - 1;

			item = static_cast<menuCommon_t *>(m->items[cursor]);
			if (UI_IsItemSelectable(item))
				break;
		} while (cursor != pos);
	}

	Menu_SetFocus(item);

	return QMS_MOVE;
}


static void Menu_DrawStatus(menuFrameWork_t *menu)
{
    int     linewidth = uis.width / CONCHAR_WIDTH;
    int     x, y, l, count;
    char    *txt, *p;
    int     lens[8];
    char    *ptrs[8];

    txt = menu->status;
    x = 0;

    count = 0;
    ptrs[0] = txt;

    while (*txt) {
        // count word length
        for (p = txt; *p > 32; p++)
            ;
        l = p - txt;

        // word wrap
        if ((l < linewidth && x + l > linewidth) || (x == linewidth)) {
            if (count == 7)
                break;
            lens[count++] = x;
            ptrs[count] = txt;
            x = 0;
        }

        // display character and advance
        txt++;
        x++;
    }

    lens[count++] = x;

    const int lineHeight = SCR_FontLineHeight(1, uis.fontHandle);

    R_DrawFill8(0, menu->y2 - count * lineHeight, uis.width, count * lineHeight, 4);

    for (l = 0; l < count; l++) {
        const int lineWidth = SCR_MeasureString(1, 0, lens[l], ptrs[l], uis.fontHandle);
        x = (uis.width - lineWidth) / 2;
        y = menu->y2 - (count - l) * lineHeight;
        SCR_DrawStringStretch(x, y, 1, 0, lens[l], ptrs[l], COLOR_WHITE, uis.fontHandle);
    }
}

/*
=================
Menu_Draw
=================
*/
void Menu_Draw(menuFrameWork_t *menu)
{
    color_t color = COLOR_WHITE;

    Menu_UpdateConditionalState(menu);

    menuCommon_t *focusItem = Menu_ItemAtCursor(menu);
    if (focusItem && !UI_IsItemSelectable(focusItem)) {
        focusItem->flags &= ~QMF_HASFOCUS;
        focusItem = NULL;
    }

    if (!focusItem) {
        for (int i = 0; i < Menu_ItemCount(menu); i++) {
            auto *candidate = static_cast<menuCommon_t *>(menu->items[i]);
            if (candidate->flags & QMF_HIDDEN)
                continue;
            if (!UI_IsItemSelectable(candidate)) {
                candidate->flags &= ~QMF_HASFOCUS;
                continue;
            }
            Menu_SetFocus(candidate);
            focusItem = candidate;
            break;
        }
        if (!focusItem)
            menu->status = NULL;
    }

    Menu_UpdateGroupBounds(menu);

	menuDropdown_t *openDropdown = UI_MenuController().ActiveDropdown(menu);
	if (openDropdown && !openDropdown->open)
		openDropdown = NULL;

//
// draw background
//
    if (menu->image) {
        R_DrawKeepAspectPic(0, menu->y1, uis.width,
                            menu->y2 - menu->y1, color, menu->image);
    } else {
        R_DrawFill32(0, menu->y1, uis.width,
                     menu->y2 - menu->y1, menu->color);
    }

//
// draw title bar
//
    if (menu->title) {
        UI_DrawString(uis.width / 2, menu->y1,
                      UI_CENTER | UI_ALTCOLOR, color, menu->title);
    }

//
// draw banner, plaque and logo
//
    if (menu->banner) {
        R_DrawPic(menu->banner_rc.x, menu->banner_rc.y, color, menu->banner);
    }
    if (menu->plaque) {
        R_DrawPic(menu->plaque_rc.x, menu->plaque_rc.y, color, menu->plaque);
    }
    if (menu->logo) {
        R_DrawPic(menu->logo_rc.x, menu->logo_rc.y, color, menu->logo);
    }

    Menu_DrawGroups(menu);

//
// draw contents
//
	const size_t itemCount = std::min<size_t>(menu->itemsCpp.size(), menu->items.size());

	for (size_t i = 0; i < itemCount; i++) {
		auto *item = static_cast<menuCommon_t *>(menu->items[i]);

		if (item->flags & QMF_HIDDEN) {
			continue;
		}

		menu->itemsCpp[i]->Draw();

		if (ui_debug->integer) {
			UI_DrawRect8(&item->rect, 1, 223);
		}
	}

    if (openDropdown)
        Dropdown_DrawList(openDropdown);

//
// draw status bar
//
    if (menu->status) {
        Menu_DrawStatus(menu);
    }
}

/*
=============
Menu_SelectItem

Activates the currently focused menu item, favoring C++ handlers before
falling back to legacy callbacks.
=============
*/
menuSound_t Menu_SelectItem(menuFrameWork_t *s)
{
	menuCommon_t *item;

	if (!(item = Menu_ItemAtCursor(s))) {
		return QMS_NOTHANDLED;
	}

	if (MenuItem *wrapped = Menu_FindCppItem(s, item)) {
		if (wrapped->Activate()) {
			return QMS_IN;
		}
	}

	switch (item->type) {
	//case MTYPE_SLIDER:
	//	return Slider_DoSlide((menuSlider_t *)item, 1);
	case MTYPE_SPINCONTROL:
	case MTYPE_BITFIELD:
	case MTYPE_PAIRS:
	case MTYPE_VALUES:
	case MTYPE_STRINGS:
	case MTYPE_TOGGLE:
	case MTYPE_IMAGESPINCONTROL:
	case MTYPE_EPISODE:
	case MTYPE_UNIT:
		return static_cast<menuSound_t>(
			SpinControl_DoEnter(reinterpret_cast<menuSpinControl_t *>(item)));
	case MTYPE_KEYBIND:
		return Keybind_DoEnter(reinterpret_cast<menuKeybind_t *>(item));
	case MTYPE_FIELD:
	case MTYPE_ACTION:
	case MTYPE_LIST:
	case MTYPE_BITMAP:
	case MTYPE_SAVEGAME:
	case MTYPE_LOADGAME:
	case MTYPE_CHECKBOX:
	case MTYPE_DROPDOWN:
	case MTYPE_RADIO:
		return Common_DoEnter(item);
	default:
		return QMS_NOTHANDLED;
	}
}

/*
=============
Menu_SlideItem

Routes directional input through MenuItem handlers before falling back.
=============
*/
menuSound_t Menu_SlideItem(menuFrameWork_t *s, int dir)
{
	menuCommon_t *item;

	if (!(item = Menu_ItemAtCursor(s))) {
		return QMS_NOTHANDLED;
	}

	MenuItem *wrapped = Menu_FindCppItem(s, item);
	MenuItem::MenuEvent event{};
	event.type = MenuItem::MenuEvent::Type::Key;
	event.key = (dir < 0) ? K_LEFTARROW : K_RIGHTARROW;
	event.x = uis.mouseCoords[0];
	event.y = uis.mouseCoords[1];

	if (wrapped) {
		if (wrapped->HandleEvent(event)) {
			return QMS_SILENT;
		}
	}

	menuSound_t sound = QMS_NOTHANDLED;

	switch (item->type) {
	case MTYPE_SLIDER:
		sound = Slider_DoSlide(reinterpret_cast<menuSlider_t *>(item), dir);
		break;
	case MTYPE_SPINCONTROL:
	case MTYPE_BITFIELD:
	case MTYPE_PAIRS:
	case MTYPE_VALUES:
	case MTYPE_STRINGS:
	case MTYPE_TOGGLE:
	case MTYPE_IMAGESPINCONTROL:
	case MTYPE_EPISODE:
	case MTYPE_UNIT:
		sound = static_cast<menuSound_t>(
				SpinControl_DoSlide(reinterpret_cast<menuSpinControl_t *>(item), dir));
		break;
	case MTYPE_CHECKBOX:
		sound = Checkbox_Slide(reinterpret_cast<menuCheckbox_t *>(item), dir);
		break;
	case MTYPE_DROPDOWN:
		sound = Dropdown_Slide(reinterpret_cast<menuDropdown_t *>(item), dir);
		break;
	default:
		break;
	}

	if (wrapped) {
		Menu_SyncLegacyToCpp(item, *wrapped);
	}

	return sound;
}

/*
=============
Menu_KeyEvent

Builds a MenuEvent for key presses and dispatches to C++ handlers before
legacy fallbacks.
=============
*/
menuSound_t Menu_KeyEvent(menuCommon_t *item, int key)
{
	MenuItem *wrapped = Menu_FindCppItem(item->parent, item);

	if (wrapped) {
		MenuItem::MenuEvent event{};
		event.type = MenuItem::MenuEvent::Type::Key;
		event.key = key;
		event.x = uis.mouseCoords[0];
		event.y = uis.mouseCoords[1];

		if (wrapped->HandleEvent(event)) {
			return QMS_SILENT;
		}
	}

	menuSound_t sound = QMS_NOTHANDLED;

	if (item->keydown) {
		sound = item->keydown(item, key);
		if (sound != QMS_NOTHANDLED) {
			if (wrapped) {
				Menu_SyncLegacyToCpp(item, *wrapped);
			}
			return sound;
		}
	}

	switch (item->type) {
	case MTYPE_FIELD:
		sound = static_cast<menuSound_t>(
				Field_Key(reinterpret_cast<menuField_t *>(item), key));
		break;
	case MTYPE_LIST:
		sound = MenuList_Key(reinterpret_cast<menuList_t *>(item), key);
		break;
	case MTYPE_SLIDER:
		sound = Slider_Key(reinterpret_cast<menuSlider_t *>(item), key);
		break;
	case MTYPE_KEYBIND:
		sound = Keybind_Key(reinterpret_cast<menuKeybind_t *>(item), key);
		break;
	default:
		sound = QMS_NOTHANDLED;
		break;
	}

	if (wrapped) {
		Menu_SyncLegacyToCpp(item, *wrapped);
	}

	return sound;
}


/*
=============
Menu_CharEvent

Sends character input through MenuItem handlers before legacy fallbacks.
=============
*/
menuSound_t Menu_CharEvent(menuCommon_t *item, int key)
{
	MenuItem *wrapped = Menu_FindCppItem(item->parent, item);

	if (wrapped) {
		MenuItem::MenuEvent event{};
		event.type = MenuItem::MenuEvent::Type::Key;
		event.key = key;
		event.x = uis.mouseCoords[0];
		event.y = uis.mouseCoords[1];

		if (wrapped->HandleEvent(event)) {
			return QMS_SILENT;
		}
	}

	menuSound_t sound = QMS_NOTHANDLED;

	switch (item->type) {
	case MTYPE_FIELD:
		sound = static_cast<menuSound_t>(
				Field_Char(reinterpret_cast<menuField_t *>(item), key));
		break;
	default:
		break;
	}

	if (wrapped) {
		Menu_SyncLegacyToCpp(item, *wrapped);
	}

	return sound;
}


/*
=============
Menu_MouseMove

Translates pointer movement into MenuEvents for C++ handlers before legacy dispatch.
=============
*/
menuSound_t Menu_MouseMove(menuCommon_t *item)
{
	MenuItem *wrapped = Menu_FindCppItem(item->parent, item);

	if (wrapped) {
		MenuItem::MenuEvent event{};
		event.type = MenuItem::MenuEvent::Type::Pointer;
		event.x = uis.mouseCoords[0];
		event.y = uis.mouseCoords[1];

		if (wrapped->HandleEvent(event)) {
			return QMS_SILENT;
		}
	}

	menuSound_t sound = QMS_NOTHANDLED;

	switch (item->type) {
	case MTYPE_LIST:
		sound = MenuList_MouseMove(reinterpret_cast<menuList_t *>(item));
		break;
	case MTYPE_SLIDER:
		sound = Slider_MouseMove(reinterpret_cast<menuSlider_t *>(item));
		break;
	case MTYPE_DROPDOWN:
		sound = Dropdown_MouseMove(reinterpret_cast<menuDropdown_t *>(item));
		break;
	default:
		break;
	}

	if (wrapped) {
		Menu_SyncLegacyToCpp(item, *wrapped);
	}

	return sound;
}


/*
=============
Menu_DefaultKey

Handles menu-level shortcuts while forwarding pointer activation into MenuItem handlers.
=============
*/
static menuSound_t Menu_DefaultKey(menuFrameWork_t *m, int key)
{
	return UI_MenuController().HandleKey(m, key);
}

menuSound_t Menu_Keydown(menuFrameWork_t *menu, int key)
{
    menuCommon_t *item;
    menuSound_t sound;

    if (menu->keywait) {
    }

    if (menu->keydown) {
        sound = menu->keydown(menu, key);
        if (sound != QMS_NOTHANDLED) {
            return sound;
        }
    }

    item = Menu_ItemAtCursor(menu);
    if (item) {
        sound = Menu_KeyEvent(item, key);
        if (sound != QMS_NOTHANDLED) {
            return sound;
        }
    }

    sound = Menu_DefaultKey(menu, key);
    return sound;
}


menuCommon_t *Menu_HitTest(menuFrameWork_t *menu)
{
    if (menu->keywait) {
        return NULL;
    }

	menuDropdown_t *activeDropdown = UI_MenuController().ActiveDropdown(menu);
	if (activeDropdown && activeDropdown->open) {
		if (UI_CursorInRect(&activeDropdown->listRect))
			return &activeDropdown->spin.generic;
	}

    for (int i = 0; i < Menu_ItemCount(menu); i++) {
        auto *item = static_cast<menuCommon_t *>(menu->items[i]);
        if (item->flags & QMF_HIDDEN) {
            continue;
        }

        if (UI_CursorInRect(&item->rect)) {
            return item;
        }
    }

    return NULL;
}

bool Menu_Push(menuFrameWork_t *menu)
{
	for (int i = 0; i < Menu_ItemCount(menu); i++) {
		void *rawItem = menu->items[i];
		auto *item = static_cast<menuCommon_t *>(rawItem);

		switch (item->type) {
		case MTYPE_SLIDER:
			Slider_Push(static_cast<menuSlider_t *>(rawItem));
			break;
		case MTYPE_BITFIELD:
			BitField_Push(static_cast<menuSpinControl_t *>(rawItem));
			break;
		case MTYPE_PAIRS:
			Pairs_Push(static_cast<menuSpinControl_t *>(rawItem));
			break;
		case MTYPE_STRINGS:
			Strings_Push(static_cast<menuSpinControl_t *>(rawItem));
			break;
		case MTYPE_SPINCONTROL:
			SpinControl_Push(static_cast<menuSpinControl_t *>(rawItem));
			break;
		case MTYPE_TOGGLE:
			Toggle_Push(static_cast<menuSpinControl_t *>(rawItem));
			break;
		case MTYPE_DROPDOWN:
			Dropdown_Push(reinterpret_cast<menuDropdown_t *>(rawItem));
			break;
		case MTYPE_KEYBIND:
			Keybind_Push(static_cast<menuKeybind_t *>(rawItem));
			break;
		case MTYPE_FIELD:
			Field_Push(static_cast<menuField_t *>(rawItem));
			break;
		case MTYPE_SAVEGAME:
		case MTYPE_LOADGAME:
			Savegame_Push(static_cast<menuAction_t *>(rawItem));
			break;
		case MTYPE_IMAGESPINCONTROL:
			ImageSpinControl_Push(static_cast<menuSpinControl_t *>(rawItem));
			break;
		case MTYPE_EPISODE:
			Episode_Push(static_cast<menuEpisodeSelector_t *>(rawItem));
			break;
		case MTYPE_UNIT:
			Unit_Push(static_cast<menuUnitSelector_t *>(rawItem));
			break;
		default:
			break;
		}
	}

	const size_t itemCount = std::min<size_t>(menu->itemsCpp.size(), menu->items.size());

	for (size_t i = 0; i < itemCount; i++) {
		menu->itemsCpp[i]->OnAttach();
	}

	Menu_UpdateConditionalState(menu);
	return true;
}


void Menu_Pop(menuFrameWork_t *menu)
{
	for (int i = 0; i < Menu_ItemCount(menu); i++) {
		void *rawItem = menu->items[i];
		auto *item = static_cast<menuCommon_t *>(rawItem);

		switch (item->type) {
		case MTYPE_SLIDER:
			Slider_Pop(static_cast<menuSlider_t *>(rawItem));
			break;
		case MTYPE_BITFIELD:
			BitField_Pop(static_cast<menuSpinControl_t *>(rawItem));
			break;
		case MTYPE_PAIRS:
			Pairs_Pop(static_cast<menuSpinControl_t *>(rawItem));
			break;
		case MTYPE_STRINGS:
			Strings_Pop(static_cast<menuSpinControl_t *>(rawItem));
			break;
		case MTYPE_SPINCONTROL:
			SpinControl_Pop(static_cast<menuSpinControl_t *>(rawItem));
			break;
		case MTYPE_TOGGLE:
			Toggle_Pop(static_cast<menuSpinControl_t *>(rawItem));
			break;
		case MTYPE_DROPDOWN:
			Dropdown_Pop(reinterpret_cast<menuDropdown_t *>(rawItem));
			break;
		case MTYPE_KEYBIND:
			Keybind_Pop(static_cast<menuKeybind_t *>(rawItem));
			break;
		case MTYPE_FIELD:
			Field_Pop(static_cast<menuField_t *>(rawItem));
			break;
		case MTYPE_IMAGESPINCONTROL:
			ImageSpinControl_Pop(static_cast<menuSpinControl_t *>(rawItem));
			break;
		case MTYPE_EPISODE:
			Episode_Pop(static_cast<menuEpisodeSelector_t *>(rawItem));
			break;
		case MTYPE_UNIT:
			Unit_Pop(static_cast<menuUnitSelector_t *>(rawItem));
			break;
		default:
			break;
		}
	}

	const size_t itemCount = std::min<size_t>(menu->itemsCpp.size(), menu->items.size());

	for (size_t i = 0; i < itemCount; i++) {
		menu->itemsCpp[i]->OnDetach();
	}
}


void Menu_Free(menuFrameWork_t *menu)
{
	UI_MenuController().ResetDropdown(menu);

	menu->items.clear();
	menu->itemsCpp.clear();
	menu->groups.clear();
	menu->numGroups = 0;

	Z_Free(menu->title);
	Z_Free(menu->name);

	delete menu;
}

#ifdef UNIT_TESTS
static menuCommon_t menuTest_baseItem;
static menuCommon_t menuTest_overlayItem;
static menuFrameWork_t menuTest_base;
static menuFrameWork_t menuTest_overlay;
static bool menuTest_baseHandled;
static bool menuTest_overlayHandled;

/*
=============
MenuTest_ResetState

Clears shared UI state before each regression check.
=============
*/
static void MenuTest_ResetState(void)
{
	uis = {};
	menuTest_base = menuFrameWork_t{};
	menuTest_overlay = menuFrameWork_t{};
	menuTest_baseItem = {};
	menuTest_overlayItem = {};
	menuTest_baseHandled = false;
	menuTest_overlayHandled = false;

	uis.scale = 1.0f;
	uis.width = 640;
	uis.height = 480;
}

/*
=============
MenuTest_BuildStack

Constructs a two-layer stack with a non-modal overlay.
=============
*/
static void MenuTest_BuildStack(void)
{
	MenuTest_ResetState();

        Menu_AddItem(&menuTest_base, &menuTest_baseItem);
	menuTest_base.modal = true;
	menuTest_base.allowInputPassthrough = false;
	menuTest_base.opacity = 1.0f;
	menuTest_base.drawsBackdrop = false;
	menuTest_baseItem.type = MTYPE_ACTION;
	menuTest_baseItem.rect.x = 0;
	menuTest_baseItem.rect.y = 0;
	menuTest_baseItem.rect.width = 96;
	menuTest_baseItem.rect.height = 96;
	menuTest_baseItem.parent = &menuTest_base;

        Menu_AddItem(&menuTest_overlay, &menuTest_overlayItem);
	menuTest_overlay.modal = false;
	menuTest_overlay.allowInputPassthrough = true;
	menuTest_overlay.opacity = 1.0f;
	menuTest_overlay.drawsBackdrop = false;
	menuTest_overlayItem.type = MTYPE_ACTION;
	menuTest_overlayItem.rect.x = 200;
	menuTest_overlayItem.rect.y = 200;
	menuTest_overlayItem.rect.width = 32;
	menuTest_overlayItem.rect.height = 32;
	menuTest_overlayItem.parent = &menuTest_overlay;

	uis.layers[0] = &menuTest_base;
	uis.layers[1] = &menuTest_overlay;
	uis.menuDepth = 2;
	uis.mouseCoords[0] = 8;
	uis.mouseCoords[1] = 8;
}

/*
=============
MenuTest_OverlayKeydown

Records key routing that reaches the overlay layer.
=============
*/
static menuSound_t MenuTest_OverlayKeydown(menuFrameWork_t *, int)
{
	menuTest_overlayHandled = true;
	return QMS_NOTHANDLED;
}

/*
=============
MenuTest_BaseKeydown

Records key routing that reaches the base layer.
=============
*/
static menuSound_t MenuTest_BaseKeydown(menuFrameWork_t *, int)
{
	menuTest_baseHandled = true;
	return QMS_IN;
}

/*
=============
MenuTest_VerifyMouseRouting

Ensures mouse focus skips non-modal overlays.
=============
*/
static bool MenuTest_VerifyMouseRouting(void)
{
	MenuTest_BuildStack();
	uis.activeMenu = uis.layers[uis.menuDepth - 1];
	UI_DoHitTest();

	return (menuTest_baseItem.flags & QMF_HASFOCUS) &&
		!(menuTest_overlayItem.flags & QMF_HASFOCUS) &&
		uis.activeMenu == &menuTest_base;
}

/*
=============
MenuTest_VerifyKeyRouting

Ensures keys pass through non-modal overlays.
=============
*/
static bool MenuTest_VerifyKeyRouting(void)
{
	MenuTest_BuildStack();
	menuTest_overlay.keydown = MenuTest_OverlayKeydown;
	menuTest_base.keydown = MenuTest_BaseKeydown;
	uis.activeMenu = uis.layers[uis.menuDepth - 1];

	UI_KeyEvent(K_ENTER, true);

	return menuTest_overlayHandled && menuTest_baseHandled && uis.activeMenu == &menuTest_base;
}

/*
=============
main
=============
*/
int main()
{
	if (!MenuTest_VerifyMouseRouting()) {
		return 1;
	}

	if (!MenuTest_VerifyKeyRouting()) {
		return 2;
	}

	return 0;
}
#endif

