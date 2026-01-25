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
    int widthTotal = width;
    int spacing = RowSpacing();

    if (mlFlags & MLF_HEADER) {
        drawY = DrawHeader(drawY);
        drawHeight -= spacing;
    }

    if (mlFlags & MLF_SCROLLBAR) {
        int barHeight = drawHeight - spacing * 2;
        int yy = drawY + spacing;

        int bar_x = x + widthTotal - MLIST_SCROLLBAR_WIDTH;
        int bar_w = MLIST_SCROLLBAR_WIDTH - 1;
        color_t track = COLOR_SETA_U8(uis.color.normal, 120);
        color_t border = COLOR_SETA_U8(uis.color.selection, 160);
        R_DrawFill32(bar_x, yy, bar_w, barHeight, track);
        R_DrawFill32(bar_x, yy, bar_w, 1, border);
        R_DrawFill32(bar_x, yy + barHeight - 1, bar_w, 1, border);
        R_DrawFill32(bar_x, yy, 1, barHeight, border);
        R_DrawFill32(bar_x + bar_w - 1, yy, 1, barHeight, border);

        float pageFrac = 1.0f;
        float prestepFrac = 0.0f;
        if (numItems > maxItems) {
            pageFrac = static_cast<float>(maxItems) / numItems;
            prestepFrac = static_cast<float>(prestep) / numItems;
        }

        int thumb_h = max(6, Q_rint(barHeight * pageFrac));
        int thumb_y = yy + Q_rint((barHeight - thumb_h) * prestepFrac);
        color_t thumb = COLOR_SETA_U8(uis.color.active, 200);
        R_DrawFill32(bar_x + 1, thumb_y, max(1, bar_w - 2), thumb_h, thumb);
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

} // namespace ui
