#include "ui/ui_internal.h"

namespace ui {

namespace {

void UpdateMenuBlurRect(bool allowBlur, bool transparent, int menuTop, int menuBottom)
{
    if (!allowBlur || !transparent || menuBottom <= menuTop || uis.scale <= 0.0f) {
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

struct ColumnLayout {
    bool active = false;
    int labelWidth = 0;
    int valueX = 0;
    int valueWidth = 0;
};

static ColumnLayout ComputeColumnLayout(const std::vector<std::unique_ptr<Widget>> &widgets,
                                        int leftX, int contentWidth)
{
    ColumnLayout layout;
    if (contentWidth <= 0)
        return layout;

    int max_label_width = 0;
    int count = 0;
    for (const auto &widget : widgets) {
        if (!widget || widget->IsHidden() || !widget->UsesColumns())
            continue;
        const char *label = widget->LabelText();
        if (!label || !*label)
            continue;
        max_label_width = max(max_label_width, TextWidth(label));
        count++;
    }

    if (!count)
        return layout;

    int gap = GenericSpacing(CONCHAR_WIDTH);
    int min_value = max(CONCHAR_WIDTH * 12, Q_rint(contentWidth * 0.3f));
    int max_label_allowed = contentWidth - min_value - gap;
    if (max_label_allowed < 0)
        max_label_allowed = max(0, contentWidth - gap);

    int min_label = Q_rint(contentWidth * 0.38f);
    int max_label_ratio = Q_rint(contentWidth * 0.6f);

    int label_width = max(max_label_width, min_label);
    label_width = min(label_width, max_label_allowed);
    label_width = min(label_width, max_label_ratio);
    label_width = max(0, label_width);

    layout.active = true;
    layout.labelWidth = label_width;
    layout.valueX = leftX + label_width + gap;
    layout.valueWidth = max(0, contentWidth - (label_width + gap));
    return layout;
}

static void ApplyColumnLayout(const ColumnLayout &layout,
                              const std::vector<std::unique_ptr<Widget>> &widgets)
{
    for (const auto &widget : widgets) {
        if (!widget)
            continue;
        if (layout.active && widget->UsesColumns() && !widget->IsHidden()) {
            widget->SetColumns(layout.labelWidth, layout.valueX, layout.valueWidth);
        } else {
            widget->ClearColumns();
        }
    }
}

static Widget *FindExpandedOverlay(const std::vector<std::unique_ptr<Widget>> &widgets)
{
    for (const auto &widget : widgets) {
        if (!widget)
            continue;
        if (auto *dropdown = dynamic_cast<DropdownWidget *>(widget.get())) {
            if (dropdown->IsExpanded())
                return dropdown;
        }
        if (auto *image = dynamic_cast<ImageSpinWidget *>(widget.get())) {
            if (image->IsExpanded())
                return image;
        }
    }
    return nullptr;
}

static bool UpdateExpandedHover(const std::vector<std::unique_ptr<Widget>> &widgets, int x, int y)
{
    for (const auto &widget : widgets) {
        if (!widget)
            continue;
        if (auto *dropdown = dynamic_cast<DropdownWidget *>(widget.get())) {
            if (dropdown->IsExpanded())
                return dropdown->HoverAt(x, y);
        }
        if (auto *image = dynamic_cast<ImageSpinWidget *>(widget.get())) {
            if (image->IsExpanded())
                return image->HoverAt(x, y);
        }
    }
    return false;
}

static color_t MenuAdjustColor(color_t color, float delta)
{
    float amount = Q_clipf(delta, -1.0f, 1.0f);
    if (amount >= 0.0f) {
        color.r = Q_rint(color.r + (255 - color.r) * amount);
        color.g = Q_rint(color.g + (255 - color.g) * amount);
        color.b = Q_rint(color.b + (255 - color.b) * amount);
    } else {
        float factor = 1.0f + amount;
        color.r = Q_rint(color.r * factor);
        color.g = Q_rint(color.g * factor);
        color.b = Q_rint(color.b * factor);
    }
    return color;
}

} // namespace

Menu::Menu(std::string name)
    : name_(std::move(name))
{
    backgroundColor_ = uis.color.background;
    transparent_ = uis.transparent;
    frameFill_ = COLOR_RGBA(0, 0, 0, 200);
    frameBorder_ = COLOR_RGBA(255, 255, 255, 40);
    framePadding_ = GenericSpacing(CONCHAR_HEIGHT);
}

void Menu::SetLayoutBounds(int leftX, int contentWidth)
{
    customLayout_ = true;
    customLeftX_ = max(0, leftX);
    customContentWidth_ = max(0, contentWidth);
}

void Menu::ClearLayoutBounds()
{
    customLayout_ = false;
    customLeftX_ = 0;
    customContentWidth_ = 0;
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

void Menu::SetFrameStyle(bool enabled, color_t fill, color_t border, int padding, int borderWidth)
{
    frameEnabled_ = enabled;
    frameFill_ = fill;
    frameBorder_ = border;
    framePadding_ = max(0, padding);
    frameBorderWidth_ = max(0, borderWidth);
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

    if (name_ == "download_status") {
        AddHintLeft(K_ESCAPE, "Cancel", "Esc");
        return;
    }

    bool hasSelectable = false;
    for (const auto &widget : widgets_) {
        if (widget && widget->IsSelectable()) {
            hasSelectable = true;
            break;
        }
    }
    if (!hasSelectable)
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
        if (auto *bitmap = dynamic_cast<BitmapWidget *>(widget)) {
            if (bitmap->HasFixedPosition()) {
                itemHeights_[i] = bitmap->ImageHeight();
                itemYs_[i] = bitmap->FixedY();
                continue;
            }
        }
        itemHeights_[i] = h;
        total += h;
    }

    contentHeight_ = total;
    int availableHeight = uis.height - HintHeight();
    if (availableHeight < 0)
        availableHeight = 0;
    if (fixedLayout_) {
        contentTop_ = 0;
        contentBottom_ = uis.height;
        contentHeight_ = uis.height;
    } else {
        contentTop_ = (availableHeight - contentHeight_) / 2;
        if (contentTop_ < 0)
            contentTop_ = 0;
        contentBottom_ = min(contentTop_ + contentHeight_, availableHeight);
    }

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
        if (auto *bitmap = dynamic_cast<BitmapWidget *>(widget)) {
            if (bitmap->HasFixedPosition())
                continue;
        }
        itemYs_[i] = y;
        y += itemHeights_[i];
    }

    int widest_bitmap = 0;
    int bitmap_top = 0;
    int bitmap_bottom = 0;
    bool bitmap_bounds_set = false;
    for (size_t i = 0; i < widgets_.size(); i++) {
        const auto *bitmap = dynamic_cast<const BitmapWidget *>(widgets_[i].get());
        if (!bitmap || widgets_[i]->IsHidden())
            continue;
        widest_bitmap = max(widest_bitmap, bitmap->ImageWidth());
        int top = itemYs_[i];
        int bottom = top + itemHeights_[i];
        if (!bitmap_bounds_set) {
            bitmap_top = top;
            bitmap_bottom = bottom;
            bitmap_bounds_set = true;
        } else {
            bitmap_top = min(bitmap_top, top);
            bitmap_bottom = max(bitmap_bottom, bottom);
        }
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

    int plaque_center_y = uis.height / 2;
    if (alignContentToBitmaps_ && bitmap_bounds_set)
        plaque_center_y = (bitmap_top + bitmap_bottom) / 2;

    if (plaque_ && !plaqueFixed_) {
        int total_plaque = plaqueRect_.height + (logo_ ? (logoRect_.height + 5) : 0);
        plaqueRect_.x = bitmapBaseX_ - CURSOR_WIDTH - plaqueRect_.width;
        plaqueRect_.y = plaque_center_y - total_plaque / 2;
    }
    if (logo_ && !logoFixed_) {
        int total_plaque = (plaque_ ? plaqueRect_.height : 0) + logoRect_.height + 5;
        logoRect_.x = bitmapBaseX_ - CURSOR_WIDTH - logoRect_.width;
        logoRect_.y = plaque_center_y + total_plaque / 2 - logoRect_.height;
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

void Menu::GetLayoutBounds(int *leftX, int *centerX, int *contentWidth) const
{
    int lx = 0;
    int cw = 0;

    if (customLayout_) {
        lx = customLeftX_;
    } else if (alignContentToBitmaps_ && hasBitmaps_) {
        lx = bitmapBaseX_;
    } else {
        lx = (uis.width / 2) - (CONCHAR_WIDTH * 16);
    }

    if (lx < 0)
        lx = 0;

    int max_width = max(0, uis.width - lx);
    if (customLayout_) {
        cw = min(customContentWidth_, max_width);
    } else if (alignContentToBitmaps_ && hasBitmaps_) {
        int right_pad = GenericSpacing(CONCHAR_WIDTH);
        cw = max(0, uis.width - lx - right_pad);
    } else {
        cw = uis.width - (lx * 2);
    }

    if (cw < 0)
        cw = 0;

    if (leftX)
        *leftX = lx;
    if (contentWidth)
        *contentWidth = cw;
    if (centerX)
        *centerX = lx + cw / 2;
}

int Menu::HitTest(int x, int y)
{
    int leftX = 0;
    int centerX = 0;
    int contentWidth = 0;
    GetLayoutBounds(&leftX, &centerX, &contentWidth);

    ColumnLayout columns = ComputeColumnLayout(widgets_, leftX, contentWidth);
    ApplyColumnLayout(columns, widgets_);

    for (size_t i = 0; i < widgets_.size(); i++) {
        Widget *widget = widgets_[i].get();
        if (!widget || widget->IsHidden() || !widget->IsSelectable())
            continue;

        int draw_y = itemYs_[i] - scrollY_;
        int height = itemHeights_[i];
        if (auto *bitmap = dynamic_cast<BitmapWidget *>(widget)) {
            if (bitmap->HasFixedPosition()) {
                draw_y = bitmap->FixedY();
                height = bitmap->ImageHeight();
            }
        }
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

bool Menu::HandleScrollBarPress(int mx, int my)
{
    int viewHeight = contentBottom_ - contentTop_;
    int maxScroll = max(0, contentHeight_ - viewHeight);
    if (viewHeight <= 0 || maxScroll <= 0)
        return false;

    int bar_x = uis.width - MLIST_SCROLLBAR_WIDTH;
    int bar_w = MLIST_SCROLLBAR_WIDTH - 1;
    int bar_y = contentTop_;
    int bar_h = viewHeight;
    if (bar_w <= 0 || bar_h <= 0)
        return false;

    if (mx < bar_x || mx >= bar_x + bar_w || my < bar_y || my >= bar_y + bar_h)
        return false;

    int arrow_h = max(MLIST_SCROLLBAR_WIDTH, lineHeight_);
    arrow_h = min(arrow_h, bar_h / 2);
    if (arrow_h < 1)
        arrow_h = max(1, bar_h / 2);

    int track_y = bar_y + arrow_h;
    int track_h = max(0, bar_h - arrow_h * 2);
    if (track_h <= 0)
        return true;

    float pageFrac = static_cast<float>(viewHeight) / contentHeight_;
    float scrollFrac = static_cast<float>(scrollY_) / maxScroll;
    int thumb_h = max(6, Q_rint(track_h * pageFrac));
    if (thumb_h > track_h)
        thumb_h = track_h;
    int thumb_y = track_y + Q_rint((track_h - thumb_h) * scrollFrac);

    if (my < bar_y + arrow_h) {
        scrollY_ = max(0, scrollY_ - lineHeight_);
        return true;
    }
    if (my >= bar_y + bar_h - arrow_h) {
        scrollY_ = min(maxScroll, scrollY_ + lineHeight_);
        return true;
    }
    if (my >= thumb_y && my < thumb_y + thumb_h) {
        scrollDragging_ = true;
        scrollDragOffset_ = my - thumb_y;
        return true;
    }
    if (my < thumb_y) {
        scrollY_ = max(0, scrollY_ - viewHeight);
        return true;
    }
    if (my >= thumb_y + thumb_h) {
        scrollY_ = min(maxScroll, scrollY_ + viewHeight);
        return true;
    }

    return true;
}

bool Menu::HandleScrollBarDrag(int mx, int my, bool mouseDown)
{
    int viewHeight = contentBottom_ - contentTop_;
    int maxScroll = max(0, contentHeight_ - viewHeight);
    if (viewHeight <= 0 || maxScroll <= 0) {
        scrollDragging_ = false;
        return false;
    }

    if (!mouseDown) {
        scrollDragging_ = false;
        return false;
    }

    int bar_x = uis.width - MLIST_SCROLLBAR_WIDTH;
    int bar_w = MLIST_SCROLLBAR_WIDTH - 1;
    int bar_h = viewHeight;
    int bar_y = contentTop_;
    int arrow_h = max(MLIST_SCROLLBAR_WIDTH, lineHeight_);
    arrow_h = min(arrow_h, bar_h / 2);
    if (arrow_h < 1)
        arrow_h = max(1, bar_h / 2);

    int track_y = bar_y + arrow_h;
    int track_h = max(0, bar_h - arrow_h * 2);
    if (track_h <= 0) {
        scrollDragging_ = false;
        return false;
    }

    float pageFrac = static_cast<float>(viewHeight) / contentHeight_;
    int thumb_h = max(6, Q_rint(track_h * pageFrac));
    if (thumb_h > track_h)
        thumb_h = track_h;
    int thumb_y = track_y + Q_rint((track_h - thumb_h) *
                                   (static_cast<float>(scrollY_) / maxScroll));

    if (!scrollDragging_) {
        bool over_thumb = mx >= bar_x && mx < bar_x + bar_w &&
            my >= thumb_y && my < thumb_y + thumb_h;
        if (!over_thumb)
            return false;
        scrollDragging_ = true;
        scrollDragOffset_ = my - thumb_y;
    }

    int new_thumb = Q_clip(my - scrollDragOffset_, track_y, track_y + track_h - thumb_h);
    float frac = static_cast<float>(new_thumb - track_y) /
        static_cast<float>(max(1, track_h - thumb_h));
    scrollY_ = Q_clip(Q_rint(frac * maxScroll), 0, maxScroll);
    return true;
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
    UpdateMenuBlurRect(allowBlur_, transparent_, menuTop, menuBottom);
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

    constexpr size_t kMaxLines = 8;
    std::vector<std::string> lines;
    lines.reserve(kMaxLines);

    const char *txt = text.c_str();
    std::string line;

    while (*txt && lines.size() < kMaxLines) {
        while (*txt && *txt <= 32)
            txt++;
        if (!*txt)
            break;

        const char *word_start = txt;
        while (*txt && *txt > 32)
            txt++;
        std::string word(word_start, txt - word_start);

        std::string candidate = line.empty() ? word : (line + " " + word);
        int candidate_width = UI_FontMeasureString(0, candidate.size(), candidate.c_str(), nullptr);
        if (!line.empty() && candidate_width > uis.width) {
            lines.push_back(line);
            line = word;
        } else {
            line = std::move(candidate);
        }
    }

    if (!line.empty() && lines.size() < kMaxLines)
        lines.push_back(line);

    int line_height = UI_FontLineHeight(1);
    int count = static_cast<int>(lines.size());
    for (int l = 0; l < count; l++) {
        const std::string &line_text = lines[l];
        int width = UI_FontMeasureString(0, line_text.size(), line_text.c_str(), nullptr);
        int x = (uis.width - width) / 2;
        int y = bottom - (count - l) * line_height;
        UI_FontDrawString(x, y, 0, line_text.size(), line_text.c_str(), COLOR_WHITE);
    }
}

static std::vector<std::string> WrapTooltipLines(const std::string &text, int maxWidth, size_t maxLines)
{
    std::vector<std::string> lines;
    if (text.empty() || maxWidth <= 0 || maxLines == 0)
        return lines;

    lines.reserve(maxLines);

    const char *ptr = text.c_str();
    std::string line;

    while (*ptr && lines.size() < maxLines) {
        while (*ptr && *ptr <= 32)
            ptr++;
        if (!*ptr)
            break;

        const char *word_start = ptr;
        while (*ptr && *ptr > 32)
            ptr++;

        std::string word(word_start, ptr - word_start);
        std::string candidate = line.empty() ? word : (line + " " + word);
        int width = UI_FontMeasureString(0, candidate.size(), candidate.c_str(), nullptr);
        if (!line.empty() && width > maxWidth) {
            lines.push_back(line);
            line = std::move(word);
        } else {
            line = std::move(candidate);
        }
    }

    if (!line.empty() && lines.size() < maxLines)
        lines.push_back(line);

    return lines;
}

static void DrawTooltipPanel(int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    color_t shadow = COLOR_RGBA(0, 0, 0, 120);
    R_DrawFill32(x + 2, y + 2, width, height, shadow);

    color_t fill = COLOR_RGBA(10, 12, 16, 230);
    color_t border = COLOR_SETA_U8(uis.color.active, 220);
    R_DrawFill32(x, y, width, height, fill);
    R_DrawFill32(x, y, width, 1, border);
    R_DrawFill32(x, y + height - 1, width, 1, border);
    R_DrawFill32(x, y, 1, height, border);
    R_DrawFill32(x + width - 1, y, 1, height, border);

    if (width > 2 && height > 2) {
        color_t highlight = COLOR_RGBA(255, 255, 255, 25);
        R_DrawFill32(x + 1, y + 1, width - 2, 1, highlight);
    }
}

static void DrawCursorTooltip(const std::string &text, int mouseX, int mouseY, int hintHeight)
{
    if (text.empty())
        return;

    int max_width = max(CONCHAR_WIDTH * 18, uis.width / 3);
    max_width = min(max_width, max(1, uis.width - (CONCHAR_WIDTH * 4)));

    constexpr size_t kMaxLines = 6;
    std::vector<std::string> lines = WrapTooltipLines(text, max_width, kMaxLines);
    if (lines.empty())
        return;

    int line_height = max(1, UI_FontLineHeight(1));
    int text_width = 0;
    for (const auto &line : lines) {
        int width = UI_FontMeasureString(0, line.size(), line.c_str(), nullptr);
        text_width = max(text_width, width);
    }

    int padding = max(4, CONCHAR_WIDTH / 2);
    int box_w = text_width + padding * 2;
    int box_h = static_cast<int>(lines.size()) * line_height + padding * 2;

    int offset = max(8, CONCHAR_WIDTH);
    int x = mouseX + offset;
    int y = mouseY + offset;
    int bottom_limit = max(0, uis.height - hintHeight);

    if (x + box_w > uis.width)
        x = mouseX - box_w - offset;
    if (x < 0)
        x = 0;
    if (y + box_h > bottom_limit)
        y = mouseY - box_h - offset;
    if (y < 0)
        y = 0;

    DrawTooltipPanel(x, y, box_w, box_h);

    int draw_x = x + padding;
    int draw_y = y + padding;
    for (const auto &line : lines) {
        UI_FontDrawString(draw_x, draw_y, 0, line.size(), line.c_str(), COLOR_WHITE);
        draw_y += line_height;
    }
}

void Menu::Draw()
{
    Layout();
    int hintHeight = HintHeight();

    int menuTop = compact_ ? max(0, contentTop_ - lineHeight_) : 0;
    int menuBottom = compact_ ? min(uis.height, contentBottom_ + lineHeight_) : uis.height;

    UpdateMenuBlurRect(allowBlur_, transparent_, menuTop, menuBottom);

    if (backgroundImage_) {
        R_DrawKeepAspectPic(0, menuTop, uis.width, menuBottom - menuTop, COLOR_WHITE, backgroundImage_);
    } else {
        R_DrawFill32(0, menuTop, uis.width, menuBottom - menuTop, backgroundColor_);
    }

    int leftX = 0;
    int centerX = 0;
    int contentWidth = 0;
    GetLayoutBounds(&leftX, &centerX, &contentWidth);

    ColumnLayout columns = ComputeColumnLayout(widgets_, leftX, contentWidth);
    ApplyColumnLayout(columns, widgets_);

    if (frameEnabled_) {
        int frameLeft = max(0, leftX - framePadding_);
        int frameRight = min(uis.width, leftX + contentWidth + framePadding_);
        int frameTop = max(0, contentTop_ - framePadding_);
        int frameBottom = min(uis.height, contentBottom_ + framePadding_);
        int frameWidth = frameRight - frameLeft;
        int frameHeight = frameBottom - frameTop;

        if (frameWidth > 0 && frameHeight > 0) {
            if (frameFill_.a > 0) {
                R_DrawFill32(frameLeft, frameTop, frameWidth, frameHeight, frameFill_);
            }
            if (frameBorder_.a > 0 && frameBorderWidth_ > 0) {
                int bw = frameBorderWidth_;
                R_DrawFill32(frameLeft, frameTop, frameWidth, bw, frameBorder_);
                R_DrawFill32(frameLeft, frameBottom - bw, frameWidth, bw, frameBorder_);
                R_DrawFill32(frameLeft, frameTop, bw, frameHeight, frameBorder_);
                R_DrawFill32(frameRight - bw, frameTop, bw, frameHeight, frameBorder_);
            }
        }
    }

    if (!title_.empty()) {
        UI_DrawString(uis.width / 2, menuTop, UI_CENTER | UI_ALTCOLOR, COLOR_WHITE, title_.c_str());
    }

    if (showPlayerName_) {
        if (!playerNameCvar_)
            playerNameCvar_ = Cvar_WeakGet("name");
        const char *name = playerNameCvar_ ? playerNameCvar_->string : "";
        if (name && *name) {
            int pad = GenericSpacing(CONCHAR_WIDTH);
            int text_y = max(0, pad / 2);
            UI_DrawString(uis.width - pad, text_y, UI_RIGHT, COLOR_WHITE, name);
        }
    }

    if (banner_)
        R_DrawPic(bannerRect_.x, bannerRect_.y, COLOR_WHITE, banner_);
    if (plaque_) {
        int draw_x = plaqueRect_.x;
        if (plaqueFixed_ && plaqueAnchor_ == 2)
            draw_x = max(0, uis.width - plaqueRect_.width - plaqueRect_.x);
        if (plaqueFixed_)
            R_DrawStretchPic(draw_x, plaqueRect_.y, plaqueRect_.width, plaqueRect_.height, COLOR_WHITE, plaque_);
        else
            R_DrawPic(draw_x, plaqueRect_.y, COLOR_WHITE, plaque_);
    }
    if (logo_) {
        int draw_x = logoRect_.x;
        if (logoFixed_ && logoAnchor_ == 2)
            draw_x = max(0, uis.width - logoRect_.width - logoRect_.x);
        if (logoFixed_)
            R_DrawStretchPic(draw_x, logoRect_.y, logoRect_.width, logoRect_.height, COLOR_WHITE, logo_);
        else
            R_DrawPic(draw_x, logoRect_.y, COLOR_WHITE, logo_);
    }

    if (!footerText_.empty() || !footerSubtext_.empty()) {
        int size = footerSizeSet_ ? footerSize_ : max(8, CONCHAR_HEIGHT - 4);
        color_t color = footerColorSet_ ? footerColor_ : COLOR_RGBA(180, 180, 180, 255);
        int line_h = UI_FontLineHeightSized(size);
        int pad = GenericSpacing(CONCHAR_HEIGHT) / 2;
        int y = uis.height - hintHeight - pad - line_h;
        if (!footerSubtext_.empty())
            y -= line_h;
        if (!footerText_.empty()) {
            UI_FontDrawStringSized(uis.width / 2, y, UI_CENTER, MAX_STRING_CHARS,
                                   footerText_.c_str(), color, size);
            y += line_h;
        }
        if (!footerSubtext_.empty()) {
            UI_FontDrawStringSized(uis.width / 2, y, UI_CENTER, MAX_STRING_CHARS,
                                   footerSubtext_.c_str(), color, size);
        }
    }

    int viewTop = contentTop_;
    int viewBottom = contentBottom_;

    hoverTextInput_ = false;
    std::string hover_status;
    bool overlay_open = FindExpandedOverlay(widgets_) != nullptr;
    for (size_t i = 0; i < widgets_.size(); i++) {
        Widget *widget = widgets_[i].get();
        if (!widget || widget->IsHidden())
            continue;

        int draw_y = itemYs_[i] - scrollY_;
        int height = itemHeights_[i];
        if (auto *bitmap = dynamic_cast<BitmapWidget *>(widget)) {
            if (bitmap->HasFixedPosition()) {
                draw_y = bitmap->FixedY();
                height = bitmap->ImageHeight();
            }
        }

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

        if (!overlay_open &&
            RectContains(widget->Rect(), uis.mouseCoords[0], uis.mouseCoords[1])) {
            if (dynamic_cast<FieldWidget *>(widget) || dynamic_cast<ComboWidget *>(widget)) {
                hoverTextInput_ = true;
            }
            if (hover_status.empty() && !widget->Status().empty()) {
                hover_status = widget->Status();
            }
        }

        widget->Draw(static_cast<int>(i) == focusedIndex_);
    }

    int viewHeight = contentBottom_ - contentTop_;
    int maxScroll = max(0, contentHeight_ - viewHeight);
    if (viewHeight > 0 && maxScroll > 0) {
        int bar_x = uis.width - MLIST_SCROLLBAR_WIDTH;
        int bar_w = MLIST_SCROLLBAR_WIDTH - 1;
        int bar_y = contentTop_;
        int bar_h = viewHeight;
        int arrow_h = max(MLIST_SCROLLBAR_WIDTH, lineHeight_);
        arrow_h = min(arrow_h, bar_h / 2);
        if (arrow_h < 1)
            arrow_h = max(1, bar_h / 2);

        int track_y = bar_y + arrow_h;
        int track_h = max(0, bar_h - arrow_h * 2);

        color_t track = COLOR_SETA_U8(uis.color.normal, 180);
        color_t border = COLOR_SETA_U8(uis.color.selection, 200);
        R_DrawFill32(bar_x, bar_y, bar_w, bar_h, track);
        R_DrawFill32(bar_x, bar_y, bar_w, 1, border);
        R_DrawFill32(bar_x, bar_y + bar_h - 1, bar_w, 1, border);
        R_DrawFill32(bar_x, bar_y, 1, bar_h, border);
        R_DrawFill32(bar_x + bar_w - 1, bar_y, 1, bar_h, border);

        if (arrow_h > 1) {
            int mouse_x = uis.mouseCoords[0];
            int mouse_y = uis.mouseCoords[1];
            bool mouse_down = Key_IsDown(K_MOUSE1) != 0;
            bool over_up = mouse_x >= bar_x && mouse_x < bar_x + bar_w &&
                mouse_y >= bar_y && mouse_y < bar_y + arrow_h;
            bool over_down = mouse_x >= bar_x && mouse_x < bar_x + bar_w &&
                mouse_y >= bar_y + bar_h - arrow_h && mouse_y < bar_y + bar_h;
            color_t arrow_fill = COLOR_SETA_U8(uis.color.selection, 200);
            if (over_up && mouse_down)
                arrow_fill = MenuAdjustColor(arrow_fill, -0.2f);
            else if (over_up)
                arrow_fill = MenuAdjustColor(arrow_fill, 0.15f);
            R_DrawFill32(bar_x + 1, bar_y + 1, max(1, bar_w - 2),
                         max(1, arrow_h - 2), arrow_fill);
            arrow_fill = COLOR_SETA_U8(uis.color.selection, 200);
            if (over_down && mouse_down)
                arrow_fill = MenuAdjustColor(arrow_fill, -0.2f);
            else if (over_down)
                arrow_fill = MenuAdjustColor(arrow_fill, 0.15f);
            R_DrawFill32(bar_x + 1, bar_y + bar_h - arrow_h + 1,
                         max(1, bar_w - 2), max(1, arrow_h - 2), arrow_fill);

            int text_h = max(1, UI_FontLineHeight(1));
            int up_y = bar_y + (arrow_h - text_h) / 2;
            int down_y = bar_y + bar_h - arrow_h + (arrow_h - text_h) / 2;
            int mid_x = bar_x + bar_w / 2;
            UI_DrawString(mid_x, up_y, UI_CENTER, uis.color.active, "^");
            UI_DrawString(mid_x, down_y, UI_CENTER, uis.color.active, "v");
        }

        if (track_h > 0) {
            float pageFrac = static_cast<float>(viewHeight) / contentHeight_;
            float scrollFrac = static_cast<float>(scrollY_) / maxScroll;
            int thumb_h = max(6, Q_rint(track_h * pageFrac));
            if (thumb_h > track_h)
                thumb_h = track_h;
            int thumb_y = track_y + Q_rint((track_h - thumb_h) * scrollFrac);
            color_t thumb = COLOR_SETA_U8(uis.color.active, 220);
            int mouse_x = uis.mouseCoords[0];
            int mouse_y = uis.mouseCoords[1];
            bool mouse_down = Key_IsDown(K_MOUSE1) != 0;
            vrect_t thumb_rect{ bar_x + 1, thumb_y, max(1, bar_w - 2), thumb_h };
            bool over_thumb = RectContains(thumb_rect, mouse_x, mouse_y);
            bool pressed = scrollDragging_ && mouse_down;
            if (pressed)
                thumb = MenuAdjustColor(thumb, -0.2f);
            else if (over_thumb)
                thumb = MenuAdjustColor(thumb, 0.15f);
            R_DrawFill32(bar_x + 1, thumb_y, max(1, bar_w - 2), thumb_h, thumb);
        }
    }

    for (const auto &widget : widgets_) {
        if (!widget || widget->IsHidden())
            continue;
        if (auto *dropdown = dynamic_cast<DropdownWidget *>(widget.get())) {
            dropdown->DrawOverlay();
            continue;
        }
        if (auto *image = dynamic_cast<ImageSpinWidget *>(widget.get())) {
            image->DrawOverlay();
            continue;
        }
    }

    if (!overlay_open && !hover_status.empty())
        DrawCursorTooltip(hover_status, uis.mouseCoords[0], uis.mouseCoords[1], hintHeight);

    DrawStatusText(status_, uis.height - hintHeight);
    UI_DrawHintBar(hintsLeft_, hintsRight_, uis.height);
}

Sound Menu::KeyEvent(int key)
{
    if (widgets_.empty())
        return Sound::NotHandled;

    if (Widget *overlay = FindExpandedOverlay(widgets_)) {
        Sound sound = overlay->KeyEvent(key);
        if (sound != Sound::NotHandled)
            return sound;
        if (key != K_MOUSE1)
            return Sound::NotHandled;
    }

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
        if (HandleScrollBarPress(uis.mouseCoords[0], uis.mouseCoords[1]))
            return Sound::Move;
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

    if (Widget *overlay = FindExpandedOverlay(widgets_)) {
        bool mouse_down = Key_IsDown(K_MOUSE1) != 0;
        if (auto *dropdown = dynamic_cast<DropdownWidget *>(overlay)) {
            if (dropdown->HandleMouseDrag(x, y, mouse_down))
                return;
        } else if (auto *image = dynamic_cast<ImageSpinWidget *>(overlay)) {
            if (image->HandleMouseDrag(x, y, mouse_down))
                return;
        }
        UpdateExpandedHover(widgets_, x, y);
        return;
    }

    bool mouse_down = Key_IsDown(K_MOUSE1) != 0;
    if (HandleScrollBarDrag(x, y, mouse_down))
        return;
    if (auto *slider = dynamic_cast<SliderWidget *>(FocusedWidget())) {
        if (slider->HandleMouseDrag(x, y, mouse_down))
            return;
    }

    int hit = HitTest(x, y);
    if (hit >= 0) {
        focusedIndex_ = hit;
        UpdateStatusFromFocus();
        if (mouse_down) {
            if (auto *slider = dynamic_cast<SliderWidget *>(FocusedWidget())) {
                if (slider->HandleMouseDrag(x, y, mouse_down))
                    return;
            }
        }
    }
}

} // namespace ui
