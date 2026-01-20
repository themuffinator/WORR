#include "ui/ui_internal.h"

#include <sstream>

namespace ui {
namespace {

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

    int draw_x = alignLeft_ ? rect_.x : rect_.x + rect_.width / 2;
    int flags = alignLeft_ ? UI_LEFT : UI_CENTER;
    if (!focused && !disabled)
        flags |= UI_ALTCOLOR;
    UI_DrawString(draw_x, rect_.y, flags, color, LabelText());
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
    modified_ = false;
}

void SliderWidget::OnClose()
{
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
    default:
        break;
    }

    return Sound::NotHandled;
}

void SliderWidget::Draw(bool focused) const
{
    bool disabled = IsDisabled();
    color_t color = disabled ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled)
        color = uis.color.active;

    int flags = UI_LEFT;
    if (!focused && !disabled)
        flags |= UI_ALTCOLOR;
    const char *label = LabelText();
    UI_DrawString(rect_.x, rect_.y, flags, color, label);

    int slider_x = rect_.x + TextWidth(label) + (CONCHAR_WIDTH * 2);
    UI_DrawChar(slider_x, rect_.y, UI_LEFT, color, 128);

    for (int i = 0; i < SLIDER_RANGE; i++) {
        UI_DrawChar(slider_x + CONCHAR_WIDTH + i * CONCHAR_WIDTH, rect_.y, UI_LEFT, color, 129);
    }

    UI_DrawChar(slider_x + CONCHAR_WIDTH + SLIDER_RANGE * CONCHAR_WIDTH, rect_.y, UI_LEFT, color, 130);

    float pos = Q_clipf((curValue_ - minValue_) / (maxValue_ - minValue_), 0.0f, 1.0f);
    int knob_x = slider_x + CONCHAR_WIDTH + static_cast<int>((SLIDER_RANGE - 1) * CONCHAR_WIDTH * pos);
    UI_DrawChar(knob_x, rect_.y, UI_LEFT, color, 131);
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

    int flags = UI_LEFT;
    if (!focused && !disabled)
        flags |= UI_ALTCOLOR;
    const char *label = LabelText();
    UI_DrawString(rect_.x, rect_.y, flags, color, label);
    UI_DrawString(rect_.x + TextWidth(label) + (CONCHAR_WIDTH * 2), rect_.y,
                  UI_LEFT, color, CurrentLabel().c_str());

    if (focused && !disabled && ((uis.realtime >> 8) & 1)) {
        UI_DrawChar(rect_.x - CONCHAR_WIDTH, rect_.y, UI_LEFT, color, 13);
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

    int flags = UI_LEFT;
    if (!focused && !disabled)
        flags |= UI_ALTCOLOR;
    const char *label = LabelText();
    UI_DrawString(rect_.x, rect_.y, flags, color, label);

    int value_x = rect_.x + TextWidth(label) + (CONCHAR_WIDTH * 2);
    std::string value = CurrentLabel();
    UI_DrawString(value_x, rect_.y, UI_LEFT, color, value.c_str());

    int arrow_x = value_x + TextWidth(value) + CONCHAR_WIDTH;
    UI_DrawChar(arrow_x, rect_.y, UI_LEFT, color, 'v');
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

    int flags = UI_LEFT;
    if (!focused && !disabled)
        flags |= UI_ALTCOLOR;
    const char *label = LabelText();
    UI_DrawString(rect_.x, rect_.y, flags, color, label);

    if (style_ == SwitchStyle::Checkbox) {
        const char *box = value_ ? "[x]" : "[ ]";
        int box_x = rect_.x + TextWidth(label) + (CONCHAR_WIDTH * 2);
        UI_DrawString(box_x, rect_.y, UI_LEFT, color, box);
        return;
    }

    int track_width = CONCHAR_WIDTH * 4;
    int track_height = max(4, CONCHAR_HEIGHT - 2);
    int track_x = rect_.x + TextWidth(label) + (CONCHAR_WIDTH * 2);
    int track_y = rect_.y + (CONCHAR_HEIGHT - track_height) / 2;

    color_t track = COLOR_SETA_U8(uis.color.disabled, 120);
    if (disabled) {
        track = COLOR_SETA_U8(uis.color.disabled, 80);
    } else if (value_) {
        track = COLOR_SETA_U8(uis.color.active, focused ? 255 : 200);
    } else if (focused) {
        track = COLOR_SETA_U8(uis.color.normal, 180);
    }

    R_DrawFill32(track_x, track_y, track_width, track_height, track);

    int knob_size = max(2, track_height - 2);
    int knob_x = value_
        ? (track_x + track_width - knob_size - 1)
        : (track_x + 1);
    int knob_y = track_y + (track_height - knob_size) / 2;
    color_t knob = COLOR_RGBA(255, 255, 255, disabled ? 120 : 220);
    R_DrawFill32(knob_x, knob_y, knob_size, knob_size, knob);
}

ImageSpinWidget::ImageSpinWidget(std::string label, cvar_t *cvar, std::string path,
                                 std::string filter, int width, int height)
    : SpinWidget(std::move(label), cvar, SpinType::String)
    , path_(std::move(path))
    , filter_(std::move(filter))
    , previewWidth_(width)
    , previewHeight_(height)
{
}

int ImageSpinWidget::Height(int lineHeight) const
{
    return lineHeight + GenericSpacing(previewHeight_);
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

    const char *val = cvar_ ? cvar_->string : "";
    size_t val_len = strlen(val);
    curValue_ = -1;

    size_t path_offset = path_.size();
    if (path_offset && path_[path_offset - 1] != '/')
        path_offset++;

    for (int i = 0; i < entryCount_; i++) {
        const char *file_value = entries_[i] + path_offset;
        const char *dot = strchr(file_value, '.');
        size_t file_len = dot ? static_cast<size_t>(dot - file_value) : strlen(file_value);
        if (!Q_strncasecmp(val, file_value, max(val_len, file_len))) {
            curValue_ = i;
            break;
        }
    }
}

void ImageSpinWidget::OnClose()
{
    if (cvar_ && curValue_ >= 0 && curValue_ < entryCount_) {
        size_t path_offset = path_.size();
        if (path_offset && path_[path_offset - 1] != '/')
            path_offset++;

        const char *file_value = entries_[curValue_] + path_offset;
        const char *dot = strchr(file_value, '.');
        size_t file_len = dot ? static_cast<size_t>(dot - file_value) : strlen(file_value);
        Cvar_SetEx(cvar_->name, va("%.*s", static_cast<int>(file_len), file_value), FROM_MENU);
    }
    if (entries_) {
        FS_FreeList((void **)entries_);
        entries_ = nullptr;
        entryCount_ = 0;
    }
}

void ImageSpinWidget::Draw(bool focused) const
{
    SpinWidget::Draw(focused);
    (void)previewWidth_;

    if (curValue_ < 0 || curValue_ >= entryCount_)
        return;

    qhandle_t pic = R_RegisterTempPic(va("/%s", entries_[curValue_]));
    int w, h;
    R_GetPicSize(&w, &h, pic);

    int draw_x = rect_.x + (TextWidth(LabelText()) / 2) - (w / 2);
    int draw_y = rect_.y + GenericSpacing(CONCHAR_HEIGHT);
    R_DrawPic(draw_x, draw_y, COLOR_WHITE, pic);
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
        UI_DrawString(x, rect_.y, label_flags, color, label);
        x += TextWidth(label) + (CONCHAR_WIDTH * 2);
    }

    int flags = center_ ? UI_CENTER : UI_LEFT;
    IF_Draw(&field_, x, rect_.y, flags, uis.fontHandle);

    if (focused && !disabled && ((uis.realtime >> 8) & 1)) {
        int cursor_x = x + static_cast<int>(field_.cursorPos) * CONCHAR_WIDTH;
        UI_DrawChar(cursor_x, rect_.y, UI_LEFT, color, 11);
    }
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
        UI_DrawString(x, rect_.y, label_flags, color, label);
        x += TextWidth(label) + (CONCHAR_WIDTH * 2);
    }

    int flags = center_ ? UI_CENTER : UI_LEFT;
    IF_Draw(&field_, x, rect_.y, flags, uis.fontHandle);

    int arrow_x = x + width_ * CONCHAR_WIDTH + CONCHAR_WIDTH;
    UI_DrawChar(arrow_x, rect_.y, UI_LEFT, color, 'v');

    if (focused && !disabled && ((uis.realtime >> 8) & 1)) {
        int cursor_x = x + static_cast<int>(field_.cursorPos) * CONCHAR_WIDTH;
        UI_DrawChar(cursor_x, rect_.y, UI_LEFT, color, 11);
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
    int text_y = rect_.y + (rect_.height - CONCHAR_HEIGHT) / 2;
    const char *label = LabelText();
    UI_DrawString(rect_.x, text_y, flags, color, label);

    int icon_height = max(1, rect_.height);
    int icon_y = rect_.y + (rect_.height - icon_height) / 2;
    int draw_x = rect_.x + TextWidth(label) + (CONCHAR_WIDTH * 2);

    if (primaryKey_ < 0) {
        UI_DrawKeyIcon(draw_x, icon_y, icon_height, color, -1, "???");
        return;
    }

    draw_x += UI_DrawKeyIcon(draw_x, icon_y, icon_height, color, primaryKey_, nullptr);
    if (altKey_ >= 0) {
        draw_x += CONCHAR_WIDTH / 2;
        UI_DrawKeyIcon(draw_x, icon_y, icon_height, color, altKey_, nullptr);
    }
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

    UI_DrawString(rect_.x, rect_.y, UI_LEFT | UI_ALTCOLOR, uis.color.normal, label);

    int line_x = rect_.x + TextWidth(label) + CONCHAR_WIDTH;
    int line_y = rect_.y + CONCHAR_HEIGHT / 2;
    int line_w = rect_.width - (line_x - rect_.x);
    if (line_w > 0) {
        color_t line = COLOR_SETA_U8(uis.color.normal, 160);
        R_DrawFill32(line_x, line_y, line_w, 1, line);
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
    UI_DrawString(rect_.x, rect_.y, flags, color, LabelText());
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
