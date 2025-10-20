/*
Copyright (C) 2008 Andrey Nazarov

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
#include "common/json.hpp"

#include <limits.h>
#include <string.h>

static menuSound_t Activate(menuCommon_t *self)
{
    switch (self->type) {
    case MTYPE_ACTION:
        Cbuf_AddText(&cmd_buffer, ((menuAction_t *)self)->cmd);
        Cbuf_AddText(&cmd_buffer, "\n");
        break;
    case MTYPE_BITMAP:
        Cbuf_AddText(&cmd_buffer, ((menuBitmap_t *)self)->cmd);
        Cbuf_AddText(&cmd_buffer, "\n");
        break;
    case MTYPE_SAVEGAME:
        Cbuf_AddText(&cmd_buffer, va("save \"%s\"; forcemenuoff\n", ((menuAction_t *)self)->cmd));
        break;
    case MTYPE_LOADGAME:
        Cbuf_AddText(&cmd_buffer, va("load \"%s\"\n", ((menuAction_t *)self)->cmd));
        break;
    default:
        break;
    }

    return QMS_NOTHANDLED;
}

static void add_string_len(menuSpinControl_t *s, const char *tok, size_t len)
{
    if (s->numItems >= MAX_MENU_ITEMS)
        return;

    s->itemnames = Z_Realloc(s->itemnames, Q_ALIGN(s->numItems + 2, MIN_MENU_ITEMS) * sizeof(char *));

    char *copy = UI_Malloc(len + 1);
    memcpy(copy, tok, len);
    copy[len] = '\0';

    s->itemnames[s->numItems++] = copy;
}

static void add_string(menuSpinControl_t *s, const char *tok)
{
    add_string_len(s, tok, strlen(tok));
}

static void add_expand(menuSpinControl_t *s, const char *tok)
{
    char buf[MAX_STRING_CHARS], *temp = NULL;
    const char *data;

    cmd_macro_t *macro = Cmd_FindMacro(tok);
    if (macro) {
        size_t len = macro->function(buf, sizeof(buf));
        if (len < sizeof(buf)) {
            data = buf;
        } else if (len < INT_MAX) {
            data = temp = UI_Malloc(len + 1);
            macro->function(temp, len + 1);
        } else {
            Com_Printf("Expanded line exceeded %i chars, discarded.\n", INT_MAX);
            return;
        }
    } else {
        cvar_t *var = Cvar_FindVar(tok);
        if (var && !(var->flags & CVAR_PRIVATE))
            data = var->string;
        else
            return;
    }

    while (1) {
        tok = COM_Parse(&data);
        if (!data)
            break;
        add_string(s, tok);
    }

    Z_Free(temp);
}

typedef enum {
    ITEM_KIND_VALUES,
    ITEM_KIND_STRINGS,
    ITEM_KIND_PAIRS,
    ITEM_KIND_RANGE,
    ITEM_KIND_ACTION,
    ITEM_KIND_BITMAP,
    ITEM_KIND_BIND,
    ITEM_KIND_SAVEGAME,
    ITEM_KIND_LOADGAME,
    ITEM_KIND_TOGGLE,
    ITEM_KIND_FIELD,
    ITEM_KIND_BLANK,
    ITEM_KIND_IMAGEVALUES,
    ITEM_KIND_EPISODE,
    ITEM_KIND_UNIT,
} menuItemKind;

typedef struct {
    menuType_t type;
    menuItemKind kind;
} menuItemTypeInfo;

static menuItemTypeInfo ParseItemType(const char *type)
{
    if (!Q_stricmp(type, "values"))
        return { MTYPE_SPINCONTROL, ITEM_KIND_VALUES };
    if (!Q_stricmp(type, "strings"))
        return { MTYPE_STRINGS, ITEM_KIND_STRINGS };
    if (!Q_stricmp(type, "pairs"))
        return { MTYPE_PAIRS, ITEM_KIND_PAIRS };
    if (!Q_stricmp(type, "range"))
        return { MTYPE_SLIDER, ITEM_KIND_RANGE };
    if (!Q_stricmp(type, "action"))
        return { MTYPE_ACTION, ITEM_KIND_ACTION };
    if (!Q_stricmp(type, "bitmap"))
        return { MTYPE_BITMAP, ITEM_KIND_BITMAP };
    if (!Q_stricmp(type, "bind"))
        return { MTYPE_KEYBIND, ITEM_KIND_BIND };
    if (!Q_stricmp(type, "savegame"))
        return { MTYPE_SAVEGAME, ITEM_KIND_SAVEGAME };
    if (!Q_stricmp(type, "loadgame"))
        return { MTYPE_LOADGAME, ITEM_KIND_LOADGAME };
    if (!Q_stricmp(type, "toggle"))
        return { MTYPE_TOGGLE, ITEM_KIND_TOGGLE };
    if (!Q_stricmp(type, "field"))
        return { MTYPE_FIELD, ITEM_KIND_FIELD };
    if (!Q_stricmp(type, "blank"))
        return { MTYPE_SEPARATOR, ITEM_KIND_BLANK };
    if (!Q_stricmp(type, "imagevalues"))
        return { MTYPE_IMAGESPINCONTROL, ITEM_KIND_IMAGEVALUES };
    if (!Q_stricmp(type, "episode_selector"))
        return { MTYPE_EPISODE, ITEM_KIND_EPISODE };
    if (!Q_stricmp(type, "unit_selector"))
        return { MTYPE_UNIT, ITEM_KIND_UNIT };

    return { MTYPE_BAD, ITEM_KIND_VALUES };
}

static char *Json_CopyStringUI(json_parse_t *parser)
{
    jsmntok_t *tok = Json_Ensure(parser, JSMN_STRING);
    size_t len = tok->end - tok->start;

    char *out = UI_Malloc(len + 1);
    memcpy(out, parser->buffer + tok->start, len);
    out[len] = '\0';

    Json_Next(parser);

    return out;
}

static void Json_CopyStringToBuffer(json_parse_t *parser, char *buffer, size_t size)
{
    jsmntok_t *tok = Json_Ensure(parser, JSMN_STRING);
    size_t len = tok->end - tok->start;
    if (len >= size)
        len = size - 1;

    memcpy(buffer, parser->buffer + tok->start, len);
    buffer[len] = '\0';

    Json_Next(parser);
}

static int Json_ReadInt(json_parse_t *parser)
{
    jsmntok_t *tok = Json_Ensure(parser, JSMN_PRIMITIVE);
    int value = Q_atoi(parser->buffer + tok->start);
    Json_Next(parser);
    return value;
}

static float Json_ReadFloat(json_parse_t *parser)
{
    jsmntok_t *tok = Json_Ensure(parser, JSMN_PRIMITIVE);
    float value = Q_atof(parser->buffer + tok->start);
    Json_Next(parser);
    return value;
}

static bool Json_ReadBool(json_parse_t *parser)
{
    jsmntok_t *tok = Json_Ensure(parser, JSMN_PRIMITIVE);
    bool value = parser->buffer[tok->start] == 't';
    Json_Next(parser);
    return value;
}

static void FinalizeSpinItems(menuSpinControl_t *s)
{
    if (!s->itemnames)
        return;

    s->itemnames = Z_Realloc(s->itemnames, Q_ALIGN(s->numItems + 1, MIN_MENU_ITEMS) * sizeof(char *));
    s->itemnames[s->numItems] = NULL;
}

static void ParseSpinOptions(json_parse_t *parser, menuSpinControl_t *s)
{
    jsmntok_t *array = Json_EnsureNext(parser, JSMN_ARRAY);

    for (int i = 0; i < array->size; i++) {
        Json_Ensure(parser, JSMN_STRING);
        const char *start = parser->buffer + parser->pos->start;
        size_t len = parser->pos->end - parser->pos->start;

        if (len && start[0] == '$') {
            if (len > 1 && start[1] == '$') {
                add_string_len(s, start + 1, len - 1);
            } else if (len > 1) {
                char *expand = UI_Malloc(len);
                memcpy(expand, start + 1, len - 1);
                expand[len - 1] = '\0';
                add_expand(s, expand);
                Z_Free(expand);
            }
        } else if (len) {
            add_string_len(s, start, len);
        }

        Json_Next(parser);
    }

    FinalizeSpinItems(s);
}

static void ParsePairOptions(json_parse_t *parser, menuSpinControl_t *s)
{
    jsmntok_t *array = Json_EnsureNext(parser, JSMN_ARRAY);

    s->numItems = array->size;
    s->itemnames = UI_Mallocz(sizeof(char *) * (s->numItems + 1));
    s->itemvalues = UI_Mallocz(sizeof(char *) * (s->numItems + 1));

    for (int i = 0; i < array->size; i++) {
        jsmntok_t *object = Json_EnsureNext(parser, JSMN_OBJECT);
        char *label = NULL;
        char *value = NULL;

        for (int j = 0; j < object->size; j++) {
            if (!Json_Strcmp(parser, "label")) {
                Json_Next(parser);
                label = Json_CopyStringUI(parser);
            } else if (!Json_Strcmp(parser, "value")) {
                Json_Next(parser);
                value = Json_CopyStringUI(parser);
            } else {
                Json_Next(parser);
                Json_SkipToken(parser);
            }
        }

        if (!label || !value)
            Json_Error(parser, object, "pair entries require label and value");

        s->itemnames[i] = label;
        s->itemvalues[i] = value;
    }
}

static void ParseMenuItem(json_parse_t *parser, menuFrameWork_t *menu)
{
    static const char *const yes_no_names[] = { "no", "yes", NULL };

    jsmntok_t *object = Json_EnsureNext(parser, JSMN_OBJECT);
    jsmntok_t *start = parser->pos;

    menuItemTypeInfo info = { MTYPE_BAD, ITEM_KIND_VALUES };

    for (int i = 0; i < object->size; i++) {
        if (!Json_Strcmp(parser, "type")) {
            Json_Next(parser);
            char type[32];
            Json_CopyStringToBuffer(parser, type, sizeof(type));
            info = ParseItemType(type);
            if (info.type == MTYPE_BAD)
                Json_Error(parser, object, "unknown menu item type");
            break;
        }

        Json_Next(parser);
        Json_SkipToken(parser);
    }

    if (info.type == MTYPE_BAD)
        Json_Error(parser, object, "menu item missing type");

    parser->pos = start;

    menuCommon_t *common = NULL;
    menuSpinControl_t *spin = NULL;
    menuSlider_t *slider = NULL;
    menuAction_t *action = NULL;
    menuBitmap_t *bitmap = NULL;
    menuKeybind_t *bind = NULL;
    menuField_t *field = NULL;
    menuSeparator_t *separator = NULL;
    menuEpisodeSelector_t *episode = NULL;
    menuUnitSelector_t *unit = NULL;

    switch (info.kind) {
    case ITEM_KIND_VALUES:
    case ITEM_KIND_STRINGS:
    case ITEM_KIND_PAIRS:
    case ITEM_KIND_IMAGEVALUES:
    case ITEM_KIND_TOGGLE:
        spin = UI_Mallocz(sizeof(*spin));
        common = &spin->generic;
        common->type = info.type;
        break;
    case ITEM_KIND_RANGE:
        slider = UI_Mallocz(sizeof(*slider));
        common = &slider->generic;
        common->type = info.type;
        break;
    case ITEM_KIND_ACTION:
    case ITEM_KIND_SAVEGAME:
    case ITEM_KIND_LOADGAME:
        action = UI_Mallocz(sizeof(*action));
        common = &action->generic;
        common->type = info.type;
        common->activate = Activate;
        if (info.kind == ITEM_KIND_ACTION)
            common->uiFlags = UI_CENTER;
        else
            common->uiFlags = UI_CENTER;
        if (info.kind == ITEM_KIND_SAVEGAME || info.kind == ITEM_KIND_LOADGAME)
            action->generic.name = UI_CopyString("<EMPTY>");
        if (info.kind == ITEM_KIND_LOADGAME)
            common->flags |= QMF_GRAYED;
        break;
    case ITEM_KIND_BITMAP:
        bitmap = UI_Mallocz(sizeof(*bitmap));
        common = &bitmap->generic;
        common->type = info.type;
        common->activate = Activate;
        break;
    case ITEM_KIND_BIND:
        bind = UI_Mallocz(sizeof(*bind));
        common = &bind->generic;
        common->type = info.type;
        common->uiFlags = UI_CENTER;
        bind->generic.status = UI_CopyString("Press Enter to change, Backspace to clear");
        bind->altstatus = UI_CopyString("Press the desired key, Escape to cancel");
        break;
    case ITEM_KIND_FIELD:
        field = UI_Mallocz(sizeof(*field));
        common = &field->generic;
        common->type = info.type;
        break;
    case ITEM_KIND_BLANK:
        separator = UI_Mallocz(sizeof(*separator));
        common = &separator->generic;
        common->type = info.type;
        break;
    case ITEM_KIND_EPISODE:
        episode = UI_Mallocz(sizeof(*episode));
        spin = &episode->spin;
        common = &spin->generic;
        common->type = info.type;
        break;
    case ITEM_KIND_UNIT:
        unit = UI_Mallocz(sizeof(*unit));
        spin = &unit->spin;
        common = &spin->generic;
        common->type = info.type;
        spin->generic.uiFlags |= UI_MULTILINE;
        break;
    }

    for (int i = 0; i < object->size; i++) {
        if (!Json_Strcmp(parser, "type")) {
            Json_Next(parser);
            Json_SkipToken(parser);
            continue;
        }

        if (info.type == MTYPE_BAD)
            Json_Error(parser, object, "menu item type must be specified before other fields");

        if (!Json_Strcmp(parser, "label")) {
            Json_Next(parser);
            Z_Free(common->name);
            common->name = Json_CopyStringUI(parser);
        } else if (!Json_Strcmp(parser, "status")) {
            Json_Next(parser);
            Z_Free(common->status);
            common->status = Json_CopyStringUI(parser);
        } else if (!Json_Strcmp(parser, "command")) {
            Json_Next(parser);
            char *cmd = Json_CopyStringUI(parser);
            if (action)
                action->cmd = cmd;
            else if (bitmap)
                bitmap->cmd = cmd;
            else if (bind)
                bind->cmd = cmd;
            else
                Z_Free(cmd);
        } else if (!Json_Strcmp(parser, "align")) {
            Json_Next(parser);
            char align[16];
            Json_CopyStringToBuffer(parser, align, sizeof(align));
            if (common->type == MTYPE_ACTION) {
                if (!Q_stricmp(align, "left"))
                    common->uiFlags = UI_LEFT | UI_ALTCOLOR;
                else
                    common->uiFlags = UI_CENTER;
            }
        } else if (!Json_Strcmp(parser, "cvar")) {
            Json_Next(parser);
            char *cvar = Json_CopyStringUI(parser);
            if (spin)
                spin->cvar = Cvar_WeakGet(cvar);
            else if (slider)
                slider->cvar = Cvar_WeakGet(cvar);
            else if (field)
                field->cvar = Cvar_WeakGet(cvar);
            Z_Free(cvar);
        } else if (!Json_Strcmp(parser, "options")) {
            Json_Next(parser);
            if (!spin)
                Json_Error(parser, object, "options only valid for spin controls");
            if (info.kind == ITEM_KIND_PAIRS)
                ParsePairOptions(parser, spin);
            else
                ParseSpinOptions(parser, spin);
        } else if (!Json_Strcmp(parser, "min")) {
            Json_Next(parser);
            if (!slider)
                Json_Error(parser, object, "min only valid for sliders");
            slider->minvalue = Json_ReadFloat(parser);
        } else if (!Json_Strcmp(parser, "max")) {
            Json_Next(parser);
            if (!slider)
                Json_Error(parser, object, "max only valid for sliders");
            slider->maxvalue = Json_ReadFloat(parser);
        } else if (!Json_Strcmp(parser, "step")) {
            Json_Next(parser);
            if (!slider)
                Json_Error(parser, object, "step only valid for sliders");
            slider->step = Json_ReadFloat(parser);
        } else if (!Json_Strcmp(parser, "altStatus")) {
            Json_Next(parser);
            if (!bind)
                Json_Error(parser, object, "altStatus only valid for keybinds");
            Z_Free(bind->altstatus);
            bind->altstatus = Json_CopyStringUI(parser);
        } else if (!Json_Strcmp(parser, "selectedImage")) {
            Json_Next(parser);
            if (!bitmap)
                Json_Error(parser, object, "selectedImage only valid for bitmaps");
            char *name = Json_CopyStringUI(parser);
            bitmap->pics[1] = R_RegisterPic(name);
            Z_Free(name);
        } else if (!Json_Strcmp(parser, "image")) {
            Json_Next(parser);
            if (!bitmap)
                Json_Error(parser, object, "image only valid for bitmaps");
            char *name = Json_CopyStringUI(parser);
            bitmap->pics[0] = R_RegisterPic(name);
            if (bitmap->pics[0])
                R_GetPicSize(&bitmap->generic.width, &bitmap->generic.height, bitmap->pics[0]);
            Z_Free(name);
        } else if (!Json_Strcmp(parser, "negate")) {
            Json_Next(parser);
            if (!spin || (info.kind != ITEM_KIND_TOGGLE))
                Json_Error(parser, object, "negate only valid for toggles");
            spin->negate = Json_ReadBool(parser);
        } else if (!Json_Strcmp(parser, "bit")) {
            Json_Next(parser);
            if (!spin || (info.kind != ITEM_KIND_TOGGLE))
                Json_Error(parser, object, "bit only valid for toggles");
            int bit = Json_ReadInt(parser);
            if (bit < 0 || bit >= 32)
                Json_Error(parser, object, "toggle bit must be between 0 and 31");
            spin->mask = 1u << bit;
            spin->generic.type = MTYPE_BITFIELD;
        } else if (!Json_Strcmp(parser, "center")) {
            Json_Next(parser);
            if (!field)
                Json_Error(parser, object, "center only valid for fields");
            if (Json_ReadBool(parser)) {
                Z_Free(common->name);
                common->name = NULL;
            }
        } else if (!Json_Strcmp(parser, "numeric")) {
            Json_Next(parser);
            if (!field)
                Json_Error(parser, object, "numeric only valid for fields");
            if (Json_ReadBool(parser))
                field->generic.flags |= QMF_NUMBERSONLY;
        } else if (!Json_Strcmp(parser, "width")) {
            Json_Next(parser);
            int width = Json_ReadInt(parser);
            if (field) {
                if (width < 1 || width > 32)
                    Json_Error(parser, object, "field width must be between 1 and 32");
                field->width = width;
            } else if (spin && info.kind == ITEM_KIND_IMAGEVALUES) {
                spin->generic.width = width;
            }
        } else if (!Json_Strcmp(parser, "height")) {
            Json_Next(parser);
            if (!spin || info.kind != ITEM_KIND_IMAGEVALUES)
                Json_Error(parser, object, "height only valid for image spin controls");
            spin->generic.height = Json_ReadInt(parser);
        } else if (!Json_Strcmp(parser, "path")) {
            Json_Next(parser);
            if (!spin || info.kind != ITEM_KIND_IMAGEVALUES)
                Json_Error(parser, object, "path only valid for image spin controls");
            spin->path = Json_CopyStringUI(parser);
        } else if (!Json_Strcmp(parser, "filter")) {
            Json_Next(parser);
            if (!spin || info.kind != ITEM_KIND_IMAGEVALUES)
                Json_Error(parser, object, "filter only valid for image spin controls");
            spin->filter = Json_CopyStringUI(parser);
        } else {
            Json_Next(parser);
            Json_SkipToken(parser);
        }
    }

    switch (info.kind) {
    case ITEM_KIND_VALUES:
    case ITEM_KIND_STRINGS:
        if (!spin || !spin->cvar || !spin->itemnames)
            Json_Error(parser, object, "values/strings require cvar and options");
        break;
    case ITEM_KIND_PAIRS:
        if (!spin || !spin->cvar || !spin->itemnames || !spin->itemvalues)
            Json_Error(parser, object, "pairs require cvar and options");
        break;
    case ITEM_KIND_RANGE:
        if (!slider || !slider->cvar)
            Json_Error(parser, object, "range requires cvar");
        if (!slider->step)
            slider->step = (slider->maxvalue - slider->minvalue) / SLIDER_RANGE;
        break;
    case ITEM_KIND_ACTION:
        if (!action || !action->cmd)
            Json_Error(parser, object, "action requires command");
        break;
    case ITEM_KIND_BITMAP:
        if (!bitmap || !bitmap->pics[0] || !bitmap->cmd)
            Json_Error(parser, object, "bitmap requires image and command");
        if (!bitmap->pics[1])
            bitmap->pics[1] = bitmap->pics[0];
        break;
    case ITEM_KIND_BIND:
        if (!bind || !bind->cmd)
            Json_Error(parser, object, "bind requires command");
        break;
    case ITEM_KIND_SAVEGAME:
    case ITEM_KIND_LOADGAME:
        if (!action || !action->cmd)
            Json_Error(parser, object, "save/load game requires path");
        break;
    case ITEM_KIND_TOGGLE:
        if (!spin || !spin->cvar)
            Json_Error(parser, object, "toggle requires cvar");
        if (!spin->itemnames)
            spin->itemnames = (char **)yes_no_names;
        if (!spin->mask && spin->generic.type == MTYPE_BITFIELD)
            spin->mask = 1;
        spin->numItems = 2;
        break;
    case ITEM_KIND_FIELD:
        if (!field || !field->cvar)
            Json_Error(parser, object, "field requires cvar");
        if (!field->width)
            field->width = 16;
        break;
    case ITEM_KIND_IMAGEVALUES:
        if (!spin || !spin->cvar || !spin->path || !spin->filter)
            Json_Error(parser, object, "imagevalues require cvar, path and filter");
        break;
    case ITEM_KIND_EPISODE:
        if (!spin || !spin->cvar)
            Json_Error(parser, object, "episode selector requires cvar");
        UI_MapDB_FetchEpisodes(&spin->itemnames, &spin->numItems);
        break;
    case ITEM_KIND_UNIT:
        if (!unit || !spin->cvar)
            Json_Error(parser, object, "unit selector requires cvar");
        UI_MapDB_FetchUnits(&spin->itemnames, &unit->itemindices, &spin->numItems);
        break;
    case ITEM_KIND_BLANK:
        break;
    }

    Menu_AddItem(menu, common);
}

static void ApplyMenuBackground(menuFrameWork_t *menu, const char *value)
{
    if (SCR_ParseColor(value, &menu->color)) {
        menu->image = 0;
        menu->transparent = menu->color.a != 255;
    } else {
        menu->image = R_RegisterPic(value);
        menu->transparent = R_GetPicSize(NULL, NULL, menu->image);
    }
}

static void ParseMenuPlaque(json_parse_t *parser, menuFrameWork_t *menu)
{
    jsmntok_t *obj = Json_EnsureNext(parser, JSMN_OBJECT);

    for (int i = 0; i < obj->size; i++) {
        if (!Json_Strcmp(parser, "image")) {
            Json_Next(parser);
            char *name = Json_CopyStringUI(parser);
            menu->plaque = R_RegisterPic(name);
            if (menu->plaque)
                R_GetPicSize(&menu->plaque_rc.width, &menu->plaque_rc.height, menu->plaque);
            Z_Free(name);
        } else if (!Json_Strcmp(parser, "logo")) {
            Json_Next(parser);
            char *name = Json_CopyStringUI(parser);
            menu->logo = R_RegisterPic(name);
            if (menu->logo)
                R_GetPicSize(&menu->logo_rc.width, &menu->logo_rc.height, menu->logo);
            Z_Free(name);
        } else {
            Json_Next(parser);
            Json_SkipToken(parser);
        }
    }
}

static void ParseMenuStyle(json_parse_t *parser, menuFrameWork_t *menu)
{
    jsmntok_t *obj = Json_EnsureNext(parser, JSMN_OBJECT);

    for (int i = 0; i < obj->size; i++) {
        if (!Json_Strcmp(parser, "compact")) {
            Json_Next(parser);
            menu->compact = Json_ReadBool(parser);
        } else if (!Json_Strcmp(parser, "transparent")) {
            Json_Next(parser);
            menu->transparent = Json_ReadBool(parser);
        } else {
            Json_Next(parser);
            Json_SkipToken(parser);
        }
    }
}

static void ParseMenu(json_parse_t *parser)
{
    jsmntok_t *object = Json_EnsureNext(parser, JSMN_OBJECT);
    jsmntok_t *start = parser->pos;
    char *name = NULL;

    for (int i = 0; i < object->size; i++) {
        if (!Json_Strcmp(parser, "name")) {
            Json_Next(parser);
            name = Json_CopyStringUI(parser);
            break;
        }

        Json_Next(parser);
        Json_SkipToken(parser);
    }

    if (!name)
        Json_Error(parser, object, "menu missing name");

    parser->pos = start;

    menuFrameWork_t *menu = UI_FindMenu(name);
    if (menu) {
        List_Remove(&menu->entry);
        if (menu->free)
            menu->free(menu);
        menu = NULL;
    }

    menu = UI_Mallocz(sizeof(*menu));
    menu->name = name;
    menu->push = Menu_Push;
    menu->pop = Menu_Pop;
    menu->free = Menu_Free;
    menu->image = uis.backgroundHandle;
    menu->color.u32 = uis.color.background.u32;
    menu->transparent = uis.transparent;

    for (int i = 0; i < object->size; i++) {
        if (!Json_Strcmp(parser, "name")) {
            Json_Next(parser);
            Json_SkipToken(parser);
        } else if (!Json_Strcmp(parser, "title")) {
            Json_Next(parser);
            Z_Free(menu->title);
            menu->title = Json_CopyStringUI(parser);
        } else if (!Json_Strcmp(parser, "status")) {
            Json_Next(parser);
            Z_Free(menu->status);
            menu->status = Json_CopyStringUI(parser);
        } else if (!Json_Strcmp(parser, "banner")) {
            Json_Next(parser);
            char *name = Json_CopyStringUI(parser);
            menu->banner = R_RegisterPic(name);
            if (menu->banner)
                R_GetPicSize(&menu->banner_rc.width, &menu->banner_rc.height, menu->banner);
            Z_Free(name);
        } else if (!Json_Strcmp(parser, "plaque")) {
            Json_Next(parser);
            ParseMenuPlaque(parser, menu);
        } else if (!Json_Strcmp(parser, "background")) {
            Json_Next(parser);
            if (parser->pos->type == JSMN_STRING) {
                char *value = Json_CopyStringUI(parser);
                ApplyMenuBackground(menu, value);
                Z_Free(value);
            } else {
                Json_Error(parser, parser->pos, "menu background must be a string");
            }
        } else if (!Json_Strcmp(parser, "style")) {
            Json_Next(parser);
            ParseMenuStyle(parser, menu);
        } else if (!Json_Strcmp(parser, "items")) {
            Json_Next(parser);
            jsmntok_t *array = Json_EnsureNext(parser, JSMN_ARRAY);
            for (int j = 0; j < array->size; j++)
                ParseMenuItem(parser, menu);
        } else {
            Json_Next(parser);
            Json_SkipToken(parser);
        }
    }

    if (!menu->nitems) {
        Com_WPrintf("Menu '%s' defined without items\n", menu->name);
        menu->free(menu);
        return;
    }

    List_Append(&ui_menus, &menu->entry);
}

static void ParseColors(json_parse_t *parser)
{
    jsmntok_t *object = Json_EnsureNext(parser, JSMN_OBJECT);

    for (int i = 0; i < object->size; i++) {
        if (!Json_Strcmp(parser, "normal")) {
            Json_Next(parser);
            char *value = Json_CopyStringUI(parser);
            SCR_ParseColor(value, &uis.color.normal);
            Z_Free(value);
        } else if (!Json_Strcmp(parser, "active")) {
            Json_Next(parser);
            char *value = Json_CopyStringUI(parser);
            SCR_ParseColor(value, &uis.color.active);
            Z_Free(value);
        } else if (!Json_Strcmp(parser, "selection")) {
            Json_Next(parser);
            char *value = Json_CopyStringUI(parser);
            SCR_ParseColor(value, &uis.color.selection);
            Z_Free(value);
        } else if (!Json_Strcmp(parser, "disabled")) {
            Json_Next(parser);
            char *value = Json_CopyStringUI(parser);
            SCR_ParseColor(value, &uis.color.disabled);
            Z_Free(value);
        } else {
            Json_Next(parser);
            Json_SkipToken(parser);
        }
    }
}

static void ParseGlobalBackground(json_parse_t *parser)
{
    if (parser->pos->type != JSMN_STRING)
        Json_Error(parser, parser->pos, "background must be a string");

    char *value = Json_CopyStringUI(parser);

    if (SCR_ParseColor(value, &uis.color.background)) {
        uis.backgroundHandle = 0;
        uis.transparent = uis.color.background.a != 255;
    } else {
        uis.backgroundHandle = R_RegisterPic(value);
        uis.transparent = R_GetPicSize(NULL, NULL, uis.backgroundHandle);
    }

    Z_Free(value);
}

static void ParseCursor(json_parse_t *parser)
{
    if (parser->pos->type != JSMN_STRING)
        Json_Error(parser, parser->pos, "cursor must be a string");

    char *name = Json_CopyStringUI(parser);
    uis.cursorHandle = R_RegisterPic(name);
    R_GetPicSize(&uis.cursorWidth, &uis.cursorHeight, uis.cursorHandle);
    Z_Free(name);
}

static void ParseMenus(json_parse_t *parser)
{
    jsmntok_t *array = Json_EnsureNext(parser, JSMN_ARRAY);

    for (int i = 0; i < array->size; i++)
        ParseMenu(parser);
}

void UI_LoadScript(void)
{
    json_parse_t parser = {};

    if (Json_ErrorHandler(parser)) {
        Com_WPrintf("Failed to load/parse %s[%s]: %s\n", UI_DEFAULT_FILE, parser.error_loc, parser.error);
        Json_Free(&parser);
        return;
    }

    Json_Load(UI_DEFAULT_FILE, &parser);

    jsmntok_t *object = Json_EnsureNext(&parser, JSMN_OBJECT);

    for (int i = 0; i < object->size; i++) {
        if (!Json_Strcmp(&parser, "background")) {
            Json_Next(&parser);
            ParseGlobalBackground(&parser);
        } else if (!Json_Strcmp(&parser, "font")) {
            Json_Next(&parser);
            char *font = Json_CopyStringUI(&parser);
            uis.fontHandle = R_RegisterFont(font);
            Z_Free(font);
        } else if (!Json_Strcmp(&parser, "cursor")) {
            Json_Next(&parser);
            ParseCursor(&parser);
        } else if (!Json_Strcmp(&parser, "weapon")) {
            Json_Next(&parser);
            char *weapon = Json_CopyStringUI(&parser);
            Q_strlcpy(uis.weaponModel, weapon, sizeof(uis.weaponModel));
            Z_Free(weapon);
        } else if (!Json_Strcmp(&parser, "colors")) {
            Json_Next(&parser);
            ParseColors(&parser);
        } else if (!Json_Strcmp(&parser, "menus")) {
            Json_Next(&parser);
            ParseMenus(&parser);
        } else {
            Json_Next(&parser);
            Json_SkipToken(&parser);
        }
    }

    Json_Free(&parser);
}
