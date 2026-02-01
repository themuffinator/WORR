#include "ui/ui_internal.h"

#include "common/common.h"

namespace ui {

struct MenuItemData {
    std::string type;
    std::string label;
    std::string labelCvar;
    std::string cvar;
    std::string command;
    std::string commandCvar;
    std::string status;
    std::string altStatus;
    std::string image;
    std::string imageSelected;
    std::string path;
    std::string filter;
    std::string valuePrefix;
    std::string slot;
    std::vector<std::string> items;
    std::vector<std::pair<std::string, std::string>> pairs;
    std::vector<MenuCondition> showConditions;
    std::vector<MenuCondition> enableConditions;
    std::vector<MenuCondition> defaultConditions;
    float minValue = 0.0f;
    float maxValue = 0.0f;
    float step = 0.0f;
    bool stepSet = false;
    int bit = -1;
    bool negate = false;
    bool alignLeft = false;
    bool center = false;
    bool numeric = false;
    bool integer = false;
    bool isDefault = false;
    bool defaultSet = false;
    bool selectable = true;
    bool selectableSet = false;
    int width = 16;
    int imageWidth = 0;
    int imageHeight = 0;
    int maxLines = 0;
    int wrapWidth = 0;
    bool ingameOnly = false;
    bool deathmatchOnly = false;
    int textOffset = 0;
    bool textOffsetSet = false;
    int textSize = 0;
    bool textSizeSet = false;
    int x = 0;
    int y = 0;
    bool positionSet = false;
    int anchor = 0;
    bool anchorSet = false;
    color_t textColor{};
    bool textColorSet = false;
    color_t selectedTextColor{};
    bool selectedTextColorSet = false;
};

static std::string Json_ReadString(json_parse_t *parser)
{
    jsmntok_t *tok = Json_EnsureNext(parser, JSMN_STRING);
    return std::string(parser->buffer + tok->start,
                       parser->buffer + tok->end);
}

static bool Json_ReadBool(json_parse_t *parser)
{
    jsmntok_t *tok = Json_EnsureNext(parser, JSMN_PRIMITIVE);
    std::string value(parser->buffer + tok->start, parser->buffer + tok->end);
    return value == "true" || value == "1";
}

static double Json_ReadNumber(json_parse_t *parser)
{
    jsmntok_t *tok = Json_EnsureNext(parser, JSMN_PRIMITIVE);
    std::string value(parser->buffer + tok->start, parser->buffer + tok->end);
    return atof(value.c_str());
}

static void AddExpandedItems(const char *token, std::vector<std::string> &out)
{
    if (!token || !*token)
        return;

    if (strncmp(token, "$$", 2) != 0) {
        out.emplace_back(token);
        return;
    }

    const char *name = token + 2;
    if (!*name)
        return;

    char buf[MAX_STRING_CHARS];
    const char *data = nullptr;
    char *temp = nullptr;

    cmd_macro_t *macro = Cmd_FindMacro(name);
    if (macro) {
        size_t len = macro->function(buf, sizeof(buf));
        if (len < sizeof(buf)) {
            data = buf;
        } else if (len < INT_MAX) {
            temp = static_cast<char *>(UI_Malloc(len + 1));
            data = temp;
            macro->function(temp, len + 1);
        } else {
            Com_Printf("$ui_cmd_expanded_line_too_long", INT_MAX);
            return;
        }
    } else {
        cvar_t *var = Cvar_FindVar(name);
        if (var && !(var->flags & CVAR_PRIVATE))
            data = var->string;
        else
            return;
    }

    while (1) {
        const char *tok = COM_Parse(&data);
        if (!data)
            break;
        out.emplace_back(tok);
    }

    Z_Free(temp);
}

static void ParseItemsArray(json_parse_t *parser, std::vector<std::string> &out)
{
    jsmntok_t *array = Json_EnsureNext(parser, JSMN_ARRAY);
    for (int i = 0; i < array->size; i++) {
        if (parser->pos->type == JSMN_STRING) {
            std::string value = Json_ReadString(parser);
            AddExpandedItems(value.c_str(), out);
            continue;
        }
        if (parser->pos->type == JSMN_OBJECT) {
            jsmntok_t *obj = Json_EnsureNext(parser, JSMN_OBJECT);
            for (int k = 0; k < obj->size; k++) {
                if (Json_Strcmp(parser, "expand") == 0) {
                    Json_Next(parser);
                    std::string value = Json_ReadString(parser);
                    AddExpandedItems(value.c_str(), out);
                } else {
                    Json_Next(parser);
                    Json_SkipToken(parser);
                }
            }
            continue;
        }
        Json_SkipToken(parser);
    }
}

static void ParsePairsArray(json_parse_t *parser, std::vector<std::pair<std::string, std::string>> &out)
{
    jsmntok_t *array = Json_EnsureNext(parser, JSMN_ARRAY);
    for (int i = 0; i < array->size; i++) {
        if (parser->pos->type != JSMN_OBJECT) {
            Json_SkipToken(parser);
            continue;
        }

        std::string label;
        std::string value;
        jsmntok_t *obj = Json_EnsureNext(parser, JSMN_OBJECT);
        for (int k = 0; k < obj->size; k++) {
            if (Json_Strcmp(parser, "label") == 0) {
                Json_Next(parser);
                label = Json_ReadString(parser);
            } else if (Json_Strcmp(parser, "value") == 0) {
                Json_Next(parser);
                value = Json_ReadString(parser);
            } else {
                Json_Next(parser);
                Json_SkipToken(parser);
            }
        }

        if (!label.empty() && !value.empty())
            out.emplace_back(std::move(label), std::move(value));
    }
}

static std::string TrimWhitespace(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() && value[start] <= ' ')
        start++;

    size_t end = value.size();
    while (end > start && value[end - 1] <= ' ')
        end--;

    return value.substr(start, end - start);
}

static std::string StripQuotes(const std::string &value)
{
    if (value.size() >= 2) {
        char first = value.front();
        char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
            return value.substr(1, value.size() - 2);
    }
    return value;
}

static bool IsMatchMenuName(const std::string &name)
{
    if (name.empty())
        return false;

    if (!Q_stricmp(name.c_str(), "join") ||
        !Q_stricmp(name.c_str(), "map_selector") ||
        !Q_stricmp(name.c_str(), "match_stats")) {
        return true;
    }

    static const char *prefixes[] = {
        "dm_",
        "callvote_",
        "mymap_",
        "forfeit_",
        "admin_",
        "tourney_",
        "vote_"
    };

    for (const char *prefix : prefixes) {
        if (!Q_strncasecmp(name.c_str(), prefix, strlen(prefix)))
            return true;
    }

    return false;
}

static bool ParseConditionString(const std::string &raw, MenuCondition *out)
{
    std::string text = TrimWhitespace(raw);
    if (text.empty())
        return false;

    bool negate = false;
    if (text[0] == '!' && (text.size() < 2 || text[1] != '=')) {
        negate = true;
        text = TrimWhitespace(text.substr(1));
    }

    if (text.empty())
        return false;

    bool force_cvar = false;
    if (!Q_strncasecmp(text.c_str(), "cvar:", 5)) {
        force_cvar = true;
        text = TrimWhitespace(text.substr(5));
    } else if (!Q_strncasecmp(text.c_str(), "var:", 4)) {
        force_cvar = true;
        text = TrimWhitespace(text.substr(4));
    }

    if (!force_cvar) {
        if (!Q_stricmp(text.c_str(), "ingame") ||
            !Q_stricmp(text.c_str(), "in_game") ||
            !Q_stricmp(text.c_str(), "in-game")) {
            MenuCondition condition;
            condition.kind = ConditionKind::InGame;
            condition.negate = negate;
            *out = std::move(condition);
            return true;
        }
        if (!Q_stricmp(text.c_str(), "deathmatch") ||
            !Q_stricmp(text.c_str(), "dm")) {
            MenuCondition condition;
            condition.kind = ConditionKind::Deathmatch;
            condition.negate = negate;
            *out = std::move(condition);
            return true;
        }
    }

    MenuCondition condition;
    condition.kind = ConditionKind::Cvar;
    condition.negate = negate;

    struct OpToken {
        const char *token;
        ConditionOp op;
    };

    static const OpToken kOps[] = {
        { "!=", ConditionOp::NotEqual },
        { "==", ConditionOp::Equal },
        { ">=", ConditionOp::GreaterEqual },
        { "<=", ConditionOp::LessEqual },
        { ">", ConditionOp::Greater },
        { "<", ConditionOp::Less },
        { "=", ConditionOp::Equal }
    };

    size_t match_pos = std::string::npos;
    size_t match_len = 0;
    ConditionOp op = ConditionOp::Exists;

    for (const auto &entry : kOps) {
        size_t found = text.find(entry.token);
        if (found != std::string::npos) {
            match_pos = found;
            match_len = strlen(entry.token);
            op = entry.op;
            break;
        }
    }

    if (match_pos != std::string::npos) {
        condition.name = TrimWhitespace(text.substr(0, match_pos));
        condition.value = StripQuotes(TrimWhitespace(text.substr(match_pos + match_len)));
        if (condition.name.empty() || condition.value.empty()) {
            condition.name = TrimWhitespace(text);
            condition.value.clear();
            condition.op = ConditionOp::Exists;
        } else {
            condition.op = op;
        }
    } else {
        condition.name = TrimWhitespace(text);
        condition.op = ConditionOp::Exists;
    }

    if (condition.name.empty())
        return false;

    *out = std::move(condition);
    return true;
}

static void ParseConditions(json_parse_t *parser, std::vector<MenuCondition> &out)
{
    if (parser->pos->type == JSMN_STRING) {
        std::string value = Json_ReadString(parser);
        MenuCondition condition;
        if (ParseConditionString(value, &condition))
            out.push_back(std::move(condition));
        return;
    }

    if (parser->pos->type != JSMN_ARRAY) {
        Json_SkipToken(parser);
        return;
    }

    jsmntok_t *array = Json_EnsureNext(parser, JSMN_ARRAY);
    for (int i = 0; i < array->size; i++) {
        if (parser->pos->type == JSMN_STRING) {
            std::string value = Json_ReadString(parser);
            MenuCondition condition;
            if (ParseConditionString(value, &condition))
                out.push_back(std::move(condition));
            continue;
        }
        Json_SkipToken(parser);
    }
}

static std::unique_ptr<MenuPage> CreateFeederMenu(const std::string &feeder)
{
    if (feeder == "servers")
        return CreateServerBrowserPage();
    if (feeder == "demos")
        return CreateDemoBrowserPage();
    if (feeder == "players")
        return CreatePlayerConfigPage();
    return nullptr;
}

static void ApplyConditions(const MenuItemData &item, Widget *widget)
{
    if (!widget)
        return;

    std::vector<MenuCondition> show = item.showConditions;
    std::vector<MenuCondition> enable = item.enableConditions;

    if (item.ingameOnly) {
        MenuCondition condition;
        condition.kind = ConditionKind::InGame;
        show.push_back(std::move(condition));
    }

    if (item.deathmatchOnly) {
        MenuCondition condition;
        condition.kind = ConditionKind::Deathmatch;
        show.push_back(std::move(condition));
    }

    if (!show.empty())
        widget->SetShowConditions(std::move(show));
    if (!enable.empty())
        widget->SetEnableConditions(std::move(enable));
    if (item.defaultSet)
        widget->SetDefault(item.isDefault);
    if (!item.defaultConditions.empty())
        widget->SetDefaultConditions(item.defaultConditions);
}

static std::unique_ptr<Widget> BuildWidget(const MenuItemData &item)
{
    std::unique_ptr<Widget> widget;

    if (item.type == "action") {
        auto action = std::make_unique<ActionWidget>(item.command);
        action->SetLabel(item.label);
        action->SetStatus(item.status);
        action->SetAlignLeft(item.alignLeft);
        if (!item.commandCvar.empty())
            action->SetCommandCvar(Cvar_WeakGet(item.commandCvar.c_str()));
        widget = std::move(action);
    } else if (item.type == "button") {
        qhandle_t image = R_RegisterPic(item.image.c_str());
        qhandle_t imageSelected = item.imageSelected.empty()
            ? R_RegisterPic(va("%s_sel", item.image.c_str()))
            : R_RegisterPic(item.imageSelected.c_str());
        auto button = std::make_unique<MenuButtonWidget>(image, imageSelected, item.command);
        button->SetLabel(item.label);
        if (item.imageWidth > 0 || item.imageHeight > 0)
            button->SetDrawSize(item.imageWidth, item.imageHeight);
        if (item.positionSet)
            button->SetPosition(item.x, item.y);
        if (item.anchorSet) {
            if (item.anchor == 1)
                button->SetAnchorLeft();
            else if (item.anchor == 2)
                button->SetAnchorRight();
            else
                button->SetAnchorCenter();
        }
        if (item.textOffsetSet)
            button->SetTextOffset(item.textOffset);
        if (item.textSizeSet)
            button->SetTextSize(item.textSize);
        if (item.textColorSet)
            button->SetTextColor(item.textColor);
        if (item.selectedTextColorSet)
            button->SetSelectedTextColor(item.selectedTextColor);
        widget = std::move(button);
    } else if (item.type == "bitmap") {
        qhandle_t image = R_RegisterPic(item.image.c_str());
        qhandle_t imageSelected = item.imageSelected.empty()
            ? R_RegisterPic(va("%s_sel", item.image.c_str()))
            : R_RegisterPic(item.imageSelected.c_str());
        auto bitmap = std::make_unique<BitmapWidget>(image, imageSelected, item.command);
        if (item.imageWidth > 0 || item.imageHeight > 0)
            bitmap->SetDrawSize(item.imageWidth, item.imageHeight);
        if (item.positionSet)
            bitmap->SetPosition(item.x, item.y);
        if (item.anchorSet) {
            if (item.anchor == 1)
                bitmap->SetAnchorLeft();
            else if (item.anchor == 2)
                bitmap->SetAnchorRight();
            else
                bitmap->SetAnchorCenter();
        }
        widget = std::move(bitmap);
    } else if (item.type == "range") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        auto slider = std::make_unique<SliderWidget>(item.label, cvar,
                                                     item.minValue, item.maxValue,
                                                     item.stepSet ? item.step : (item.maxValue - item.minValue) / SLIDER_RANGE);
        slider->SetStatus(item.status);
        widget = std::move(slider);
    } else if (item.type == "values") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        auto spin = std::make_unique<SpinWidget>(item.label, cvar, SpinType::Index);
        for (const auto &entry : item.items)
            spin->AddOption(entry);
        spin->SetStatus(item.status);
        widget = std::move(spin);
    } else if (item.type == "strings") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        auto spin = std::make_unique<SpinWidget>(item.label, cvar, SpinType::String);
        for (const auto &entry : item.items)
            spin->AddOption(entry);
        spin->SetStatus(item.status);
        widget = std::move(spin);
    } else if (item.type == "pairs") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        auto spin = std::make_unique<SpinWidget>(item.label, cvar, SpinType::Pair);
        for (const auto &entry : item.pairs)
            spin->AddOption(entry.first, entry.second);
        spin->SetStatus(item.status);
        widget = std::move(spin);
    } else if (item.type == "dropdown") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        SpinType type = item.pairs.empty() ? SpinType::Index : SpinType::Pair;
        auto dropdown = std::make_unique<DropdownWidget>(item.label, cvar, type);
        if (type == SpinType::Pair) {
            for (const auto &entry : item.pairs)
                dropdown->AddOption(entry.first, entry.second);
        } else {
            for (const auto &entry : item.items)
                dropdown->AddOption(entry);
        }
        dropdown->SetStatus(item.status);
        widget = std::move(dropdown);
    } else if (item.type == "toggle") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        auto spin = std::make_unique<SpinWidget>(item.label, cvar, item.bit >= 0 ? SpinType::Bitfield : SpinType::Toggle);
        spin->AddOption("no");
        spin->AddOption("yes");
        if (item.bit >= 0)
            spin->SetBitfield(1U << item.bit, item.negate);
        else
            spin->SetBitfield(0, item.negate);
        spin->SetStatus(item.status);
        widget = std::move(spin);
    } else if (item.type == "switch") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        auto sw = std::make_unique<SwitchWidget>(item.label, cvar, item.bit, item.negate);
        sw->SetStatus(item.status);
        widget = std::move(sw);
    } else if (item.type == "checkbox") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        auto sw = std::make_unique<SwitchWidget>(item.label, cvar, item.bit, item.negate);
        sw->SetStyle(SwitchStyle::Checkbox);
        sw->SetStatus(item.status);
        widget = std::move(sw);
    } else if (item.type == "progress") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        float minValue = item.minValue;
        float maxValue = item.maxValue;
        if (maxValue <= minValue) {
            minValue = 0.0f;
            maxValue = 100.0f;
        }
        widget = std::make_unique<ProgressWidget>(cvar, minValue, maxValue);
    } else if (item.type == "bind") {
        widget = std::make_unique<KeyBindWidget>(item.label, item.command, item.status,
                                                 item.altStatus.empty() ? "Press the desired key, Escape to cancel" : item.altStatus);
    } else if (item.type == "field") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        auto field = std::make_unique<FieldWidget>(item.label, cvar, item.width,
                                                   item.center, item.numeric, item.integer);
        field->SetStatus(item.status);
        widget = std::move(field);
    } else if (item.type == "combobox") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        auto combo = std::make_unique<ComboWidget>(item.label, cvar, item.width,
                                                   item.center, item.numeric, item.integer);
        for (const auto &entry : item.items)
            combo->AddOption(entry);
        combo->SetStatus(item.status);
        widget = std::move(combo);
    } else if (item.type == "wrappedtext") {
        auto wrapped = std::make_unique<WrappedTextWidget>(item.label, item.maxLines,
                                                           item.wrapWidth, item.alignLeft);
        widget = std::move(wrapped);
    } else if (item.type == "blank") {
        auto blank = std::make_unique<SeparatorWidget>();
        if (!item.label.empty())
            blank->SetLabel(item.label);
        widget = std::move(blank);
    } else if (item.type == "heading") {
        auto heading = std::make_unique<HeadingWidget>();
        heading->SetLabel(item.label);
        widget = std::move(heading);
    } else if (item.type == "imagevalues") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        auto image = std::make_unique<ImageSpinWidget>(item.label, cvar,
                                                       item.path, item.filter,
                                                       item.imageWidth, item.imageHeight,
                                                       item.numeric, item.valuePrefix);
        image->SetStatus(item.status);
        widget = std::move(image);
    } else if (item.type == "episode_selector") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        auto spin = std::make_unique<SpinWidget>(item.label, cvar, SpinType::Episode);
        char **items = nullptr;
        int count = 0;
        UI_MapDB_FetchEpisodes(&items, &count);
        for (int i = 0; i < count; i++)
            spin->AddOption(items[i]);
        for (int i = 0; i < count; i++)
            Z_Free(items[i]);
        Z_Free(items);
        widget = std::move(spin);
    } else if (item.type == "unit_selector") {
        cvar_t *cvar = Cvar_WeakGet(item.cvar.c_str());
        auto spin = std::make_unique<SpinWidget>(item.label, cvar, SpinType::Unit);
        char **items = nullptr;
        int *indices = nullptr;
        int count = 0;
        UI_MapDB_FetchUnits(&items, &indices, &count);
        std::vector<int> index_list;
        for (int i = 0; i < count; i++) {
            spin->AddOption(items[i]);
            index_list.push_back(indices[i]);
            Z_Free(items[i]);
        }
        spin->SetIndices(std::move(index_list));
        Z_Free(items);
        Z_Free(indices);
        widget = std::move(spin);
    } else if (item.type == "savegame") {
        widget = std::make_unique<SaveGameWidget>(item.slot, false);
    } else if (item.type == "loadgame") {
        widget = std::make_unique<SaveGameWidget>(item.slot, true);
    }

    if (widget) {
        if (!item.labelCvar.empty())
            widget->SetLabelCvar(Cvar_WeakGet(item.labelCvar.c_str()));
        if (item.selectableSet)
            widget->SetSelectable(item.selectable);
        ApplyConditions(item, widget.get());
    }

    return widget;
}

static bool ParseMenuItem(json_parse_t *parser, Menu &menu)
{
    jsmntok_t *obj = Json_EnsureNext(parser, JSMN_OBJECT);
    MenuItemData data;

    for (int i = 0; i < obj->size; i++) {
        if (Json_Strcmp(parser, "type") == 0) {
            Json_Next(parser);
            data.type = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "label") == 0) {
            Json_Next(parser);
            data.label = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "labelCvar") == 0) {
            Json_Next(parser);
            data.labelCvar = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "cvar") == 0) {
            Json_Next(parser);
            data.cvar = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "command") == 0) {
            Json_Next(parser);
            data.command = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "commandCvar") == 0) {
            Json_Next(parser);
            data.commandCvar = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "status") == 0) {
            Json_Next(parser);
            data.status = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "altStatus") == 0) {
            Json_Next(parser);
            data.altStatus = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "items") == 0) {
            Json_Next(parser);
            ParseItemsArray(parser, data.items);
        } else if (Json_Strcmp(parser, "pairs") == 0) {
            Json_Next(parser);
            ParsePairsArray(parser, data.pairs);
        } else if (Json_Strcmp(parser, "showIf") == 0) {
            Json_Next(parser);
            ParseConditions(parser, data.showConditions);
        } else if (Json_Strcmp(parser, "enableIf") == 0) {
            Json_Next(parser);
            ParseConditions(parser, data.enableConditions);
        } else if (Json_Strcmp(parser, "default") == 0) {
            Json_Next(parser);
            data.isDefault = Json_ReadBool(parser);
            data.defaultSet = true;
        } else if (Json_Strcmp(parser, "defaultIf") == 0) {
            Json_Next(parser);
            ParseConditions(parser, data.defaultConditions);
        } else if (Json_Strcmp(parser, "ingameOnly") == 0) {
            Json_Next(parser);
            data.ingameOnly = Json_ReadBool(parser);
        } else if (Json_Strcmp(parser, "deathmatchOnly") == 0) {
            Json_Next(parser);
            data.deathmatchOnly = Json_ReadBool(parser);
        } else if (Json_Strcmp(parser, "min") == 0) {
            Json_Next(parser);
            data.minValue = static_cast<float>(Json_ReadNumber(parser));
        } else if (Json_Strcmp(parser, "max") == 0) {
            Json_Next(parser);
            data.maxValue = static_cast<float>(Json_ReadNumber(parser));
        } else if (Json_Strcmp(parser, "step") == 0) {
            Json_Next(parser);
            data.step = static_cast<float>(Json_ReadNumber(parser));
            data.stepSet = true;
        } else if (Json_Strcmp(parser, "bit") == 0) {
            Json_Next(parser);
            data.bit = static_cast<int>(Json_ReadNumber(parser));
        } else if (Json_Strcmp(parser, "negate") == 0) {
            Json_Next(parser);
            data.negate = Json_ReadBool(parser);
        } else if (Json_Strcmp(parser, "align") == 0) {
            Json_Next(parser);
            std::string align = Json_ReadString(parser);
            data.alignLeft = (align == "left");
        } else if (Json_Strcmp(parser, "center") == 0) {
            Json_Next(parser);
            data.center = Json_ReadBool(parser);
        } else if (Json_Strcmp(parser, "numeric") == 0) {
            Json_Next(parser);
            data.numeric = Json_ReadBool(parser);
        } else if (Json_Strcmp(parser, "integer") == 0) {
            Json_Next(parser);
            data.integer = Json_ReadBool(parser);
        } else if (Json_Strcmp(parser, "selectable") == 0) {
            Json_Next(parser);
            data.selectable = Json_ReadBool(parser);
            data.selectableSet = true;
        } else if (Json_Strcmp(parser, "width") == 0) {
            Json_Next(parser);
            data.width = static_cast<int>(Json_ReadNumber(parser));
        } else if (Json_Strcmp(parser, "maxLines") == 0) {
            Json_Next(parser);
            data.maxLines = static_cast<int>(Json_ReadNumber(parser));
        } else if (Json_Strcmp(parser, "wrapWidth") == 0) {
            Json_Next(parser);
            data.wrapWidth = static_cast<int>(Json_ReadNumber(parser));
        } else if (Json_Strcmp(parser, "image") == 0) {
            Json_Next(parser);
            data.image = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "imageSelected") == 0) {
            Json_Next(parser);
            data.imageSelected = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "path") == 0) {
            Json_Next(parser);
            data.path = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "filter") == 0) {
            Json_Next(parser);
            data.filter = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "valuePrefix") == 0) {
            Json_Next(parser);
            data.valuePrefix = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "imageWidth") == 0) {
            Json_Next(parser);
            data.imageWidth = static_cast<int>(Json_ReadNumber(parser));
        } else if (Json_Strcmp(parser, "imageHeight") == 0) {
            Json_Next(parser);
            data.imageHeight = static_cast<int>(Json_ReadNumber(parser));
        } else if (Json_Strcmp(parser, "textOffset") == 0) {
            Json_Next(parser);
            data.textOffset = static_cast<int>(Json_ReadNumber(parser));
            data.textOffsetSet = true;
        } else if (Json_Strcmp(parser, "textSize") == 0) {
            Json_Next(parser);
            data.textSize = static_cast<int>(Json_ReadNumber(parser));
            data.textSizeSet = true;
        } else if (Json_Strcmp(parser, "x") == 0) {
            Json_Next(parser);
            data.x = static_cast<int>(Json_ReadNumber(parser));
            data.positionSet = true;
        } else if (Json_Strcmp(parser, "y") == 0) {
            Json_Next(parser);
            data.y = static_cast<int>(Json_ReadNumber(parser));
            data.positionSet = true;
        } else if (Json_Strcmp(parser, "anchor") == 0) {
            Json_Next(parser);
            std::string value = Json_ReadString(parser);
            if (value == "left") {
                data.anchor = 1;
                data.anchorSet = true;
            } else if (value == "right") {
                data.anchor = 2;
                data.anchorSet = true;
            } else {
                data.anchor = 0;
                data.anchorSet = true;
            }
        } else if (Json_Strcmp(parser, "textColor") == 0) {
            Json_Next(parser);
            std::string value = Json_ReadString(parser);
            if (SCR_ParseColor(value.c_str(), &data.textColor))
                data.textColorSet = true;
        } else if (Json_Strcmp(parser, "textSelectedColor") == 0) {
            Json_Next(parser);
            std::string value = Json_ReadString(parser);
            if (SCR_ParseColor(value.c_str(), &data.selectedTextColor))
                data.selectedTextColorSet = true;
        } else if (Json_Strcmp(parser, "slot") == 0) {
            Json_Next(parser);
            data.slot = Json_ReadString(parser);
        } else {
            Json_Next(parser);
            Json_SkipToken(parser);
        }
    }

    auto widget = BuildWidget(data);
    if (widget)
        menu.AddWidget(std::move(widget));
    return widget != nullptr;
}

static void ParseMenu(json_parse_t *parser)
{
    jsmntok_t *obj = Json_EnsureNext(parser, JSMN_OBJECT);
    std::string name;
    std::string title;
    std::string background;
    std::string banner;
    std::string plaque;
    std::string logo;
    std::string feeder;
    std::string closeCommand;
    bool compact = false;
    bool transparent = false;
    bool allowBlur = true;
    bool allowBlurSet = false;
    bool showPlayerName = false;
    bool alignToBitmaps = false;
    bool fixedLayout = false;
    bool plaqueRectSet = false;
    bool logoRectSet = false;
    vrect_t plaqueRect{};
    vrect_t logoRect{};
    int plaqueAnchor = 0;
    int logoAnchor = 0;
    bool plaqueAnchorSet = false;
    bool logoAnchorSet = false;
    std::string footerText;
    std::string footerSubtext;
    bool footerColorSet = false;
    color_t footerColor{};
    bool footerSizeSet = false;
    int footerSize = 0;
    bool frame = false;
    bool frameSet = false;
    int framePadding = GenericSpacing(CONCHAR_HEIGHT);
    int frameBorderWidth = 1;
    color_t frameFill = COLOR_RGBA(0, 0, 0, 200);
    color_t frameBorder = COLOR_RGBA(255, 255, 255, 40);

    std::unique_ptr<Menu> menu;

    for (int i = 0; i < obj->size; i++) {
        if (Json_Strcmp(parser, "name") == 0) {
            Json_Next(parser);
            name = Json_ReadString(parser);
            if (feeder.empty())
                menu = std::make_unique<Menu>(name);
        } else if (Json_Strcmp(parser, "feeder") == 0) {
            Json_Next(parser);
            feeder = Json_ReadString(parser);
            menu.reset();
        } else if (Json_Strcmp(parser, "title") == 0) {
            Json_Next(parser);
            title = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "background") == 0) {
            Json_Next(parser);
            background = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "banner") == 0) {
            Json_Next(parser);
            banner = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "plaque") == 0) {
            Json_Next(parser);
            if (parser->pos->type == JSMN_OBJECT) {
                jsmntok_t *plaque_obj = Json_EnsureNext(parser, JSMN_OBJECT);
                for (int p = 0; p < plaque_obj->size; p++) {
                    if (Json_Strcmp(parser, "image") == 0) {
                        Json_Next(parser);
                        plaque = Json_ReadString(parser);
                    } else if (Json_Strcmp(parser, "logo") == 0) {
                        Json_Next(parser);
                        logo = Json_ReadString(parser);
                    } else if (Json_Strcmp(parser, "plaqueRect") == 0) {
                        Json_Next(parser);
                        jsmntok_t *rect = Json_EnsureNext(parser, JSMN_OBJECT);
                        for (int r = 0; r < rect->size; r++) {
                            if (Json_Strcmp(parser, "x") == 0) {
                                Json_Next(parser);
                                plaqueRect.x = static_cast<int>(Json_ReadNumber(parser));
                            } else if (Json_Strcmp(parser, "y") == 0) {
                                Json_Next(parser);
                                plaqueRect.y = static_cast<int>(Json_ReadNumber(parser));
                            } else if (Json_Strcmp(parser, "w") == 0) {
                                Json_Next(parser);
                                plaqueRect.width = static_cast<int>(Json_ReadNumber(parser));
                            } else if (Json_Strcmp(parser, "h") == 0) {
                                Json_Next(parser);
                                plaqueRect.height = static_cast<int>(Json_ReadNumber(parser));
                            } else {
                                Json_Next(parser);
                                Json_SkipToken(parser);
                            }
                        }
                        plaqueRectSet = true;
                    } else if (Json_Strcmp(parser, "plaqueAnchor") == 0) {
                        Json_Next(parser);
                        std::string value = Json_ReadString(parser);
                        if (value == "left")
                            plaqueAnchor = 1;
                        else if (value == "right")
                            plaqueAnchor = 2;
                        else
                            plaqueAnchor = 0;
                        plaqueAnchorSet = true;
                    } else if (Json_Strcmp(parser, "logoRect") == 0) {
                        Json_Next(parser);
                        jsmntok_t *rect = Json_EnsureNext(parser, JSMN_OBJECT);
                        for (int r = 0; r < rect->size; r++) {
                            if (Json_Strcmp(parser, "x") == 0) {
                                Json_Next(parser);
                                logoRect.x = static_cast<int>(Json_ReadNumber(parser));
                            } else if (Json_Strcmp(parser, "y") == 0) {
                                Json_Next(parser);
                                logoRect.y = static_cast<int>(Json_ReadNumber(parser));
                            } else if (Json_Strcmp(parser, "w") == 0) {
                                Json_Next(parser);
                                logoRect.width = static_cast<int>(Json_ReadNumber(parser));
                            } else if (Json_Strcmp(parser, "h") == 0) {
                                Json_Next(parser);
                                logoRect.height = static_cast<int>(Json_ReadNumber(parser));
                            } else {
                                Json_Next(parser);
                                Json_SkipToken(parser);
                            }
                        }
                        logoRectSet = true;
                    } else if (Json_Strcmp(parser, "logoAnchor") == 0) {
                        Json_Next(parser);
                        std::string value = Json_ReadString(parser);
                        if (value == "left")
                            logoAnchor = 1;
                        else if (value == "right")
                            logoAnchor = 2;
                        else
                            logoAnchor = 0;
                        logoAnchorSet = true;
                    } else {
                        Json_Next(parser);
                        Json_SkipToken(parser);
                    }
                }
            } else {
                plaque = Json_ReadString(parser);
            }
        } else if (Json_Strcmp(parser, "playerName") == 0) {
            Json_Next(parser);
            showPlayerName = Json_ReadBool(parser);
        } else if (Json_Strcmp(parser, "alignToBitmaps") == 0) {
            Json_Next(parser);
            alignToBitmaps = Json_ReadBool(parser);
        } else if (Json_Strcmp(parser, "fixedLayout") == 0) {
            Json_Next(parser);
            fixedLayout = Json_ReadBool(parser);
        } else if (Json_Strcmp(parser, "footer") == 0) {
            Json_Next(parser);
            if (parser->pos->type == JSMN_OBJECT) {
                jsmntok_t *footer = Json_EnsureNext(parser, JSMN_OBJECT);
                for (int f = 0; f < footer->size; f++) {
                    if (Json_Strcmp(parser, "text") == 0) {
                        Json_Next(parser);
                        footerText = Json_ReadString(parser);
                    } else if (Json_Strcmp(parser, "subtext") == 0) {
                        Json_Next(parser);
                        footerSubtext = Json_ReadString(parser);
                    } else if (Json_Strcmp(parser, "color") == 0) {
                        Json_Next(parser);
                        std::string value = Json_ReadString(parser);
                        if (SCR_ParseColor(value.c_str(), &footerColor))
                            footerColorSet = true;
                    } else if (Json_Strcmp(parser, "size") == 0) {
                        Json_Next(parser);
                        footerSize = static_cast<int>(Json_ReadNumber(parser));
                        footerSizeSet = true;
                    } else {
                        Json_Next(parser);
                        Json_SkipToken(parser);
                    }
                }
            } else {
                footerText = Json_ReadString(parser);
            }
        } else if (Json_Strcmp(parser, "closeCommand") == 0) {
            Json_Next(parser);
            closeCommand = Json_ReadString(parser);
        } else if (Json_Strcmp(parser, "style") == 0) {
            Json_Next(parser);
            jsmntok_t *style = Json_EnsureNext(parser, JSMN_OBJECT);
            for (int s = 0; s < style->size; s++) {
                if (Json_Strcmp(parser, "compact") == 0) {
                    Json_Next(parser);
                    compact = Json_ReadBool(parser);
                } else if (Json_Strcmp(parser, "transparent") == 0) {
                    Json_Next(parser);
                    transparent = Json_ReadBool(parser);
                } else if (Json_Strcmp(parser, "blur") == 0) {
                    Json_Next(parser);
                    allowBlur = Json_ReadBool(parser);
                    allowBlurSet = true;
                } else if (Json_Strcmp(parser, "frame") == 0) {
                    Json_Next(parser);
                    frameSet = true;
                    if (parser->pos->type == JSMN_OBJECT) {
                        frame = true;
                        jsmntok_t *frame_obj = Json_EnsureNext(parser, JSMN_OBJECT);
                        for (int f = 0; f < frame_obj->size; f++) {
                            if (Json_Strcmp(parser, "fill") == 0) {
                                Json_Next(parser);
                                std::string value = Json_ReadString(parser);
                                SCR_ParseColor(value.c_str(), &frameFill);
                            } else if (Json_Strcmp(parser, "border") == 0) {
                                Json_Next(parser);
                                std::string value = Json_ReadString(parser);
                                SCR_ParseColor(value.c_str(), &frameBorder);
                            } else if (Json_Strcmp(parser, "padding") == 0) {
                                Json_Next(parser);
                                framePadding = static_cast<int>(Json_ReadNumber(parser));
                            } else if (Json_Strcmp(parser, "borderWidth") == 0) {
                                Json_Next(parser);
                                frameBorderWidth = static_cast<int>(Json_ReadNumber(parser));
                            } else {
                                Json_Next(parser);
                                Json_SkipToken(parser);
                            }
                        }
                    } else if (parser->pos->type == JSMN_PRIMITIVE) {
                        frame = Json_ReadBool(parser);
                    } else {
                        Json_SkipToken(parser);
                    }
                } else {
                    Json_Next(parser);
                    Json_SkipToken(parser);
                }
            }
        } else if (Json_Strcmp(parser, "items") == 0) {
            Json_Next(parser);
            if (!feeder.empty()) {
                Json_SkipToken(parser);
                continue;
            }
            jsmntok_t *items = Json_EnsureNext(parser, JSMN_ARRAY);
            if (!menu)
                menu = std::make_unique<Menu>(name);
            for (int m = 0; m < items->size; m++) {
                ParseMenuItem(parser, *menu);
            }
        } else {
            Json_Next(parser);
            Json_SkipToken(parser);
        }
    }

    if (!feeder.empty()) {
        auto feeder_menu = CreateFeederMenu(feeder);
        if (!feeder_menu) {
            Com_WPrintf("$ui_unknown_menu_feeder", feeder.c_str());
            return;
        }
        GetMenuSystem().RegisterMenu(std::move(feeder_menu));
        return;
    }

    if (!menu)
        return;

    if (!title.empty())
        menu->SetTitle(title);
    if (compact)
        menu->SetCompact(true);
    if (transparent)
        menu->SetTransparent(true);
    if (!allowBlurSet && IsMatchMenuName(name))
        allowBlur = false;
    menu->SetAllowBlur(allowBlur);
    if (frameSet && frame) {
        menu->SetFrameStyle(true, frameFill, frameBorder, framePadding, frameBorderWidth);
    }
    if (!closeCommand.empty())
        menu->SetCloseCommand(std::move(closeCommand));

    if (!background.empty()) {
        color_t color;
        if (SCR_ParseColor(background.c_str(), &color)) {
            menu->SetBackground(color);
        } else {
            qhandle_t image = R_RegisterPic(background.c_str());
            bool transparent_bg = R_GetPicSize(NULL, NULL, image);
            menu->SetBackgroundImage(image, transparent_bg);
        }
    }

    if (!banner.empty()) {
        qhandle_t pic = R_RegisterPic(banner.c_str());
        vrect_t rc{};
        R_GetPicSize(&rc.width, &rc.height, pic);
        menu->SetBanner(pic, rc);
    }

    if (!plaque.empty()) {
        qhandle_t pic = R_RegisterPic(plaque.c_str());
        vrect_t rc{};
        R_GetPicSize(&rc.width, &rc.height, pic);
        if (plaqueRectSet) {
            menu->SetPlaque(pic, plaqueRect);
            menu->SetPlaqueFixed(true);
            if (plaqueAnchorSet)
                menu->SetPlaqueAnchor(plaqueAnchor);
        } else {
            menu->SetPlaque(pic, rc);
        }
    }

    if (!logo.empty()) {
        qhandle_t pic = R_RegisterPic(logo.c_str());
        vrect_t rc{};
        R_GetPicSize(&rc.width, &rc.height, pic);
        if (logoRectSet) {
            menu->SetLogo(pic, logoRect);
            menu->SetLogoFixed(true);
            if (logoAnchorSet)
                menu->SetLogoAnchor(logoAnchor);
        } else {
            menu->SetLogo(pic, rc);
        }
    }

    if (showPlayerName)
        menu->SetShowPlayerName(true);
    if (alignToBitmaps)
        menu->SetAlignContentToBitmaps(true);
    if (fixedLayout)
        menu->SetFixedLayout(true);
    if (!footerText.empty())
        menu->SetFooterText(footerText);
    if (!footerSubtext.empty())
        menu->SetFooterSubtext(footerSubtext);
    if (footerColorSet)
        menu->SetFooterColor(footerColor);
    if (footerSizeSet)
        menu->SetFooterSize(footerSize);

    GetMenuSystem().RegisterMenu(std::move(menu));
}

static void ParseGlobals(json_parse_t *parser)
{
    jsmntok_t *obj = Json_EnsureNext(parser, JSMN_OBJECT);
    for (int i = 0; i < obj->size; i++) {
        if (Json_Strcmp(parser, "background") == 0) {
            Json_Next(parser);
            std::string value = Json_ReadString(parser);
            if (SCR_ParseColor(value.c_str(), &uis.color.background)) {
                uis.backgroundHandle = 0;
                uis.transparent = uis.color.background.a != 255;
            } else {
                uis.backgroundHandle = R_RegisterPic(value.c_str());
                uis.transparent = R_GetPicSize(NULL, NULL, uis.backgroundHandle);
            }
        } else if (Json_Strcmp(parser, "font") == 0) {
            Json_Next(parser);
            std::string value = Json_ReadString(parser);
            const char *font_value = value.c_str();
            if (!value.empty() && value[0] == '$') {
                const char *cvar_name = value.c_str() + 1;
                cvar_t *ref = Cvar_FindVar(cvar_name);
                font_value = (ref && ref->string) ? ref->string : "";
            }
            if (font_value && *font_value)
                Cvar_Set("ui_font", font_value);
            uis.fontHandle = UI_FontLegacyHandle();
        } else if (Json_Strcmp(parser, "cursor") == 0) {
            Json_Next(parser);
            uis.cursorHandle = R_RegisterPic(Json_ReadString(parser).c_str());
            R_GetPicSize(&uis.cursorWidth, &uis.cursorHeight, uis.cursorHandle);
            uis.cursorWidth = UI_CURSOR_SIZE;
            uis.cursorHeight = UI_CURSOR_SIZE;
        } else if (Json_Strcmp(parser, "weapon") == 0) {
            Json_Next(parser);
            Q_strlcpy(uis.weaponModel, Json_ReadString(parser).c_str(), sizeof(uis.weaponModel));
        } else if (Json_Strcmp(parser, "colors") == 0) {
            Json_Next(parser);
            jsmntok_t *colors = Json_EnsureNext(parser, JSMN_OBJECT);
            for (int c = 0; c < colors->size; c++) {
                if (Json_Strcmp(parser, "normal") == 0) {
                    Json_Next(parser);
                    SCR_ParseColor(Json_ReadString(parser).c_str(), &uis.color.normal);
                } else if (Json_Strcmp(parser, "active") == 0) {
                    Json_Next(parser);
                    SCR_ParseColor(Json_ReadString(parser).c_str(), &uis.color.active);
                } else if (Json_Strcmp(parser, "selection") == 0) {
                    Json_Next(parser);
                    SCR_ParseColor(Json_ReadString(parser).c_str(), &uis.color.selection);
                } else if (Json_Strcmp(parser, "disabled") == 0) {
                    Json_Next(parser);
                    SCR_ParseColor(Json_ReadString(parser).c_str(), &uis.color.disabled);
                } else {
                    Json_Next(parser);
                    Json_SkipToken(parser);
                }
            }
        } else {
            Json_Next(parser);
            Json_SkipToken(parser);
        }
    }
}

bool UI_LoadJsonMenus(const char *path)
{
    json_parse_t parser{};
    if (Json_ErrorHandler(parser)) {
        Com_WPrintf("$ui_json_load_failed", path, parser.error, parser.error_loc);
        Json_Free(&parser);
        return false;
    }

    Json_Load(path, &parser);
    jsmntok_t *root = Json_EnsureNext(&parser, JSMN_OBJECT);
    for (int i = 0; i < root->size; i++) {
        if (Json_Strcmp(&parser, "globals") == 0) {
            Json_Next(&parser);
            ParseGlobals(&parser);
        } else if (Json_Strcmp(&parser, "menus") == 0) {
            Json_Next(&parser);
            jsmntok_t *menus = Json_EnsureNext(&parser, JSMN_ARRAY);
            for (int m = 0; m < menus->size; m++) {
                ParseMenu(&parser);
            }
        } else {
            Json_Next(&parser);
            Json_SkipToken(&parser);
        }
    }

    Json_Free(&parser);
    return true;
}

} // namespace ui

