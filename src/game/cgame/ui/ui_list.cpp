#include "ui/ui_internal.h"

namespace ui {

static int List_TextHeight(const ListWidget &list)
{
    if (list.fontSize > 0)
        return max(1, UI_FontLineHeightSized(list.fontSize));
    return max(CONCHAR_HEIGHT, UI_FontLineHeight(1));
}

static int List_StringWidth(const char *string, int flags, int fontSize)
{
    if (!string || !*string)
        return 0;
    int measure_flags = flags & ~(UI_CENTER | UI_RIGHT);
    if (fontSize > 0)
        return UI_FontMeasureStringSized(measure_flags, strlen(string), string, nullptr, fontSize);
    return UI_FontMeasureString(measure_flags, strlen(string), string, nullptr);
}

static void List_DrawAlignedString(int x, int y, int flags, color_t color,
                                   const char *string, int fontSize)
{
    if (!string || !*string)
        return;

    if (fontSize > 0) {
        int draw_x = x;
        if ((flags & UI_CENTER) == UI_CENTER)
            draw_x -= List_StringWidth(string, flags, fontSize) / 2;
        else if (flags & UI_RIGHT)
            draw_x -= List_StringWidth(string, flags, fontSize);

        UI_FontDrawStringSized(draw_x, y, flags, MAX_STRING_CHARS, string,
                               COLOR_SETA_U8(color, 255), fontSize);
        return;
    }

    UI_DrawString(x, y, flags, color, string);
}

static color_t List_DarkenColor(color_t color, float shade)
{
    float factor = Q_clipf(shade, 0.0f, 1.0f);
    return COLOR_RGBA(Q_rint(color.r * factor),
                      Q_rint(color.g * factor),
                      Q_rint(color.b * factor),
                      color.a);
}

static color_t List_AdjustColor(color_t color, float delta)
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

static void List_DrawPanel(int x, int y, int width, int height, color_t fill,
                           color_t border, int borderWidth)
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

bool ListWidget::ScrollbarMetrics(int *bar_x, int *bar_y, int *bar_w, int *bar_h,
                                  int *arrow_h, int *track_y, int *track_h,
                                  int *thumb_y, int *thumb_h) const
{
    if (!(mlFlags & MLF_SCROLLBAR))
        return false;

    int bx = x + width - MLIST_SCROLLBAR_WIDTH;
    int bw = MLIST_SCROLLBAR_WIDTH - 1;
    int by = y;
    int bh = height;
    if (bw <= 0 || bh <= 0)
        return false;

    int spacing = RowSpacing();
    int arrows = max(MLIST_SCROLLBAR_WIDTH, spacing);
    arrows = min(arrows, bh / 2);
    if (arrows < 1)
        arrows = max(1, bh / 2);

    int ty = by + arrows;
    int th = max(0, bh - arrows * 2);

    int thumbh = th;
    int thumby = ty;
    if (th > 0) {
        float pageFrac = 1.0f;
        float prestepFrac = 0.0f;
        if (numItems > 0 && maxItems > 0 && numItems > maxItems) {
            pageFrac = static_cast<float>(maxItems) / static_cast<float>(numItems);
            int maxStart = max(0, numItems - maxItems);
            if (maxStart > 0)
                prestepFrac = static_cast<float>(prestep) / static_cast<float>(maxStart);
        }
        pageFrac = Q_clipf(pageFrac, 0.0f, 1.0f);
        prestepFrac = Q_clipf(prestepFrac, 0.0f, 1.0f);

        thumbh = max(6, Q_rint(th * pageFrac));
        if (thumbh > th)
            thumbh = th;
        thumby = ty + Q_rint((th - thumbh) * prestepFrac);
    }

    if (bar_x)
        *bar_x = bx;
    if (bar_y)
        *bar_y = by;
    if (bar_w)
        *bar_w = bw;
    if (bar_h)
        *bar_h = bh;
    if (arrow_h)
        *arrow_h = arrows;
    if (track_y)
        *track_y = ty;
    if (track_h)
        *track_h = th;
    if (thumb_y)
        *thumb_y = thumby;
    if (thumb_h)
        *thumb_h = thumbh;

    return true;
}

int ListWidget::RowSpacing() const
{
    int spacing = rowSpacing > 0 ? rowSpacing : MLIST_SPACING;
    return max(spacing, List_TextHeight(*this));
}

void ListWidget::ValidatePrestep()
{
    if (prestep > numItems - maxItems)
        prestep = numItems - maxItems;
    if (prestep < 0)
        prestep = 0;
}

void ListWidget::AdjustPrestep()
{
    if (numItems > maxItems && curvalue > 0) {
        if (prestep > curvalue)
            prestep = curvalue;
        else if (prestep < curvalue - maxItems + 1)
            prestep = curvalue - maxItems + 1;
    } else {
        prestep = 0;
    }
}

void ListWidget::Init()
{
    int avail = height;
    int spacing = RowSpacing();
    if (mlFlags & MLF_HEADER)
        avail -= spacing;

    maxItems = spacing > 0 ? (avail / spacing) : 0;
    scrollDragging = false;
    ValidatePrestep();
}

void ListWidget::SetValue(int value)
{
    curvalue = Q_clip(value, 0, numItems - 1);
    if (onChange)
        onChange(this);
    AdjustPrestep();
}

void ListWidget::Sort(int offset, int (*cmpfunc)(const void *, const void *))
{
    if (items.empty())
        return;
    if (offset >= numItems)
        return;
    if (sortcol < 0 || sortcol >= static_cast<int>(columns.size()))
        return;

    void *selected = nullptr;
    if (curvalue >= 0 && curvalue < numItems)
        selected = items[curvalue];

    qsort(items.data() + offset, numItems - offset, sizeof(void *), cmpfunc);

    if (selected) {
        for (int i = 0; i < numItems; i++) {
            if (items[i] == selected) {
                curvalue = i;
                break;
            }
        }
    }
}

static void List_DrawString(int x, int y, int rowSpacing, int textHeight, int fontSize,
                            int flags, const ListColumn &column, color_t color,
                            const char *string)
{
    clipRect_t rc;
    int spacing = max(rowSpacing, textHeight);
    int text_y = y + (spacing - textHeight) / 2;

    rc.left = x;
    rc.right = x + column.width - 1;
    rc.top = text_y;
    rc.bottom = text_y + textHeight;

    if ((column.uiFlags & UI_CENTER) == UI_CENTER) {
        x += column.width / 2 - 1;
    } else if (column.uiFlags & UI_RIGHT) {
        x += column.width - MLIST_PRESTEP;
    } else {
        x += MLIST_PRESTEP;
    }

    R_SetClipRect(&rc);
    List_DrawAlignedString(x, text_y, column.uiFlags | flags, color, string, fontSize);
    R_SetClipRect(NULL);
}

int ListWidget::DrawHeader(int drawY) const
{
    if (!(mlFlags & MLF_HEADER))
        return drawY;

    int textHeight = List_TextHeight(*this);
    int spacing = RowSpacing();
    int xx = x;
    for (size_t j = 0; j < columns.size(); j++) {
        if (!columns[j].width)
            continue;
        int flags = UI_ALTCOLOR;
        color_t color = uis.color.normal;
        if (sortcol == static_cast<int>(j) && sortdir) {
            flags = 0;
            color = uis.color.active;
        }

        R_DrawFill32(xx, drawY, columns[j].width - 1, spacing - 1, color);
        if (!columns[j].name.empty())
            List_DrawString(xx, drawY, spacing, textHeight, fontSize, flags,
                            columns[j], COLOR_WHITE, columns[j].name.c_str());
        xx += columns[j].width;
    }
    return drawY + spacing;
}

int ListWidget::DrawItems(int drawY) const
{
    int height_left = height;
    int spacing = RowSpacing();
    int textHeight = List_TextHeight(*this);
    if (mlFlags & MLF_HEADER)
        height_left -= spacing;

    int yy = drawY;
    int end = min(numItems, prestep + maxItems);
    color_t color = COLOR_WHITE;

    for (int i = prestep; i < end; i++) {
        if (yy + spacing > drawY + height_left)
            break;

        bool selected = (i == curvalue);
        if (alternateRows && !selected && (i & 1)) {
            int xx = x;
            for (size_t j = 0; j < columns.size(); j++) {
                if (!columns[j].width)
                    continue;
                color_t base = uis.color.normal;
                if (sortcol == static_cast<int>(j) && sortdir)
                    base = uis.color.active;
                R_DrawFill32(xx, yy, columns[j].width - 1, spacing,
                             List_DarkenColor(base, alternateShade));
                xx += columns[j].width;
            }
        }

        if (i == curvalue) {
            int xx = x;
            for (size_t j = 0; j < columns.size(); j++) {
                if (!columns[j].width)
                    continue;
                R_DrawFill32(xx, yy, columns[j].width - 1, spacing, uis.color.selection);
                xx += columns[j].width;
            }
        }

        char *s = static_cast<char *>(items[i]) + extrasize;
        if (mlFlags & MLF_COLOR)
            color = COLOR_U32(*((uint32_t *)(s - 4)));

        int xx = x;
        for (size_t j = 0; j < columns.size(); j++) {
            if (!*s)
                break;
            if (columns[j].width) {
                List_DrawString(xx, yy, spacing, textHeight, fontSize, 0,
                                columns[j], color, s);
                xx += columns[j].width;
            }
            s += strlen(s) + 1;
        }

        yy += spacing;
    }

    return yy;
}

void ListWidget::Draw()
{
    int drawY = y;
    int drawHeight = height;
    int spacing = RowSpacing();

    if (mlFlags & MLF_HEADER) {
        drawY = DrawHeader(drawY);
        drawHeight -= spacing;
    }

    if (mlFlags & MLF_SCROLLBAR) {
        int bar_x = 0;
        int bar_y = 0;
        int bar_w = 0;
        int bar_h = 0;
        int arrow_h = 0;
        int track_y = 0;
        int track_h = 0;
        int thumb_y = 0;
        int thumb_h = 0;
        if (ScrollbarMetrics(&bar_x, &bar_y, &bar_w, &bar_h, &arrow_h,
                             &track_y, &track_h, &thumb_y, &thumb_h)) {
            color_t track = COLOR_SETA_U8(uis.color.normal, 180);
            color_t border = COLOR_SETA_U8(uis.color.selection, 200);
            List_DrawPanel(bar_x, bar_y, bar_w, bar_h, track, border, 1);

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
                    arrow_fill = List_AdjustColor(arrow_fill, -0.2f);
                else if (over_up)
                    arrow_fill = List_AdjustColor(arrow_fill, 0.15f);
                List_DrawPanel(bar_x + 1, bar_y + 1, max(1, bar_w - 2),
                               max(1, arrow_h - 2), arrow_fill, COLOR_A(0), 0);
                arrow_fill = COLOR_SETA_U8(uis.color.selection, 200);
                if (over_down && mouse_down)
                    arrow_fill = List_AdjustColor(arrow_fill, -0.2f);
                else if (over_down)
                    arrow_fill = List_AdjustColor(arrow_fill, 0.15f);
                List_DrawPanel(bar_x + 1, bar_y + bar_h - arrow_h + 1,
                               max(1, bar_w - 2), max(1, arrow_h - 2),
                               arrow_fill, COLOR_A(0), 0);

                int text_h = List_TextHeight(*this);
                int up_y = bar_y + (arrow_h - text_h) / 2;
                int down_y = bar_y + bar_h - arrow_h + (arrow_h - text_h) / 2;
                int mid_x = bar_x + bar_w / 2;
                UI_DrawString(mid_x, up_y, UI_CENTER, uis.color.active, "^");
                UI_DrawString(mid_x, down_y, UI_CENTER, uis.color.active, "v");
            }

            if (track_h > 0 && thumb_h > 0) {
                color_t thumb = COLOR_SETA_U8(uis.color.active, 220);
                int mouse_x = uis.mouseCoords[0];
                int mouse_y = uis.mouseCoords[1];
                bool mouse_down = Key_IsDown(K_MOUSE1) != 0;
                vrect_t thumb_rect{ bar_x + 1, thumb_y, max(1, bar_w - 2), thumb_h };
                bool over_thumb = RectContains(thumb_rect, mouse_x, mouse_y);
                bool pressed = scrollDragging && mouse_down;
                if (pressed)
                    thumb = List_AdjustColor(thumb, -0.2f);
                else if (over_thumb)
                    thumb = List_AdjustColor(thumb, 0.15f);
                List_DrawPanel(bar_x + 1, thumb_y, max(1, bar_w - 2), thumb_h,
                               thumb, COLOR_SETA_U8(uis.color.active, 255), 1);
            }
        }
    }

    int xx = x;
    for (size_t j = 0; j < columns.size(); j++) {
        if (!columns[j].width)
            continue;
        color_t color = uis.color.normal;
        if (sortcol == static_cast<int>(j) && sortdir)
            color = uis.color.active;

        R_DrawFill32(xx, drawY, columns[j].width - 1, drawHeight, color);
        xx += columns[j].width;
    }

    DrawItems(drawY);
}

int ListWidget::HitTestRow(int mx, int my) const
{
    int yy = y;
    int spacing = RowSpacing();
    if (mlFlags & MLF_HEADER)
        yy += spacing;

    int max_x = x + width;
    if (mlFlags & MLF_SCROLLBAR)
        max_x -= MLIST_SCROLLBAR_WIDTH;

    if (mx < x || mx >= max_x)
        return -1;
    if (my < yy)
        return -1;

    int row = (my - yy) / spacing;
    int index = prestep + row;
    if (index < 0 || index >= numItems)
        return -1;
    return index;
}

Sound ListWidget::ClickAt(int mx, int my)
{
    int index = HitTestRow(mx, my);
    if (index < 0)
        return Sound::NotHandled;

    curvalue = index;
    if (onChange)
        onChange(this);

    if (onActivate)
        return onActivate(this);
    return Sound::In;
}

Sound ListWidget::KeyEvent(int key)
{
    switch (key) {
    case K_UPARROW:
    case K_KP_UPARROW:
    case 'k':
        if (curvalue > 0) {
            curvalue--;
            if (onChange)
                onChange(this);
            AdjustPrestep();
            return Sound::Move;
        }
        return Sound::Beep;
    case K_DOWNARROW:
    case K_KP_DOWNARROW:
    case 'j':
        if (curvalue < numItems - 1) {
            curvalue++;
            if (onChange)
                onChange(this);
            AdjustPrestep();
            return Sound::Move;
        }
        return Sound::Beep;
    case K_HOME:
    case K_KP_HOME:
        prestep = 0;
        curvalue = 0;
        if (onChange)
            onChange(this);
        return Sound::Move;
    case K_END:
    case K_KP_END:
        if (!numItems) {
            prestep = 0;
            curvalue = 0;
        } else {
            if (numItems > maxItems)
                prestep = numItems - maxItems;
            curvalue = numItems - 1;
        }
        if (onChange)
            onChange(this);
        return Sound::Move;
    case K_MWHEELUP:
        prestep -= Key_IsDown(K_CTRL) ? 4 : 2;
        ValidatePrestep();
        return Sound::Silent;
    case K_MWHEELDOWN:
        prestep += Key_IsDown(K_CTRL) ? 4 : 2;
        ValidatePrestep();
        return Sound::Silent;
    case K_PGUP:
    case K_KP_PGUP:
        if (curvalue > 0) {
            curvalue -= maxItems - 1;
            if (curvalue < 0)
                curvalue = 0;
            if (onChange)
                onChange(this);
            AdjustPrestep();
            return Sound::Move;
        }
        return Sound::Beep;
    case K_PGDN:
    case K_KP_PGDN:
        if (curvalue < numItems - 1) {
            curvalue += maxItems - 1;
            if (curvalue > numItems - 1)
                curvalue = numItems - 1;
            if (onChange)
                onChange(this);
            AdjustPrestep();
            return Sound::Move;
        }
        return Sound::Beep;
    case K_ENTER:
    case K_KP_ENTER:
        if (onActivate)
            return onActivate(this);
        return Sound::In;
    case K_MOUSE1:
        if (HandleMouseDrag(uis.mouseCoords[0], uis.mouseCoords[1], true))
            return Sound::Move;
        return ClickAt(uis.mouseCoords[0], uis.mouseCoords[1]);
    default:
        break;
    }

    return Sound::NotHandled;
}

void ListWidget::HoverAt(int mx, int my)
{
    int index = HitTestRow(mx, my);
    if (index < 0 || index == curvalue)
        return;

    curvalue = index;
    if (onChange)
        onChange(this);
    AdjustPrestep();
}

bool ListWidget::HandleMouseDrag(int mx, int my, bool mouseDown)
{
    if (!(mlFlags & MLF_SCROLLBAR)) {
        scrollDragging = false;
        return false;
    }

    int bar_x = 0;
    int bar_y = 0;
    int bar_w = 0;
    int bar_h = 0;
    int arrow_h = 0;
    int track_y = 0;
    int track_h = 0;
    int thumb_y = 0;
    int thumb_h = 0;
    if (!ScrollbarMetrics(&bar_x, &bar_y, &bar_w, &bar_h, &arrow_h,
                          &track_y, &track_h, &thumb_y, &thumb_h)) {
        scrollDragging = false;
        return false;
    }

    if (!mouseDown) {
        scrollDragging = false;
        return false;
    }

    bool in_bar = mx >= bar_x && mx < bar_x + bar_w && my >= bar_y && my < bar_y + bar_h;
    if (!scrollDragging && in_bar) {
        if (my < bar_y + arrow_h) {
            prestep -= 1;
            ValidatePrestep();
            return true;
        }
        if (my >= bar_y + bar_h - arrow_h) {
            prestep += 1;
            ValidatePrestep();
            return true;
        }
        if (track_h > 0) {
            if (my >= thumb_y && my < thumb_y + thumb_h) {
                scrollDragging = true;
                scrollDragOffset = my - thumb_y;
                return true;
            }
            if (my < thumb_y) {
                prestep -= max(1, maxItems);
                ValidatePrestep();
                return true;
            }
            if (my >= thumb_y + thumb_h) {
                prestep += max(1, maxItems);
                ValidatePrestep();
                return true;
            }
        }
    }

    if (!scrollDragging)
        return false;

    int maxStart = max(0, numItems - maxItems);
    if (maxStart <= 0 || track_h <= 0 || thumb_h <= 0) {
        scrollDragging = false;
        prestep = 0;
        return true;
    }

    int new_thumb = Q_clip(my - scrollDragOffset, track_y, track_y + track_h - thumb_h);
    float frac = static_cast<float>(new_thumb - track_y) /
        static_cast<float>(max(1, track_h - thumb_h));
    prestep = Q_clip(Q_rint(frac * maxStart), 0, maxStart);
    ValidatePrestep();
    return true;
}

} // namespace ui
