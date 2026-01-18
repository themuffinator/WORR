#include "ui/ui_internal.h"

namespace ui {

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
    color_t color = uis.color.normal;
    if (disabled_)
        color = uis.color.disabled;
    else if (focused)
        color = uis.color.active;

    int draw_x = alignLeft_ ? rect_.x : rect_.x + rect_.width / 2;
    int flags = alignLeft_ ? (UI_LEFT | UI_ALTCOLOR) : (UI_CENTER | UI_ALTCOLOR);
    UI_DrawString(draw_x, rect_.y, flags, color, label_.c_str());
}

Sound ActionWidget::Activate()
{
    if (disabled_)
        return Sound::Beep;
    if (!command_.empty()) {
        Cbuf_AddText(&cmd_buffer, command_.c_str());
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
    if (disabled_)
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
    color_t color = disabled_ ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled_)
        color = uis.color.active;

    UI_DrawString(rect_.x, rect_.y, UI_LEFT | UI_ALTCOLOR, color, label_.c_str());

    int slider_x = rect_.x + TextWidth(label_) + (CONCHAR_WIDTH * 2);
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
    if (disabled_)
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
    color_t color = disabled_ ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled_)
        color = uis.color.active;

    UI_DrawString(rect_.x, rect_.y, UI_LEFT | UI_ALTCOLOR, color, label_.c_str());
    UI_DrawString(rect_.x + TextWidth(label_) + (CONCHAR_WIDTH * 2), rect_.y,
                  UI_LEFT, color, CurrentLabel().c_str());

    if (focused && !disabled_ && ((uis.realtime >> 8) & 1)) {
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

    int draw_x = rect_.x + (TextWidth(label_) / 2) - (w / 2);
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
    if (disabled_)
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
    if (disabled_)
        return Sound::Beep;
    if (!TestChar(ch))
        return Sound::Beep;
    bool handled = IF_CharEvent(&field_, ch);
    return handled ? Sound::Silent : Sound::NotHandled;
}

void FieldWidget::Draw(bool focused) const
{
    color_t color = disabled_ ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled_)
        color = uis.color.active;

    int draw_x = center_ ? rect_.x + rect_.width / 2 : rect_.x;
    int x = draw_x;
    if (!label_.empty() && !center_) {
        UI_DrawString(x, rect_.y, UI_LEFT | UI_ALTCOLOR, color, label_.c_str());
        x += TextWidth(label_) + (CONCHAR_WIDTH * 2);
    }

    int flags = center_ ? UI_CENTER : UI_LEFT;
    IF_Draw(&field_, x, rect_.y, flags, uis.fontHandle);

    if (focused && !disabled_ && ((uis.realtime >> 8) & 1)) {
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

    altBinding_[0] = 0;
    if (key == -1) {
        Q_strlcpy(binding_, "???", sizeof(binding_));
    } else {
        Q_strlcpy(binding_, Key_KeynumToString(key), sizeof(binding_));
        key = Key_EnumBindings(key + 1, command_.c_str());
        if (key != -1) {
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
    color_t color = disabled_ ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled_)
        color = uis.color.active;

    UI_DrawString(rect_.x, rect_.y, UI_LEFT | UI_ALTCOLOR, color, label_.c_str());

    char display[MAX_STRING_CHARS];
    if (altBinding_[0]) {
        Q_concat(display, sizeof(display), binding_, " or ", altBinding_);
    } else if (binding_[0]) {
        Q_strlcpy(display, binding_, sizeof(display));
    } else {
        Q_strlcpy(display, "???", sizeof(display));
    }

    UI_DrawString(rect_.x + TextWidth(label_) + (CONCHAR_WIDTH * 2), rect_.y,
                  UI_LEFT, color, display);
}

SeparatorWidget::SeparatorWidget()
{
    selectable_ = false;
}

void SeparatorWidget::Draw(bool focused) const
{
    (void)focused;
    if (!label_.empty())
        UI_DrawString(rect_.x, rect_.y, UI_LEFT, uis.color.normal, label_.c_str());
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
    color_t color = disabled_ ? uis.color.disabled : uis.color.normal;
    if (focused && !disabled_)
        color = uis.color.active;
    UI_DrawString(rect_.x, rect_.y, UI_LEFT | UI_ALTCOLOR, color, label_.c_str());
}

Sound SaveGameWidget::Activate()
{
    if (disabled_)
        return Sound::Beep;

    if (isLoad_) {
        Cbuf_AddText(&cmd_buffer, va("load \"%s\"\n", slot_.c_str()));
    } else {
        Cbuf_AddText(&cmd_buffer, va("save \"%s\"; forcemenuoff\n", slot_.c_str()));
    }
    return Sound::In;
}

} // namespace ui
