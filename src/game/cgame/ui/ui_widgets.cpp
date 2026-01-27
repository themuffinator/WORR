#include "ui/ui_internal.h"

#include <cmath>
#include <sstream>

namespace ui {
namespace {

static int WidgetTextHeight();

static std::string DecodeEscapes(const char *text)
{
    std::string out;
    if (!text || !*text)
        return out;

    for (size_t i = 0; text[i]; i++) {
        char ch = text[i];
        if (ch == '\\' && text[i + 1]) {
            char next = text[i + 1];
            if (next == 'n' || next == 'r' || next == 't' || next == 'v' || next == 'f') {
                out.push_back(' ');
                i++;
                continue;
            }
            if (next == '\\' || next == '"') {
                out.push_back(next);
                i++;
                continue;
            }
            if (next == 'x' && text[i + 2] && text[i + 3]) {
                out.push_back(' ');
                i += 3;
                continue;
            }
        }

        if (ch < 32)
            out.push_back(' ');
        else
            out.push_back(ch);
    }

    return out;
}

static std::vector<std::string> WrapText(const std::string &text, size_t maxWidth, size_t maxLines)
{
    std::vector<std::string> lines;
    if (text.empty() || maxWidth == 0 || maxLines == 0)
        return lines;

    std::istringstream stream(text);
    std::string word;
    std::string currentLine;

    while (stream >> word) {
        if (lines.size() >= maxLines)
            break;

        if (currentLine.empty()) {
            currentLine = word;
        } else if (currentLine.size() + 1 + word.size() <= maxWidth) {
            currentLine += " " + word;
        } else {
            lines.push_back(currentLine);
            currentLine = word;
            if (lines.size() >= maxLines)
                break;
        }
    }

    if (!currentLine.empty() && lines.size() < maxLines)
        lines.push_back(currentLine);

    return lines;
}

static int DefaultWrapWidthChars()
{
    int columns = max(1, uis.width / CONCHAR_WIDTH);
    int maxChars = columns - 8;
    if (maxChars < 20)
        maxChars = columns;
    return Q_clip(maxChars, 20, 60);
}

static const color_t kRainbowColors[26] = {
    COLOR_RGBA(255, 0, 0, 255),
    COLOR_RGBA(255, 64, 0, 255),
    COLOR_RGBA(255, 128, 0, 255),
    COLOR_RGBA(255, 192, 0, 255),
    COLOR_RGBA(255, 255, 0, 255),
    COLOR_RGBA(192, 255, 0, 255),
    COLOR_RGBA(128, 255, 0, 255),
    COLOR_RGBA(64, 255, 0, 255),
    COLOR_RGBA(0, 255, 0, 255),
    COLOR_RGBA(0, 255, 64, 255),
    COLOR_RGBA(0, 255, 128, 255),
    COLOR_RGBA(0, 255, 192, 255),
    COLOR_RGBA(0, 255, 255, 255),
    COLOR_RGBA(0, 192, 255, 255),
    COLOR_RGBA(0, 128, 255, 255),
    COLOR_RGBA(0, 64, 255, 255),
    COLOR_RGBA(0, 0, 255, 255),
    COLOR_RGBA(64, 0, 255, 255),
    COLOR_RGBA(128, 0, 255, 255),
    COLOR_RGBA(192, 0, 255, 255),
    COLOR_RGBA(255, 0, 255, 255),
    COLOR_RGBA(255, 0, 192, 255),
    COLOR_RGBA(255, 0, 128, 255),
    COLOR_RGBA(255, 0, 64, 255),
    COLOR_RGBA(255, 255, 255, 255),
    COLOR_RGBA(128, 128, 128, 255)
};

static size_t InputFieldClampChars(const char *text, size_t max_chars)
{
    if (!text)
        return 0;
    size_t available = UTF8_CountChars(text, strlen(text));
    if (max_chars)
        available = min(available, max_chars);
    return available;
}

static size_t InputFieldCharsForWidth(const char *text, size_t max_chars, int pixel_width)
{
    if (!text || pixel_width <= 0 || max_chars == 0)
        return 0;

    size_t available = InputFieldClampChars(text, max_chars);
    size_t chars = 0;
    while (chars < available) {
        size_t next_bytes = UTF8_OffsetForChars(text, chars + 1);
        int width = UI_FontMeasureString(0, next_bytes, text, nullptr);
        if (width > pixel_width)
            break;
        chars++;
    }
    return chars;
}

static size_t InputFieldOffsetForWidth(const char *text, size_t cursor_chars, int pixel_width)
{
    if (!text || pixel_width <= 0)
        return 0;

    size_t cursor_bytes = UTF8_OffsetForChars(text, cursor_chars);
    for (size_t start = 0; start < cursor_chars; ++start) {
        size_t start_bytes = UTF8_OffsetForChars(text, start);
        size_t len_bytes = cursor_bytes - start_bytes;
        int width = UI_FontMeasureString(0, len_bytes, text + start_bytes, nullptr);
        if (width <= pixel_width)
            return start;
    }
    return cursor_chars;
}

static int DrawInputFieldText(const inputField_t *field, int x, int y, int flags,
                              int pixel_width, size_t max_chars,
                              int *out_cursor_x, int *out_cursor_w)
{
    if (out_cursor_x)
        *out_cursor_x = x;
    if (out_cursor_w)
        *out_cursor_w = 0;
    if (!field || !field->maxChars || !field->visibleChars)
        return 0;

    size_t total_chars = InputFieldClampChars(field->text, max_chars);
    size_t cursor_chars = UTF8_CountChars(field->text, field->cursorPos);
    if (cursor_chars > total_chars)
        cursor_chars = total_chars;

    size_t offset_chars = InputFieldOffsetForWidth(field->text, cursor_chars, pixel_width);
    if (offset_chars > total_chars)
        offset_chars = total_chars;

    size_t remaining_chars = (offset_chars < total_chars) ? (total_chars - offset_chars) : 0;
    size_t offset = UTF8_OffsetForChars(field->text, offset_chars);
    const char *text = field->text + offset;
    size_t draw_chars = InputFieldCharsForWidth(text, remaining_chars, pixel_width);

    size_t cursor_chars_visible = (cursor_chars > offset_chars) ? (cursor_chars - offset_chars) : 0;
    if (cursor_chars_visible > draw_chars)
        cursor_chars_visible = draw_chars;

    size_t draw_len = UTF8_OffsetForChars(text, draw_chars);
    size_t cursor_bytes = UTF8_OffsetForChars(text, cursor_chars_visible);

    if (field->selecting && field->selectionAnchor != field->cursorPos) {
        size_t sel_start = min(field->selectionAnchor, field->cursorPos);
        size_t sel_end = max(field->selectionAnchor, field->cursorPos);
        size_t sel_start_chars = UTF8_CountChars(field->text, sel_start);
        size_t sel_end_chars = UTF8_CountChars(field->text, sel_end);
        if (sel_start_chars > total_chars)
            sel_start_chars = total_chars;
        if (sel_end_chars > total_chars)
            sel_end_chars = total_chars;
        size_t sel_start_visible = (sel_start_chars > offset_chars)
            ? (sel_start_chars - offset_chars)
            : 0;
        size_t sel_end_visible = (sel_end_chars > offset_chars)
            ? (sel_end_chars - offset_chars)
            : 0;
        if (sel_start_visible > draw_chars)
            sel_start_visible = draw_chars;
        if (sel_end_visible > draw_chars)
            sel_end_visible = draw_chars;

        if (sel_end_visible > sel_start_visible) {
            size_t sel_start_bytes = UTF8_OffsetForChars(text, sel_start_visible);
            size_t sel_end_bytes = UTF8_OffsetForChars(text, sel_end_visible);
            int sel_start_x = x + UI_FontMeasureString(0, sel_start_bytes, text, nullptr);
            int sel_end_x = x + UI_FontMeasureString(0, sel_end_bytes, text, nullptr);
            int sel_w = max(0, sel_end_x - sel_start_x);
            color_t highlight = COLOR_SETA_U8(uis.color.active, 120);
            R_DrawFill32(sel_start_x, y, sel_w, WidgetTextHeight(), highlight);
        }
    }

    int end_x = UI_FontDrawString(x, y, flags, draw_len, text, COLOR_WHITE);
    int cursor_x = x + UI_FontMeasureString(0, cursor_bytes, text, nullptr);
    if (out_cursor_x)
        *out_cursor_x = cursor_x;
    if (out_cursor_w) {
        size_t next_chars = min(draw_chars, cursor_chars_visible + 1);
        size_t next_bytes = UTF8_OffsetForChars(text, next_chars);
        int next_x = x + UI_FontMeasureString(0, next_bytes, text, nullptr);
        *out_cursor_w = max(0, next_x - cursor_x);
    }

    return end_x;
}

static void UpdateInputFieldVisibleChars(inputField_t *field, int pixel_width)
{
    if (!field || !field->maxChars)
        return;

    int char_w = UI_FontMeasureString(0, 1, "i", nullptr);
    if (char_w <= 0)
        char_w = CONCHAR_WIDTH;
    int visible = max(1, pixel_width / char_w);
    if (visible > static_cast<int>(field->maxChars))
        visible = static_cast<int>(field->maxChars);
    field->visibleChars = static_cast<size_t>(visible);
}

static int InputFieldCharWidth()
{
    int char_w = UI_FontMeasureString(0, 1, "M", nullptr);
    if (char_w <= 0)
        char_w = CONCHAR_WIDTH;
    return char_w;
}

static int InputFieldBoxWidth(int chars, bool includeArrow)
{
    int char_w = InputFieldCharWidth();
    int box_w = max(1, chars * char_w);
    if (includeArrow)
        box_w += char_w;
    return box_w;
}

static size_t InputFieldCursorFromMouse(const inputField_t *field, int mouse_x,
                                        int text_x, int pixel_width)
{
    if (!field || !field->maxChars || !field->visibleChars)
        return 0;

    size_t total_chars = InputFieldClampChars(field->text, field->maxChars);
    size_t cursor_chars = UTF8_CountChars(field->text, field->cursorPos);
    if (cursor_chars > total_chars)
        cursor_chars = total_chars;

    size_t offset_chars = InputFieldOffsetForWidth(field->text, cursor_chars, pixel_width);
    if (offset_chars > total_chars)
        offset_chars = total_chars;

    size_t offset = UTF8_OffsetForChars(field->text, offset_chars);
    const char *text = field->text + offset;
    size_t remaining_chars = (offset_chars < total_chars) ? (total_chars - offset_chars) : 0;
    size_t max_chars = InputFieldCharsForWidth(text, remaining_chars, pixel_width);
    int click_x = mouse_x - text_x;
    if (click_x < 0)
        click_x = 0;

    size_t click_bytes = 0;
    for (size_t i = 0; i < max_chars; ++i) {
        size_t len_bytes = UTF8_OffsetForChars(text, i + 1);
        int width = UI_FontMeasureString(0, len_bytes, text, nullptr);
        if (width > click_x)
            break;
        click_bytes = len_bytes;
    }

    return offset + click_bytes;
}

static int WidgetTextHeight()
{
    return max(1, UI_FontLineHeight(1));
}

static int WidgetTextY(int y, int height)
{
    int text_h = WidgetTextHeight();
    return y + (height - text_h) / 2;
}

static int WidgetGap()
{
    return CONCHAR_WIDTH * 2;
}

static int WidgetPadding()
{
    return max(2, CONCHAR_WIDTH / 2);
}

static void DrawPanel(int x, int y, int width, int height, color_t fill, color_t border, int borderWidth)
{
    if (width <= 0 || height <= 0)
        return;

    if (fill.a > 0)
        R_DrawFill32(x, y, width, height, fill);
    if (border.a > 0 && borderWidth > 0) {
        R_DrawFill32(x, y, width, borderWidth, border);
        R_DrawFill32(x, y + height - borderWidth, width, borderWidth, border);
        R_DrawFill32(x, y, borderWidth, height, border);
        R_DrawFill32(x + width - borderWidth, y, borderWidth, height, border);
    }
}

static void DrawControlBox(int x, int y, int width, int height, bool focused, bool disabled)
{
    color_t base = disabled ? uis.color.disabled : uis.color.normal;
    color_t fill = COLOR_SETA_U8(base, disabled ? 70 : 110);
    color_t border = disabled
        ? COLOR_SETA_U8(uis.color.disabled, 140)
        : (focused ? COLOR_SETA_U8(uis.color.active, 220)
                   : COLOR_SETA_U8(uis.color.selection, 160));

    DrawPanel(x, y, width, height, fill, border, 1);

    if (!disabled && width > 2 && height > 2) {
        color_t highlight = COLOR_RGBA(255, 255, 255, focused ? 60 : 30);
        R_DrawFill32(x + 1, y + 1, width - 2, 1, highlight);
    }
}

static color_t WidgetContrastText(color_t background)
{
    int lum = (background.r * 54 + background.g * 183 + background.b * 19) >> 8;
    if (lum > 140)
        return COLOR_RGBA(0, 0, 0, 255);
    return COLOR_WHITE;
}

static void DrawRowHighlight(int x, int y, int width, int height, bool focused, bool disabled)
{
    if (!focused || disabled)
        return;
    color_t fill = COLOR_SETA_U8(uis.color.active, 70);
    color_t border = COLOR_SETA_U8(uis.color.active, 140);

    if (width > 0 && height > 0 && fill.a > 0) {
        int step = max(1, width / 128);
        int center = width / 2;
        int max_dist = max(1, center);
        for (int i = 0; i < width; i += step) {
            int seg_w = min(step, width - i);
            int mid = i + seg_w / 2;
            float weight = 1.0f - fabsf((float)(mid - center) / (float)max_dist);
            weight = Q_clipf(weight, 0.0f, 1.0f);
            int alpha = Q_rint(fill.a * weight);
            if (alpha <= 0)
                continue;
            color_t seg = fill;
            seg.a = (uint8_t)Q_clip(alpha, 0, 255);
            R_DrawFill32(x + i, y, seg_w, height, seg);
        }
    }

    DrawPanel(x, y, width, height, COLOR_A(0), border, 1);
}

static void DrawTextBox(int x, int y, int width, int height, bool focused, bool disabled,
                        int *out_box_y, int *out_box_h)
{
    int box_w = max(1, width);
    int text_h = WidgetTextHeight();
    int target_h = max(8, text_h + 6);
    int box_h = min(height, target_h);
    if (box_h < 6)
        box_h = max(1, height);
    int box_y = y + (height - box_h) / 2;

    DrawControlBox(x, box_y, box_w, box_h, focused, disabled);
    if (out_box_y)
        *out_box_y = box_y;
    if (out_box_h)
        *out_box_h = box_h;
}

static void DrawInputCursor(int x, int y, int height, int char_width, bool overstrike,
                            color_t color)
{
    int cursor_h = max(1, height);
    if (overstrike) {
        int width = max(2, char_width);
        if (width < 2)
            width = max(2, cursor_h / 2);
        color_t fill = COLOR_SETA_U8(color, 160);
        R_DrawFill32(x, y, width, cursor_h, fill);
        return;
    }

    color_t fill = COLOR_SETA_U8(color, 220);
    R_DrawFill32(x, y, 1, cursor_h, fill);
}

} // namespace

int Widget::Height(int lineHeight) const
{
    return lineHeight;
}

void Widget::Layout(int x, int y, int width, int lineHeight)
{
    rect_.x = x;
    rect_.y = y;
    rect_.width = width;
    rect_.height = Height(lineHeight);
}

const char *Widget::LabelText() const
{
    if (!labelCvar_) {
        return label_.c_str();
    }

    const char *raw = labelCvar_->string ? labelCvar_->string : "";
    labelCache_ = DecodeEscapes(raw);
    return labelCache_.c_str();
}

void Widget::SetColumns(int labelWidth, int valueX, int valueWidth)
{
    columnLabelWidth_ = max(0, labelWidth);
    columnValueX_ = max(0, valueX);
    columnValueWidth_ = max(0, valueWidth);
    useColumns_ = true;
}

void Widget::ClearColumns()
{
    useColumns_ = false;
    columnLabelWidth_ = 0;
    columnValueX_ = 0;
    columnValueWidth_ = 0;
}

ActionWidget::ActionWidget(std::string command)
    : command_(std::move(command))
{
}

void ActionWidget::Layout(int x, int y, int width, int lineHeight)
{
    rect_.y = y;
    rect_.width = width;
    rect_.height = Height(lineHeight);
    rect_.x = alignLeft_ ? x : x - width / 2;
}

void ActionWidget::Draw(bool focused) const
{
    bool disabled = IsDisabled();
    color_t color = disabled ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled)
        color = uis.color.active;

    DrawRowHighlight(rect_.x, rect_.y, rect_.width, rect_.height, focused, disabled);

    int draw_x = alignLeft_ ? rect_.x : rect_.x + rect_.width / 2;
    int draw_y = WidgetTextY(rect_.y, rect_.height);
    int flags = alignLeft_ ? UI_LEFT : UI_CENTER;
    if (!focused && !disabled)
        flags |= UI_ALTCOLOR;
    UI_DrawString(draw_x, draw_y, flags, color, LabelText());
}

Sound ActionWidget::Activate()
{
    if (IsDisabled())
        return Sound::Beep;
    const char *command = nullptr;
    if (commandCvar_ && commandCvar_->string && commandCvar_->string[0]) {
        command = commandCvar_->string;
    } else if (!command_.empty()) {
        command = command_.c_str();
    }
    if (command) {
        Cbuf_AddText(&cmd_buffer, command);
        Cbuf_AddText(&cmd_buffer, "\n");
    }
    return Sound::In;
}

BitmapWidget::BitmapWidget(qhandle_t image, qhandle_t imageSelected, std::string command)
    : image_(image)
    , imageSelected_(imageSelected)
    , command_(std::move(command))
{
    R_GetPicSize(&imageWidth_, &imageHeight_, image_);
    selectable_ = true;
}

int BitmapWidget::Height(int lineHeight) const
{
    (void)lineHeight;
    return GenericSpacing(imageHeight_);
}

void BitmapWidget::Layout(int x, int y, int width, int lineHeight)
{
    (void)width;
    (void)lineHeight;
    rect_.x = x - imageWidth_ / 2;
    rect_.y = y;
    rect_.width = imageWidth_;
    rect_.height = imageHeight_;
}

void BitmapWidget::Draw(bool focused) const
{
    if (focused) {
        unsigned frame = (uis.realtime / 100) % NUM_CURSOR_FRAMES;
        R_DrawPic(rect_.x - CURSOR_OFFSET, rect_.y, COLOR_WHITE, uis.bitmapCursors[frame]);
        R_DrawPic(rect_.x, rect_.y, COLOR_WHITE, imageSelected_);
    } else {
        R_DrawPic(rect_.x, rect_.y, COLOR_WHITE, image_);
    }
}

Sound BitmapWidget::Activate()
{
    if (!command_.empty()) {
        Cbuf_AddText(&cmd_buffer, command_.c_str());
        Cbuf_AddText(&cmd_buffer, "\n");
    }
    return Sound::In;
}

SliderWidget::SliderWidget(std::string label, cvar_t *cvar, float minValue, float maxValue, float step)
    : cvar_(cvar)
    , minValue_(minValue)
    , maxValue_(maxValue)
    , step_(step)
{
    label_ = std::move(label);
    selectable_ = true;
}

void SliderWidget::OnOpen()
{
    if (cvar_)
        curValue_ = cvar_->value;
    if (step_ != 0.0f) {
        float step = fabsf(step_);
        if (step > 0.0f) {
            float snapped = roundf((curValue_ - minValue_) / step);
            curValue_ = minValue_ + snapped * step;
        }
    }
    curValue_ = Q_clipf(curValue_, minValue_, maxValue_);
    modified_ = false;
    hoverFrac_ = 0.0f;
    hoverTime_ = 0;
    dragging_ = false;
}

void SliderWidget::OnClose()
{
    dragging_ = false;
    if (cvar_ && modified_)
        Cvar_SetValue(cvar_, curValue_, FROM_MENU);
}

Sound SliderWidget::KeyEvent(int key)
{
    if (IsDisabled())
        return Sound::Beep;

    switch (key) {
    case K_LEFTARROW:
    case K_KP_LEFTARROW:
    case K_MWHEELDOWN:
        curValue_ = Q_circ_clipf(curValue_ - step_, minValue_, maxValue_);
        modified_ = true;
        return Sound::Move;
    case K_RIGHTARROW:
    case K_KP_RIGHTARROW:
    case K_MWHEELUP:
        curValue_ = Q_circ_clipf(curValue_ + step_, minValue_, maxValue_);
        modified_ = true;
        return Sound::Move;
    case K_HOME:
        curValue_ = minValue_;
        modified_ = true;
        return Sound::Move;
    case K_END:
        curValue_ = maxValue_;
        modified_ = true;
        return Sound::Move;
    case K_MOUSE1: {
        int value_x = 0;
        int value_w = 0;
        ValueRect(&value_x, &value_w);
        vrect_t value_rect{ value_x, rect_.y, value_w, rect_.height };
        if (!RectContains(value_rect, uis.mouseCoords[0], uis.mouseCoords[1])) {
            dragging_ = false;
            return Sound::NotHandled;
        }
        bool changed = SetValueFromMouse(uis.mouseCoords[0], value_x, value_w);
        dragging_ = true;
        return changed ? Sound::Move : Sound::Silent;
    }
    default:
        break;
    }

    return Sound::NotHandled;
}

void SliderWidget::ValueRect(int *out_x, int *out_w) const
{
    const char *label = LabelText();
    int value_x = UseColumns() ? ColumnValueX() : rect_.x + TextWidth(label) + WidgetGap();
    int value_w = UseColumns() ? ColumnValueWidth() : rect_.x + rect_.width - value_x;
    value_w = max(1, value_w);
    if (out_x)
        *out_x = value_x;
    if (out_w)
        *out_w = value_w;
}

bool SliderWidget::SetValueFromMouse(int mx, int value_x, int value_w)
{
    if (value_w <= 0)
        return false;

    float range = maxValue_ - minValue_;
    float pos = range != 0.0f ? (static_cast<float>(mx - value_x) / value_w) : 0.0f;
    pos = Q_clipf(pos, 0.0f, 1.0f);
    float next = minValue_ + pos * range;

    float step = fabsf(step_);
    if (step > 0.0f) {
        float snapped = roundf((next - minValue_) / step);
        next = minValue_ + snapped * step;
    }

    next = Q_clipf(next, minValue_, maxValue_);
    if (next != curValue_) {
        curValue_ = next;
        modified_ = true;
        return true;
    }
    return false;
}

bool SliderWidget::HandleMouseDrag(int mx, int my, bool mouseDown)
{
    (void)my;
    if (IsDisabled()) {
        dragging_ = false;
        return false;
    }
    if (!mouseDown) {
        dragging_ = false;
        return false;
    }
    int value_x = 0;
    int value_w = 0;
    ValueRect(&value_x, &value_w);
    vrect_t value_rect{ value_x, rect_.y, value_w, rect_.height };
    if (!dragging_) {
        if (!RectContains(value_rect, mx, my))
            return false;
        dragging_ = true;
    }
    SetValueFromMouse(mx, value_x, value_w);
    return true;
}

void SliderWidget::Draw(bool focused) const
{
    bool disabled = IsDisabled();
    color_t color = disabled ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled)
        color = uis.color.active;

    int label_flags = UI_LEFT;
    if (!focused && !disabled)
        label_flags |= UI_ALTCOLOR;
    const char *label = LabelText();
    int text_y = WidgetTextY(rect_.y, rect_.height);
    if (label && *label) {
        if (UseColumns()) {
            clipRect_t clip{};
            clip.left = rect_.x;
            clip.right = rect_.x + max(1, ColumnLabelWidth()) - 1;
            clip.top = rect_.y;
            clip.bottom = rect_.y + rect_.height;
            R_SetClipRect(&clip);
        }
        UI_DrawString(rect_.x, text_y, label_flags, color, label);
        if (UseColumns())
            R_SetClipRect(NULL);
    }

    int value_x = UseColumns() ? ColumnValueX() : rect_.x + TextWidth(label) + WidgetGap();
    int value_w = UseColumns() ? ColumnValueWidth() : rect_.x + rect_.width - value_x;
    value_w = max(1, value_w);
    int track_h = max(6, rect_.height / 3);
    int track_y = rect_.y + (rect_.height - track_h) / 2;
    DrawControlBox(value_x, track_y, value_w, track_h, focused, disabled);

    float range = maxValue_ - minValue_;
    float pos = (range != 0.0f) ? ((curValue_ - minValue_) / range) : 0.0f;
    pos = Q_clipf(pos, 0.0f, 1.0f);
    int inner_w = max(0, value_w - 2);
    int inner_h = max(1, track_h - 2);

    bool rainbow = false;
    if (cvar_ && cvar_->name) {
        if (!Q_stricmp(cvar_->name, "cl_crosshair_color") ||
            !Q_stricmp(cvar_->name, "cl_crosshair_hit_color")) {
            rainbow = true;
        }
    }
    if (!rainbow) {
        float step = fabsf(step_);
        if (step > 0.0f &&
            fabsf(minValue_ - 1.0f) < 0.001f &&
            fabsf(maxValue_ - 26.0f) < 0.001f &&
            fabsf(step - 1.0f) < 0.001f) {
            rainbow = true;
        }
    }

    if (rainbow && inner_w > 0) {
        for (int i = 0; i < static_cast<int>(q_countof(kRainbowColors)); i++) {
            float start = static_cast<float>(i) / q_countof(kRainbowColors);
            float end = static_cast<float>(i + 1) / q_countof(kRainbowColors);
            int seg_x = value_x + 1 + Q_rint(inner_w * start);
            int seg_end = value_x + 1 + Q_rint(inner_w * end);
            int seg_w = max(1, seg_end - seg_x);
            color_t seg = kRainbowColors[i];
            if (disabled)
                seg = COLOR_SETA_U8(seg, 100);
            R_DrawFill32(seg_x, track_y + 1, seg_w, inner_h, seg);
        }
    } else {
        int fill_w = Q_rint(inner_w * pos);
        if (fill_w > 0) {
            color_t fill = COLOR_SETA_U8(uis.color.active, disabled ? 90 : 180);
            R_DrawFill32(value_x + 1, track_y + 1, fill_w, inner_h, fill);
        }
    }

    float step = step_ != 0.0f ? step_ : ((maxValue_ - minValue_) / SLIDER_RANGE);
    float step_abs = fabsf(step);
    float range_abs = fabsf(maxValue_ - minValue_);
    int tick_count = 0;
    if (step_abs > 0.0f)
        tick_count = Q_rint(range_abs / step_abs) + 1;
    if (tick_count > 1 && inner_w > 0) {
        int tick_h = max(2, track_h / 3);
        int tick_y = track_y + (track_h - tick_h) / 2;
        color_t tick = COLOR_SETA_U8(uis.color.selection, disabled ? 80 : 180);
        int max_ticks = max(2, min(tick_count, 128));
        for (int i = 0; i < max_ticks; i++) {
            float frac = (max_ticks > 1) ? (static_cast<float>(i) / (max_ticks - 1)) : 0.0f;
            int tick_x = value_x + 1 + Q_rint(inner_w * frac);
            R_DrawFill32(tick_x, tick_y, 1, tick_h, tick);
        }
    }

    bool hovered = !disabled && RectContains(rect_, uis.mouseCoords[0], uis.mouseCoords[1]);
    unsigned now = uis.realtime;
    float dt = hoverTime_ ? (now - hoverTime_) * 0.001f : 0.0f;
    hoverTime_ = now;
    float target = hovered ? 1.0f : 0.0f;
    float step_t = Q_clipf(dt * 8.0f, 0.0f, 1.0f);
    hoverFrac_ += (target - hoverFrac_) * step_t;

    float scale = 1.0f + hoverFrac_ * 0.15f;
    int knob_w = max(6, track_h + 2);
    if (knob_w > value_w)
        knob_w = value_w;
    int knob_h = max(track_h + 4, track_h);
    if (knob_h > rect_.height)
        knob_h = rect_.height;
    knob_w = Q_rint(knob_w * scale);
    knob_h = Q_rint(knob_h * scale);
    if (knob_w > value_w)
        knob_w = value_w;
    if (knob_h > rect_.height)
        knob_h = rect_.height;
    int knob_x = value_x + Q_rint((value_w - knob_w) * pos);
    int knob_y = rect_.y + (rect_.height - knob_h) / 2;

    color_t thumb_fill = COLOR_SETA_U8(uis.color.active, disabled ? 90 : 220);
    if (rainbow) {
        int index = Q_clip(Q_rint(curValue_), 1, static_cast<int>(q_countof(kRainbowColors)));
        thumb_fill = kRainbowColors[index - 1];
        if (disabled)
            thumb_fill = COLOR_SETA_U8(thumb_fill, 110);
    }

    float lighten = hoverFrac_ * 0.3f;
    if (lighten > 0.0f) {
        thumb_fill.r = Q_rint(thumb_fill.r + (255 - thumb_fill.r) * lighten);
        thumb_fill.g = Q_rint(thumb_fill.g + (255 - thumb_fill.g) * lighten);
        thumb_fill.b = Q_rint(thumb_fill.b + (255 - thumb_fill.b) * lighten);
    }

    color_t knob_border = disabled
        ? COLOR_SETA_U8(uis.color.disabled, 160)
        : COLOR_SETA_U8(uis.color.active, 220);
    DrawPanel(knob_x, knob_y, knob_w, knob_h, thumb_fill, knob_border, 1);
}

SpinWidget::SpinWidget(std::string label, cvar_t *cvar, SpinType type)
    : cvar_(cvar)
    , type_(type)
{
    label_ = std::move(label);
    selectable_ = true;
}

void SpinWidget::AddOption(std::string label)
{
    labels_.push_back(std::move(label));
}

void SpinWidget::AddOption(std::string label, std::string value)
{
    labels_.push_back(std::move(label));
    values_.push_back(std::move(value));
}

void SpinWidget::ClearOptions()
{
    labels_.clear();
    values_.clear();
    indices_.clear();
    curValue_ = -1;
}

void SpinWidget::SetBitfield(int mask, bool negate)
{
    mask_ = mask;
    negate_ = negate;
}

void SpinWidget::SetIndices(std::vector<int> indices)
{
    indices_ = std::move(indices);
}

void SpinWidget::OnOpen()
{
    SyncFromCvar();
}

void SpinWidget::OnClose()
{
    ApplyToCvar();
}

void SpinWidget::Advance(int dir)
{
    if (labels_.empty())
        return;

    curValue_ += dir;
    if (curValue_ < 0)
        curValue_ = static_cast<int>(labels_.size()) - 1;
    else if (curValue_ >= static_cast<int>(labels_.size()))
        curValue_ = 0;
}

Sound SpinWidget::KeyEvent(int key)
{
    if (IsDisabled())
        return Sound::Beep;

    switch (key) {
    case K_LEFTARROW:
    case K_KP_LEFTARROW:
    case K_MWHEELDOWN:
        Advance(-1);
        return Sound::Move;
    case K_RIGHTARROW:
    case K_KP_RIGHTARROW:
    case K_MWHEELUP:
        Advance(1);
        return Sound::Move;
    case K_ENTER:
    case K_KP_ENTER:
        Advance(1);
        return Sound::Move;
    default:
        break;
    }
    return Sound::NotHandled;
}

std::string SpinWidget::CurrentLabel() const
{
    if (curValue_ < 0 || curValue_ >= static_cast<int>(labels_.size()))
        return "???";
    return labels_[curValue_];
}

void SpinWidget::Draw(bool focused) const
{
    bool disabled = IsDisabled();
    color_t color = disabled ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled)
        color = uis.color.active;

    int label_flags = UI_LEFT;
    if (!focused && !disabled)
        label_flags |= UI_ALTCOLOR;
    const char *label = LabelText();
    int text_y = WidgetTextY(rect_.y, rect_.height);
    if (label && *label) {
        if (UseColumns()) {
            clipRect_t clip{};
            clip.left = rect_.x;
            clip.right = rect_.x + max(1, ColumnLabelWidth()) - 1;
            clip.top = rect_.y;
            clip.bottom = rect_.y + rect_.height;
            R_SetClipRect(&clip);
        }
        UI_DrawString(rect_.x, text_y, label_flags, color, label);
        if (UseColumns())
            R_SetClipRect(NULL);
    }

    int value_x = UseColumns() ? ColumnValueX() : rect_.x + TextWidth(label) + WidgetGap();
    int value_w = UseColumns() ? ColumnValueWidth() : rect_.x + rect_.width - value_x;
    value_w = max(1, value_w);
    int box_h = min(rect_.height, max(8, WidgetTextHeight() + 6));
    int box_y = rect_.y + (rect_.height - box_h) / 2;
    DrawControlBox(value_x, box_y, value_w, box_h, focused, disabled);

    int padding = WidgetPadding();
    int arrow_area = (value_w >= CONCHAR_WIDTH * 6) ? (CONCHAR_WIDTH * 2) : 0;
    int text_left = value_x + padding;
    int text_right = value_x + value_w - padding - arrow_area;
    if (text_right > text_left) {
        clipRect_t clip{};
        clip.left = text_left;
        clip.right = text_right - 1;
        clip.top = box_y;
        clip.bottom = box_y + box_h;
        R_SetClipRect(&clip);
        UI_DrawString(text_left, text_y, UI_LEFT, color, CurrentLabel().c_str());
        R_SetClipRect(NULL);
    }

    if (arrow_area > 0) {
        int divider_x = value_x + value_w - arrow_area;
        color_t divider = COLOR_SETA_U8(uis.color.selection, 160);
        R_DrawFill32(divider_x, box_y + 1, 1, max(1, box_h - 2), divider);

        int arrow_y = text_y;
        UI_DrawChar(divider_x + (CONCHAR_WIDTH / 2), arrow_y, UI_LEFT, color, '<');
        UI_DrawChar(divider_x + CONCHAR_WIDTH + (CONCHAR_WIDTH / 2), arrow_y, UI_LEFT, color, '>');
    }
}

void SpinWidget::SyncFromCvar()
{
    if (!cvar_)
        return;

    switch (type_) {
    case SpinType::Index:
    case SpinType::Episode:
        curValue_ = cvar_->integer;
        if (curValue_ < 0 || curValue_ >= static_cast<int>(labels_.size()))
            curValue_ = -1;
        break;
    case SpinType::Unit: {
        int index = cvar_->integer;
        curValue_ = -1;
        for (size_t i = 0; i < indices_.size(); i++) {
            if (indices_[i] == index) {
                curValue_ = static_cast<int>(i);
                break;
            }
        }
        break;
    }
    case SpinType::String:
        curValue_ = -1;
        for (size_t i = 0; i < labels_.size(); i++) {
            if (!Q_stricmp(labels_[i].c_str(), cvar_->string)) {
                curValue_ = static_cast<int>(i);
                break;
            }
        }
        break;
    case SpinType::Pair:
        curValue_ = -1;
        for (size_t i = 0; i < values_.size(); i++) {
            if (!Q_stricmp(values_[i].c_str(), cvar_->string)) {
                curValue_ = static_cast<int>(i);
                break;
            }
        }
        break;
    case SpinType::Toggle:
        if (cvar_->integer == 0 || cvar_->integer == 1)
            curValue_ = cvar_->integer ^ (negate_ ? 1 : 0);
        else
            curValue_ = -1;
        break;
    case SpinType::Bitfield:
        curValue_ = (cvar_->integer & mask_) ? (1 ^ (negate_ ? 1 : 0)) : (0 ^ (negate_ ? 1 : 0));
        break;
    }
}

void SpinWidget::ApplyToCvar()
{
    if (!cvar_)
        return;

    switch (type_) {
    case SpinType::Index:
    case SpinType::Episode:
        if (curValue_ >= 0 && curValue_ < static_cast<int>(labels_.size()))
            Cvar_SetInteger(cvar_, curValue_, FROM_MENU);
        break;
    case SpinType::Unit:
        if (curValue_ >= 0 && curValue_ < static_cast<int>(indices_.size()))
            Cvar_SetInteger(cvar_, indices_[curValue_], FROM_MENU);
        break;
    case SpinType::String:
        if (curValue_ >= 0 && curValue_ < static_cast<int>(labels_.size()))
            Cvar_SetByVar(cvar_, labels_[curValue_].c_str(), FROM_MENU);
        break;
    case SpinType::Pair:
        if (curValue_ >= 0 && curValue_ < static_cast<int>(values_.size()))
            Cvar_SetByVar(cvar_, values_[curValue_].c_str(), FROM_MENU);
        break;
    case SpinType::Toggle:
        if (curValue_ == 0 || curValue_ == 1)
            Cvar_SetInteger(cvar_, curValue_ ^ (negate_ ? 1 : 0), FROM_MENU);
        break;
    case SpinType::Bitfield: {
        int val = cvar_->integer;
        if (curValue_ ^ (negate_ ? 1 : 0))
            val |= mask_;
        else
            val &= ~mask_;
        Cvar_SetInteger(cvar_, val, FROM_MENU);
        break;
    }
    }
}

DropdownWidget::DropdownWidget(std::string label, cvar_t *cvar, SpinType type)
    : SpinWidget(std::move(label), cvar, type)
{
}

void DropdownWidget::Draw(bool focused) const
{
    bool disabled = IsDisabled();
    color_t color = disabled ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled)
        color = uis.color.active;

    int label_flags = UI_LEFT;
    if (!focused && !disabled)
        label_flags |= UI_ALTCOLOR;
    const char *label = LabelText();
    int text_y = WidgetTextY(rect_.y, rect_.height);
    if (label && *label) {
        if (UseColumns()) {
            clipRect_t clip{};
            clip.left = rect_.x;
            clip.right = rect_.x + max(1, ColumnLabelWidth()) - 1;
            clip.top = rect_.y;
            clip.bottom = rect_.y + rect_.height;
            R_SetClipRect(&clip);
        }
        UI_DrawString(rect_.x, text_y, label_flags, color, label);
        if (UseColumns())
            R_SetClipRect(NULL);
    }

    int value_x = ValueX(label);
    int value_w = ValueWidth(value_x);
    value_w = max(1, value_w);
    int box_h = min(rect_.height, max(8, WidgetTextHeight() + 6));
    int box_y = rect_.y + (rect_.height - box_h) / 2;
    DrawControlBox(value_x, box_y, value_w, box_h, focused, disabled);

    int padding = WidgetPadding();
    int arrow_area = min(CONCHAR_WIDTH * 2, max(0, value_w - 2));
    if (arrow_area < CONCHAR_WIDTH)
        arrow_area = 0;
    int text_left = value_x + padding;
    int text_right = value_x + value_w - padding - arrow_area;
    std::string value = CurrentLabel();
    color_t box_base = disabled ? uis.color.disabled : uis.color.normal;
    color_t box_fill = COLOR_SETA_U8(box_base, disabled ? 70 : 110);
    color_t value_color = disabled ? uis.color.disabled : WidgetContrastText(box_fill);
    if (text_right > text_left) {
        clipRect_t clip{};
        clip.left = text_left;
        clip.right = text_right - 1;
        clip.top = box_y;
        clip.bottom = box_y + box_h;
        R_SetClipRect(&clip);
        UI_DrawString(text_left, text_y, UI_LEFT, value_color, value.c_str());
        R_SetClipRect(NULL);
    }

    if (arrow_area > 0) {
        int divider_x = value_x + value_w - arrow_area;
        color_t divider = COLOR_SETA_U8(uis.color.selection, 160);
        R_DrawFill32(divider_x, box_y + 1, 1, max(1, box_h - 2), divider);

        int arrow_x = divider_x + (arrow_area - CONCHAR_WIDTH) / 2;
        UI_DrawChar(arrow_x, text_y, UI_LEFT, color, expanded_ ? '^' : 'v');
    }
}

void DropdownWidget::DrawOverlay() const
{
    if (!expanded_ || labels_.empty())
        return;

    int row_height = max(1, rect_.height);
    int visible = ComputeVisibleCount(row_height);
    if (visible <= 0)
        return;

    int scroll_width = 0;
    vrect_t list_rect = ComputeListRect(visible, row_height, &scroll_width);
    color_t list_fill = COLOR_SETA_U8(uis.color.normal, 255);
    color_t list_border = COLOR_SETA_U8(uis.color.selection, 255);
    DrawPanel(list_rect.x, list_rect.y, list_rect.width, list_rect.height,
              list_fill, list_border, 1);

    int inner_x = list_rect.x + 1;
    int inner_y = list_rect.y + 1;
    int inner_w = list_rect.width - 2 - scroll_width;
    int text_h = WidgetTextHeight();
    int list_padding = WidgetPadding();
    int total = static_cast<int>(labels_.size());
    int max_start = max(0, total - visible);
    int draw_start = Q_clip(listStart_, 0, max_start);
    int draw_cursor = Q_clip(listCursor_, 0, max(0, total - 1));

    for (int i = 0; i < visible; i++) {
        int index = draw_start + i;
        if (index >= total)
            break;
        int row_y = inner_y + i * row_height;
        if (index == draw_cursor) {
            color_t row_fill = COLOR_SETA_U8(uis.color.selection, 255);
            color_t row_border = COLOR_SETA_U8(uis.color.active, 255);
            DrawPanel(inner_x, row_y, inner_w, row_height, row_fill, row_border, 1);
        } else if (i & 1) {
            color_t shade = COLOR_SETA_U8(uis.color.normal, 255);
            R_DrawFill32(inner_x, row_y, inner_w, row_height, shade);
        }

        color_t row_fill = COLOR_SETA_U8(uis.color.normal, 255);
        if (index == draw_cursor)
            row_fill = COLOR_SETA_U8(uis.color.selection, 255);
        else if (i & 1)
            row_fill = COLOR_SETA_U8(uis.color.normal, 255);

        color_t text_color = WidgetContrastText(row_fill);
        int row_text_y = row_y + (row_height - text_h) / 2;
        clipRect_t clip{};
        clip.left = inner_x + list_padding;
        clip.right = inner_x + max(1, inner_w - list_padding) - 1;
        clip.top = row_y;
        clip.bottom = row_y + row_height;
        R_SetClipRect(&clip);
        UI_DrawString(inner_x + list_padding, row_text_y, UI_LEFT, text_color,
                      labels_[index].c_str());
        R_SetClipRect(NULL);
    }

    if (scroll_width > 0) {
        int bar_x = list_rect.x + list_rect.width - scroll_width;
        int bar_y = list_rect.y + 1;
        int bar_h = list_rect.height - 2;
        color_t track = COLOR_SETA_U8(uis.color.normal, 255);
        color_t track_border = COLOR_SETA_U8(uis.color.selection, 255);
        DrawPanel(bar_x, bar_y, scroll_width, bar_h, track, track_border, 1);

        float page_frac = static_cast<float>(visible) / static_cast<float>(total);
        float start_frac = static_cast<float>(draw_start) / max(1, total - visible);
        int thumb_h = max(6, Q_rint((bar_h - 2) * page_frac));
        int thumb_y = bar_y + 1 + Q_rint((bar_h - 2 - thumb_h) * start_frac);
        color_t thumb_fill = COLOR_SETA_U8(uis.color.active, 255);
        color_t thumb_border = COLOR_SETA_U8(uis.color.active, 255);
        DrawPanel(bar_x + 1, thumb_y, max(1, scroll_width - 2), thumb_h,
                  thumb_fill, thumb_border, 1);
    }
}

Sound DropdownWidget::Activate()
{
    if (IsDisabled())
        return Sound::Beep;
    if (expanded_) {
        CloseList(true);
        return Sound::In;
    }
    OpenList();
    return Sound::In;
}

Sound DropdownWidget::KeyEvent(int key)
{
    if (IsDisabled())
        return Sound::Beep;

    if (!expanded_) {
        switch (key) {
        case K_ENTER:
        case K_KP_ENTER:
        case K_MOUSE1:
            return Sound::NotHandled;
        default:
            break;
        }
        return SpinWidget::KeyEvent(key);
    }

    int total = static_cast<int>(labels_.size());
    if (total <= 0) {
        CloseList(false);
        return Sound::Beep;
    }

    int row_height = max(1, rect_.height);
    int visible = max(1, ComputeVisibleCount(row_height));

    switch (key) {
    case K_ESCAPE:
    case K_MOUSE2:
        CloseList(false);
        return Sound::Out;
    case K_UPARROW:
    case K_KP_UPARROW:
        listCursor_ = (listCursor_ <= 0) ? (total - 1) : (listCursor_ - 1);
        curValue_ = listCursor_;
        EnsureListVisible(visible);
        return Sound::Move;
    case K_DOWNARROW:
    case K_KP_DOWNARROW:
        listCursor_ = (listCursor_ >= total - 1) ? 0 : (listCursor_ + 1);
        curValue_ = listCursor_;
        EnsureListVisible(visible);
        return Sound::Move;
    case K_LEFTARROW:
    case K_KP_LEFTARROW:
        listCursor_ = (listCursor_ <= 0) ? (total - 1) : (listCursor_ - 1);
        curValue_ = listCursor_;
        EnsureListVisible(visible);
        return Sound::Move;
    case K_RIGHTARROW:
    case K_KP_RIGHTARROW:
        listCursor_ = (listCursor_ >= total - 1) ? 0 : (listCursor_ + 1);
        curValue_ = listCursor_;
        EnsureListVisible(visible);
        return Sound::Move;
    case K_HOME:
    case K_KP_HOME:
        listCursor_ = 0;
        curValue_ = listCursor_;
        EnsureListVisible(visible);
        return Sound::Move;
    case K_END:
    case K_KP_END:
        listCursor_ = total - 1;
        curValue_ = listCursor_;
        EnsureListVisible(visible);
        return Sound::Move;
    case K_PGUP:
    case K_KP_PGUP:
        listCursor_ = max(0, listCursor_ - (visible - 1));
        curValue_ = listCursor_;
        EnsureListVisible(visible);
        return Sound::Move;
    case K_PGDN:
    case K_KP_PGDN:
        listCursor_ = min(total - 1, listCursor_ + (visible - 1));
        curValue_ = listCursor_;
        EnsureListVisible(visible);
        return Sound::Move;
    case K_MWHEELUP:
        listCursor_ = max(0, listCursor_ - 1);
        curValue_ = listCursor_;
        EnsureListVisible(visible);
        return Sound::Silent;
    case K_MWHEELDOWN:
        listCursor_ = min(total - 1, listCursor_ + 1);
        curValue_ = listCursor_;
        EnsureListVisible(visible);
        return Sound::Silent;
    case K_ENTER:
    case K_KP_ENTER:
        CloseList(true);
        return Sound::In;
    case K_MOUSE1: {
        int scroll_width = 0;
        vrect_t list_rect = ComputeListRect(visible, row_height, &scroll_width);
        if (scroll_width > 0) {
            int bar_x = list_rect.x + list_rect.width - scroll_width;
            int bar_y = list_rect.y + 1;
            int bar_h = list_rect.height - 2;
            if (uis.mouseCoords[0] >= bar_x && uis.mouseCoords[0] < list_rect.x + list_rect.width &&
                uis.mouseCoords[1] >= bar_y && uis.mouseCoords[1] < bar_y + bar_h) {
                int max_start = max(0, total - visible);
                if (max_start > 0) {
                    int track_h = max(1, bar_h - 2);
                    float frac = static_cast<float>(uis.mouseCoords[1] - (bar_y + 1)) / track_h;
                    int new_start = Q_rint(frac * max_start);
                    listStart_ = Q_clip(new_start, 0, max_start);
                    int min_cursor = listStart_;
                    int max_cursor = min(total - 1, listStart_ + visible - 1);
                    listCursor_ = Q_clip(listCursor_, min_cursor, max_cursor);
                    curValue_ = listCursor_;
                    return Sound::Move;
                }
            }
        }

        int hit = HitTestList(uis.mouseCoords[0], uis.mouseCoords[1], list_rect,
                              row_height, visible, scroll_width);
        if (hit >= 0) {
            listCursor_ = hit;
            curValue_ = listCursor_;
            CloseList(true);
            return Sound::In;
        }

        int value_x = ValueX(LabelText());
        int value_w = ValueWidth(value_x);
        vrect_t value_rect{};
        value_rect.x = value_x;
        value_rect.y = rect_.y;
        value_rect.width = value_w;
        value_rect.height = rect_.height;
        if (RectContains(value_rect, uis.mouseCoords[0], uis.mouseCoords[1])) {
            CloseList(true);
            return Sound::Out;
        }

        CloseList(false);
        return Sound::NotHandled;
    }
    default:
        break;
    }

    return Sound::NotHandled;
}

bool DropdownWidget::HoverAt(int mx, int my)
{
    if (!expanded_ || labels_.empty())
        return false;

    int row_height = max(1, rect_.height);
    int visible = ComputeVisibleCount(row_height);
    if (visible <= 0)
        return false;

    int scroll_width = 0;
    vrect_t list_rect = ComputeListRect(visible, row_height, &scroll_width);
    int hit = HitTestList(mx, my, list_rect, row_height, visible, scroll_width);
    if (hit < 0)
        return false;

    listCursor_ = hit;
    curValue_ = listCursor_;
    return true;
}

void DropdownWidget::OnClose()
{
    CloseList(true);
    SpinWidget::OnClose();
}

void DropdownWidget::OpenList()
{
    if (labels_.empty())
        return;
    expanded_ = true;
    openValue_ = curValue_;
    if (curValue_ < 0 || curValue_ >= static_cast<int>(labels_.size()))
        curValue_ = 0;
    listCursor_ = curValue_;
    listStart_ = 0;
    int row_height = max(1, rect_.height);
    int visible = ComputeVisibleCount(row_height);
    if (visible > 0)
        EnsureListVisible(visible);
}

void DropdownWidget::CloseList(bool accept)
{
    if (!expanded_)
        return;
    expanded_ = false;
    if (!accept)
        curValue_ = openValue_;
    openValue_ = -1;
}

void DropdownWidget::EnsureListVisible(int visibleCount)
{
    int total = static_cast<int>(labels_.size());
    if (total <= 0) {
        listStart_ = 0;
        return;
    }

    listCursor_ = Q_clip(listCursor_, 0, total - 1);
    int max_start = max(0, total - visibleCount);
    listStart_ = Q_clip(listStart_, 0, max_start);

    if (listCursor_ < listStart_)
        listStart_ = listCursor_;
    else if (listCursor_ >= listStart_ + visibleCount)
        listStart_ = listCursor_ - visibleCount + 1;
}

vrect_t DropdownWidget::ComputeListRect(int visibleCount, int rowHeight, int *out_scroll_width) const
{
    int total = static_cast<int>(labels_.size());
    int scroll_width = (total > visibleCount) ? max(6, MLIST_SCROLLBAR_WIDTH) : 0;
    if (out_scroll_width)
        *out_scroll_width = scroll_width;

    int padding = WidgetPadding();
    int max_label_w = 0;
    for (const auto &entry : labels_) {
        max_label_w = max(max_label_w, TextWidth(entry));
    }

    int value_x = ValueX(LabelText());
    int value_w = ValueWidth(value_x);
    int desired_w = max_label_w + padding * 2 + scroll_width + 2;
    int list_w = max(value_w, desired_w);
    int max_w = max(1, uis.width - WidgetGap());
    if (list_w > max_w)
        list_w = max_w;

    int list_h = visibleCount * rowHeight + 2;
    int below = uis.height - (rect_.y + rect_.height);
    int above = rect_.y;
    bool place_below = true;
    if (below < list_h && above > below)
        place_below = false;

    int list_x = value_x;
    if (list_x + list_w > uis.width)
        list_x = max(0, uis.width - list_w);
    if (list_x < 0)
        list_x = 0;
    int list_y = place_below ? (rect_.y + rect_.height) : (rect_.y - list_h);

    vrect_t rect{};
    rect.x = list_x;
    rect.y = list_y;
    rect.width = list_w;
    rect.height = list_h;
    return rect;
}

int DropdownWidget::ComputeVisibleCount(int rowHeight) const
{
    int total = static_cast<int>(labels_.size());
    if (total <= 0)
        return 0;

    constexpr int kMaxVisible = 8;
    int max_visible = min(total, kMaxVisible);
    int below = uis.height - (rect_.y + rect_.height);
    int above = rect_.y;
    int needed = max_visible * rowHeight + 2;
    int available = below;
    if (below < needed && above > below)
        available = above;

    int available_rows = max(1, available / rowHeight);
    if (max_visible > available_rows)
        max_visible = available_rows;
    return max_visible;
}

int DropdownWidget::ValueX(const char *label) const
{
    return UseColumns() ? ColumnValueX() : rect_.x + TextWidth(label) + WidgetGap();
}

int DropdownWidget::ValueWidth(int valueX) const
{
    int width = UseColumns() ? ColumnValueWidth() : rect_.x + rect_.width - valueX;
    return max(1, width);
}

int DropdownWidget::HitTestList(int mx, int my, const vrect_t &listRect,
                                int rowHeight, int visibleCount, int scrollWidth) const
{
    int list_right = listRect.x + listRect.width - scrollWidth;
    if (mx < listRect.x || mx >= list_right)
        return -1;
    if (my < listRect.y + 1 || my >= listRect.y + listRect.height - 1)
        return -1;
    int row = (my - (listRect.y + 1)) / rowHeight;
    int total = static_cast<int>(labels_.size());
    int max_start = max(0, total - visibleCount);
    int start = Q_clip(listStart_, 0, max_start);
    int index = start + row;
    if (index < 0 || index >= total)
        return -1;
    return index;
}

SwitchWidget::SwitchWidget(std::string label, cvar_t *cvar, int bit, bool negate)
    : cvar_(cvar)
    , negate_(negate)
{
    label_ = std::move(label);
    selectable_ = true;
    if (bit >= 0)
        mask_ = 1 << bit;
}

void SwitchWidget::OnOpen()
{
    SyncFromCvar();
    modified_ = false;
}

void SwitchWidget::OnClose()
{
    ApplyToCvar();
}

void SwitchWidget::SyncFromCvar()
{
    if (!cvar_)
        return;

    bool value = false;
    if (mask_) {
        value = (cvar_->integer & mask_) != 0;
    } else {
        value = cvar_->integer != 0;
    }
    value_ = negate_ ? !value : value;
}

void SwitchWidget::ApplyToCvar()
{
    if (!cvar_ || !modified_)
        return;

    bool value = negate_ ? !value_ : value_;
    if (mask_) {
        int val = cvar_->integer;
        if (value)
            val |= mask_;
        else
            val &= ~mask_;
        Cvar_SetInteger(cvar_, val, FROM_MENU);
    } else {
        Cvar_SetInteger(cvar_, value ? 1 : 0, FROM_MENU);
    }

    modified_ = false;
}

void SwitchWidget::Toggle()
{
    value_ = !value_;
    modified_ = true;
}

Sound SwitchWidget::KeyEvent(int key)
{
    if (IsDisabled())
        return Sound::Beep;

    switch (key) {
    case K_LEFTARROW:
    case K_KP_LEFTARROW:
    case K_MWHEELDOWN:
        if (value_) {
            value_ = false;
            modified_ = true;
        }
        return Sound::Move;
    case K_RIGHTARROW:
    case K_KP_RIGHTARROW:
    case K_MWHEELUP:
        if (!value_) {
            value_ = true;
            modified_ = true;
        }
        return Sound::Move;
    case K_ENTER:
    case K_KP_ENTER:
    case K_SPACE:
        Toggle();
        return Sound::Move;
    default:
        break;
    }

    return Sound::NotHandled;
}

Sound SwitchWidget::Activate()
{
    if (IsDisabled())
        return Sound::Beep;
    Toggle();
    return Sound::Move;
}

void SwitchWidget::Draw(bool focused) const
{
    bool disabled = IsDisabled();
    color_t color = disabled ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled)
        color = uis.color.active;

    int label_flags = UI_LEFT;
    if (!focused && !disabled)
        label_flags |= UI_ALTCOLOR;
    const char *label = LabelText();
    int text_y = WidgetTextY(rect_.y, rect_.height);
    if (label && *label) {
        if (UseColumns()) {
            clipRect_t clip{};
            clip.left = rect_.x;
            clip.right = rect_.x + max(1, ColumnLabelWidth()) - 1;
            clip.top = rect_.y;
            clip.bottom = rect_.y + rect_.height;
            R_SetClipRect(&clip);
        }
        UI_DrawString(rect_.x, text_y, label_flags, color, label);
        if (UseColumns())
            R_SetClipRect(NULL);
    }

    int value_x = UseColumns() ? ColumnValueX() : rect_.x + TextWidth(label) + WidgetGap();
    int value_w = UseColumns() ? ColumnValueWidth() : rect_.x + rect_.width - value_x;
    value_w = max(1, value_w);
    int control_h = min(rect_.height, max(8, WidgetTextHeight() + 6));
    int control_y = rect_.y + (rect_.height - control_h) / 2;

    if (style_ == SwitchStyle::Checkbox) {
        int box_size = max(10, min(control_h, value_w));
        int box_x = value_x;
        DrawControlBox(box_x, control_y, box_size, box_size, focused, disabled);
        if (value_) {
            int check_size = max(2, box_size - 6);
            int check_x = box_x + (box_size - check_size) / 2;
            int check_y = control_y + (box_size - check_size) / 2;
            color_t check = disabled
                ? COLOR_SETA_U8(uis.color.disabled, 160)
                : COLOR_SETA_U8(uis.color.active, 220);
            R_DrawFill32(check_x, check_y, check_size, check_size, check);
        }
        return;
    }

    int track_h = max(10, min(control_h, CONCHAR_HEIGHT));
    int track_w = max(track_h * 2, min(value_w, track_h * 3));
    int track_x = value_x;
    int track_y = rect_.y + (rect_.height - track_h) / 2;

    color_t track_fill = COLOR_SETA_U8(uis.color.normal, 120);
    if (disabled)
        track_fill = COLOR_SETA_U8(uis.color.disabled, 80);
    else if (value_)
        track_fill = COLOR_SETA_U8(uis.color.active, focused ? 200 : 160);

    color_t track_border = disabled
        ? COLOR_SETA_U8(uis.color.disabled, 140)
        : COLOR_SETA_U8(uis.color.selection, 160);
    DrawPanel(track_x, track_y, track_w, track_h, track_fill, track_border, 1);

    int knob_size = max(6, track_h - 4);
    int knob_x = value_
        ? (track_x + track_w - knob_size - 2)
        : (track_x + 2);
    int knob_y = track_y + (track_h - knob_size) / 2;
    color_t knob_fill = disabled ? COLOR_SETA_U8(uis.color.disabled, 120)
                                 : COLOR_RGBA(255, 255, 255, 220);
    DrawPanel(knob_x, knob_y, knob_size, knob_size, knob_fill, track_border, 1);
}

ProgressWidget::ProgressWidget(cvar_t *cvar, float minValue, float maxValue)
    : cvar_(cvar)
    , minValue_(minValue)
    , maxValue_(maxValue)
{
    selectable_ = false;
}

void ProgressWidget::Draw(bool focused) const
{
    (void)focused;

    float value = cvar_ ? cvar_->value : 0.0f;
    float range = maxValue_ - minValue_;
    float frac = range > 0.0f ? (value - minValue_) / range : 0.0f;
    frac = Q_clipf(frac, 0.0f, 1.0f);

    int bar_margin = WidgetPadding();
    int bar_x = rect_.x + bar_margin;
    int bar_width = rect_.width - bar_margin * 2;
    if (bar_width < 1)
        return;

    int bar_height = max(8, rect_.height / 3);
    int bar_y = rect_.y + (rect_.height - bar_height) / 2;

    DrawControlBox(bar_x, bar_y, bar_width, bar_height, false, false);

    int fill_width = Q_rint((bar_width - 2) * frac);
    if (fill_width > 0) {
        color_t fill = COLOR_SETA_U8(uis.color.active, 200);
        R_DrawFill32(bar_x + 1, bar_y + 1, fill_width, max(1, bar_height - 2), fill);
    }
}

ImageSpinWidget::ImageSpinWidget(std::string label, cvar_t *cvar, std::string path,
                                 std::string filter, int width, int height,
                                 bool numericValues, std::string valuePrefix)
    : SpinWidget(std::move(label), cvar, SpinType::String)
    , path_(std::move(path))
    , filter_(std::move(filter))
    , previewWidth_(width)
    , previewHeight_(height)
    , numericValues_(numericValues)
    , valuePrefix_(std::move(valuePrefix))
{
}

int ImageSpinWidget::Height(int lineHeight) const
{
    int preview_h = previewHeight_ > 0 ? GenericSpacing(previewHeight_) : 0;
    return max(lineHeight, preview_h);
}

void ImageSpinWidget::OnOpen()
{
    entryCount_ = 0;
    entries_ = (char **)FS_ListFiles(NULL, va("%s/%s", path_.c_str(), filter_.c_str()),
                                     FS_SEARCH_BYFILTER, &entryCount_);
    labels_.clear();

    if (entries_) {
        for (int i = 0; i < entryCount_; i++) {
            labels_.emplace_back(entries_[i]);
        }
    }

    curValue_ = -1;

    size_t path_offset = path_.size();
    if (path_offset && path_[path_offset - 1] != '/')
        path_offset++;

    int target_value = 0;
    if (numericValues_ && cvar_) {
        target_value = cvar_->integer;
        if (target_value <= 0 && cvar_->string && *cvar_->string) {
            int parsed = 0;
            if (ParseEntryValue(cvar_->string, &parsed))
                target_value = parsed;
        }
    }

    for (int i = 0; i < entryCount_; i++) {
        const char *file_value = entries_[i] + path_offset;
        const char *dot = strchr(file_value, '.');
        size_t file_len = dot ? static_cast<size_t>(dot - file_value) : strlen(file_value);
        if (numericValues_) {
            int parsed = 0;
            if (ParseEntryValue(file_value, &parsed) && parsed == target_value) {
                curValue_ = i;
                break;
            }
        } else {
            const char *val = cvar_ ? cvar_->string : "";
            size_t val_len = strlen(val);
            if (!Q_strncasecmp(val, file_value, max(val_len, file_len))) {
                curValue_ = i;
                break;
            }
        }
    }

    expanded_ = false;
    openValue_ = -1;
    listStart_ = 0;
    listCursor_ = curValue_;
}

void ImageSpinWidget::OnClose()
{
    CloseList(true);
    if (cvar_ && curValue_ >= 0 && curValue_ < entryCount_) {
        size_t path_offset = path_.size();
        if (path_offset && path_[path_offset - 1] != '/')
            path_offset++;

        const char *file_value = entries_[curValue_] + path_offset;
        const char *dot = strchr(file_value, '.');
        size_t file_len = dot ? static_cast<size_t>(dot - file_value) : strlen(file_value);
        if (numericValues_) {
            int parsed = 0;
            if (ParseEntryValue(file_value, &parsed))
                Cvar_SetInteger(cvar_, parsed, FROM_MENU);
        } else {
            Cvar_SetEx(cvar_->name, va("%.*s", static_cast<int>(file_len), file_value), FROM_MENU);
        }
    }
    if (entries_) {
        FS_FreeList((void **)entries_);
        entries_ = nullptr;
        entryCount_ = 0;
    }
}

void ImageSpinWidget::Draw(bool focused) const
{
    bool disabled = IsDisabled();
    color_t color = disabled ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled)
        color = uis.color.active;

    int header_y = rect_.y;
    int header_h = rect_.height;
    const char *label = LabelText();
    int text_y = WidgetTextY(header_y, header_h);
    if (label && *label) {
        if (UseColumns()) {
            clipRect_t clip{};
            clip.left = rect_.x;
            clip.right = rect_.x + max(1, ColumnLabelWidth()) - 1;
            clip.top = header_y;
            clip.bottom = header_y + header_h;
            R_SetClipRect(&clip);
        }
        UI_DrawString(rect_.x, text_y, UI_LEFT | (!focused && !disabled ? UI_ALTCOLOR : 0),
                      color, label);
        if (UseColumns())
            R_SetClipRect(NULL);
    }

    int value_x = ValueX(label);
    int value_w = ValueWidth(value_x);
    value_w = max(1, value_w);
    int box_h = max(1, header_h);
    int box_y = header_y;
    DrawControlBox(value_x, box_y, value_w, box_h, focused, disabled);

    int padding = WidgetPadding();
    int arrow_area = (value_w >= CONCHAR_WIDTH * 6) ? (CONCHAR_WIDTH * 2) : 0;
    int content_left = value_x + padding;
    int content_right = value_x + value_w - padding - arrow_area;
    int content_w = max(0, content_right - content_left);
    int content_h = max(0, box_h - padding * 2);

    bool drew_image = false;
    if (curValue_ >= 0 && curValue_ < entryCount_ && content_w > 0 && content_h > 0) {
        qhandle_t pic = R_RegisterTempPic(va("/%s", entries_[curValue_]));
        int w = 0;
        int h = 0;
        R_GetPicSize(&w, &h, pic);
        if (w > 0 && h > 0) {
            int target_w = previewWidth_ > 0 ? previewWidth_ : w;
            int target_h = previewHeight_ > 0 ? previewHeight_ : h;
            target_w = min(target_w, content_w);
            target_h = min(target_h, content_h);
            if (target_w > 0 && target_h > 0) {
                float scale_w = static_cast<float>(target_w) / w;
                float scale_h = static_cast<float>(target_h) / h;
                float scale = min(scale_w, scale_h);
                int draw_w = max(1, Q_rint(w * scale));
                int draw_h = max(1, Q_rint(h * scale));
                int draw_x = content_left + (content_w - draw_w) / 2;
                int draw_y = box_y + (box_h - draw_h) / 2;
                R_DrawStretchPic(draw_x, draw_y, draw_w, draw_h, COLOR_WHITE, pic);
                drew_image = true;
            }
        }
    }

    if (!drew_image && content_right > content_left) {
        color_t value_color = disabled ? uis.color.disabled : COLOR_WHITE;
        clipRect_t clip{};
        clip.left = content_left;
        clip.right = content_right - 1;
        clip.top = box_y;
        clip.bottom = box_y + box_h;
        R_SetClipRect(&clip);
        UI_DrawString(content_left, text_y, UI_LEFT, value_color, CurrentLabel().c_str());
        R_SetClipRect(NULL);
    }

    if (arrow_area > 0) {
        int divider_x = value_x + value_w - arrow_area;
        color_t divider = COLOR_SETA_U8(uis.color.selection, 160);
        R_DrawFill32(divider_x, box_y + 1, 1, max(1, box_h - 2), divider);

        UI_DrawChar(divider_x + (CONCHAR_WIDTH / 2), text_y, UI_LEFT, color, '<');
        UI_DrawChar(divider_x + CONCHAR_WIDTH + (CONCHAR_WIDTH / 2), text_y, UI_LEFT, color, '>');
    }

}

void ImageSpinWidget::DrawOverlay() const
{
    if (!expanded_ || entryCount_ <= 0)
        return;

    int tile_pad = max(2, WidgetPadding());
    int tile_w = max(1, max(previewWidth_, CONCHAR_WIDTH) + tile_pad * 2);
    int tile_h = max(1, max(previewHeight_, CONCHAR_HEIGHT) + tile_pad * 2);

    const char *label = LabelText();
    int value_x = ValueX(label);
    int value_w = ValueWidth(value_x);

    int columns = ComputeColumns(tile_w, value_w, 0);
    int total = entryCount_;
    int total_rows = max(1, (total + columns - 1) / columns);
    int visible_rows = min(total_rows, ComputeVisibleRows(tile_h));
    int scroll_width = (total_rows > visible_rows) ? max(6, MLIST_SCROLLBAR_WIDTH) : 0;
    columns = ComputeColumns(tile_w, value_w, scroll_width);
    total_rows = max(1, (total + columns - 1) / columns);
    visible_rows = min(total_rows, ComputeVisibleRows(tile_h));
    scroll_width = (total_rows > visible_rows) ? max(6, MLIST_SCROLLBAR_WIDTH) : 0;

    vrect_t list_rect = ComputeListRect(visible_rows, columns, tile_h, &scroll_width);

    color_t list_fill = COLOR_SETA_U8(uis.color.normal, 255);
    color_t list_border = COLOR_SETA_U8(uis.color.selection, 255);
    DrawPanel(list_rect.x, list_rect.y, list_rect.width, list_rect.height,
              list_fill, list_border, 1);

    int inner_x = list_rect.x + 1;
    int inner_y = list_rect.y + 1;
    int inner_w = list_rect.width - 2 - scroll_width;
    int max_start = max(0, total_rows - visible_rows);
    int draw_start = Q_clip(listStart_, 0, max_start);
    int draw_cursor = Q_clip(listCursor_, 0, total - 1);

    for (int row = 0; row < visible_rows; row++) {
        int row_index = draw_start + row;
        int row_y = inner_y + row * tile_h;
        for (int col = 0; col < columns; col++) {
            int index = row_index * columns + col;
            if (index >= total || col * tile_w >= inner_w)
                break;

            int tile_x = inner_x + col * tile_w;
            bool hovered = (index == draw_cursor);
            bool selected = (index == curValue_);
            color_t tile_fill = COLOR_SETA_U8(uis.color.normal, 255);
            color_t tile_border = COLOR_SETA_U8(uis.color.selection, 255);
            if (selected) {
                tile_fill = COLOR_SETA_U8(uis.color.selection, 255);
                tile_border = COLOR_SETA_U8(uis.color.active, 255);
            }
            if (hovered) {
                tile_border = COLOR_SETA_U8(uis.color.active, 255);
            }
            DrawPanel(tile_x, row_y, tile_w, tile_h, tile_fill, tile_border, 1);

            qhandle_t pic = R_RegisterTempPic(va("/%s", entries_[index]));
            int w = 0;
            int h = 0;
            R_GetPicSize(&w, &h, pic);
            int img_area_w = max(1, tile_w - tile_pad * 2);
            int img_area_h = max(1, tile_h - tile_pad * 2);
            if (w > 0 && h > 0 && img_area_w > 0 && img_area_h > 0) {
                int target_w = previewWidth_ > 0 ? previewWidth_ : w;
                int target_h = previewHeight_ > 0 ? previewHeight_ : h;
                target_w = min(target_w, img_area_w);
                target_h = min(target_h, img_area_h);
                if (target_w > 0 && target_h > 0) {
                    float scale_w = static_cast<float>(target_w) / w;
                    float scale_h = static_cast<float>(target_h) / h;
                    float scale = min(scale_w, scale_h);
                    int draw_w = max(1, Q_rint(w * scale));
                    int draw_h = max(1, Q_rint(h * scale));
                    int img_x = tile_x + (tile_w - draw_w) / 2;
                    int img_y = row_y + (tile_h - draw_h) / 2;
                    R_DrawStretchPic(img_x, img_y, draw_w, draw_h, COLOR_WHITE, pic);
                }
            }
        }
    }

    if (scroll_width > 0) {
        int bar_x = list_rect.x + list_rect.width - scroll_width;
        int bar_y = list_rect.y + 1;
        int bar_h = list_rect.height - 2;
        color_t track = COLOR_SETA_U8(uis.color.normal, 255);
        color_t track_border = COLOR_SETA_U8(uis.color.selection, 255);
        DrawPanel(bar_x, bar_y, scroll_width, bar_h, track, track_border, 1);

        float page_frac = static_cast<float>(visible_rows) / static_cast<float>(total_rows);
        float start_frac = static_cast<float>(draw_start) / max(1, total_rows - visible_rows);
        int thumb_h = max(6, Q_rint((bar_h - 2) * page_frac));
        int thumb_y = bar_y + 1 + Q_rint((bar_h - 2 - thumb_h) * start_frac);
        color_t thumb_fill = COLOR_SETA_U8(uis.color.active, 255);
        color_t thumb_border = COLOR_SETA_U8(uis.color.active, 255);
        DrawPanel(bar_x + 1, thumb_y, max(1, scroll_width - 2), thumb_h,
                  thumb_fill, thumb_border, 1);
    }
}

Sound ImageSpinWidget::Activate()
{
    if (IsDisabled())
        return Sound::Beep;
    if (expanded_) {
        CloseList(true);
        return Sound::In;
    }
    OpenList();
    return Sound::In;
}

Sound ImageSpinWidget::KeyEvent(int key)
{
    if (IsDisabled())
        return Sound::Beep;

    if (!expanded_) {
        switch (key) {
        case K_ENTER:
        case K_KP_ENTER:
        case K_MOUSE1:
            return Sound::NotHandled;
        default:
            break;
        }
        return SpinWidget::KeyEvent(key);
    }

    int total = entryCount_;
    if (total <= 0) {
        CloseList(false);
        return Sound::Beep;
    }

    int tile_pad = max(2, WidgetPadding());
    int tile_w = max(1, max(previewWidth_, CONCHAR_WIDTH) + tile_pad * 2);
    int tile_h = max(1, max(previewHeight_, CONCHAR_HEIGHT) + tile_pad * 2);
    int value_w = ValueWidth(ValueX(LabelText()));

    int columns = ComputeColumns(tile_w, value_w, 0);
    int total_rows = max(1, (total + columns - 1) / columns);
    int visible_rows = min(total_rows, ComputeVisibleRows(tile_h));
    int scroll_width = (total_rows > visible_rows) ? max(6, MLIST_SCROLLBAR_WIDTH) : 0;
    columns = ComputeColumns(tile_w, value_w, scroll_width);
    total_rows = max(1, (total + columns - 1) / columns);
    visible_rows = min(total_rows, ComputeVisibleRows(tile_h));
    scroll_width = (total_rows > visible_rows) ? max(6, MLIST_SCROLLBAR_WIDTH) : 0;

    switch (key) {
    case K_ESCAPE:
    case K_MOUSE2:
        CloseList(false);
        return Sound::Out;
    case K_LEFTARROW:
    case K_KP_LEFTARROW:
        listCursor_ = (listCursor_ <= 0) ? (total - 1) : (listCursor_ - 1);
        curValue_ = listCursor_;
        EnsureListVisible(visible_rows, columns);
        return Sound::Move;
    case K_RIGHTARROW:
    case K_KP_RIGHTARROW:
        listCursor_ = (listCursor_ >= total - 1) ? 0 : (listCursor_ + 1);
        curValue_ = listCursor_;
        EnsureListVisible(visible_rows, columns);
        return Sound::Move;
    case K_UPARROW:
    case K_KP_UPARROW: {
        if (listCursor_ - columns >= 0) {
            listCursor_ -= columns;
        } else {
            int col = listCursor_ % columns;
            int row = total_rows - 1;
            int index = row * columns + col;
            while (index >= total && row > 0) {
                row--;
                index = row * columns + col;
            }
            listCursor_ = max(0, min(total - 1, index));
        }
        curValue_ = listCursor_;
        EnsureListVisible(visible_rows, columns);
        return Sound::Move;
    }
    case K_DOWNARROW:
    case K_KP_DOWNARROW: {
        int next = listCursor_ + columns;
        if (next < total) {
            listCursor_ = next;
        } else {
            int col = listCursor_ % columns;
            listCursor_ = min(col, total - 1);
        }
        curValue_ = listCursor_;
        EnsureListVisible(visible_rows, columns);
        return Sound::Move;
    }
    case K_HOME:
    case K_KP_HOME:
        listCursor_ = 0;
        curValue_ = listCursor_;
        EnsureListVisible(visible_rows, columns);
        return Sound::Move;
    case K_END:
    case K_KP_END:
        listCursor_ = total - 1;
        curValue_ = listCursor_;
        EnsureListVisible(visible_rows, columns);
        return Sound::Move;
    case K_PGUP:
    case K_KP_PGUP:
        listCursor_ = max(0, listCursor_ - columns * (visible_rows - 1));
        curValue_ = listCursor_;
        EnsureListVisible(visible_rows, columns);
        return Sound::Move;
    case K_PGDN:
    case K_KP_PGDN:
        listCursor_ = min(total - 1, listCursor_ + columns * (visible_rows - 1));
        curValue_ = listCursor_;
        EnsureListVisible(visible_rows, columns);
        return Sound::Move;
    case K_MWHEELUP:
        listCursor_ = max(0, listCursor_ - columns);
        curValue_ = listCursor_;
        EnsureListVisible(visible_rows, columns);
        return Sound::Silent;
    case K_MWHEELDOWN:
        listCursor_ = min(total - 1, listCursor_ + columns);
        curValue_ = listCursor_;
        EnsureListVisible(visible_rows, columns);
        return Sound::Silent;
    case K_ENTER:
    case K_KP_ENTER:
        CloseList(true);
        return Sound::In;
    case K_MOUSE1: {
        int scroll_width_hit = 0;
        vrect_t list_rect = ComputeListRect(visible_rows, columns, tile_h, &scroll_width_hit);
        int bar_x = list_rect.x + list_rect.width - scroll_width_hit;
        int bar_y = list_rect.y + 1;
        int bar_h = list_rect.height - 2;
        if (scroll_width_hit > 0 &&
            uis.mouseCoords[0] >= bar_x && uis.mouseCoords[0] < list_rect.x + list_rect.width &&
            uis.mouseCoords[1] >= bar_y && uis.mouseCoords[1] < bar_y + bar_h) {
            int max_start = max(0, total_rows - visible_rows);
            if (max_start > 0) {
                int track_h = max(1, bar_h - 2);
                float frac = static_cast<float>(uis.mouseCoords[1] - (bar_y + 1)) / track_h;
                int new_start = Q_rint(frac * max_start);
                listStart_ = Q_clip(new_start, 0, max_start);
                int min_row = listStart_;
                int max_row = min(total_rows - 1, listStart_ + visible_rows - 1);
                int cur_row = listCursor_ / columns;
                if (cur_row < min_row || cur_row > max_row) {
                    listCursor_ = min(total - 1, listStart_ * columns);
                    curValue_ = listCursor_;
                }
                return Sound::Move;
            }
        }

        int hit = HitTestList(uis.mouseCoords[0], uis.mouseCoords[1], list_rect,
                              tile_h, visible_rows, columns, scroll_width_hit);
        if (hit >= 0) {
            listCursor_ = hit;
            curValue_ = listCursor_;
            CloseList(true);
            return Sound::In;
        }

        int value_x = ValueX(LabelText());
        int value_w = ValueWidth(value_x);
        vrect_t value_rect{};
        value_rect.x = value_x;
        value_rect.y = rect_.y;
        value_rect.width = value_w;
        value_rect.height = rect_.height;
        if (RectContains(value_rect, uis.mouseCoords[0], uis.mouseCoords[1])) {
            CloseList(true);
            return Sound::Out;
        }

        CloseList(false);
        return Sound::NotHandled;
    }
    default:
        break;
    }

    return Sound::NotHandled;
}

bool ImageSpinWidget::HoverAt(int mx, int my)
{
    if (!expanded_ || entryCount_ <= 0)
        return false;

    int tile_pad = max(2, WidgetPadding());
    int tile_w = max(1, max(previewWidth_, CONCHAR_WIDTH) + tile_pad * 2);
    int tile_h = max(1, max(previewHeight_, CONCHAR_HEIGHT) + tile_pad * 2);
    int value_w = ValueWidth(ValueX(LabelText()));

    int columns = ComputeColumns(tile_w, value_w, 0);
    int total_rows = max(1, (entryCount_ + columns - 1) / columns);
    int visible_rows = min(total_rows, ComputeVisibleRows(tile_h));
    int scroll_width = (total_rows > visible_rows) ? max(6, MLIST_SCROLLBAR_WIDTH) : 0;
    columns = ComputeColumns(tile_w, value_w, scroll_width);
    total_rows = max(1, (entryCount_ + columns - 1) / columns);
    visible_rows = min(total_rows, ComputeVisibleRows(tile_h));
    scroll_width = (total_rows > visible_rows) ? max(6, MLIST_SCROLLBAR_WIDTH) : 0;

    vrect_t list_rect = ComputeListRect(visible_rows, columns, tile_h, &scroll_width);
    int hit = HitTestList(mx, my, list_rect, tile_h, visible_rows, columns, scroll_width);
    if (hit < 0)
        return false;

    listCursor_ = hit;
    curValue_ = listCursor_;
    return true;
}

void ImageSpinWidget::OpenList()
{
    if (entryCount_ <= 0)
        return;

    expanded_ = true;
    openValue_ = curValue_;
    if (curValue_ < 0 || curValue_ >= entryCount_)
        curValue_ = 0;
    listCursor_ = curValue_;
    listStart_ = 0;

    int tile_pad = max(2, WidgetPadding());
    int tile_w = max(1, max(previewWidth_, CONCHAR_WIDTH) + tile_pad * 2);
    int tile_h = max(1, max(previewHeight_, CONCHAR_HEIGHT) + tile_pad * 2);
    int value_w = ValueWidth(ValueX(LabelText()));
    int columns = ComputeColumns(tile_w, value_w, 0);
    int total_rows = max(1, (entryCount_ + columns - 1) / columns);
    int visible_rows = min(total_rows, ComputeVisibleRows(tile_h));
    if (visible_rows > 0)
        EnsureListVisible(visible_rows, columns);
}

void ImageSpinWidget::CloseList(bool accept)
{
    if (!expanded_)
        return;

    expanded_ = false;
    if (!accept)
        curValue_ = openValue_;
    openValue_ = -1;
}

void ImageSpinWidget::EnsureListVisible(int visibleRows, int columns)
{
    if (entryCount_ <= 0) {
        listStart_ = 0;
        return;
    }

    listCursor_ = Q_clip(listCursor_, 0, entryCount_ - 1);
    if (columns < 1)
        columns = 1;
    int total_rows = max(1, (entryCount_ + columns - 1) / columns);
    int max_start = max(0, total_rows - visibleRows);
    listStart_ = Q_clip(listStart_, 0, max_start);

    int row = listCursor_ / columns;
    if (row < listStart_)
        listStart_ = row;
    else if (row >= listStart_ + visibleRows)
        listStart_ = row - visibleRows + 1;
}

int ImageSpinWidget::ValueX(const char *label) const
{
    return UseColumns() ? ColumnValueX() : rect_.x + TextWidth(label) + WidgetGap();
}

int ImageSpinWidget::ValueWidth(int valueX) const
{
    int width = UseColumns() ? ColumnValueWidth() : rect_.x + rect_.width - valueX;
    return max(1, width);
}

int ImageSpinWidget::ComputeColumns(int tileWidth, int valueWidth, int scrollWidth) const
{
    int max_width = max(1, uis.width - WidgetGap());
    int max_columns = 3;
    if (previewWidth_ > 0 && previewHeight_ > 0) {
        float ratio = static_cast<float>(previewWidth_) / max(1, previewHeight_);
        if (ratio >= 1.5f)
            max_columns = 1;
    }
    if (entryCount_ > 0)
        max_columns = min(max_columns, entryCount_);

    int preferred_w = tileWidth * max_columns + scrollWidth + 2;
    int max_list_w = min(max_width, max(valueWidth, preferred_w));
    int available = max_list_w - scrollWidth - 2;
    if (available < tileWidth)
        available = tileWidth;
    int columns = max(1, available / tileWidth);
    columns = min(columns, max_columns);
    return max(1, columns);
}

int ImageSpinWidget::ComputeVisibleRows(int rowHeight) const
{
    if (entryCount_ <= 0)
        return 0;

    constexpr int kMaxVisibleRows = 4;
    int max_visible = kMaxVisibleRows;
    int below = uis.height - (rect_.y + rect_.height);
    int above = rect_.y;
    int needed = max_visible * rowHeight + 2;
    int available = below;
    if (below < needed && above > below)
        available = above;

    int available_rows = max(1, available / rowHeight);
    if (max_visible > available_rows)
        max_visible = available_rows;
    return max_visible;
}

vrect_t ImageSpinWidget::ComputeListRect(int visibleRows, int columns, int rowHeight,
                                         int *out_scroll_width) const
{
    int total_rows = max(1, (entryCount_ + columns - 1) / columns);
    int scroll_width = (total_rows > visibleRows) ? max(6, MLIST_SCROLLBAR_WIDTH) : 0;
    if (out_scroll_width)
        *out_scroll_width = scroll_width;

    int tile_pad = max(2, WidgetPadding());
    int tile_w = max(1, max(previewWidth_, CONCHAR_WIDTH) + tile_pad * 2);
    int list_w = columns * tile_w + scroll_width + 2;
    int max_w = max(1, uis.width - WidgetGap());
    if (list_w > max_w)
        list_w = max_w;

    int list_h = visibleRows * rowHeight + 2;
    int below = uis.height - (rect_.y + rect_.height);
    int above = rect_.y;
    bool place_below = true;
    if (below < list_h && above > below)
        place_below = false;

    const char *label = LabelText();
    int value_x = ValueX(label);
    int list_x = value_x;
    if (list_x + list_w > uis.width)
        list_x = max(0, uis.width - list_w);
    if (list_x < 0)
        list_x = 0;
    int list_y = place_below ? (rect_.y + rect_.height) : (rect_.y - list_h);

    vrect_t rect{};
    rect.x = list_x;
    rect.y = list_y;
    rect.width = list_w;
    rect.height = list_h;
    return rect;
}

int ImageSpinWidget::HitTestList(int mx, int my, const vrect_t &listRect, int rowHeight,
                                 int visibleRows, int columns, int scrollWidth) const
{
    int tile_pad = max(2, WidgetPadding());
    int tile_w = max(1, max(previewWidth_, CONCHAR_WIDTH) + tile_pad * 2);
    int inner_x = listRect.x + 1;
    int inner_y = listRect.y + 1;
    int inner_w = listRect.width - 2 - scrollWidth;
    int inner_h = listRect.height - 2;
    if (mx < inner_x || mx >= inner_x + inner_w)
        return -1;
    if (my < inner_y || my >= inner_y + inner_h)
        return -1;

    int col = (mx - inner_x) / tile_w;
    int row = (my - inner_y) / rowHeight;
    if (col < 0 || col >= columns || row < 0 || row >= visibleRows)
        return -1;
    if (col * tile_w >= inner_w)
        return -1;

    int index = (listStart_ + row) * columns + col;
    if (index < 0 || index >= entryCount_)
        return -1;
    return index;
}

bool ImageSpinWidget::ParseEntryValue(const char *file_value, int *out_value) const
{
    if (!numericValues_ || !file_value || !*file_value)
        return false;

    const char *dot = strchr(file_value, '.');
    size_t file_len = dot ? static_cast<size_t>(dot - file_value) : strlen(file_value);
    if (!file_len)
        return false;

    char name_buf[MAX_QPATH];
    size_t copy_len = min(file_len, sizeof(name_buf) - 1);
    memcpy(name_buf, file_value, copy_len);
    name_buf[copy_len] = '\0';

    const char *scan = name_buf;
    if (!valuePrefix_.empty()) {
        size_t prefix_len = valuePrefix_.size();
        if (copy_len < prefix_len)
            return false;
        if (Q_strncasecmp(scan, valuePrefix_.c_str(), prefix_len) != 0)
            return false;
        scan += prefix_len;
    } else {
        while (*scan && !Q_isdigit(*scan))
            scan++;
    }

    if (!*scan || !Q_isdigit(*scan))
        return false;

    int parsed = atoi(scan);
    if (out_value)
        *out_value = parsed;
    return true;
}

FieldWidget::FieldWidget(std::string label, cvar_t *cvar, int width, bool center,
                         bool numeric, bool integer)
    : cvar_(cvar)
    , width_(width)
    , center_(center)
    , numeric_(numeric)
    , integer_(integer)
{
    label_ = std::move(label);
    selectable_ = true;
}

void FieldWidget::Layout(int x, int y, int width, int lineHeight)
{
    rect_.y = y;
    rect_.width = width;
    rect_.height = Height(lineHeight);
    rect_.x = center_ ? x - width / 2 : x;

    int draw_x = center_ ? rect_.x + rect_.width / 2 : rect_.x;
    int value_x = draw_x;
    const char *label = LabelText();
    if (label && *label && !center_) {
        value_x = UseColumns() ? ColumnValueX() : (value_x + TextWidth(label) + WidgetGap());
    }

    int box_w = InputFieldBoxWidth(width_, false);
    if (!center_) {
        int max_w = UseColumns() ? ColumnValueWidth() : (rect_.x + rect_.width - value_x);
        if (max_w > 0)
            box_w = min(box_w, max_w);
    }

    int inner_w = max(1, box_w - WidgetPadding() * 2);
    UpdateInputFieldVisibleChars(&field_, inner_w);
}

void FieldWidget::OnOpen()
{
    IF_Init(&field_, width_, MAX_FIELD_TEXT);
    if (cvar_)
        IF_Replace(&field_, cvar_->string);
}

void FieldWidget::OnClose()
{
    if (cvar_)
        Cvar_SetByVar(cvar_, field_.text, FROM_MENU);
}

Sound FieldWidget::KeyEvent(int key)
{
    if (IsDisabled())
        return Sound::Beep;

    if (key == K_MOUSE1) {
        int draw_x = center_ ? rect_.x + rect_.width / 2 : rect_.x;
        int x = draw_x;
        const char *label = LabelText();
        if (label && *label && !center_) {
            x = UseColumns() ? ColumnValueX() : (x + TextWidth(label) + WidgetGap());
        }

        int box_w = InputFieldBoxWidth(width_, false);
        if (!center_) {
            int max_w = UseColumns() ? ColumnValueWidth() : (rect_.x + rect_.width - x);
            if (max_w > 0)
                box_w = min(box_w, max_w);
        }
        int box_x = center_ ? (x - box_w / 2) : x;
        int text_h = WidgetTextHeight();
        int target_h = max(8, text_h + 6);
        int box_h = min(rect_.height, target_h);
        if (box_h < 6)
            box_h = max(1, rect_.height);
        int box_y = rect_.y + (rect_.height - box_h) / 2;
        vrect_t box_rect{ box_x, box_y, box_w, box_h };

        if (RectContains(box_rect, uis.mouseCoords[0], uis.mouseCoords[1])) {
            int text_x = box_x + WidgetPadding();
            int text_w = max(1, box_w - WidgetPadding() * 2);
            size_t new_pos = InputFieldCursorFromMouse(&field_, uis.mouseCoords[0], text_x, text_w);
            IF_SetCursor(&field_, new_pos, Key_IsDown(K_SHIFT));
            return Sound::Move;
        }
    }

    bool handled = IF_KeyEvent(&field_, key);
    return handled ? Sound::Move : Sound::NotHandled;
}

bool FieldWidget::TestChar(int ch) const
{
    if (!numeric_ && !integer_)
        return true;

    if (ch >= '0' && ch <= '9')
        return true;
    if (ch == '-' || ch == '+')
        return true;
    if (!integer_ && ch == '.')
        return true;
    return false;
}

Sound FieldWidget::CharEvent(int ch)
{
    if (IsDisabled())
        return Sound::Beep;
    if (!TestChar(ch))
        return Sound::Beep;
    bool handled = IF_CharEvent(&field_, ch);
    return handled ? Sound::Silent : Sound::NotHandled;
}

void FieldWidget::Draw(bool focused) const
{
    bool disabled = IsDisabled();
    color_t color = disabled ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled)
        color = uis.color.active;

    int draw_x = center_ ? rect_.x + rect_.width / 2 : rect_.x;
    int x = draw_x;
    const char *label = LabelText();
    if (label && *label && !center_) {
        int label_flags = UI_LEFT;
        if (!focused && !disabled)
            label_flags |= UI_ALTCOLOR;
        if (UseColumns()) {
            clipRect_t clip{};
            clip.left = rect_.x;
            clip.right = rect_.x + max(1, ColumnLabelWidth()) - 1;
            clip.top = rect_.y;
            clip.bottom = rect_.y + rect_.height;
            R_SetClipRect(&clip);
        }
        UI_DrawString(x, WidgetTextY(rect_.y, rect_.height), label_flags, color, label);
        if (UseColumns())
            R_SetClipRect(NULL);
        x = UseColumns() ? ColumnValueX() : (x + TextWidth(label) + WidgetGap());
    }

    int flags = center_ ? UI_CENTER : UI_LEFT;
    int box_w = InputFieldBoxWidth(width_, false);
    if (!center_) {
        int max_w = UseColumns() ? ColumnValueWidth() : (rect_.x + rect_.width - x);
        if (max_w > 0)
            box_w = min(box_w, max_w);
    }
    int box_x = center_ ? (x - box_w / 2) : x;
    int box_y = rect_.y;
    int box_h = rect_.height;
    DrawTextBox(box_x, rect_.y, box_w, rect_.height, focused, disabled, &box_y, &box_h);
    int text_y = box_y + (box_h - WidgetTextHeight()) / 2;

    int cursor_x = box_x + WidgetPadding();
    int cursor_w = 0;
    int text_w = max(1, box_w - WidgetPadding() * 2);
    clipRect_t clip{};
    clip.left = box_x + 1;
    clip.right = box_x + box_w - 1;
    clip.top = box_y;
    clip.bottom = box_y + box_h;
    R_SetClipRect(&clip);
    DrawInputFieldText(&field_, cursor_x, text_y, flags, text_w, field_.maxChars,
                       &cursor_x, &cursor_w);
    if (focused && !disabled && ((uis.realtime >> 8) & 1)) {
        DrawInputCursor(cursor_x, text_y, WidgetTextHeight(), cursor_w,
                        Key_GetOverstrikeMode() != 0, color);
    }
    R_SetClipRect(NULL);
}

ComboWidget::ComboWidget(std::string label, cvar_t *cvar, int width, bool center,
                         bool numeric, bool integer)
    : cvar_(cvar)
    , width_(width)
    , center_(center)
    , numeric_(numeric)
    , integer_(integer)
{
    label_ = std::move(label);
    selectable_ = true;
}

void ComboWidget::Layout(int x, int y, int width, int lineHeight)
{
    rect_.y = y;
    rect_.width = width;
    rect_.height = Height(lineHeight);
    rect_.x = center_ ? x - width / 2 : x;

    int draw_x = center_ ? rect_.x + rect_.width / 2 : rect_.x;
    int value_x = draw_x;
    const char *label = LabelText();
    if (label && *label && !center_) {
        value_x = UseColumns() ? ColumnValueX() : (value_x + TextWidth(label) + WidgetGap());
    }

    int box_w = InputFieldBoxWidth(width_, true);
    if (!center_) {
        int max_w = UseColumns() ? ColumnValueWidth() : (rect_.x + rect_.width - value_x);
        if (max_w > 0)
            box_w = min(box_w, max_w);
    }

    int arrow_area = min(CONCHAR_WIDTH * 2, max(0, box_w - 2));
    if (arrow_area < CONCHAR_WIDTH)
        arrow_area = 0;

    int inner_w = max(1, box_w - WidgetPadding() * 2 - arrow_area);
    UpdateInputFieldVisibleChars(&field_, inner_w);
}

void ComboWidget::OnOpen()
{
    IF_Init(&field_, width_, MAX_FIELD_TEXT);
    if (cvar_)
        IF_Replace(&field_, cvar_->string);
    SyncIndexFromText();
}

void ComboWidget::OnClose()
{
    if (cvar_)
        Cvar_SetByVar(cvar_, field_.text, FROM_MENU);
}

void ComboWidget::AddOption(std::string value)
{
    items_.push_back(std::move(value));
}

void ComboWidget::ClearOptions()
{
    items_.clear();
    currentIndex_ = -1;
}

bool ComboWidget::TestChar(int ch) const
{
    if (!numeric_ && !integer_)
        return true;

    if (ch >= '0' && ch <= '9')
        return true;
    if (ch == '-' || ch == '+')
        return true;
    if (!integer_ && ch == '.')
        return true;
    return false;
}

void ComboWidget::SetFromIndex(int index)
{
    if (items_.empty())
        return;

    if (index < 0)
        index = static_cast<int>(items_.size()) - 1;
    else if (index >= static_cast<int>(items_.size()))
        index = 0;

    currentIndex_ = index;
    IF_Replace(&field_, items_[currentIndex_].c_str());
}

void ComboWidget::SyncIndexFromText()
{
    currentIndex_ = -1;
    if (items_.empty())
        return;

    for (size_t i = 0; i < items_.size(); i++) {
        if (Q_stricmp(items_[i].c_str(), field_.text) == 0) {
            currentIndex_ = static_cast<int>(i);
            break;
        }
    }
}

Sound ComboWidget::KeyEvent(int key)
{
    if (IsDisabled())
        return Sound::Beep;

    if (key == K_MOUSE1) {
        int draw_x = center_ ? rect_.x + rect_.width / 2 : rect_.x;
        int x = draw_x;
        const char *label = LabelText();
        if (label && *label && !center_) {
            x = UseColumns() ? ColumnValueX() : (x + TextWidth(label) + WidgetGap());
        }

        int box_w = InputFieldBoxWidth(width_, true);
        if (!center_) {
            int max_w = UseColumns() ? ColumnValueWidth() : (rect_.x + rect_.width - x);
            if (max_w > 0)
                box_w = min(box_w, max_w);
        }
        int box_x = center_ ? (x - box_w / 2) : x;
        int text_h = WidgetTextHeight();
        int target_h = max(8, text_h + 6);
        int box_h = min(rect_.height, target_h);
        if (box_h < 6)
            box_h = max(1, rect_.height);
        int box_y = rect_.y + (rect_.height - box_h) / 2;
        vrect_t box_rect{ box_x, box_y, box_w, box_h };

        if (RectContains(box_rect, uis.mouseCoords[0], uis.mouseCoords[1])) {
            int text_x = box_x + WidgetPadding();
            int arrow_area = min(CONCHAR_WIDTH * 2, max(0, box_w - 2));
            if (arrow_area < CONCHAR_WIDTH)
                arrow_area = 0;
            int text_w = max(1, box_w - WidgetPadding() * 2 - arrow_area);
            size_t new_pos = InputFieldCursorFromMouse(&field_, uis.mouseCoords[0], text_x, text_w);
            IF_SetCursor(&field_, new_pos, Key_IsDown(K_SHIFT));
            return Sound::Move;
        }
    }

    if (key == K_TAB && !items_.empty()) {
        int dir = Key_IsDown(K_SHIFT) ? -1 : 1;
        if (currentIndex_ < 0)
            SetFromIndex(dir > 0 ? 0 : static_cast<int>(items_.size()) - 1);
        else
            SetFromIndex(currentIndex_ + dir);
        return Sound::Move;
    }

    bool handled = IF_KeyEvent(&field_, key);
    if (handled)
        SyncIndexFromText();
    return handled ? Sound::Move : Sound::NotHandled;
}

Sound ComboWidget::CharEvent(int ch)
{
    if (IsDisabled())
        return Sound::Beep;
    if (!TestChar(ch))
        return Sound::Beep;
    bool handled = IF_CharEvent(&field_, ch);
    if (handled)
        SyncIndexFromText();
    return handled ? Sound::Silent : Sound::NotHandled;
}

void ComboWidget::Draw(bool focused) const
{
    bool disabled = IsDisabled();
    color_t color = disabled ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled)
        color = uis.color.active;

    int draw_x = center_ ? rect_.x + rect_.width / 2 : rect_.x;
    int x = draw_x;
    const char *label = LabelText();
    if (label && *label && !center_) {
        int label_flags = UI_LEFT;
        if (!focused && !disabled)
            label_flags |= UI_ALTCOLOR;
        if (UseColumns()) {
            clipRect_t clip{};
            clip.left = rect_.x;
            clip.right = rect_.x + max(1, ColumnLabelWidth()) - 1;
            clip.top = rect_.y;
            clip.bottom = rect_.y + rect_.height;
            R_SetClipRect(&clip);
        }
        UI_DrawString(x, WidgetTextY(rect_.y, rect_.height), label_flags, color, label);
        if (UseColumns())
            R_SetClipRect(NULL);
        x = UseColumns() ? ColumnValueX() : (x + TextWidth(label) + WidgetGap());
    }

    int flags = center_ ? UI_CENTER : UI_LEFT;
    int box_w = InputFieldBoxWidth(width_, true);
    if (!center_) {
        int max_w = UseColumns() ? ColumnValueWidth() : (rect_.x + rect_.width - x);
        if (max_w > 0)
            box_w = min(box_w, max_w);
    }
    int box_x = center_ ? (x - box_w / 2) : x;
    int box_y = rect_.y;
    int box_h = rect_.height;
    DrawTextBox(box_x, rect_.y, box_w, rect_.height, focused, disabled, &box_y, &box_h);
    int text_y = box_y + (box_h - WidgetTextHeight()) / 2;

    int arrow_area = min(CONCHAR_WIDTH * 2, max(0, box_w - 2));
    if (arrow_area < CONCHAR_WIDTH)
        arrow_area = 0;
    int arrow_x = box_x + box_w - arrow_area;
    if (arrow_area > 0) {
        color_t divider = COLOR_SETA_U8(uis.color.selection, 160);
        R_DrawFill32(arrow_x, box_y + 1, 1, max(1, box_h - 2), divider);
    }

    int cursor_x = box_x + WidgetPadding();
    int cursor_w = 0;
    int text_w = max(1, box_w - WidgetPadding() * 2 - arrow_area);
    clipRect_t clip{};
    clip.left = box_x + 1;
    clip.right = (arrow_area > 0) ? (arrow_x - 1) : (box_x + box_w - 1);
    clip.top = box_y;
    clip.bottom = box_y + box_h;
    R_SetClipRect(&clip);
    DrawInputFieldText(&field_, cursor_x, text_y, flags, text_w, field_.maxChars,
                       &cursor_x, &cursor_w);
    if (focused && !disabled && ((uis.realtime >> 8) & 1)) {
        DrawInputCursor(cursor_x, text_y, WidgetTextHeight(), cursor_w,
                        Key_GetOverstrikeMode() != 0, color);
    }
    R_SetClipRect(NULL);

    if (arrow_area > 0) {
        int arrow_y = text_y;
        UI_DrawChar(arrow_x + (arrow_area - CONCHAR_WIDTH) / 2, arrow_y, UI_LEFT, color, 'v');
    }

}

KeyBindWidget::KeyBindWidget(std::string label, std::string command, std::string status,
                             std::string altStatus)
    : command_(std::move(command))
    , altStatus_(std::move(altStatus))
{
    label_ = std::move(label);
    status_ = std::move(status);
    selectable_ = true;
}

int KeyBindWidget::Height(int lineHeight) const
{
    int height = lineHeight + lineHeight / 2;
    return max(1, height);
}

void KeyBindWidget::RemoveBindings(const char *cmd)
{
    for (int key = 0;; key++) {
        key = Key_EnumBindings(key, cmd);
        if (key == -1)
            break;
        Key_SetBinding(key, NULL);
    }
}

bool KeyBindWidget::KeybindCallback(void *arg, int key)
{
    auto *self = static_cast<KeyBindWidget *>(arg);

    if (key == '`') {
        UI_StartSound(Sound::Beep);
        return false;
    }

    if (key != K_ESCAPE) {
        if (self->altBinding_[0])
            RemoveBindings(self->command_.c_str());
        Key_SetBinding(key, self->command_.c_str());
    }

    self->RefreshBindings();

    uis.keywait = false;
    if (self->owner_)
        self->owner_->SetStatus(self->status_);

    Key_WaitKey(NULL, NULL);
    UI_StartSound(Sound::Out);
    return false;
}

void KeyBindWidget::RefreshBindings()
{
    int key = Key_EnumBindings(0, command_.c_str());
    primaryKey_ = key;
    altKey_ = -1;

    altBinding_[0] = 0;
    if (key == -1) {
        Q_strlcpy(binding_, "???", sizeof(binding_));
    } else {
        Q_strlcpy(binding_, Key_KeynumToString(key), sizeof(binding_));
        key = Key_EnumBindings(key + 1, command_.c_str());
        if (key != -1) {
            altKey_ = key;
            Q_strlcpy(altBinding_, Key_KeynumToString(key), sizeof(altBinding_));
        }
    }
}

void KeyBindWidget::OnOpen()
{
    RefreshBindings();
}

void KeyBindWidget::OnClose()
{
    Key_WaitKey(NULL, NULL);
}

Sound KeyBindWidget::Activate()
{
    uis.keywait = true;
    if (owner_)
        owner_->SetStatus(altStatus_);
    Key_WaitKey(KeybindCallback, this);
    return Sound::In;
}

Sound KeyBindWidget::KeyEvent(int key)
{
    if (key == K_BACKSPACE) {
        RemoveBindings(command_.c_str());
        RefreshBindings();
        return Sound::Out;
    }
    return Sound::NotHandled;
}

void KeyBindWidget::Draw(bool focused) const
{
    bool disabled = IsDisabled();
    color_t color = disabled ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled)
        color = uis.color.active;

    int flags = UI_LEFT;
    if (!focused && !disabled)
        flags |= UI_ALTCOLOR;
    const char *label = LabelText();
    int text_y = WidgetTextY(rect_.y, rect_.height);
    if (label && *label) {
        if (UseColumns()) {
            clipRect_t clip{};
            clip.left = rect_.x;
            clip.right = rect_.x + max(1, ColumnLabelWidth()) - 1;
            clip.top = rect_.y;
            clip.bottom = rect_.y + rect_.height;
            R_SetClipRect(&clip);
        }
        UI_DrawString(rect_.x, text_y, flags, color, label);
        if (UseColumns())
            R_SetClipRect(NULL);
    }

    int value_x = UseColumns() ? ColumnValueX() : rect_.x + TextWidth(label) + WidgetGap();
    int value_w = UseColumns() ? ColumnValueWidth() : rect_.x + rect_.width - value_x;
    value_w = max(1, value_w);
    int icon_height = max(1, rect_.height - 4);
    int box_h = min(rect_.height, max(icon_height + 4, 12));
    int box_y = rect_.y + (rect_.height - box_h) / 2;
    DrawControlBox(value_x, box_y, value_w, box_h, focused, disabled);
    int icon_y = box_y + (box_h - icon_height) / 2;
    int draw_x = value_x + WidgetPadding();
    clipRect_t clip{};
    clip.left = value_x + 1;
    clip.right = value_x + value_w - 1;
    clip.top = box_y;
    clip.bottom = box_y + box_h;
    R_SetClipRect(&clip);

    if (primaryKey_ < 0) {
        UI_DrawKeyIcon(draw_x, icon_y, icon_height, color, -1, "???");
        R_SetClipRect(NULL);
        return;
    }

    draw_x += UI_DrawKeyIcon(draw_x, icon_y, icon_height, color, primaryKey_, nullptr);
    if (altKey_ >= 0) {
        draw_x += CONCHAR_WIDTH / 2;
        UI_DrawKeyIcon(draw_x, icon_y, icon_height, color, altKey_, nullptr);
    }
    R_SetClipRect(NULL);
}

SeparatorWidget::SeparatorWidget()
{
    selectable_ = false;
}

void SeparatorWidget::Draw(bool focused) const
{
    (void)focused;
    const char *label = LabelText();
    if (!label || !*label)
        return;

    int text_y = WidgetTextY(rect_.y, rect_.height);
    UI_DrawString(rect_.x, text_y, UI_LEFT | UI_ALTCOLOR, uis.color.normal, label);

    int line_x = rect_.x + TextWidth(label) + CONCHAR_WIDTH;
    int line_y = rect_.y + rect_.height / 2;
    int line_w = rect_.width - (line_x - rect_.x);
    if (line_w > 0) {
        color_t line = COLOR_SETA_U8(uis.color.selection, 160);
        R_DrawFill32(line_x, line_y, line_w, 1, line);
    }
}

HeadingWidget::HeadingWidget()
{
    selectable_ = false;
}

int HeadingWidget::Height(int lineHeight) const
{
    int font_size = CONCHAR_HEIGHT + 2;
    int text_h = max(1, UI_FontLineHeightSized(font_size));
    int padding = max(2, CONCHAR_HEIGHT / 2);
    int height = text_h + padding;
    return max(lineHeight, height);
}

void HeadingWidget::Draw(bool focused) const
{
    (void)focused;
    const char *label = LabelText();
    if (!label || !*label)
        return;

    int font_size = CONCHAR_HEIGHT + 2;
    int text_h = max(1, UI_FontLineHeightSized(font_size));
    int text_y = rect_.y + (rect_.height - text_h) / 2;
    color_t color = uis.color.active;

    UI_FontDrawStringSized(rect_.x + 1, text_y, UI_LEFT, strlen(label), label,
                           COLOR_SETA_U8(color, 220), font_size);
    UI_FontDrawStringSized(rect_.x, text_y, UI_LEFT, strlen(label), label,
                           COLOR_SETA_U8(color, 255), font_size);

    int text_w = UI_FontMeasureStringSized(0, strlen(label), label, nullptr, font_size);
    int line_x = rect_.x + text_w + CONCHAR_WIDTH;
    int line_y = text_y + text_h / 2;
    int line_w = rect_.width - (line_x - rect_.x);
    if (line_w > 0) {
        color_t line = COLOR_SETA_U8(uis.color.selection, 180);
        R_DrawFill32(line_x, line_y, line_w, 2, line);
    }
}

WrappedTextWidget::WrappedTextWidget(std::string text, int maxLines, int maxWidthChars, bool alignLeft)
    : maxLines_(maxLines)
    , maxWidthChars_(maxWidthChars)
    , alignLeft_(alignLeft)
{
    label_ = std::move(text);
    selectable_ = false;
}

void WrappedTextWidget::RebuildLines(int lineHeight) const
{
    const char *label = LabelText();
    std::string text = label ? label : "";
    int width_chars = maxWidthChars_ > 0 ? maxWidthChars_ : DefaultWrapWidthChars();

    if (cachedLineHeight_ == lineHeight &&
        cachedWidthChars_ == width_chars &&
        cachedText_ == text) {
        return;
    }

    cachedLineHeight_ = lineHeight;
    cachedWidthChars_ = width_chars;
    cachedText_ = std::move(text);

    int maxLines = maxLines_ > 0 ? maxLines_ : 8;
    lines_ = WrapText(cachedText_, static_cast<size_t>(width_chars), static_cast<size_t>(maxLines));
}

int WrappedTextWidget::Height(int lineHeight) const
{
    RebuildLines(lineHeight);
    int lines = static_cast<int>(lines_.size());
    if (lines < 1)
        lines = 1;
    return lines * lineHeight;
}

void WrappedTextWidget::Layout(int x, int y, int width, int lineHeight)
{
    RebuildLines(lineHeight);
    rect_.x = x;
    rect_.y = y;
    rect_.width = width;
    rect_.height = Height(lineHeight);
}

void WrappedTextWidget::Draw(bool focused) const
{
    (void)focused;
    int lineHeight = cachedLineHeight_ > 0 ? cachedLineHeight_ : GenericSpacing(CONCHAR_HEIGHT);
    RebuildLines(lineHeight);
    int flags = alignLeft_ ? (UI_LEFT | UI_ALTCOLOR) : (UI_CENTER | UI_ALTCOLOR);
    int draw_x = alignLeft_ ? rect_.x : rect_.x + rect_.width / 2;
    int y = rect_.y;
    for (const auto &line : lines_) {
        UI_DrawString(draw_x, y, flags, uis.color.normal, line.c_str());
        y += lineHeight;
    }
}

SaveGameWidget::SaveGameWidget(std::string slot, bool isLoad)
    : slot_(std::move(slot))
    , isLoad_(isLoad)
{
    selectable_ = true;
}

void SaveGameWidget::OnOpen()
{
    char *info = SV_GetSaveInfo(slot_.c_str());
    if (info) {
        label_ = info;
        disabled_ = false;
    } else {
        label_ = "<EMPTY>";
        disabled_ = isLoad_;
    }
}

void SaveGameWidget::Draw(bool focused) const
{
    bool disabled = IsDisabled();
    color_t color = disabled ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled)
        color = uis.color.active;
    int flags = UI_LEFT;
    if (!focused && !disabled)
        flags |= UI_ALTCOLOR;
    DrawRowHighlight(rect_.x, rect_.y, rect_.width, rect_.height, focused, disabled);
    UI_DrawString(rect_.x, WidgetTextY(rect_.y, rect_.height), flags, color, LabelText());
}

Sound SaveGameWidget::Activate()
{
    if (IsDisabled())
        return Sound::Beep;

    if (isLoad_) {
        Cbuf_AddText(&cmd_buffer, va("load \"%s\"\n", slot_.c_str()));
    } else {
        Cbuf_AddText(&cmd_buffer, va("save \"%s\"; forcemenuoff\n", slot_.c_str()));
    }
    return Sound::In;
}

} // namespace ui
