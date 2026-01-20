#include "ui/ui_internal.h"

namespace ui {

namespace {

void UpdateMenuBlurRect(bool transparent, int menuTop, int menuBottom)
{
    if (!transparent || menuBottom <= menuTop || uis.scale <= 0.0f) {
        UI_Sys_SetMenuBlurRect(nullptr);
        return;
    }

    float inv_scale = (uis.scale > 0.0f) ? (1.0f / uis.scale) : 1.0f;
    clipRect_t rect{};
    rect.left = 0;
    rect.right = Q_rint(uis.width * inv_scale);
    rect.top = Q_rint(menuTop * inv_scale);
    rect.bottom = Q_rint(menuBottom * inv_scale);

    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        UI_Sys_SetMenuBlurRect(nullptr);
        return;
    }

    UI_Sys_SetMenuBlurRect(&rect);
}

} // namespace

Menu::Menu(std::string name)
    : name_(std::move(name))
{
    backgroundColor_ = uis.color.background;
    transparent_ = uis.transparent;
}

void Menu::SetBackground(color_t color)
{
    backgroundImage_ = 0;
    backgroundColor_ = color;
    transparent_ = color.a != 255;
}

void Menu::SetBackgroundImage(qhandle_t image, bool transparent)
{
    backgroundImage_ = image;
    transparent_ = transparent;
}

void Menu::SetBanner(qhandle_t banner, const vrect_t &rc)
{
    banner_ = banner;
    bannerRect_ = rc;
}

void Menu::SetPlaque(qhandle_t plaque, const vrect_t &rc)
{
    plaque_ = plaque;
    plaqueRect_ = rc;
}

void Menu::SetLogo(qhandle_t logo, const vrect_t &rc)
{
    logo_ = logo;
    logoRect_ = rc;
}

void Menu::ClearHints()
{
    hintsLeft_.clear();
    hintsRight_.clear();
}

void Menu::AddHintLeft(int key, std::string label, std::string keyLabel)
{
    hintsLeft_.push_back(UiHint{ key, std::move(label), std::move(keyLabel) });
}

void Menu::AddHintRight(int key, std::string label, std::string keyLabel)
{
    hintsRight_.push_back(UiHint{ key, std::move(label), std::move(keyLabel) });
}

int Menu::HintHeight() const
{
    return UI_GetHintBarHeight(hintsLeft_, hintsRight_);
}

void Menu::BuildDefaultHints()
{
    ClearHints();
    if (name_ == "main")
        return;

    AddHintLeft(K_ESCAPE, "Back", "Esc");
    AddHintRight(K_ENTER, "Select", "Enter");

    bool hasKeybind = false;
    for (const auto &widget : widgets_) {
        if (dynamic_cast<const KeyBindWidget *>(widget.get())) {
            hasKeybind = true;
            break;
        }
    }

    if (hasKeybind)
        AddHintLeft(K_BACKSPACE, "Unbind", "Bksp");
}

void Menu::AddWidget(std::unique_ptr<Widget> widget)
{
    if (!widget)
        return;
    widget->SetOwner(this);
    widgets_.push_back(std::move(widget));
}

Widget *Menu::FocusedWidget()
{
    if (focusedIndex_ < 0 || focusedIndex_ >= static_cast<int>(widgets_.size()))
        return nullptr;
    return widgets_[focusedIndex_].get();
}

const Widget *Menu::FocusedWidget() const
{
    if (focusedIndex_ < 0 || focusedIndex_ >= static_cast<int>(widgets_.size()))
        return nullptr;
    return widgets_[focusedIndex_].get();
}

int Menu::FindNextSelectable(int start, int dir) const
{
    if (widgets_.empty())
        return -1;

    int index = start;
    for (;;) {
        index += dir;
        if (index < 0)
            index = static_cast<int>(widgets_.size()) - 1;
        else if (index >= static_cast<int>(widgets_.size()))
            index = 0;

        const Widget *widget = widgets_[index].get();
        if (widget && widget->IsSelectable())
            return index;

        if (index == start)
            break;
    }

    return start;
}

void Menu::UpdateStatusFromFocus()
{
    const Widget *widget = FocusedWidget();
    if (widget && !widget->Status().empty())
        status_ = widget->Status();
}

void Menu::Layout()
{
    lineHeight_ = GenericSpacing(CONCHAR_HEIGHT);
    itemYs_.assign(widgets_.size(), 0);
    itemHeights_.assign(widgets_.size(), 0);

    for (auto &widget : widgets_)
        widget->UpdateConditions();

    int total = 0;
    int banner_spacing = 0;
    if (banner_) {
        banner_spacing = GenericSpacing(bannerRect_.height);
        total += banner_spacing;
    }

    for (size_t i = 0; i < widgets_.size(); i++) {
        Widget *widget = widgets_[i].get();
        if (!widget || widget->IsHidden())
            continue;
        int h = widget->Height(lineHeight_);
        itemHeights_[i] = h;
        total += h;
    }

    contentHeight_ = total;
    int availableHeight = uis.height - HintHeight();
    if (availableHeight < 0)
        availableHeight = 0;
    contentTop_ = (availableHeight - contentHeight_) / 2;
    if (contentTop_ < 0)
        contentTop_ = 0;
    contentBottom_ = min(contentTop_ + contentHeight_, availableHeight);

    int y = contentTop_;
    if (banner_) {
        bannerRect_.x = (uis.width - bannerRect_.width) / 2;
        bannerRect_.y = y;
        y += banner_spacing;
    }

    for (size_t i = 0; i < widgets_.size(); i++) {
        Widget *widget = widgets_[i].get();
        if (!widget || widget->IsHidden())
            continue;
        itemYs_[i] = y;
        y += itemHeights_[i];
    }

    int widest_bitmap = 0;
    for (const auto &widget : widgets_) {
        const auto *bitmap = dynamic_cast<const BitmapWidget *>(widget.get());
        if (bitmap)
            widest_bitmap = max(widest_bitmap, bitmap->ImageWidth());
    }

    hasBitmaps_ = widest_bitmap > 0;
    bitmapBaseX_ = uis.width / 2;
    if (hasBitmaps_) {
        int side_width = 0;
        if (plaque_ || logo_)
            side_width = max(plaqueRect_.width, logoRect_.width);
        int total_width = widest_bitmap + CURSOR_WIDTH + side_width;
        bitmapBaseX_ = (uis.width + total_width) / 2 - widest_bitmap;
    }

    if (plaque_) {
        int total_plaque = plaqueRect_.height + (logo_ ? (logoRect_.height + 5) : 0);
        plaqueRect_.x = bitmapBaseX_ - CURSOR_WIDTH - plaqueRect_.width;
        plaqueRect_.y = (uis.height - total_plaque) / 2;
    }
    if (logo_) {
        int total_plaque = (plaque_ ? plaqueRect_.height : 0) + logoRect_.height + 5;
        logoRect_.x = bitmapBaseX_ - CURSOR_WIDTH - logoRect_.width;
        logoRect_.y = (uis.height + total_plaque) / 2 - logoRect_.height;
    }

    int firstSelectable = -1;
    for (size_t i = 0; i < widgets_.size(); i++) {
        if (widgets_[i]->IsSelectable()) {
            firstSelectable = static_cast<int>(i);
            break;
        }
    }

    if (focusedIndex_ < 0 || focusedIndex_ >= static_cast<int>(widgets_.size()) ||
        (focusedIndex_ >= 0 && !widgets_[focusedIndex_]->IsSelectable())) {
        focusedIndex_ = firstSelectable;
        UpdateStatusFromFocus();
    }

    EnsureVisible(focusedIndex_);
}

int Menu::HitTest(int x, int y)
{
    int leftX = uis.width / 2 - (CONCHAR_WIDTH * 16);
    if (leftX < 0)
        leftX = 0;
    int centerX = uis.width / 2;
    int contentWidth = uis.width - (leftX * 2);
    if (contentWidth < 0)
        contentWidth = 0;

    for (size_t i = 0; i < widgets_.size(); i++) {
        Widget *widget = widgets_[i].get();
        if (!widget || widget->IsHidden() || !widget->IsSelectable())
            continue;

        int draw_y = itemYs_[i] - scrollY_;
        int height = itemHeights_[i];
        if (draw_y + height < contentTop_ || draw_y > contentBottom_)
            continue;

        int draw_x = leftX;
        if (auto *bitmap = dynamic_cast<BitmapWidget *>(widget)) {
            int bitmap_center = centerX;
            if (hasBitmaps_)
                bitmap_center = bitmapBaseX_ + bitmap->ImageWidth() / 2;
            draw_x = bitmap_center;
            bitmap->Layout(draw_x, draw_y, contentWidth, lineHeight_);
        } else if (auto *action = dynamic_cast<ActionWidget *>(widget)) {
            draw_x = action->AlignLeft() ? leftX : centerX;
            action->Layout(draw_x, draw_y, contentWidth, lineHeight_);
        } else if (auto *field = dynamic_cast<FieldWidget *>(widget)) {
            draw_x = field->Center() ? centerX : leftX;
            field->Layout(draw_x, draw_y, contentWidth, lineHeight_);
        } else if (auto *combo = dynamic_cast<ComboWidget *>(widget)) {
            draw_x = combo->Center() ? centerX : leftX;
            combo->Layout(draw_x, draw_y, contentWidth, lineHeight_);
        } else {
            widget->Layout(draw_x, draw_y, contentWidth, lineHeight_);
        }

        if (RectContains(widget->Rect(), x, y))
            return static_cast<int>(i);
    }

    return -1;
}

void Menu::EnsureVisible(int index)
{
    if (index < 0 || index >= static_cast<int>(itemYs_.size()))
        return;

    int viewTop = contentTop_;
    int viewBottom = contentBottom_;
    int itemTop = itemYs_[index];
    int itemBottom = itemTop + itemHeights_[index];

    if (itemTop - scrollY_ < viewTop)
        scrollY_ = itemTop - viewTop;
    else if (itemBottom - scrollY_ > viewBottom)
        scrollY_ = itemBottom - viewBottom;

    scrollY_ = max(scrollY_, 0);
    int maxScroll = max(0, contentHeight_ - (viewBottom - viewTop));
    scrollY_ = min(scrollY_, maxScroll);
}

void Menu::OnOpen()
{
    for (auto &widget : widgets_)
        widget->OnOpen();

    for (auto &widget : widgets_)
        widget->UpdateConditions();

    focusedIndex_ = -1;
    for (size_t i = 0; i < widgets_.size(); i++) {
        if (widgets_[i]->IsSelectable() && widgets_[i]->IsDefault()) {
            focusedIndex_ = static_cast<int>(i);
            break;
        }
    }
    if (focusedIndex_ < 0) {
        for (size_t i = 0; i < widgets_.size(); i++) {
            if (widgets_[i]->IsSelectable()) {
                focusedIndex_ = static_cast<int>(i);
                break;
            }
        }
    }
    scrollY_ = 0;
    BuildDefaultHints();
    Layout();
    UpdateStatusFromFocus();
    int menuTop = compact_ ? max(0, contentTop_ - lineHeight_) : 0;
    int menuBottom = compact_ ? min(uis.height, contentBottom_ + lineHeight_) : uis.height;
    UpdateMenuBlurRect(transparent_, menuTop, menuBottom);
}

void Menu::OnClose()
{
    for (auto &widget : widgets_)
        widget->OnClose();
}

static void DrawStatusText(const std::string &text, int bottom)
{
    if (text.empty())
        return;

    int linewidth = uis.width / CONCHAR_WIDTH;
    int count = 0;
    int lens[8]{};
    const char *ptrs[8]{};

    const char *txt = text.c_str();
    ptrs[0] = txt;

    while (*txt) {
        const char *p = txt;
        while (*p > 32)
            p++;
        int len = static_cast<int>(p - txt);

        if (count >= 8)
            break;

        if (lens[count] + len > linewidth) {
            count++;
            if (count >= 8)
                break;
            lens[count] = 0;
            ptrs[count] = txt;
        }

        lens[count] += len;
        txt = p;
        while (*txt <= 32 && *txt)
            txt++;
        lens[count] += static_cast<int>(txt - p);
    }

    count++;
    for (int l = 0; l < count; l++) {
        int x = (uis.width - lens[l] * CONCHAR_WIDTH) / 2;
        int y = bottom - (count - l) * CONCHAR_HEIGHT;
        R_DrawString(x, y, 0, lens[l], ptrs[l], COLOR_WHITE, uis.fontHandle);
    }
}

void Menu::Draw()
{
    Layout();
    int hintHeight = HintHeight();

    int menuTop = compact_ ? max(0, contentTop_ - lineHeight_) : 0;
    int menuBottom = compact_ ? min(uis.height, contentBottom_ + lineHeight_) : uis.height;

    UpdateMenuBlurRect(transparent_, menuTop, menuBottom);

    if (backgroundImage_) {
        R_DrawKeepAspectPic(0, menuTop, uis.width, menuBottom - menuTop, COLOR_WHITE, backgroundImage_);
    } else {
        R_DrawFill32(0, menuTop, uis.width, menuBottom - menuTop, backgroundColor_);
    }

    if (!title_.empty()) {
        UI_DrawString(uis.width / 2, menuTop, UI_CENTER | UI_ALTCOLOR, COLOR_WHITE, title_.c_str());
    }

    if (banner_)
        R_DrawPic(bannerRect_.x, bannerRect_.y, COLOR_WHITE, banner_);
    if (plaque_)
        R_DrawPic(plaqueRect_.x, plaqueRect_.y, COLOR_WHITE, plaque_);
    if (logo_)
        R_DrawPic(logoRect_.x, logoRect_.y, COLOR_WHITE, logo_);

    int leftX = uis.width / 2 - (CONCHAR_WIDTH * 16);
    if (leftX < 0)
        leftX = 0;
    int centerX = uis.width / 2;
    int contentWidth = uis.width - (leftX * 2);
    if (contentWidth < 0)
        contentWidth = 0;

    int viewTop = contentTop_;
    int viewBottom = contentBottom_;

    for (size_t i = 0; i < widgets_.size(); i++) {
        Widget *widget = widgets_[i].get();
        if (!widget || widget->IsHidden())
            continue;

        int draw_y = itemYs_[i] - scrollY_;
        int height = itemHeights_[i];

        if (draw_y + height < viewTop || draw_y > viewBottom)
            continue;

        int x = leftX;
        if (auto *bitmap = dynamic_cast<BitmapWidget *>(widget)) {
            int bitmap_center = centerX;
            if (hasBitmaps_)
                bitmap_center = bitmapBaseX_ + bitmap->ImageWidth() / 2;
            x = bitmap_center;
            bitmap->Layout(x, draw_y, contentWidth, lineHeight_);
        } else if (auto *action = dynamic_cast<ActionWidget *>(widget)) {
            x = action->AlignLeft() ? leftX : centerX;
            action->Layout(x, draw_y, contentWidth, lineHeight_);
        } else if (auto *field = dynamic_cast<FieldWidget *>(widget)) {
            x = field->Center() ? centerX : leftX;
            field->Layout(x, draw_y, contentWidth, lineHeight_);
        } else if (auto *combo = dynamic_cast<ComboWidget *>(widget)) {
            x = combo->Center() ? centerX : leftX;
            combo->Layout(x, draw_y, contentWidth, lineHeight_);
        } else {
            widget->Layout(leftX, draw_y, contentWidth, lineHeight_);
        }

        widget->Draw(static_cast<int>(i) == focusedIndex_);
    }

    int viewHeight = contentBottom_ - contentTop_;
    if (contentHeight_ > viewHeight && viewHeight > 0) {
        int bar_x = uis.width - MLIST_SCROLLBAR_WIDTH;
        int bar_y = contentTop_;
        int bar_h = viewHeight;
        int bar_w = MLIST_SCROLLBAR_WIDTH - 1;

        R_DrawFill32(bar_x, bar_y, bar_w, bar_h, uis.color.normal);

        float pageFrac = static_cast<float>(viewHeight) / contentHeight_;
        float scrollFrac = static_cast<float>(scrollY_) / (contentHeight_ - viewHeight);
        int thumb_h = max(6, Q_rint(bar_h * pageFrac));
        int thumb_y = bar_y + Q_rint((bar_h - thumb_h) * scrollFrac);
        R_DrawFill32(bar_x, thumb_y, bar_w, thumb_h, uis.color.selection);
    }

    DrawStatusText(status_, uis.height - hintHeight);
    UI_DrawHintBar(hintsLeft_, hintsRight_, uis.height);
}

Sound Menu::KeyEvent(int key)
{
    if (widgets_.empty())
        return Sound::NotHandled;

    if (key == K_ESCAPE || key == K_MOUSE2) {
        if (!closeCommand_.empty()) {
            Cbuf_AddText(&cmd_buffer, closeCommand_.c_str());
            Cbuf_AddText(&cmd_buffer, "\n");
            return Sound::Out;
        }
        GetMenuSystem().Pop();
        return Sound::Out;
    }

    if (key == K_UPARROW || key == K_KP_UPARROW || key == K_MWHEELUP) {
        focusedIndex_ = FindNextSelectable(focusedIndex_ < 0 ? 0 : focusedIndex_, -1);
        EnsureVisible(focusedIndex_);
        UpdateStatusFromFocus();
        return Sound::Move;
    }

    if (key == K_DOWNARROW || key == K_KP_DOWNARROW || key == K_MWHEELDOWN) {
        focusedIndex_ = FindNextSelectable(focusedIndex_ < 0 ? 0 : focusedIndex_, 1);
        EnsureVisible(focusedIndex_);
        UpdateStatusFromFocus();
        return Sound::Move;
    }

    if (key == K_PGUP) {
        scrollY_ = max(0, scrollY_ - (contentBottom_ - contentTop_));
        return Sound::Move;
    }

    if (key == K_PGDN) {
        int maxScroll = max(0, contentHeight_ - (contentBottom_ - contentTop_));
        scrollY_ = min(maxScroll, scrollY_ + (contentBottom_ - contentTop_));
        return Sound::Move;
    }

    if (key == K_MOUSE1) {
        int hit = HitTest(uis.mouseCoords[0], uis.mouseCoords[1]);
        if (hit < 0)
            return Sound::NotHandled;
        focusedIndex_ = hit;
        UpdateStatusFromFocus();
    }

    Widget *widget = FocusedWidget();
    if (!widget)
        return Sound::NotHandled;

    Sound sound = widget->KeyEvent(key);
    if (sound != Sound::NotHandled)
        return sound;

    if (key == K_MOUSE1 || key == K_ENTER || key == K_KP_ENTER) {
        sound = widget->Activate();
        if (sound != Sound::NotHandled)
            return sound;
        return Sound::In;
    }

    return Sound::NotHandled;
}

Sound Menu::CharEvent(int ch)
{
    Widget *widget = FocusedWidget();
    if (!widget)
        return Sound::NotHandled;
    return widget->CharEvent(ch);
}

void Menu::MouseEvent(int x, int y, bool down)
{
    (void)down;
    if (uis.keywait)
        return;

    int hit = HitTest(x, y);
    if (hit >= 0) {
        focusedIndex_ = hit;
        UpdateStatusFromFocus();
    }
}

} // namespace ui
