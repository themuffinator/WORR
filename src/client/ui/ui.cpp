/*
Copyright (C) 2003-2006 Andrey Nazarov

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
#include "client/input.hpp"
#include "common/prompt.hpp"
#include "ux.hpp"
#include <vector>

uiStatic_t    uis;

LIST_DECL(ui_menus);

cvar_t    *ui_debug;
static cvar_t    *ui_open;
static cvar_t    *ui_scale;
static cvar_t    *ui_font;
static cvar_t    *ui_font_fallback;
static cvar_t    *ui_font_size;
static cvar_t    *ui_cursor_theme;
static cvar_t    *ui_color_theme;
static UiManager ui_manager;

static void UI_UpdateLayoutMetrics(void);
static void UI_UpdateTypographySet(void);
static void UI_PopulateDefaultPalette(bool lightTheme);
static void UI_UpdateLegacyColorsFromPalette(void);
static int UI_KeyForNavigationDirection(ui::ux::NavigationDirection direction);

/*
=============
	UI_GetManager

Returns the global UI manager instance.
=============
*/
UiManager &UI_GetManager(void)
{
	return ui_manager;
}

/*
=============
LegacyMenuWidget

Wraps a legacy menu frame in a UIX widget container.
=============
*/
class LegacyMenuWidget : public ui::ux::Widget {
	public:
	LegacyMenuWidget(menuFrameWork_t *menu);

	void OnLayout(ui::ux::LayoutEngine &engine, const vrect_t &parentBounds) override;
	void OnEvent(const ui::ux::UIEvent &event) override;
	menuFrameWork_t *Menu() const;

	private:
	menuFrameWork_t *m_menu;
};

/*
=============
UiManager::UiManager

Builds a UI manager bound to the shared UIX system.
=============
*/
UiManager::UiManager()
: m_system(&ui::ux::GetSystem())
{
}

/*
=============
UiManager::Initialize

Sets up the base scene layers and applies the current layout metrics.
=============
*/
void UiManager::Initialize()
{
	EnsureLayers();
	SyncLayout(uis.layout);
}

/*
=============
UiManager::Shutdown

Clears scene layers and detaches all UI roots.
=============
*/
void UiManager::Shutdown()
{
	if (!m_system) {
	return;
	}
		m_system->Graph().Clear();
	m_menuLayer.reset();
	m_overlayLayer.reset();
	m_hudLayer.reset();
	m_menuRoot.reset();
	m_overlayRoot.reset();
	m_hudRoot.reset();
}

/*
=============
UiManager::SyncPalette

Copies the resolved palette into the UIX theme context.
=============
*/
void UiManager::SyncPalette(const uiStatic_t &state)
{
if (!m_system) {
return;
}

	std::vector<uiPaletteEntry_t> palette;
	palette.reserve(UI_COLOR_ROLE_COUNT);
	for (const auto &entry : state.palette) {
	palette.push_back(entry);
}

m_system->Theme().SetPalette(std::move(palette));
}

/*
=============
UiManager::SyncTypography

Copies the resolved typography set into the UIX theme context.
=============
*/
void UiManager::SyncTypography(const uiTypographySet_t &typography)
{
	if (!m_system) {
		return;
	}

	m_system->Theme().SetTypography(typography);
}

/*
=============
UiManager::SyncLayout

Updates layout metrics and reapplies sizing to all managed roots.
=============
*/
void UiManager::SyncLayout(const uiLayoutMetrics_t &metrics)
{
	if (!m_system) {
	return;
}

	m_system->UpdateLayout(metrics);
	EnsureLayers();
	LayoutRoot(m_menuRoot, metrics);
	LayoutRoot(m_overlayRoot, metrics);
	LayoutRoot(m_hudRoot, metrics);
}

/*
=============
UiManager::SyncLegacyMenus

Rebuilds the menu layer widget tree from the legacy menu stack.
=============
*/
void UiManager::SyncLegacyMenus(menuFrameWork_t **stack, int depth)
{
	EnsureLayers();
	if (!m_menuRoot) {
	return;
}

	std::vector<std::shared_ptr<ui::ux::Widget>> &children = m_menuRoot->Children();
	children.clear();

	for (int i = 0; i < depth; i++) {
	menuFrameWork_t *menu = stack[i];
	if (!menu) {
	continue;
}

	std::shared_ptr<LegacyMenuWidget> widget = std::make_shared<LegacyMenuWidget>(menu);
	children.push_back(widget);
}
}

/*
=============
UiManager::RoutePointerEvent

Sends pointer movement to all roots using the UIX interaction controller.
=============
*/
void UiManager::RoutePointerEvent(const vrect_t &cursorRegion)
{
	if (!m_system) {
	return;
}

	ui::ux::UIEvent event(ui::ux::EventType::PointerMove, cursorRegion);
	for (const auto &layer : OrderedLayers()) {
	if (layer && layer->Root()) {
	m_system->Interaction().RouteEvent(event, layer->Root());
}
}
}

/*
=============
UiManager::RouteNavigationKey

Maps legacy key presses into UIX navigation events while preserving layering.
=============
*/
void UiManager::RouteNavigationKey(int key)
{
	if (!m_system) {
	return;
	}
		ui::ux::EventType type = ui::ux::EventType::Navigate;
	ui::ux::NavigationDirection direction = ui::ux::NavigationDirection::Next;
		switch (key) {
	case K_TAB:
	direction = ui::ux::NavigationDirection::Next;
	break;
	case K_UPARROW:
	direction = ui::ux::NavigationDirection::Up;
	break;
	case K_DOWNARROW:
	direction = ui::ux::NavigationDirection::Down;
	break;
	case K_LEFTARROW:
	direction = ui::ux::NavigationDirection::Left;
	break;
	case K_RIGHTARROW:
	direction = ui::ux::NavigationDirection::Right;
	break;
	case K_ENTER:
	type = ui::ux::EventType::Activate;
	break;
	default:
	return;
	}
		ui::ux::UIEvent event(type, direction, { uis.mouseCoords[0], uis.mouseCoords[1], 1, 1 });
	for (const auto &layer : OrderedLayers()) {
	if (layer && layer->Root()) {
	m_system->Interaction().RouteEvent(event, layer->Root());
	}
}
}

/*
=============
UiManager::MenuRoot

Returns the menu root widget.
=============
*/
std::shared_ptr<ui::ux::Widget> UiManager::MenuRoot() const
	{
	return m_menuRoot;
	}

/*
=============
UiManager::OverlayRoot

Returns the overlay root widget.
=============
*/
std::shared_ptr<ui::ux::Widget> UiManager::OverlayRoot() const
	{
	return m_overlayRoot;
	}

/*
=============
UiManager::HudRoot

Returns the HUD root widget.
=============
*/
std::shared_ptr<ui::ux::Widget> UiManager::HudRoot() const
	{
	return m_hudRoot;
	}

/*
=============
UiManager::EnsureLayers

Creates base scene graph layers if they do not already exist.
=============
*/
void UiManager::EnsureLayers()
{
	if (!m_system) {
	return;
	}
		if (!m_menuLayer) {
	m_menuLayer = std::make_shared<ui::ux::SceneLayer>("menu", 10);
	m_menuRoot = std::make_shared<ui::ux::Widget>("menu-root", ui::ux::WidgetType::Container);
	m_menuLayer->SetRoot(m_menuRoot);
	m_system->Graph().AddLayer(m_menuLayer);
	}
		if (!m_overlayLayer) {
	m_overlayLayer = std::make_shared<ui::ux::SceneLayer>("overlay", 20);
	m_overlayRoot = std::make_shared<ui::ux::Widget>("overlay-root", ui::ux::WidgetType::Container);
	m_overlayLayer->SetRoot(m_overlayRoot);
	m_system->Graph().AddLayer(m_overlayLayer);
	}
		if (!m_hudLayer) {
	m_hudLayer = std::make_shared<ui::ux::SceneLayer>("hud", 0);
	m_hudRoot = std::make_shared<ui::ux::Widget>("hud-root", ui::ux::WidgetType::Container);
	m_hudLayer->SetRoot(m_hudRoot);
	m_system->Graph().AddLayer(m_hudLayer);
	}
}

/*
=============
UiManager::LayoutRoot

Applies full-screen layout constraints to the provided root widget.
=============
*/
void UiManager::LayoutRoot(const std::shared_ptr<ui::ux::Widget> &root, const uiLayoutMetrics_t &metrics) const
{
	if (!root || !m_system) {
	return;
	}
		root->SetLayout(ui::ux::LayoutRect(ui::ux::LayoutValue::Pixels(0.0f), ui::ux::LayoutValue::Pixels(0.0f),
	ui::ux::LayoutValue::Percent(1.0f), ui::ux::LayoutValue::Percent(1.0f)));
		vrect_t bounds{};
	bounds.x = 0;
	bounds.y = 0;
	bounds.width = metrics.screenWidth;
	bounds.height = metrics.screenHeight;
		root->OnLayout(m_system->Layout(), bounds);
}

/*
=============
UiManager::OrderedLayers

Returns the scene graph layers sorted by z-index.
=============
*/
std::vector<std::shared_ptr<ui::ux::SceneLayer>> UiManager::OrderedLayers() const
{
	if (!m_system) {
	return {};
	}
		return m_system->Graph().OrderedLayers();
}

/*
=============
LegacyMenuWidget::LegacyMenuWidget

Initializes a wrapper for a legacy menu.
=============
*/
LegacyMenuWidget::LegacyMenuWidget(menuFrameWork_t *menu)
	: Widget(menu ? menu->name : "legacy-menu", ui::ux::WidgetType::Container), m_menu(menu)
	{
	}

/*
=============
LegacyMenuWidget::OnLayout

Inherits the parent bounds to mirror legacy full-screen menus.
=============
*/
void LegacyMenuWidget::OnLayout(ui::ux::LayoutEngine &engine, const vrect_t &parentBounds)
{
	m_bounds = parentBounds;
	ui::ux::Widget::OnLayout(engine, parentBounds);
	}
		/*
	=============
	LegacyMenuWidget::OnEvent
		Forwards navigation and activation into the legacy menu dispatchers.
	=============
	*/
	void LegacyMenuWidget::OnEvent(const ui::ux::UIEvent &event)
	{
	if (!m_menu) {
	return;
	}
		switch (event.Type()) {
	case ui::ux::EventType::Navigate:
	UI_DispatchKeyToLayers(UI_KeyForNavigationDirection(event.Direction()));
	break;
	case ui::ux::EventType::Activate:
	UI_DispatchKeyToLayers(K_ENTER);
	break;
	default:
	ui::ux::Widget::OnEvent(event);
	break;
}
}

/*
=============
LegacyMenuWidget::Menu

Returns the underlying legacy menu.
=============
*/
menuFrameWork_t *LegacyMenuWidget::Menu() const
{
	return m_menu;
	}

/*
=============
UI_KeyForNavigationDirection

Maps a UIX navigation direction back to a legacy key code.
=============
*/
static int UI_KeyForNavigationDirection(ui::ux::NavigationDirection direction)
{
	switch (direction) {
	case ui::ux::NavigationDirection::Up:
	return K_UPARROW;
	case ui::ux::NavigationDirection::Down:
	return K_DOWNARROW;
	case ui::ux::NavigationDirection::Left:
	return K_LEFTARROW;
	case ui::ux::NavigationDirection::Right:
	return K_RIGHTARROW;
	case ui::ux::NavigationDirection::Previous:
	return K_TAB;
	case ui::ux::NavigationDirection::Next:
	default:
	return K_TAB;
}
}
/*
=============
UI_Percent

Builds a percentage-based layout value.
=============
*/
uiLayoutValue_t UI_Percent(float percent)
{
uiLayoutValue_t value;

value.value = percent;
value.unit = UI_UNIT_PERCENT;

return value;
}

/*
=============
UI_Pixels

Builds a pixel-based layout value.
=============
*/
uiLayoutValue_t UI_Pixels(float pixels)
{
	uiLayoutValue_t value;
		value.value = pixels;
	value.unit = UI_UNIT_PIXELS;
		return value;
}

/*
=============
UI_ResolveLayoutValue

Converts a relative layout value into pixels using the provided reference.
=============
*/
int UI_ResolveLayoutValue(const uiLayoutValue_t *value, int reference)
{
	if (!value) {
	return 0;
	}
		switch (value->unit) {
	case UI_UNIT_PERCENT:
	return Q_rint(value->value * reference);
	case UI_UNIT_PIXELS:
	default:
	return Q_rint(value->value);
	}
}

/*
=============
UI_LayoutToPixels

Resolves a relative layout rectangle into pixel bounds within the provided parent.
=============
*/
vrect_t UI_LayoutToPixels(const uiLayoutRect_t *rect, const vrect_t *parent)
{
	vrect_t resolved{};
	vrect_t root{};
		if (parent) {
	root = *parent;
	} else {
	root.x = 0;
	root.y = 0;
	root.width = uis.layout.screenWidth;
	root.height = uis.layout.screenHeight;
	}
		resolved.x = root.x + UI_ResolveLayoutValue(&rect->x, root.width);
	resolved.y = root.y + UI_ResolveLayoutValue(&rect->y, root.height);
	resolved.width = UI_ResolveLayoutValue(&rect->width, root.width);
	resolved.height = UI_ResolveLayoutValue(&rect->height, root.height);
		if (rect->padding) {
	resolved.x += rect->padding;
	resolved.y += rect->padding;
	resolved.width -= rect->padding * 2;
	resolved.height -= rect->padding * 2;
		if (resolved.width < 0) {
	resolved.width = 0;
	}
	if (resolved.height < 0) {
	resolved.height = 0;
	}
	}
		return resolved;
}

/*
=============
UI_SplitLayoutRow

Splits a resolved layout rectangle into label and field regions using percentages.
=============
*/
uiLayoutSplit_t UI_SplitLayoutRow(const uiLayoutRect_t *rect, const vrect_t *parent, float labelPercent)
{
	uiLayoutSplit_t split{};
		split.bounds = UI_LayoutToPixels(rect, parent);
		const int spacing = rect ? rect->spacing : 0;
	const int labelWidth = Q_rint(split.bounds.width * labelPercent);
	const int fieldStart = split.bounds.x + labelWidth + spacing;
	const int availableWidth = split.bounds.width - labelWidth - spacing;
		split.label.x = split.bounds.x;
	split.label.y = split.bounds.y;
	split.label.width = labelWidth;
	split.label.height = split.bounds.height;
		split.field.x = fieldStart;
	split.field.y = split.bounds.y;
	split.field.width = availableWidth > 0 ? availableWidth : 0;
	split.field.height = split.bounds.height;
		return split;
	}
	/*
=============
UI_GenericSpacing

Returns the scaled spacing for a base measurement.
=============
*/
int UI_GenericSpacing(int base)
	{
	if (base <= 0) {
	return 0;
	}
		return base + base / 4;
	}
	/*
=============
UI_MenuSpacing

Returns the DPI-aware spacing used for menu row separation.
=============
*/
int UI_MenuSpacing(void)
	{
	if (uis.layout.menuSpacing > 0) {
	return uis.layout.menuSpacing;
	}
		return UI_GenericSpacing(CONCHAR_HEIGHT);
	}
	/*
=============
UI_ListSpacing

Returns the DPI-aware spacing used for list rows.
=============
*/
int UI_ListSpacing(void)
	{
	if (uis.layout.listSpacing > 0) {
	return uis.layout.listSpacing;
	}
		return UI_GenericSpacing(CONCHAR_HEIGHT);
	}
	/*
=============
UI_ListScrollbarWidth

Returns the DPI-aware scrollbar width for lists.
=============
*/
int UI_ListScrollbarWidth(void)
{
if (uis.layout.listScrollbarWidth > 0) {
return uis.layout.listScrollbarWidth;
}

return UI_GenericSpacing(CONCHAR_WIDTH);
}

/*
=============
UI_ScaledFontSize

Scales a base font pixel height using the active DPI-aware multiplier.
=============
*/
int UI_ScaledFontSize(int base)
{
if (base <= 0) {
return 0;
}

float scale = uis.layout.dpiScale;
if (scale < 1.0f) {
scale = 1.0f;
}

return Q_rint(base * scale);
}

/*
=============
UI_ScaledPixels

Returns a DPI-aware scaled measurement for arbitrary pixel values.
=============
*/
float UI_ScaledPixels(float base)
{
float scale = uis.layout.dpiScale;
if (scale < 1.0f) {
scale = 1.0f;
}

return base * scale;
}

/*
=============
UI_ListPadding
		Returns the padding applied to list cells using the current metrics.
	=============
	*/
int UI_ListPadding(void)
{
return MLIST_PRESTEP * 2;
}

/*
=============
UI_LeftColumnOffset
		Returns the left column offset derived from layout metrics.
	=============
	*/
int UI_LeftColumnOffset(void)
{
if (uis.layout.columnOffset) {
return -uis.layout.columnOffset;
}

return -CONCHAR_WIDTH * 2;
}
		/*
	=============
	UI_RightColumnOffset
	Returns the right column offset derived from layout metrics.
=============
*/
int UI_RightColumnOffset(void)
{
if (uis.layout.columnOffset) {
return uis.layout.columnOffset;
}

return CONCHAR_WIDTH * 2;
}

/*
=============
UI_ColumnPadding

Returns the padding between paired columns.
=============
*/
int UI_ColumnPadding(void)
{
if (uis.layout.columnPadding) {
return uis.layout.columnPadding;
}

return CONCHAR_WIDTH * 4;
}

/*
=============
UI_CharWidth

Returns the active UI character width.
=============
*/
int UI_CharWidth(void)
{
if (uis.layout.charWidth > 0) {
return uis.layout.charWidth;
}

return CONCHAR_WIDTH;
}

/*
=============
UI_CharHeight

Returns the active UI character height.
=============
*/
int UI_CharHeight(void)
{
	if (uis.layout.charHeight > 0) {
	return uis.layout.charHeight;
	}
		return CONCHAR_HEIGHT;
}

/*
=============
UI_UpdateLayoutMetrics

Refreshes layout measurements using the active screen size and font metrics.
=============
*/
static void UI_UpdateLayoutMetrics(void)
{
	uiLayoutMetrics_t metrics{};
	uiLayoutValue_t columnValue = UI_Percent(0.05f);

	metrics.screenWidth = uis.width;
	metrics.screenHeight = uis.height;
	metrics.dpiScale = static_cast<float>(get_auto_scale());

	if (metrics.dpiScale < 1.0f) {
	metrics.dpiScale = 1.0f;
	}

	qhandle_t handle = uis.fontHandle ? uis.fontHandle : SCR_DefaultFontHandle();
	metrics.charHeight = SCR_FontLineHeight(1, handle);
	metrics.charWidth = SCR_MeasureString(1, UI_LEFT, 1, "M", handle);

	if (metrics.charWidth <= 0) {
	metrics.charWidth = Q_rint(CONCHAR_WIDTH * metrics.dpiScale);
	}
	if (metrics.charHeight <= 0) {
	metrics.charHeight = Q_rint(CONCHAR_HEIGHT * metrics.dpiScale);
	}

	metrics.genericSpacing = UI_GenericSpacing(metrics.charHeight);
	metrics.menuSpacing = metrics.genericSpacing;
	metrics.listSpacing = metrics.genericSpacing;
	metrics.listScrollbarWidth = UI_GenericSpacing(metrics.charWidth);

	metrics.columnOffset = UI_ResolveLayoutValue(&columnValue, metrics.screenWidth);
	if (metrics.columnOffset < metrics.charWidth * 2) {
	metrics.columnOffset = metrics.charWidth * 2;
	}

	metrics.columnPadding = metrics.columnOffset + metrics.charWidth;

	uis.layout = metrics;
	UI_SyncUxLayout();
}

#define UI_COMPOSITOR_FADE_TIME	200
#define UI_COMPOSITOR_SLIDE_PIXELS	32

typedef enum uiTransitionType_e {
	UI_TRANSITION_NONE,
	UI_TRANSITION_FADE_IN,
	UI_TRANSITION_FADE_OUT,
	UI_TRANSITION_SLIDE_IN,
	UI_TRANSITION_SLIDE_OUT
} uiTransitionType_t;

typedef struct uiLayerState_s {
	menuFrameWork_t *menu;
	float startOpacity;
	float targetOpacity;
	float opacity;
	vec2_t slideStart;
	vec2_t slideTarget;
	vec2_t slide;
	uiTransitionType_t transition;
	unsigned startTime;
	unsigned duration;
	bool modal;
	bool passthrough;
	bool drawBackdrop;
} uiLayerState_t;

typedef struct uiCompositor_s {
	uiLayerState_t layers[MAX_MENU_DEPTH];
	int count;

static uiCompositor_t ui_compositor;


/*
=============
UI_CompositorReset

Clears any cached compositor state when the menu stack is torn down.
=============
*/
static void UI_CompositorReset(void)
{
	memset(&ui_compositor, 0, sizeof(ui_compositor));
}

/*
=============
UI_CompositorLerp

Returns a clamped linear interpolation value for transition curves.
=============
*/
static float UI_CompositorLerp(float from, float to, float frac)
{
	if (frac < 0.0f) {
	frac = 0.0f;
	} else if (frac > 1.0f) {
	frac = 1.0f;
	}

	return from + (to - from) * frac;
}

/*
=============
UI_CompositorFindLayer

Looks for cached layer information for the provided menu.
=============
*/
static uiLayerState_t *UI_CompositorFindLayer(menuFrameWork_t *menu)
{
	for (int i = 0; i < ui_compositor.count; i++) {
	uiLayerState_t *layer = &ui_compositor.layers[i];
	if (layer->menu == menu) {
	return layer;
	}
	}

	return NULL;
}

/*
=============
UI_CompositorSync

Rebuilds the compositor layer list from the stacked menus while
preserving transition progress for existing layers.
=============
*/
static void UI_CompositorSync(void)
{
	uiLayerState_t rebuilt[MAX_MENU_DEPTH];
	int rebuildCount = 0;

	for (int i = 0; i < uis.menuDepth && rebuildCount < MAX_MENU_DEPTH; i++) {
	menuFrameWork_t *menu = uis.layers[i];
	uiLayerState_t *existing = UI_CompositorFindLayer(menu);
	uiLayerState_t *dest = &rebuilt[rebuildCount++];

	if (existing) {
	*dest = *existing;
	} else {
	memset(dest, 0, sizeof(*dest));
	dest->menu = menu;
	dest->startOpacity = 0.0f;
	dest->targetOpacity = menu->opacity > 0.0f ? menu->opacity : 1.0f;
	dest->opacity = dest->startOpacity;
	dest->transition = UI_TRANSITION_FADE_IN;
	dest->startTime = uis.realtime;
	dest->duration = UI_COMPOSITOR_FADE_TIME;
	dest->slideStart[0] = 0.0f;
	dest->slideStart[1] = UI_COMPOSITOR_SLIDE_PIXELS;
	dest->slideTarget[0] = 0.0f;
	dest->slideTarget[1] = 0.0f;
	Vector2Copy(dest->slideStart, dest->slide);
	}

	dest->modal = menu->modal || !menu->allowInputPassthrough;
	dest->passthrough = menu->allowInputPassthrough;
	dest->drawBackdrop = menu->drawsBackdrop || (menu->modal && !menu->transparent);
	}

	ui_compositor.count = rebuildCount;
	for (int i = 0; i < rebuildCount; i++) {
	ui_compositor.layers[i] = rebuilt[i];
	}
}

/*
=============
UI_CompositorUpdateLayer

Applies transition math to a layer prior to drawing.
=============
*/
static void UI_CompositorUpdateLayer(uiLayerState_t *layer)
{
	float frac = 1.0f;

	if (layer->duration) {
	unsigned elapsed = uis.realtime - layer->startTime;
	if (elapsed < layer->duration) {
	frac = static_cast<float>(elapsed) / static_cast<float>(layer->duration);
	}
	}

	switch (layer->transition) {
	case UI_TRANSITION_FADE_IN:
	case UI_TRANSITION_NONE:
	layer->opacity = UI_CompositorLerp(layer->startOpacity, layer->targetOpacity, frac);
	break;
	case UI_TRANSITION_FADE_OUT:
	layer->opacity = UI_CompositorLerp(layer->startOpacity, 0.0f, frac);
	break;
	case UI_TRANSITION_SLIDE_IN:
	Vector2Set(layer->slide,
	UI_CompositorLerp(layer->slideStart[0], layer->slideTarget[0], frac),
	UI_CompositorLerp(layer->slideStart[1], layer->slideTarget[1], frac));
	layer->opacity = UI_CompositorLerp(layer->startOpacity, layer->targetOpacity, frac);
	break;
	case UI_TRANSITION_SLIDE_OUT:
	Vector2Set(layer->slide,
	UI_CompositorLerp(layer->slideStart[0], layer->slideTarget[0], frac),
	UI_CompositorLerp(layer->slideStart[1], layer->slideTarget[1], frac));
	layer->opacity = UI_CompositorLerp(layer->startOpacity, 0.0f, frac);
	break;
	}
}

/*
=============
UI_DrawBackdropForLayer

Draws a dimmed overlay for modal layers.
=============
*/
static void UI_DrawBackdropForLayer(const uiLayerState_t *layer)
{
	if (!layer->drawBackdrop) {
	return;
	}

	int alpha = static_cast<int>(layer->opacity * 160.0f);
	if (alpha < 0) {
	alpha = 0;
	} else if (alpha > 255) {
	alpha = 255;
	}
	R_DrawFill32(Q_rint(layer->slide[0]), Q_rint(layer->slide[1]), uis.width, uis.height,
	ColorSetAlpha(COLOR_BLACK, static_cast<uint8_t>(alpha)));
}


typedef struct uiColorStack_s {
	color_t background;
	color_t normal;
	color_t active;
	color_t selection;
	color_t disabled;
} uiColorStack_t;

/*
=============
UI_CompositorPushOpacity

Modulates UI palette colors to respect a layer's opacity.
=============
*/
static void UI_CompositorPushOpacity(uiColorStack_t *backup, float opacity)
{
	*backup = uis.color;
	if (opacity >= 1.0f) {
	return;
	}

	int scaled = static_cast<int>(opacity * 255.0f);
	if (scaled < 0) {
	scaled = 0;
	} else if (scaled > 255) {
	scaled = 255;
	}
	uis.color.background = ColorSetAlpha(backup->background, static_cast<uint8_t>(scaled));
	uis.color.normal = ColorSetAlpha(backup->normal, static_cast<uint8_t>(scaled));
	uis.color.active = ColorSetAlpha(backup->active, static_cast<uint8_t>(scaled));
	uis.color.selection = ColorSetAlpha(backup->selection, static_cast<uint8_t>(scaled));
	uis.color.disabled = ColorSetAlpha(backup->disabled, static_cast<uint8_t>(scaled));
}

/*
=============
UI_CompositorPopOpacity

Restores the UI palette after a composited layer has been drawn.
=============
*/
static void UI_CompositorPopOpacity(const uiColorStack_t *backup)
{
	uis.color = *backup;
}

/*
=============
UI_UpdateActiveMenuFromStack

Ensures the active menu reference points at the topmost layer.
=============
*/
static void UI_UpdateActiveMenuFromStack(void)
{
	uis.activeMenu = NULL;
	for (int i = uis.menuDepth - 1; i >= 0; i--) {
	if (uis.layers[i]) {
	uis.activeMenu = uis.layers[i];
	break;
	}
	}
}

/*
=============
UI_ApplyMenuDefaults

Initializes modal and opacity defaults for a menu layer.
=============
*/
static void UI_ApplyMenuDefaults(menuFrameWork_t *menu)
{
	if (!menu->modal && !menu->allowInputPassthrough) {
	menu->modal = true;
	}

	if (menu->opacity <= 0.0f || menu->opacity > 1.0f) {
	menu->opacity = 1.0f;
	}

	if (!menu->drawsBackdrop && menu->modal && !menu->transparent) {
	menu->drawsBackdrop = true;
	}
}

// ===========================================================================

/*
=================
UI_PushMenu
=================
*/
void UI_PushMenu(menuFrameWork_t *menu)
{
	int i, j;

	if (!menu) {
	return;
	}

	// if this menu is already present, drop back to that level
	// to avoid stacking menus by hotkeys
	for (i = 0; i < uis.menuDepth; i++) {
	if (uis.layers[i] == menu) {
	break;
	}
	}

	if (i == uis.menuDepth) {
	if (uis.menuDepth >= MAX_MENU_DEPTH) {
	Com_EPrintf("UI_PushMenu: MAX_MENU_DEPTH exceeded\n");
	return;
	}
	uis.layers[uis.menuDepth++] = menu;
	} else {
	for (j = i; j < uis.menuDepth; j++) {
	UI_PopMenu();
	}
	uis.menuDepth = i + 1;
	}

	if (menu->push && !menu->push(menu)) {
	uis.menuDepth--;
	return;
	}

	UI_ApplyMenuDefaults(menu);

	Menu_Init(menu);

	Key_SetDest(Key_FromMask((Key_GetDest() & ~KEY_CONSOLE) | KEY_MENU));

	Con_Close(true);

	if (!uis.activeMenu) {
	// opening menu moves cursor to the nice location
	IN_WarpMouse(menu->mins[0] / uis.scale, menu->mins[1] / uis.scale);

	uis.mouseCoords[0] = menu->mins[0];
	uis.mouseCoords[1] = menu->mins[1];

	uis.entersound = true;
	}

	uis.activeMenu = menu;
	UI_UpdateActiveMenuFromStack();

	UI_DoHitTest();
	UI_CompositorSync();

	if (menu->expose) {
	menu->expose(menu);
	}
}

/*
=============
UI_Resize

Recomputes UI scale and layout metrics after a mode change.
=============
*/
static void UI_Resize(void)
{
	int i;
		uis.scale = R_ClampScale(ui_scale);
	uis.width = Q_rint(r_config.width * uis.scale);
	uis.height = Q_rint(r_config.height * uis.scale);
		UI_UpdateLayoutMetrics();
		for (i = 0; i < uis.menuDepth; i++) {
	Menu_Init(uis.layers[i]);
	}
		//CL_WarpMouse(0, 0);
}


/*
=================
UI_ForceMenuOff
=================
*/
void UI_ForceMenuOff(void)
{
	menuFrameWork_t *menu;
	int i;

	for (i = 0; i < uis.menuDepth; i++) {
	menu = uis.layers[i];
	if (menu->pop) {
	menu->pop(menu);
	}
	}

	Key_SetDest(Key_FromMask(Key_GetDest() & ~KEY_MENU));
	uis.menuDepth = 0;
	uis.activeMenu = NULL;
	uis.mouseTracker = NULL;
	uis.transparent = false;
	UI_CompositorReset();
	UI_UpdateActiveMenuFromStack();
}

/*
=================
UI_PopMenu
=================
*/
void UI_PopMenu(void)
{
	menuFrameWork_t *menu;

	Q_assert(uis.menuDepth > 0);

	menu = uis.layers[--uis.menuDepth];
	if (menu->pop) {
	menu->pop(menu);
	}

	if (!uis.menuDepth) {
	UI_ForceMenuOff();
	return;
	}

	uis.activeMenu = uis.layers[uis.menuDepth - 1];
	uis.mouseTracker = NULL;
	UI_UpdateActiveMenuFromStack();
	UI_CompositorSync();

	UI_DoHitTest();
}

/*
=================
UI_IsTransparent
=================
*/
bool UI_IsTransparent(void)
{
    if (!(Key_GetDest() & KEY_MENU)) {
        return true;
    }

    if (!uis.activeMenu) {
        return true;
    }

    return uis.activeMenu->transparent;
}

menuFrameWork_t *UI_FindMenu(const char *name)
{
    menuFrameWork_t *menu;

    LIST_FOR_EACH(menuFrameWork_t, menu, &ui_menus, entry) {
        if (!strcmp(menu->name, name)) {
            return menu;
        }
    }

    return NULL;
}

/*
=================
UI_OpenMenu
=================
*/
void UI_OpenMenu(uiMenu_t type)
{
	menuFrameWork_t *menu = NULL;

	if (!uis.initialized) {
	return;
	}

	UI_SyncMenuContext();

	// close any existing menus
	UI_ForceMenuOff();

	switch (type) {
	case UIMENU_DEFAULT:
	if (ui_open->integer) {
	menu = UI_FindMenu("main");
	}
	break;
	case UIMENU_MAIN:
	menu = UI_FindMenu("main");
	break;
	case UIMENU_GAME:
	menu = UI_FindMenu("game");
	if (!menu) {
	menu = UI_FindMenu("main");
	}
	break;
	case UIMENU_NONE:
	break;
	default:
	Q_assert(!"bad menu");
	}

	UI_PushMenu(menu);
}

//=============================================================================

/*
=================
UI_FormatColumns
=================
*/
void *UI_FormatColumns(int extrasize, ...)
{
    va_list argptr;
    char *buffer, *p;
    int i, j;
    size_t total = 0;
    char *strings[MAX_COLUMNS];
    size_t lengths[MAX_COLUMNS];

    va_start(argptr, extrasize);
    for (i = 0; i < MAX_COLUMNS; i++) {
        if ((p = va_arg(argptr, char *)) == NULL) {
            break;
        }
        strings[i] = p;
        total += lengths[i] = strlen(p) + 1;
    }
    va_end(argptr);

    buffer = UI_Malloc(extrasize + total + 1);
    p = buffer + extrasize;
    for (j = 0; j < i; j++) {
        memcpy(p, strings[j], lengths[j]);
        p += lengths[j];
    }
    *p = 0;

    return buffer;
}

char *UI_GetColumn(char *s, int n)
{
    int i;

    for (i = 0; i < n && *s; i++) {
        s += strlen(s) + 1;
    }

    return s;
}

/*
=================
UI_CursorInRect
=================
*/
bool UI_CursorInRect(const vrect_t *rect)
{
    if (uis.mouseCoords[0] < rect->x) {
        return false;
    }
    if (uis.mouseCoords[0] >= rect->x + rect->width) {
        return false;
    }
    if (uis.mouseCoords[1] < rect->y) {
        return false;
    }
    if (uis.mouseCoords[1] >= rect->y + rect->height) {
        return false;
    }
    return true;
}

// nb: all UI strings are drawn at full alpha
void UI_DrawString(int x, int y, int flags, color_t color, const char *string)
{
qhandle_t handle = UI_SelectFontHandle(string, flags);
SCR_DrawStringStretch(x, y, 1, flags, MAX_STRING_CHARS, string,
ColorSetAlpha(color, static_cast<uint8_t>(255)), handle);
}

// nb: all UI chars are drawn at full alpha
void UI_DrawChar(int x, int y, int flags, color_t color, int ch)
{
char buffer[2] = { static_cast<char>(ch), '\0' };
qhandle_t handle = UI_SelectFontHandle(buffer, flags);
R_DrawChar(x, y, flags, ch, ColorSetAlpha(color, static_cast<uint8_t>(255)), handle);
}

void UI_StringDimensions(vrect_t *rc, int flags, const char *string)
{
qhandle_t handle = UI_SelectFontHandle(string, flags);
rc->height = SCR_FontLineHeight(1, handle);
rc->width = SCR_MeasureString(1, flags & ~UI_MULTILINE, MAX_STRING_CHARS, string, handle);

    if ((flags & UI_CENTER) == UI_CENTER) {
        rc->x -= rc->width / 2;
    } else if (flags & UI_RIGHT) {
        rc->x -= rc->width;
    }
}

void UI_DrawRect8(const vrect_t *rc, int border, int c)
{
    R_DrawFill8(rc->x, rc->y, border, rc->height, c);   // left
    R_DrawFill8(rc->x + rc->width - border, rc->y, border, rc->height, c);   // right
    R_DrawFill8(rc->x + border, rc->y, rc->width - border * 2, border, c);   // top
    R_DrawFill8(rc->x + border, rc->y + rc->height - border, rc->width - border * 2, border, c);   // bottom
}

//=============================================================================
/* Menu Subsystem */

/*
=================
UI_DoHitTest
=================
*/
bool UI_DoHitTest(void)
{
menuCommon_t *item = NULL;

	if (!uis.menuDepth) {
	return false;
	}

	for (int i = uis.menuDepth - 1; i >= 0; i--) {
	menuFrameWork_t *menu = uis.layers[i];
	if (uis.mouseTracker && uis.mouseTracker->parent == menu) {
	item = uis.mouseTracker;
	} else {
	item = Menu_HitTest(menu);
	}

	if (item && UI_IsItemSelectable(item)) {
	Menu_MouseMove(item);

	if (!(item->flags & QMF_HASFOCUS)) {
	Menu_SetFocus(item);
	}

	uis.activeMenu = menu;
	return true;
	}

	if (menu->modal) {
	break;
	}
	}

UI_UpdateActiveMenuFromStack();
return false;
}

/*
=============
UI_DispatchKeyToLayers

Routes key presses through the stacked menus honoring modal barriers.
=============
*/
static menuSound_t UI_DispatchKeyToLayers(int key)
{
	menuSound_t sound = QMS_NOTHANDLED;

	for (int i = uis.menuDepth - 1; i >= 0; i--) {
	menuFrameWork_t *menu = uis.layers[i];
	sound = Menu_Keydown(menu, key);
	if (sound != QMS_NOTHANDLED) {
	uis.activeMenu = menu;
	return sound;
	}

	if (menu->modal) {
	return sound;
	}
	}

	UI_UpdateActiveMenuFromStack();
	return sound;
}

/*
=============
UI_DispatchCharToLayers

Routes printable character input with pass-through semantics.
=============
*/
static menuSound_t UI_DispatchCharToLayers(int key)
{
	for (int i = uis.menuDepth - 1; i >= 0; i--) {
	menuFrameWork_t *menu = uis.layers[i];
	menuCommon_t *item = Menu_ItemAtCursor(menu);
	menuSound_t sound = QMS_NOTHANDLED;

	if (item) {
	sound = Menu_CharEvent(item, key);
	}

	if (sound != QMS_NOTHANDLED) {
	uis.activeMenu = menu;
	return sound;
	}

	if (menu->modal) {
	return sound;
	}
	}

	UI_UpdateActiveMenuFromStack();
	return QMS_NOTHANDLED;
}

/*
=================
UI_MouseEvent
=================
*/
void UI_MouseEvent(int x, int y)
{
	x = Q_clip(x, 0, r_config.width - 1);
	y = Q_clip(y, 0, r_config.height - 1);

	uis.mouseCoords[0] = Q_rint(x * uis.scale);
	uis.mouseCoords[1] = Q_rint(y * uis.scale);
		UI_DoHitTest();
		vrect_t region{};
	region.x = uis.mouseCoords[0];
	region.y = uis.mouseCoords[1];
	region.width = 1;
	region.height = 1;
	UI_GetManager().RoutePointerEvent(region);
}

/*
=================
UI_Draw
=================
*/
void UI_Draw(unsigned realtime)
{
	uiColorStack_t colors;

	uis.realtime = realtime;

	if (!(Key_GetDest() & KEY_MENU)) {
	return;
	}

if (!uis.menuDepth) {
return;
}

	UI_GetManager().SyncLegacyMenus(uis.layers, uis.menuDepth);
UI_UpdateActiveMenuFromStack();
UI_CompositorSync();

	R_SetScale(uis.scale);

	for (int i = 0; i < ui_compositor.count; i++) {
	uiLayerState_t *layer = &ui_compositor.layers[i];
	UI_CompositorUpdateLayer(layer);
	UI_DrawBackdropForLayer(layer);
	UI_CompositorPushOpacity(&colors, layer->opacity);
	if (layer->menu->draw) {
	layer->menu->draw(layer->menu);
	} else {
	Menu_Draw(layer->menu);
	}
	UI_CompositorPopOpacity(&colors);
	}

	if (r_config.flags & QVF_FULLSCREEN) {
	R_DrawPic(uis.mouseCoords[0] - uis.cursorWidth / 2,
	uis.mouseCoords[1] - uis.cursorHeight / 2,
	COLOR_WHITE, uis.cursorHandle);
	}

	if (ui_debug->integer) {
	UI_DrawString(uis.width - 4, 4, UI_RIGHT,
	COLOR_WHITE, va("%3i %3i", uis.mouseCoords[0], uis.mouseCoords[1]));
	}

	if (uis.entersound) {
	uis.entersound = false;
	S_StartLocalSound("misc/menu1.wav");
	}

	R_SetScale(1.0f);
}

void UI_StartSound(menuSound_t sound)
{
    switch (sound) {
    case QMS_IN:
        S_StartLocalSound("misc/menu1.wav");
        break;
    case QMS_MOVE:
        S_StartLocalSound("misc/menu2.wav");
        break;
    case QMS_OUT:
        S_StartLocalSound("misc/menu3.wav");
        break;
    case QMS_BEEP:
        S_StartLocalSound("misc/talk1.wav");
        break;
    default:
        break;
    }
}

/*
=================
UI_KeyEvent
=================
*/
void UI_KeyEvent(int key, bool down)
{
	menuSound_t sound;
		if (!uis.menuDepth) {
	return;
	}
		if (!down) {
	if (key == K_MOUSE1) {
	uis.mouseTracker = NULL;
	}
	return;
	}
		UI_GetManager().RouteNavigationKey(key);
	sound = UI_DispatchKeyToLayers(key);

	if (sound != QMS_NOTHANDLED) {
	UI_StartSound(sound);
	}
}

/*
=================
UI_CharEvent
=================
*/
void UI_CharEvent(int key)
{
	menuSound_t sound;

	if (!uis.menuDepth) {
	return;
	}

	sound = UI_DispatchCharToLayers(key);
	if (sound != QMS_NOTHANDLED) {
	UI_StartSound(sound);
	}
}

static void UI_Menu_g(genctx_t *ctx)
{
    menuFrameWork_t *menu;

    LIST_FOR_EACH(menuFrameWork_t, menu, &ui_menus, entry)
        Prompt_AddMatch(ctx, menu->name);
}

static void UI_PushMenu_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        UI_Menu_g(ctx);
    }
}

static void UI_PushMenu_f(void)
{
	menuFrameWork_t *menu;
	char *s;

	if (Cmd_Argc() < 2) {
	Com_Printf("Usage: %s <menu>\n", Cmd_Argv(0));
	return;
	}
	s = Cmd_Argv(1);
	menu = UI_FindMenu(s);
	if (menu) {
	UI_PushMenu(menu);
	} else {
	Com_Printf("No such menu: %s\n", s);
	}
}

static void UI_PopMenu_f(void)
{
	if (uis.activeMenu) {
	UI_PopMenu();
	}
}

/*
=============
UI_ReloadMenus_f

Console command to force reloading menu scripts.
=============
*/
static void UI_ReloadMenus_f(void)
{
	UI_RequestMenuReload();
	UI_SyncMenuContext();
}

/*
=============
UI_SetMenuContext_f

Console command to select the active menu script context.
=============
*/
static void UI_SetMenuContext_f(void)
{
	if (Cmd_Argc() < 2) {
	Com_Printf("Usage: %s <main|ingame|auto>\n", Cmd_Argv(0));
	return;
	}

	UI_SetMenuContext(Cmd_Argv(1));
	UI_SyncMenuContext();
}


static const cmdreg_t c_ui[] = {
	{ "forcemenuoff", UI_ForceMenuOff },
	{ "pushmenu", UI_PushMenu_f, UI_PushMenu_c },
	{ "popmenu", UI_PopMenu_f },
	{ "ui_reloadmenus", UI_ReloadMenus_f },
	{ "ui_setcontext", UI_SetMenuContext_f },

	{ NULL, NULL }
};

static void ui_scale_changed(cvar_t *self)
{
UI_Resize();
}

/*
=============
UI_AutoFontPixelHeight

Derives a DPI-aware UI font height from the current renderer configuration.
=============
*/
static int UI_AutoFontPixelHeight(void)
{
int scale = get_auto_scale();
if (scale < 1)
scale = 1;

return 18 * scale;
}

/*
=============
UI_ResolvedFontPixelHeight

Returns the active UI font height, preferring the configured override when valid.
=============
*/
static int UI_ResolvedFontPixelHeight(void)
{
if (ui_font_size && ui_font_size->integer > 0)
return ui_font_size->integer;

return UI_AutoFontPixelHeight();
}

/*
=============
UI_UpdateTypographySet

Rebuilds the typography roles using the latest font handles and scaling data.
=============
*/
static void UI_UpdateTypographySet(void)
{
	const int baseHeight = UI_ResolvedFontPixelHeight();

	uiTypographySpec_t body{};
	uiTypographySpec_t label{};
	uiTypographySpec_t heading{};
	uiTypographySpec_t monospace{};

	const auto &bodyHandles = uis.typographyHandles[UI_TYPO_BODY].empty() ? uis.typographyHandles[UI_TYPO_MONOSPACE] : uis.typographyHandles[UI_TYPO_BODY];
	const auto &labelHandles = uis.typographyHandles[UI_TYPO_LABEL].empty() ? bodyHandles : uis.typographyHandles[UI_TYPO_LABEL];
	const auto &headingHandles = uis.typographyHandles[UI_TYPO_HEADING].empty() ? bodyHandles : uis.typographyHandles[UI_TYPO_HEADING];
	const auto &monoHandles = uis.typographyHandles[UI_TYPO_MONOSPACE].empty() ? bodyHandles : uis.typographyHandles[UI_TYPO_MONOSPACE];

	const auto applyHandles = [](uiTypographySpec_t &spec, const std::vector<qhandle_t> &handles) {
		spec.handles.clear();

		for (qhandle_t handle : handles) {
			if (handle)
				spec.handles.push_back(handle);
		}

		if (spec.handles.empty()) {
			const qhandle_t fallback = SCR_DefaultFontHandle();
			if (fallback)
				spec.handles.push_back(fallback);
		}
	};

	applyHandles(body, bodyHandles);
	applyHandles(label, labelHandles);
	applyHandles(heading, headingHandles);
	applyHandles(monospace, monoHandles);

	body.pixelHeight = baseHeight;
	label.pixelHeight = UI_ScaledFontSize(Q_rint(baseHeight * 0.95f));
	heading.pixelHeight = UI_ScaledFontSize(Q_rint(baseHeight * 1.25f));
	monospace.pixelHeight = baseHeight;

	uis.typography.roles[UI_TYPO_BODY] = body;
	uis.typography.roles[UI_TYPO_LABEL] = label;
	uis.typography.roles[UI_TYPO_HEADING] = heading;
	uis.typography.roles[UI_TYPO_MONOSPACE] = monospace;

	uis.fontHandle = UI_FontForRole(UI_TYPO_BODY);
	const auto &bodyChain = uis.typography.roles[UI_TYPO_BODY].handles;
	uis.fallbackFontHandle = bodyChain.size() > 1 ? bodyChain[1] : UI_FontForRole(UI_TYPO_MONOSPACE);

	UI_GetManager().SyncTypography(uis.typography);
}

/*
=============
UI_RegisterScaledFont

Registers the requested font path with a pixel height tuned for the active DPI.
=============
*/
static qhandle_t UI_RegisterScaledFont(const char *path, int pixelHeight)
{
	if (!path || !*path)
		return 0;

	return SCR_RegisterFontPathWithSize(path, pixelHeight);
}

/*
=============
UI_ClearTypographyHandles

Clears cached typography handles for each role.
=============
*/
static void UI_ClearTypographyHandles(void)
{
	for (auto &entry : uis.typographyHandles) {
		entry.clear();
	}
}

/*
=============
UI_AppendFontPath

Adds a unique font path to the provided collection.
=============
*/
static void UI_AppendFontPath(std::vector<std::string> &paths, const char *path)
{
	if (!path || !*path)
		return;

	for (const auto &existing : paths) {
		if (!Q_stricmp(existing.c_str(), path))
			return;
	}

	paths.emplace_back(path);
}

/*
=============
UI_FontPathsForRole

Builds an ordered list of font paths for a typography role using cvars, script data, and defaults.
=============
*/
static std::vector<std::string> UI_FontPathsForRole(uiTypographyRole_t role)
{
	std::vector<std::string> paths;

	if (ui_font && ui_font->string[0])
		UI_AppendFontPath(paths, ui_font->string);

	for (const auto &preferred : uis.typographyFonts[role])
		UI_AppendFontPath(paths, preferred.c_str());

	if (role != UI_TYPO_BODY) {
		for (const auto &fallback : uis.typographyFonts[UI_TYPO_BODY])
			UI_AppendFontPath(paths, fallback.c_str());
	}

	if (ui_font_fallback && ui_font_fallback->string[0])
		UI_AppendFontPath(paths, ui_font_fallback->string);

	UI_AppendFontPath(paths, "conchars.pcx");

	return paths;
}

/*
=============
UI_RegisterTypographyRole

Registers all font paths for a typography role at the desired pixel height.
=============
*/
static void UI_RegisterTypographyRole(uiTypographyRole_t role, int pixelHeight)
{
	std::vector<std::string> paths = UI_FontPathsForRole(role);
	auto &handles = uis.typographyHandles[role];

	handles.clear();

	for (const auto &path : paths) {
		qhandle_t handle = UI_RegisterScaledFont(path.c_str(), pixelHeight);
		if (handle)
			handles.push_back(handle);
	}

	if (handles.empty()) {
		qhandle_t fallback = SCR_DefaultFontHandle();
		if (fallback)
			handles.push_back(fallback);
	}
}

/*
=============
UI_RefreshFonts

Reloads UI typography handles using configured font paths and fallbacks.
=============
*/
void UI_RefreshFonts(void)
{
	const int pixelHeight = UI_ResolvedFontPixelHeight();
	uis.fontPixelHeight = pixelHeight;

	UI_ClearTypographyHandles();

	for (int role = 0; role < UI_TYPO_ROLE_COUNT; role++)
		UI_RegisterTypographyRole(static_cast<uiTypographyRole_t>(role), pixelHeight);

	UI_UpdateLayoutMetrics();
	UI_UpdateTypographySet();
}

/*
=============
UI_ComputeCursorScale

Returns a scaling factor for cursors derived from the current DPI-aware UI scale.
=============
*/
static float UI_ComputeCursorScale(void)
{
int scale = get_auto_scale();
if (scale < 1)
scale = 1;

return static_cast<float>(scale);
}

/*
=============
UI_RegisterCursorPic

Attempts to register a themed cursor image and falls back to the legacy asset.
=============
*/
static qhandle_t UI_RegisterCursorPic(const char *theme, const char *base)
{
if (theme && *theme) {
qhandle_t themed = R_RegisterPic(va("ui/cursors/%s/%s", theme, base));
if (themed)
return themed;
}

return R_RegisterPic(base);
}

/*
=============
UI_EffectiveStateColor

Returns a palette state color, falling back to the default state when needed.
=============
*/
static color_t UI_EffectiveStateColor(const uiPaletteEntry_t *entry, uiControlState_t state)
{
color_t color = entry->states[state];

if (!color.u32) {
color = entry->states[UI_STATE_DEFAULT];
}

return color;
}

/*
=============
UI_ColorForRole

Resolves a color for the requested palette role and UI state.
=============
*/
color_t UI_ColorForRole(uiColorRole_t role, uiControlState_t state)
{
if (role < 0 || role >= UI_COLOR_ROLE_COUNT) {
return ColorRGBA(0, 0, 0, 0);
}

if (state < 0 || state >= UI_STATE_COUNT) {
state = UI_STATE_DEFAULT;
}

return UI_EffectiveStateColor(&uis.palette[role], state);
}

/*
=============
UI_SetPaletteEntry

Initializes a palette entry with state-aware variants.
=============
*/
static uiPaletteEntry_t UI_SetPaletteEntry(color_t base, color_t hovered, color_t active, color_t disabled, color_t focused)
{
uiPaletteEntry_t entry{};

entry.states[UI_STATE_DEFAULT] = base;
entry.states[UI_STATE_HOVERED] = hovered.u32 ? hovered : base;
entry.states[UI_STATE_ACTIVE] = active.u32 ? active : base;
entry.states[UI_STATE_DISABLED] = disabled.u32 ? disabled : base;
entry.states[UI_STATE_FOCUSED] = focused.u32 ? focused : base;

return entry;
}

/*
=============
UI_PopulateDefaultPalette

Defines the default palette entries for the active theme.
=============
*/
static void UI_PopulateDefaultPalette(bool lightTheme)
{
color_t backgroundBase = lightTheme ? ColorRGBA(240, 240, 240, 255) : ColorRGBA(12, 12, 16, 255);
color_t backgroundHover = lightTheme ? ColorRGBA(230, 230, 230, 255) : ColorRGBA(24, 24, 32, 255);
color_t backgroundActive = lightTheme ? ColorRGBA(220, 220, 220, 255) : ColorRGBA(32, 32, 48, 255);
color_t backgroundDisabled = lightTheme ? ColorRGBA(210, 210, 210, 255) : ColorRGBA(24, 24, 24, 255);

color_t surfaceBase = lightTheme ? ColorRGBA(252, 252, 252, 240) : ColorRGBA(24, 24, 32, 240);
color_t surfaceHover = lightTheme ? ColorRGBA(246, 246, 246, 240) : ColorRGBA(32, 32, 44, 240);
color_t surfaceActive = lightTheme ? ColorRGBA(240, 240, 240, 240) : ColorRGBA(40, 40, 56, 240);
color_t surfaceDisabled = lightTheme ? ColorRGBA(232, 232, 232, 230) : ColorRGBA(28, 28, 36, 230);

color_t accentBase = lightTheme ? ColorRGBA(30, 110, 210, 200) : ColorRGBA(15, 128, 235, 180);
color_t accentHover = lightTheme ? ColorRGBA(20, 100, 200, 220) : ColorRGBA(32, 154, 255, 200);
color_t accentActive = lightTheme ? ColorRGBA(10, 80, 170, 240) : ColorRGBA(10, 120, 220, 220);
color_t accentDisabled = lightTheme ? ColorRGBA(120, 140, 170, 180) : ColorRGBA(80, 96, 120, 160);

color_t textBase = lightTheme ? ColorRGBA(18, 18, 18, 255) : ColorRGBA(235, 235, 240, 255);
color_t textMuted = lightTheme ? ColorRGBA(96, 96, 96, 255) : ColorRGBA(160, 160, 160, 255);
color_t textHighlight = lightTheme ? ColorRGBA(0, 80, 160, 255) : ColorRGBA(96, 176, 255, 255);

uis.palette[UI_COLOR_BACKGROUND] = UI_SetPaletteEntry(backgroundBase, backgroundHover, backgroundActive, backgroundDisabled, backgroundHover);
uis.palette[UI_COLOR_SURFACE] = UI_SetPaletteEntry(surfaceBase, surfaceHover, surfaceActive, surfaceDisabled, surfaceHover);
uis.palette[UI_COLOR_ACCENT] = UI_SetPaletteEntry(accentBase, accentHover, accentActive, accentDisabled, accentHover);
uis.palette[UI_COLOR_TEXT] = UI_SetPaletteEntry(textBase, textBase, textBase, textMuted, textHighlight);
uis.palette[UI_COLOR_HIGHLIGHT] = UI_SetPaletteEntry(textHighlight, accentHover, accentActive, accentDisabled, textHighlight);
uis.palette[UI_COLOR_MUTED] = UI_SetPaletteEntry(textMuted, textMuted, textMuted, textMuted, textMuted);
	UI_SyncUxPalette();
}

/*
=============
UI_UpdateLegacyColorsFromPalette

Keeps the legacy color bundle in sync with the new palette roles.
=============
*/
static void UI_UpdateLegacyColorsFromPalette(void)
{
	uis.color.background = UI_ColorForRole(UI_COLOR_BACKGROUND, UI_STATE_DEFAULT);
	uis.color.normal = UI_ColorForRole(UI_COLOR_ACCENT, UI_STATE_DEFAULT);
	uis.color.active = UI_ColorForRole(UI_COLOR_ACCENT, UI_STATE_ACTIVE);
	uis.color.selection = UI_ColorForRole(UI_COLOR_HIGHLIGHT, UI_STATE_DEFAULT);
	uis.color.disabled = UI_ColorForRole(UI_COLOR_MUTED, UI_STATE_DISABLED);
}

/*
=============
UI_SyncUxPalette

Copies the resolved palette into the UIX theming context.
=============
*/
static void UI_SyncUxPalette(void)
{
	UI_GetManager().SyncPalette(uis);
}

/*
=============
UI_SyncUxLayout

Updates the UIX layout engine and root scene graph layer using the latest metrics.
=============
*/
static void UI_SyncUxLayout(void)
{
	UI_GetManager().SyncLayout(uis.layout);
}

/*
=============
UI_RefreshCursors

Loads the cursor theme and scales it according to the current DPI-aware settings.
=============
*/
static void UI_RefreshCursors(void)
{
const char *theme = ui_cursor_theme ? ui_cursor_theme->string : "";
uis.cursorHandle = UI_RegisterCursorPic(theme, "ch1");
R_GetPicSize(&uis.cursorWidth, &uis.cursorHeight, uis.cursorHandle);

for (int i = 0; i < NUM_CURSOR_FRAMES; i++) {
uis.bitmapCursors[i] = UI_RegisterCursorPic(theme, va("m_cursor%d", i));
}

uis.cursorScale = UI_ComputeCursorScale();
uis.cursorDrawWidth = Q_rint(uis.cursorWidth * uis.cursorScale);
uis.cursorDrawHeight = Q_rint(uis.cursorHeight * uis.cursorScale);

if (uis.cursorDrawWidth <= 0)
uis.cursorDrawWidth = uis.cursorWidth;
if (uis.cursorDrawHeight <= 0)
uis.cursorDrawHeight = uis.cursorHeight;
}

/*
=============
UI_ApplyThemeColors

Sets the UI color palette according to the selected theme preference.
=============
*/
static void UI_ApplyThemeColors(void)
{
const bool light = ui_color_theme && !Q_stricmp(ui_color_theme->string, "light");

UI_PopulateDefaultPalette(light);
UI_UpdateLegacyColorsFromPalette();
}

/*
=============
UI_SelectFontHandle

Returns the most appropriate font handle for the provided string, honoring fallbacks.
=============
*/
static qhandle_t UI_SelectFontHandle(const char *string, int flags)
{
	const auto &handles = uis.typography.roles[UI_TYPO_BODY].handles;

	for (qhandle_t handle : handles) {
		const int width = SCR_MeasureString(1, flags & ~UI_MULTILINE, MAX_STRING_CHARS, string, handle);
		if (width > 0)
			return handle;
	}

	if (uis.fallbackFontHandle)
		return uis.fallbackFontHandle;

	return SCR_DefaultFontHandle();
}

/*
=============
UI_FontForRole

Returns the configured font handle for a typography role.
=============
*/
qhandle_t UI_FontForRole(uiTypographyRole_t role)
{
	if (role < 0 || role >= UI_TYPO_ROLE_COUNT) {
		return SCR_DefaultFontHandle();
	}

	const auto &handles = uis.typography.roles[role].handles;

	for (qhandle_t handle : handles) {
		if (handle)
			return handle;
	}

	return SCR_DefaultFontHandle();
}

/*
=============
UI_FontPixelHeightForRole

Returns the pixel height configured for a typography role.
=============
*/
int UI_FontPixelHeightForRole(uiTypographyRole_t role)
{
if (role < 0 || role >= UI_TYPO_ROLE_COUNT) {
return UI_ResolvedFontPixelHeight();
}

const int height = uis.typography.roles[role].pixelHeight;

if (height > 0) {
return height;
}

return UI_ResolvedFontPixelHeight();
}

void UI_ModeChanged(void)
{
    ui_scale = Cvar_Get("ui_scale", "0", 0);
    ui_scale->changed = ui_scale_changed;
    UI_Resize();
}

/*
=================
	UI_FreeMenus

Releases all registered menus.
=================
*/
static void UI_FreeMenus(void)
{
	menuFrameWork_t *menu, *next;

	LIST_FOR_EACH_SAFE(menuFrameWork_t, menu, next, &ui_menus, entry) {
	if (menu->free) {
	menu->free(menu);
	}
	}
	List_Init(&ui_menus);
}

/*
=================
UI_ClearMenus

Closes any active menus and clears the menu list.
=================
*/
void UI_ClearMenus(void)
{
	UI_ForceMenuOff();
	UI_FreeMenus();
}

/*
=================
UI_RegisterBuiltinMenus

Registers built-in menu definitions.
=================
*/
void UI_RegisterBuiltinMenus(void)
{
	M_Menu_PlayerConfig();
	M_Menu_Servers();
	M_Menu_Demos();
}

/*
=================
UI_Init
=================
*/
void UI_Init(void)
{
	Cmd_Register(c_ui);

	ui_debug = Cvar_Get("ui_debug", "0", 0);
	ui_open = Cvar_Get("ui_open", "0", 0);

	UI_InitScriptController();
	UI_ModeChanged();

	uis.fontHandle = SCR_RegisterFontPath("conchars.pcx");
	uis.cursorHandle = R_RegisterPic("ch1");
	R_GetPicSize(&uis.cursorWidth, &uis.cursorHeight, uis.cursorHandle);

	for (int i = 0; i < NUM_CURSOR_FRAMES; i++) {
	uis.bitmapCursors[i] = R_RegisterPic(va("m_cursor%d", i));
	}

	UI_PopulateDefaultPalette(false);
	UI_UpdateLegacyColorsFromPalette();
	UI_UpdateTypographySet();
	UI_GetManager().Initialize();
	strcpy(uis.weaponModel, "w_railgun.md2");

	UI_MapDB_Init();

	UI_LoadScript();

	Com_DPrintf("Registered %d menus.\n", List_Count(&ui_menus));

	uis.initialized = true;
}

/*
=================
UI_Shutdown
=================
*/
void UI_Shutdown(void)
{
		if (!uis.initialized) {
			return;
			}
		UI_ForceMenuOff();
	
		ui_scale->changed = NULL;
	
		PlayerModel_Free();
	
		UI_FreeMenus();
	
		UI_GetManager().Shutdown();
	
		Cmd_Deregister(c_ui);
	
			memset(&uis, 0, sizeof(uis));
	
		UI_MapDB_Shutdown();
	
		Z_LeakTest(TAG_UI);
}
