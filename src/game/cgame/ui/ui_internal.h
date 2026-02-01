#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "shared/shared.h"
#include "common/common.h"

#include "client/input.h"
#include "client/ui.h"
#include "client/keys.h"
#include "client/sound/sound.h"
#include "common/cmd.h"
#include "common/cvar.h"
#include "common/field.h"
#include "common/files.h"
#include "common/json.h"
#include "common/mapdb.h"
#include "common/zone.h"
#include "renderer/renderer.h"

#define UI_Malloc(s)        Z_TagMalloc((s), TAG_UI)
#define UI_Mallocz(s)       Z_TagMallocz((s), TAG_UI)
#define UI_CopyString(s)    Z_TagCopyString((s), TAG_UI)

#define UI_DEFAULT_FILE     APPLICATION ".json"

extern "C" void UI_Sys_UpdateRefConfig(void);
extern "C" void UI_Sys_UpdateGameDir(void);
extern "C" void UI_Sys_UpdateTimes(void);
extern "C" void UI_Sys_UpdateNetFrom(void);
extern "C" void UI_Sys_SetMenuBlurRect(const clipRect_t *rect);
extern "C" int UI_FontDrawString(int x, int y, int flags, size_t maxChars,
                                 const char *string, color_t color);
extern "C" int UI_FontMeasureString(int flags, size_t maxChars, const char *string,
                                    int *out_height);
extern "C" int UI_FontLineHeight(int scale);
extern "C" int UI_FontDrawStringSized(int x, int y, int flags, size_t maxChars,
                                      const char *string, color_t color, int size);
extern "C" int UI_FontMeasureStringSized(int flags, size_t maxChars, const char *string,
                                         int *out_height, int size);
extern "C" int UI_FontLineHeightSized(int size);
extern "C" qhandle_t UI_FontLegacyHandle(void);
char *SV_GetSaveInfo(const char *dir);
void UI_SetClipboardData(const char *text);

#define MAX_MENU_DEPTH 8
#define NUM_CURSOR_FRAMES 15
#define CURSOR_WIDTH 32
#define CURSOR_OFFSET 25
#define UI_CURSOR_SIZE 12
#define MAX_COLUMNS 8
#define SLIDER_RANGE 10
#define MAX_PLAYERMODELS 1024

#define MLIST_SPACING           (GenericSpacing(CONCHAR_HEIGHT))
#define MLIST_BORDER_WIDTH      1
#define MLIST_SCROLLBAR_WIDTH   (GenericSpacing(CONCHAR_WIDTH))
#define MLIST_PRESTEP           3
#define MLIST_PADDING           (MLIST_PRESTEP * 2)

#define MLF_HEADER      BIT(0)
#define MLF_SCROLLBAR   BIT(1)
#define MLF_COLOR       BIT(2)

namespace ui {

enum class Sound {
    NotHandled,
    Silent,
    In,
    Move,
    Out,
    Beep
};

enum class ConditionKind {
    InGame,
    Deathmatch,
    Cvar
};

enum class ConditionOp {
    Exists,
    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual
};

struct MenuCondition {
    ConditionKind kind = ConditionKind::InGame;
    ConditionOp op = ConditionOp::Exists;
    std::string name;
    std::string value;
    bool negate = false;
};

struct UiColors {
    color_t background{};
    color_t normal{};
    color_t active{};
    color_t selection{};
    color_t disabled{};
};

struct playerModelInfo_t {
    int nskins = 0;
    char **skindisplaynames = nullptr;
    char *directory = nullptr;
};

struct UiState {
    bool initialized = false;
    unsigned realtime = 0;
    int canvas_width = 0;
    int canvas_height = 0;
    int width = 0;
    int height = 0;
    float scale = 1.0f;
    int mouseCoords[2] = {0, 0};
    bool entersound = false;
    bool keywait = false;
    bool transparent = false;

    qhandle_t backgroundHandle = 0;
    qhandle_t fontHandle = 0;
    qhandle_t cursorHandle = 0;
    int cursorWidth = 0;
    int cursorHeight = 0;
    qhandle_t cursorTextHandle = 0;
    int cursorTextWidth = 0;
    int cursorTextHeight = 0;
    qhandle_t bitmapCursors[NUM_CURSOR_FRAMES]{};

    UiColors color{};

    int numPlayerModels = 0;
    playerModelInfo_t pmi[MAX_PLAYERMODELS]{};
    char weaponModel[32]{};
};

extern UiState uis;

struct UiHint {
    int key = 0;
    std::string label;
    std::string keyLabel;
};

inline bool RectContains(const vrect_t &rect, int x, int y)
{
    return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
}

inline int TextWidth(const char *s)
{
    if (!s || !*s)
        return 0;
    return UI_FontMeasureString(0, strlen(s), s, nullptr);
}

inline int TextWidth(const std::string &s)
{
    if (s.empty())
        return 0;
    return UI_FontMeasureString(0, s.size(), s.c_str(), nullptr);
}

inline int GenericSpacing(int x)
{
    return x + x / 4;
}

class MenuPage {
public:
    virtual ~MenuPage() = default;
    virtual const char *Name() const = 0;
    virtual void OnOpen() {}
    virtual void OnClose() {}
    virtual void SetArgs(const char *args) { (void)args; }
    virtual void Draw() = 0;
    virtual Sound KeyEvent(int key) { (void)key; return Sound::NotHandled; }
    virtual Sound CharEvent(int ch) { (void)ch; return Sound::NotHandled; }
    virtual void MouseEvent(int x, int y, bool down) { (void)x; (void)y; (void)down; }
    virtual void Frame(int msec) { (void)msec; }
    virtual void StatusEvent(const serverStatus_t *status) { (void)status; }
    virtual void ErrorEvent(const netadr_t *from) { (void)from; }
    virtual bool IsTransparent() const { return uis.transparent; }
    virtual bool WantsTextCursor(int x, int y) const { (void)x; (void)y; return false; }
};

class Widget {
public:
    virtual ~Widget() = default;
    virtual int Height(int lineHeight) const;
    virtual void Layout(int x, int y, int width, int lineHeight);
    virtual void Draw(bool focused) const = 0;
    virtual Sound KeyEvent(int key) { (void)key; return Sound::NotHandled; }
    virtual Sound CharEvent(int ch) { (void)ch; return Sound::NotHandled; }
    virtual Sound Activate() { return Sound::NotHandled; }
    virtual void OnOpen() {}
    virtual void OnClose() {}
    virtual bool UsesColumns() const { return false; }
    bool IsSelectable() const { return selectable_ && !IsHidden() && !IsDisabled(); }
    bool IsHidden() const { return hidden_ || hiddenByCondition_; }
    bool IsDisabled() const { return disabled_ || disabledByCondition_; }
    const vrect_t &Rect() const { return rect_; }
    const std::string &Label() const { return label_; }
    const std::string &Status() const { return status_; }
    const char *LabelText() const;
    void SetLabel(std::string label) { label_ = std::move(label); }
    void SetStatus(std::string status) { status_ = std::move(status); }
    void SetLabelCvar(cvar_t *cvar) { labelCvar_ = cvar; }
    void SetDisabled(bool disabled) { disabled_ = disabled; }
    void SetHidden(bool hidden) { hidden_ = hidden; }
    void SetSelectable(bool selectable) { selectable_ = selectable; }
    void SetOwner(class Menu *menu) { owner_ = menu; }
    void SetShowConditions(std::vector<MenuCondition> conditions);
    void SetEnableConditions(std::vector<MenuCondition> conditions);
    void SetDefault(bool value) { default_ = value; }
    void SetDefaultConditions(std::vector<MenuCondition> conditions);
    void UpdateConditions();
    bool IsDefault() const { return default_ || defaultByCondition_; }
    bool UseColumns() const { return useColumns_; }
    int ColumnLabelWidth() const { return columnLabelWidth_; }
    int ColumnValueX() const { return columnValueX_; }
    int ColumnValueWidth() const { return columnValueWidth_; }
    void SetColumns(int labelWidth, int valueX, int valueWidth);
    void ClearColumns();

protected:
    vrect_t rect_{};
    std::string label_{};
    std::string status_{};
    bool selectable_ = true;
    bool hidden_ = false;
    bool disabled_ = false;
    bool hiddenByCondition_ = false;
    bool disabledByCondition_ = false;
    bool default_ = false;
    bool defaultByCondition_ = false;
    std::vector<MenuCondition> showConditions_{};
    std::vector<MenuCondition> enableConditions_{};
    std::vector<MenuCondition> defaultConditions_{};
    class Menu *owner_ = nullptr;
    cvar_t *labelCvar_ = nullptr;
    mutable std::string labelCache_{};
    int columnLabelWidth_ = 0;
    int columnValueX_ = 0;
    int columnValueWidth_ = 0;
    bool useColumns_ = false;
};

class Menu : public MenuPage {
public:
    explicit Menu(std::string name);

    const char *Name() const override { return name_.c_str(); }
    void OnOpen() override;
    void OnClose() override;
    void Draw() override;
    Sound KeyEvent(int key) override;
    Sound CharEvent(int ch) override;
    void MouseEvent(int x, int y, bool down) override;
    bool IsTransparent() const override { return transparent_; }
    bool WantsTextCursor(int x, int y) const override { (void)x; (void)y; return hoverTextInput_; }

    void AddWidget(std::unique_ptr<Widget> widget);

    void SetTitle(std::string title) { title_ = std::move(title); }
    void SetStatus(std::string status) { status_ = std::move(status); }
    void SetCompact(bool compact) { compact_ = compact; }
    void SetTransparent(bool transparent) { transparent_ = transparent; }
    void SetAllowBlur(bool allow) { allowBlur_ = allow; }
    void SetLayoutBounds(int leftX, int contentWidth);
    void ClearLayoutBounds();
    void SetBackground(color_t color);
    void SetBackgroundImage(qhandle_t image, bool transparent);
    void SetBanner(qhandle_t banner, const vrect_t &rc);
    void SetPlaque(qhandle_t plaque, const vrect_t &rc);
    void SetLogo(qhandle_t logo, const vrect_t &rc);
    void SetShowPlayerName(bool show) { showPlayerName_ = show; }
    void SetAlignContentToBitmaps(bool align) { alignContentToBitmaps_ = align; }
    void SetFixedLayout(bool fixed) { fixedLayout_ = fixed; }
    void SetPlaqueFixed(bool fixed) { plaqueFixed_ = fixed; }
    void SetLogoFixed(bool fixed) { logoFixed_ = fixed; }
    void SetPlaqueAnchor(int anchor) { plaqueAnchor_ = anchor; }
    void SetLogoAnchor(int anchor) { logoAnchor_ = anchor; }
    void SetFooterText(std::string text) { footerText_ = std::move(text); }
    void SetFooterSubtext(std::string text) { footerSubtext_ = std::move(text); }
    void SetFooterColor(color_t color) { footerColor_ = color; footerColorSet_ = true; }
    void SetFooterSize(int size) { footerSize_ = size; footerSizeSet_ = true; }
    void SetCloseCommand(std::string command) { closeCommand_ = std::move(command); }
    void SetFrameStyle(bool enabled, color_t fill, color_t border, int padding, int borderWidth);
    void ClearHints();
    void AddHintLeft(int key, std::string label, std::string keyLabel = {});
    void AddHintRight(int key, std::string label, std::string keyLabel = {});
    int HintHeight() const;
    void RefreshLayout() { Layout(); }
    int ContentTop() const { return contentTop_; }
    int ContentBottom() const { return contentBottom_; }
    int ContentHeight() const { return contentHeight_; }
    bool HoverTextInput() const { return hoverTextInput_; }

private:
    void BuildDefaultHints();
    void Layout();
    int HitTest(int x, int y);
    void EnsureVisible(int index);
    int FindNextSelectable(int start, int dir) const;
    int FocusedIndex() const { return focusedIndex_; }
    Widget *FocusedWidget();
    const Widget *FocusedWidget() const;
    void UpdateStatusFromFocus();
    void GetLayoutBounds(int *leftX, int *centerX, int *contentWidth) const;
    bool HandleScrollBarPress(int mx, int my);
    bool HandleScrollBarDrag(int mx, int my, bool mouseDown);

    std::string name_;
    std::string title_;
    std::string status_;
    std::vector<std::unique_ptr<Widget>> widgets_;

    bool compact_ = false;
    bool transparent_ = false;
    bool allowBlur_ = true;
    bool frameEnabled_ = false;
    int framePadding_ = 0;
    int frameBorderWidth_ = 1;
    color_t frameFill_{};
    color_t frameBorder_{};

    qhandle_t backgroundImage_ = 0;
    color_t backgroundColor_{};

    qhandle_t banner_ = 0;
    vrect_t bannerRect_{};
    qhandle_t plaque_ = 0;
    vrect_t plaqueRect_{};
    qhandle_t logo_ = 0;
    vrect_t logoRect_{};

    int contentTop_ = 0;
    int contentBottom_ = 0;
    int lineHeight_ = 0;
    int focusedIndex_ = -1;
    int scrollY_ = 0;
    int contentHeight_ = 0;
    int bitmapBaseX_ = 0;
    bool hasBitmaps_ = false;
    std::string closeCommand_{};
    std::vector<UiHint> hintsLeft_;
    std::vector<UiHint> hintsRight_;
    std::vector<int> itemYs_;
    std::vector<int> itemHeights_;
    bool hoverTextInput_ = false;
    bool customLayout_ = false;
    int customLeftX_ = 0;
    int customContentWidth_ = 0;
    bool showPlayerName_ = false;
    bool alignContentToBitmaps_ = false;
    cvar_t *playerNameCvar_ = nullptr;
    bool fixedLayout_ = false;
    bool plaqueFixed_ = false;
    bool logoFixed_ = false;
    int plaqueAnchor_ = 0;
    int logoAnchor_ = 0;
    std::string footerText_{};
    std::string footerSubtext_{};
    color_t footerColor_{};
    bool footerColorSet_ = false;
    int footerSize_ = 0;
    bool footerSizeSet_ = false;
    bool scrollDragging_ = false;
    int scrollDragOffset_ = 0;
};

class ActionWidget : public Widget {
public:
    explicit ActionWidget(std::string command);
    void Layout(int x, int y, int width, int lineHeight) override;
    void Draw(bool focused) const override;
    Sound Activate() override;
    void SetAlignLeft(bool alignLeft) { alignLeft_ = alignLeft; }
    void SetCommandCvar(cvar_t *cvar) { commandCvar_ = cvar; }
    bool AlignLeft() const { return alignLeft_; }

private:
    std::string command_;
    cvar_t *commandCvar_ = nullptr;
    bool alignLeft_ = false;
};

class BitmapWidget : public Widget {
public:
    BitmapWidget(qhandle_t image, qhandle_t imageSelected, std::string command);
    int Height(int lineHeight) const override;
    void Layout(int x, int y, int width, int lineHeight) override;
    void Draw(bool focused) const override;
    Sound Activate() override;
    int ImageWidth() const { return imageWidth_; }
    int ImageHeight() const { return imageHeight_; }
    void SetDrawSize(int width, int height);
    void SetPosition(int x, int y);
    bool HasFixedPosition() const { return fixedPosition_; }
    int FixedX() const { return fixedX_; }
    int FixedY() const { return fixedY_; }
    void SetAnchorCenter() { anchor_ = FixedAnchor::Center; }
    void SetAnchorLeft() { anchor_ = FixedAnchor::Left; }
    void SetAnchorRight() { anchor_ = FixedAnchor::Right; }
    int Anchor() const { return static_cast<int>(anchor_); }

private:
    qhandle_t image_;
    qhandle_t imageSelected_;
    std::string command_;
    int imageWidth_ = 0;
    int imageHeight_ = 0;
    int nativeWidth_ = 0;
    int nativeHeight_ = 0;
    bool customSize_ = false;
    bool fixedPosition_ = false;
    int fixedX_ = 0;
    int fixedY_ = 0;
    enum class FixedAnchor {
        Center,
        Left,
        Right
    };
    FixedAnchor anchor_ = FixedAnchor::Center;
};

class MenuButtonWidget : public BitmapWidget {
public:
    MenuButtonWidget(qhandle_t image, qhandle_t imageSelected, std::string command);
    void Draw(bool focused) const override;
    void SetTextOffset(int offset) { textOffset_ = offset; textOffsetSet_ = true; }
    void SetTextSize(int size) { textSize_ = size; textSizeSet_ = true; }
    void SetTextColor(color_t color) { textColor_ = color; textColorSet_ = true; }
    void SetSelectedTextColor(color_t color) { selectedTextColor_ = color; selectedTextColorSet_ = true; }

private:
    int textOffset_ = 0;
    bool textOffsetSet_ = false;
    int textSize_ = 0;
    bool textSizeSet_ = false;
    color_t textColor_{};
    bool textColorSet_ = false;
    color_t selectedTextColor_{};
    bool selectedTextColorSet_ = false;
};

class SliderWidget : public Widget {
public:
    SliderWidget(std::string label, cvar_t *cvar, float minValue, float maxValue, float step);
    void Draw(bool focused) const override;
    Sound KeyEvent(int key) override;
    void OnOpen() override;
    void OnClose() override;
    bool HandleMouseDrag(int mx, int my, bool mouseDown);
    bool IsDragging() const { return dragging_; }
    bool UsesColumns() const override { return true; }

private:
    void ValueRect(int *out_x, int *out_w) const;
    bool SetValueFromMouse(int mx, int value_x, int value_w);

    cvar_t *cvar_;
    float minValue_;
    float maxValue_;
    float step_;
    float curValue_ = 0.0f;
    bool modified_ = false;
    mutable float hoverFrac_ = 0.0f;
    mutable unsigned hoverTime_ = 0;
    bool dragging_ = false;
};

enum class SpinType {
    Index,
    String,
    Pair,
    Toggle,
    Bitfield,
    Episode,
    Unit
};

class SpinWidget : public Widget {
public:
    SpinWidget(std::string label, cvar_t *cvar, SpinType type);
    void Draw(bool focused) const override;
    Sound KeyEvent(int key) override;
    void OnOpen() override;
    void OnClose() override;
    bool UsesColumns() const override { return true; }

    void AddOption(std::string label);
    void AddOption(std::string label, std::string value);
    void ClearOptions();
    void SetBitfield(int mask, bool negate);
    void SetIndices(std::vector<int> indices);
    void SetCurrent(int value) { curValue_ = value; }
    int Current() const { return curValue_; }

protected:
    std::string CurrentLabel() const;
    void Advance(int dir);
    void SyncFromCvar();
    void ApplyToCvar();

    cvar_t *cvar_;
    SpinType type_;
    std::vector<std::string> labels_;
    std::vector<std::string> values_;
    std::vector<int> indices_;
    int curValue_ = -1;
    int mask_ = 0;
    bool negate_ = false;
};

class DropdownWidget : public SpinWidget {
public:
    DropdownWidget(std::string label, cvar_t *cvar, SpinType type);
    int Height(int lineHeight) const override;
    void Draw(bool focused) const override;
    void DrawOverlay() const;
    Sound Activate() override;
    Sound KeyEvent(int key) override;
    void OnClose() override;
    bool IsExpanded() const { return expanded_; }
    bool HoverAt(int mx, int my);
    bool HandleMouseDrag(int mx, int my, bool mouseDown);

private:
    void OpenList();
    void CloseList(bool accept);
    void EnsureListVisible(int visibleCount);
    vrect_t ComputeListRect(int visibleCount, int rowHeight, int *out_scroll_width) const;
    int ComputeVisibleCount(int rowHeight) const;
    int RowHeight() const;
    int ValueX(const char *label) const;
    int ValueWidth(int valueX) const;
    int HitTestList(int mx, int my, const vrect_t &listRect, int rowHeight, int visibleCount,
                    int scrollWidth) const;

    bool expanded_ = false;
    int listStart_ = 0;
    int listCursor_ = -1;
    int openValue_ = -1;
    bool scrollDragging_ = false;
    int scrollDragOffset_ = 0;
};

enum class SwitchStyle {
    Toggle,
    Checkbox
};

class SwitchWidget : public Widget {
public:
    SwitchWidget(std::string label, cvar_t *cvar, int bit, bool negate);
    void Draw(bool focused) const override;
    Sound KeyEvent(int key) override;
    Sound Activate() override;
    void OnOpen() override;
    void OnClose() override;
    void SetStyle(SwitchStyle style) { style_ = style; }
    bool UsesColumns() const override { return true; }

private:
    void Toggle();
    void SyncFromCvar();
    void ApplyToCvar();

    cvar_t *cvar_ = nullptr;
    int mask_ = 0;
    bool negate_ = false;
    bool value_ = false;
    bool modified_ = false;
    SwitchStyle style_ = SwitchStyle::Toggle;
};

class ProgressWidget : public Widget {
public:
    ProgressWidget(cvar_t *cvar, float minValue, float maxValue);
    void Draw(bool focused) const override;

private:
    cvar_t *cvar_ = nullptr;
    float minValue_ = 0.0f;
    float maxValue_ = 100.0f;
};

class ImageSpinWidget : public SpinWidget {
public:
    ImageSpinWidget(std::string label, cvar_t *cvar, std::string path, std::string filter,
                    int width, int height, bool numericValues = false,
                    std::string valuePrefix = {});
    int Height(int lineHeight) const override;
    void Draw(bool focused) const override;
    void DrawOverlay() const;
    Sound Activate() override;
    Sound KeyEvent(int key) override;
    void OnOpen() override;
    void OnClose() override;
    bool IsExpanded() const { return expanded_; }
    bool HoverAt(int mx, int my);
    bool HandleMouseDrag(int mx, int my, bool mouseDown);

private:
    void OpenList();
    void CloseList(bool accept);
    void EnsureListVisible(int visibleRows, int columns);
    int ValueX(const char *label) const;
    int ValueWidth(int valueX) const;
    int ComputeColumns(int tileWidth, int valueWidth, int scrollWidth) const;
    int ComputeVisibleRows(int rowHeight) const;
    vrect_t ComputeListRect(int visibleRows, int columns, int rowHeight,
                            int *out_scroll_width) const;
    int HitTestList(int mx, int my, const vrect_t &listRect, int rowHeight,
                    int visibleRows, int columns, int scrollWidth) const;
    bool ParseEntryValue(const char *file_value, int *out_value) const;

    std::string path_;
    std::string filter_;
    int previewWidth_;
    int previewHeight_;
    char **entries_ = nullptr;
    int entryCount_ = 0;
    bool expanded_ = false;
    int listStart_ = 0;
    int listCursor_ = -1;
    int openValue_ = -1;
    bool scrollDragging_ = false;
    int scrollDragOffset_ = 0;
    bool numericValues_ = false;
    std::string valuePrefix_;
};

class FieldWidget : public Widget {
public:
    FieldWidget(std::string label, cvar_t *cvar, int width, bool center, bool numeric, bool integer);
    void Layout(int x, int y, int width, int lineHeight) override;
    void Draw(bool focused) const override;
    Sound KeyEvent(int key) override;
    Sound CharEvent(int ch) override;
    void OnOpen() override;
    void OnClose() override;
    bool Center() const { return center_; }
    const char *Text() const { return field_.text; }
    bool UsesColumns() const override { return !center_; }

private:
    bool TestChar(int ch) const;

    cvar_t *cvar_;
    inputField_t field_{};
    int width_ = 16;
    bool center_ = false;
    bool numeric_ = false;
    bool integer_ = false;
};

class ComboWidget : public Widget {
public:
    ComboWidget(std::string label, cvar_t *cvar, int width, bool center, bool numeric, bool integer);
    void Layout(int x, int y, int width, int lineHeight) override;
    void Draw(bool focused) const override;
    Sound KeyEvent(int key) override;
    Sound CharEvent(int ch) override;
    void OnOpen() override;
    void OnClose() override;
    bool Center() const { return center_; }
    bool UsesColumns() const override { return !center_; }

    void AddOption(std::string value);
    void ClearOptions();

private:
    bool TestChar(int ch) const;
    void SetFromIndex(int index);
    void SyncIndexFromText();

    cvar_t *cvar_ = nullptr;
    inputField_t field_{};
    std::vector<std::string> items_{};
    int width_ = 16;
    bool center_ = false;
    bool numeric_ = false;
    bool integer_ = false;
    int currentIndex_ = -1;
};

class KeyBindWidget : public Widget {
public:
    KeyBindWidget(std::string label, std::string command, std::string status, std::string altStatus);
    int Height(int lineHeight) const override;
    void Draw(bool focused) const override;
    Sound Activate() override;
    Sound KeyEvent(int key) override;
    void OnOpen() override;
    void OnClose() override;
    bool UsesColumns() const override { return true; }

private:
    void RefreshBindings();
    static bool KeybindCallback(void *arg, int key);
    static void RemoveBindings(const char *cmd);

    std::string command_;
    std::string altStatus_;
    char binding_[32]{};
    char altBinding_[32]{};
    int primaryKey_ = -1;
    int altKey_ = -1;
};

class SeparatorWidget : public Widget {
public:
    SeparatorWidget();
    void Draw(bool focused) const override;
};

class HeadingWidget : public Widget {
public:
    HeadingWidget();
    int Height(int lineHeight) const override;
    void Draw(bool focused) const override;
};

class WrappedTextWidget : public Widget {
public:
    WrappedTextWidget(std::string text, int maxLines, int maxWidthChars, bool alignLeft);
    int Height(int lineHeight) const override;
    void Layout(int x, int y, int width, int lineHeight) override;
    void Draw(bool focused) const override;

private:
    void RebuildLines(int lineHeight) const;

    int maxLines_ = 0;
    int maxWidthChars_ = 0;
    bool alignLeft_ = true;
    mutable int cachedLineHeight_ = 0;
    mutable int cachedWidthChars_ = 0;
    mutable std::string cachedText_{};
    mutable std::vector<std::string> lines_{};
};

class SaveGameWidget : public Widget {
public:
    SaveGameWidget(std::string slot, bool isLoad);
    void Draw(bool focused) const override;
    Sound Activate() override;
    void OnOpen() override;

private:
    std::string slot_;
    bool isLoad_;
};

struct ListColumn {
    std::string name;
    int width = 0;
    int uiFlags = 0;
};

class ListWidget {
public:
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int rowSpacing = 0;
    int fontSize = 0;
    bool alternateRows = false;
    float alternateShade = 0.8f;

    std::vector<void *> items;
    int numItems = 0;
    int maxItems = 0;
    int mlFlags = 0;
    int extrasize = 0;
    int prestep = 0;
    int curvalue = 0;
    unsigned clickTime = 0;
    int drag_y = 0;
    int sortdir = 0;
    int sortcol = 0;
    std::vector<ListColumn> columns;

    std::function<Sound(ListWidget *self)> onActivate;
    std::function<Sound(ListWidget *self)> onChange;
    std::function<Sound(ListWidget *self)> onSort;

    void Init();
    void SetValue(int value);
    void Sort(int offset, int (*cmpfunc)(const void *, const void *));
    Sound KeyEvent(int key);
    void HoverAt(int mx, int my);
    bool HandleMouseDrag(int mx, int my, bool mouseDown);
    void Draw();

private:
    int RowSpacing() const;
    void ValidatePrestep();
    void AdjustPrestep();
    int DrawHeader(int y) const;
    int DrawItems(int y) const;
    int HitTestRow(int mx, int my) const;
    Sound ClickAt(int mx, int my);
    bool ScrollbarMetrics(int *bar_x, int *bar_y, int *bar_w, int *bar_h,
                          int *arrow_h, int *track_y, int *track_h,
                          int *thumb_y, int *thumb_h) const;

    bool scrollDragging = false;
    int scrollDragOffset = 0;
};

class MenuSystem {
public:
    void Init();
    void Shutdown();
    void ModeChanged();

    void Push(MenuPage *menu);
    void Pop();
    void ForceOff();
    void OpenMenu(uiMenu_t menu);
    void Draw(unsigned realtime);
    void KeyEvent(int key, bool down);
    void CharEvent(int ch);
    void MouseEvent(int x, int y);
    void Frame(int msec);
    void StatusEvent(const serverStatus_t *status);
    void ErrorEvent(const netadr_t *from);
    bool IsTransparent() const;

    void RegisterMenu(std::unique_ptr<MenuPage> menu);
    MenuPage *FindMenu(const char *name) const;

private:
    void Resize();

    std::unordered_map<std::string, std::unique_ptr<MenuPage>> menus_;
    std::vector<MenuPage *> stack_;
    MenuPage *active_ = nullptr;
};

MenuSystem &GetMenuSystem();

void UI_DrawString(int x, int y, int flags, color_t color, const char *string);
void UI_DrawChar(int x, int y, int flags, color_t color, int ch);
void UI_DrawRect8(const vrect_t *rect, int border, int c);
void UI_StringDimensions(vrect_t *rc, int flags, const char *string);
void *UI_FormatColumns(int extrasize, ...);
char *UI_GetColumn(char *s, int n);
int UI_DrawKeyIcon(int x, int y, int height, color_t color, int keynum, const char *label);
int UI_GetKeyIconWidth(int height, int keynum, const char *label);
int UI_DrawHintBar(const std::vector<UiHint> &left, const std::vector<UiHint> &right, int bottom);
int UI_GetHintBarHeight(const std::vector<UiHint> &left, const std::vector<UiHint> &right);
void UI_ResetBindIconCache(void);

void PlayerModel_Load();
void PlayerModel_Free();

void UI_MapDB_Init();
void UI_MapDB_Shutdown();
void UI_MapDB_FetchEpisodes(char ***items, int *num_items);
void UI_MapDB_FetchUnits(char ***items, int **item_indices, int *num_items);

bool UI_LoadJsonMenus(const char *path);

void UI_StartSound(Sound sound);

std::unique_ptr<MenuPage> CreateDemoBrowserPage();
std::unique_ptr<MenuPage> CreateServerBrowserPage();
std::unique_ptr<MenuPage> CreatePlayerConfigPage();
std::unique_ptr<MenuPage> CreateWelcomePage();

bool UI_EvaluateConditions(const std::vector<MenuCondition> &conditions);

} // namespace ui
