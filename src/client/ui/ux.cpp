#include "ux.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ui::ux {

	static std::unique_ptr<UIXSystem> g_uixSystem;

	/*
	=============
	LayoutValue::LayoutValue

	Builds a zeroed layout value in pixel units.
	=============
	*/
	LayoutValue::LayoutValue()
	: m_value(0.0f), m_unit(LayoutUnit::Pixels)
	{
	}

	/*
	=============
	LayoutValue::LayoutValue

	Builds a layout value with the provided scalar and unit.
	=============
	*/
	LayoutValue::LayoutValue(float scalar, LayoutUnit unit)
	: m_value(scalar), m_unit(unit)
	{
	}

	/*
	=============
	LayoutValue::Percent

	Creates a percentage-based layout value.
	=============
	*/
	LayoutValue LayoutValue::Percent(float percent)
	{
		return LayoutValue(percent, LayoutUnit::Percent);
	}

	/*
	=============
	LayoutValue::Pixels

	Creates a pixel-based layout value.
	=============
	*/
	LayoutValue LayoutValue::Pixels(float pixels)
	{
		return LayoutValue(pixels, LayoutUnit::Pixels);
	}

	/*
	=============
	LayoutValue::Value

	Returns the stored scalar value.
	=============
	*/
	float LayoutValue::Value() const
	{
		return m_value;
	}

	/*
	=============
	LayoutValue::Unit

	Returns the measurement unit associated with the value.
	=============
	*/
	LayoutUnit LayoutValue::Unit() const
	{
		return m_unit;
	}

	/*
	=============
	LayoutRect::LayoutRect

	Builds an empty layout rectangle with pixel units.
	=============
	*/
	LayoutRect::LayoutRect()
	: m_x(0.0f, LayoutUnit::Pixels), m_y(0.0f, LayoutUnit::Pixels), m_width(0.0f, LayoutUnit::Pixels), m_height(0.0f, LayoutUnit::Pixels), m_padding(0), m_spacing(0)
	{
	}

	/*
	=============
	LayoutRect::LayoutRect

	Builds a layout rectangle with the provided values.
	=============
	*/
	LayoutRect::LayoutRect(LayoutValue x, LayoutValue y, LayoutValue width, LayoutValue height, int padding, int spacing)
	: m_x(x), m_y(y), m_width(width), m_height(height), m_padding(padding), m_spacing(spacing)
	{
	}

	/*
	=============
	LayoutRect::X

	Returns the X layout value.
	=============
	*/
	LayoutValue LayoutRect::X() const
	{
		return m_x;
	}

	/*
	=============
	LayoutRect::Y

	Returns the Y layout value.
	=============
	*/
	LayoutValue LayoutRect::Y() const
	{
		return m_y;
	}

	/*
	=============
	LayoutRect::Width

	Returns the width layout value.
	=============
	*/
	LayoutValue LayoutRect::Width() const
	{
		return m_width;
	}

	/*
	=============
	LayoutRect::Height

	Returns the height layout value.
	=============
	*/
	LayoutValue LayoutRect::Height() const
	{
		return m_height;
	}

	/*
	=============
	LayoutRect::Padding

	Returns the padding applied to the rectangle.
	=============
	*/
	int LayoutRect::Padding() const
	{
		return m_padding;
	}

	/*
	=============
	LayoutRect::Spacing

	Returns the spacing applied to child layout calculations.
	=============
	*/
	int LayoutRect::Spacing() const
	{
		return m_spacing;
	}

	/*
	=============
	FlexConstraint::FlexConstraint

	Builds a zeroed flex constraint with a pixel basis.
	=============
	*/
	FlexConstraint::FlexConstraint()
	: m_grow(0.0f), m_shrink(0.0f), m_basis(LayoutValue::Pixels(0.0f))
	{
	}

	/*
	=============
	FlexConstraint::FlexConstraint

	Constructs a flex constraint with explicit grow, shrink, and basis values.
	=============
	*/
	FlexConstraint::FlexConstraint(float grow, float shrink, LayoutValue basis)
	: m_grow(grow), m_shrink(shrink), m_basis(basis)
	{
	}

	/*
	=============
	FlexConstraint::Grow

	Returns the growth factor for the constraint.
	=============
	*/
	float FlexConstraint::Grow() const
	{
		return m_grow;
	}

	/*
	=============
	FlexConstraint::Shrink

	Returns the shrink factor for the constraint.
	=============
	*/
	float FlexConstraint::Shrink() const
	{
		return m_shrink;
	}

	/*
	=============
	FlexConstraint::Basis

	Returns the preferred basis size for the constraint.
	=============
	*/
	LayoutValue FlexConstraint::Basis() const
	{
		return m_basis;
	}

	/*
	=============
	LayoutEngine::LayoutEngine

	Builds a layout engine with default metrics.
	=============
	*/
	LayoutEngine::LayoutEngine()
	{
		m_metrics = {};
	}

	/*
	=============
	LayoutEngine::LayoutEngine

	Builds a layout engine with the provided metrics.
	=============
	*/
	LayoutEngine::LayoutEngine(const uiLayoutMetrics_t &metrics)
	{
		m_metrics = metrics;
	}

	/*
	=============
	LayoutEngine::UpdateMetrics

	Updates the cached layout metrics used by the engine.
	=============
	*/
	void LayoutEngine::UpdateMetrics(const uiLayoutMetrics_t &metrics)
	{
		m_metrics = metrics;
	}

	/*
	=============
	LayoutEngine::ResolveValue

	Converts a layout value into pixels using the reference size.
	=============
	*/
	int LayoutEngine::ResolveValue(const LayoutValue &value, int reference) const
	{
		if (value.Unit() == LayoutUnit::Percent) {
			return Q_rint(value.Value() * reference);
		}

		return Q_rint(value.Value());
	}

	/*
	=============
	LayoutEngine::ResolveRect

	Translates a layout rectangle into absolute pixel bounds.
	=============
	*/
	vrect_t LayoutEngine::ResolveRect(const LayoutRect &rect, const vrect_t &parent) const
	{
		vrect_t resolved{};

		resolved.x = parent.x + ResolveValue(rect.X(), parent.width);
		resolved.y = parent.y + ResolveValue(rect.Y(), parent.height);
		resolved.width = ResolveValue(rect.Width(), parent.width);
		resolved.height = ResolveValue(rect.Height(), parent.height);

		const int padding = rect.Padding();
		if (padding != 0) {
			resolved.x += padding;
			resolved.y += padding;
			resolved.width -= padding * 2;
			resolved.height -= padding * 2;
		}

		if (resolved.width < 0) {
			resolved.width = 0;
		}
		if (resolved.height < 0) {
			resolved.height = 0;
		}

		return resolved;
	}

	/*
	=============
	LayoutEngine::ApplyFlex

	Resolves a group of layout rectangles into a flex row or column.
	=============
	*/
	std::vector<vrect_t> LayoutEngine::ApplyFlex(const std::span<const LayoutRect> children, const vrect_t &parent, LayoutFlow direction, const std::span<const FlexConstraint> constraints) const
	{
		std::vector<vrect_t> resolved;
		resolved.reserve(children.size());

		const bool horizontal = direction == LayoutFlow::Row;
		const int parentLength = horizontal ? parent.width : parent.height;
		int spacing = 0;
		if (!children.empty()) {
			spacing = children.front().Spacing();
		}

		std::vector<float> bases(children.size(), 0.0f);
		float totalGrow = 0.0f;
		float totalShrink = 0.0f;
		float used = std::max(0, static_cast<int>(children.size() - 1)) * spacing;

		for (size_t i = 0; i < children.size(); i++) {
			const LayoutRect &child = children[i];
			float base = 0.0f;
			if (i < constraints.size()) {
				const FlexConstraint &constraint = constraints[i];
				base = static_cast<float>(ResolveValue(constraint.Basis(), parentLength));
				totalGrow += constraint.Grow();
				totalShrink += constraint.Shrink();
			} else {
				base = horizontal ? ResolveValue(child.Width(), parentLength) : ResolveValue(child.Height(), parentLength);
			}

			bases[i] = base;
			used += base;
		}

		const float available = static_cast<float>(parentLength - used);
		float cursorX = static_cast<float>(parent.x);
		float cursorY = static_cast<float>(parent.y);

		for (size_t i = 0; i < children.size(); i++) {
			float base = bases[i];
			float grow = 0.0f;
			float shrink = 0.0f;
			if (i < constraints.size() && available != 0.0f) {
				const FlexConstraint &constraint = constraints[i];
				if (available > 0.0f && totalGrow > 0.0f) {
					grow = available * (constraint.Grow() / totalGrow);
				} else if (available < 0.0f && totalShrink > 0.0f) {
					shrink = available * (constraint.Shrink() / totalShrink);
				}
			}

			float length = base + grow + shrink;
			vrect_t bounds{};
			if (horizontal) {
				bounds.x = Q_rint(cursorX);
				bounds.y = parent.y;
				bounds.width = Q_rint(length);
				bounds.height = ResolveValue(children[i].Height(), parent.height);
				cursorX += length + static_cast<float>(spacing);
			} else {
				bounds.x = parent.x;
				bounds.y = Q_rint(cursorY);
				bounds.width = ResolveValue(children[i].Width(), parent.width);
				bounds.height = Q_rint(length);
				cursorY += length + static_cast<float>(spacing);
			}

			resolved.push_back(bounds);
		}

		return resolved;
	}

	/*
	=============
	LayoutEngine::ApplyGrid

	Resolves a set of layout rectangles into a grid using the provided column count.
	=============
	*/
	std::vector<vrect_t> LayoutEngine::ApplyGrid(const std::span<const LayoutRect> children, const vrect_t &parent, int columns, int rowSpacing, int columnSpacing) const
	{
		std::vector<vrect_t> resolved;
		resolved.reserve(children.size());

		if (columns <= 0) {
			return resolved;
		}

		const int columnWidth = (parent.width - (columns - 1) * columnSpacing) / columns;
		int cursorX = parent.x;
		int cursorY = parent.y;
		int column = 0;

		for (const LayoutRect &child : children) {
			vrect_t bounds{};
			bounds.x = cursorX;
			bounds.y = cursorY;
			bounds.width = columnWidth;
			bounds.height = ResolveValue(child.Height(), parent.height);
			resolved.push_back(bounds);

			column++;
			if (column >= columns) {
				column = 0;
				cursorX = parent.x;
				cursorY += bounds.height + rowSpacing;
			} else {
				cursorX += columnWidth + columnSpacing;
			}
		}

		return resolved;
	}

	/*
	=============
	UIEvent::UIEvent

	Builds a navigation-focused UI event.
	=============
	*/
	UIEvent::UIEvent(EventType type, NavigationDirection direction, vrect_t region)
	: m_type(type), m_direction(direction), m_region(region)
	{
	}

	/*
	=============
	UIEvent::UIEvent

	Builds an event without navigation direction information.
	=============
	*/
	UIEvent::UIEvent(EventType type, vrect_t region)
	: m_type(type), m_direction(NavigationDirection::Next), m_region(region)
	{
	}

	/*
	=============
	UIEvent::Type

	Returns the event type.
	=============
	*/
	EventType UIEvent::Type() const
	{
		return m_type;
	}

	/*
	=============
	UIEvent::Direction

	Returns the navigation direction associated with the event.
	=============
	*/
	NavigationDirection UIEvent::Direction() const
	{
		return m_direction;
	}

	/*
	=============
	UIEvent::Region

	Returns the region attached to the event.
	=============
	*/
	vrect_t UIEvent::Region() const
	{
		return m_region;
	}

	/*
	=============
	Widget::Widget

	Builds a widget with an identifier and type.
	=============
	*/
	Widget::Widget(std::string identifier, WidgetType type)
	: m_type(type), m_identifier(std::move(identifier)), m_state(UI_STATE_DEFAULT), m_colorRole(UI_COLOR_SURFACE)
	{
		m_layout = LayoutRect(LayoutValue::Pixels(0.0f), LayoutValue::Pixels(0.0f), LayoutValue::Percent(1.0f), LayoutValue::Percent(1.0f));
		m_bounds = {};
	}

	/*
	=============
	Widget::OnLayout

	Resolves the widget bounds and cascades layout to children.
	=============
	*/
	void Widget::OnLayout(LayoutEngine &engine, const vrect_t &parentBounds)
	{
		m_bounds = engine.ResolveRect(m_layout, parentBounds);
		for (const auto &child : m_children) {
			child->OnLayout(engine, m_bounds);
		}
	}

	/*
	=============
	Widget::OnEvent

	Dispatches events to child widgets by default.
	=============
	*/
	void Widget::OnEvent(const UIEvent &event)
	{
		for (const auto &child : m_children) {
			child->OnEvent(event);
		}
	}

	/*
	=============
	Widget::AddChild

	Adds a child widget to the hierarchy.
	=============
	*/
	void Widget::AddChild(const std::shared_ptr<Widget> &child)
	{
		m_children.push_back(child);
	}

	/*
	=============
	Widget::SetLayout

	Sets the layout rectangle used to resolve bounds.
	=============
	*/
	void Widget::SetLayout(const LayoutRect &layout)
	{
		m_layout = layout;
	}

	/*
	=============
	Widget::GetLayout

	Returns the configured layout rectangle.
	=============
	*/
	const LayoutRect &Widget::GetLayout() const
	{
		return m_layout;
	}

	/*
	=============
	Widget::ResolvedBounds

	Returns the bounds resolved during layout.
	=============
	*/
	const vrect_t &Widget::ResolvedBounds() const
	{
		return m_bounds;
	}

	/*
	=============
	Widget::Type

	Returns the widget type identifier.
	=============
	*/
	WidgetType Widget::Type() const
	{
		return m_type;
	}

	/*
	=============
	Widget::Id

	Returns the stable widget identifier.
	=============
	*/
	const std::string &Widget::Id() const
	{
		return m_identifier;
	}

	/*
	=============
	Widget::SetState

	Sets the current control state for theming and input.
	=============
	*/
	void Widget::SetState(uiControlState_t state)
	{
		m_state = state;
	}

	/*
	=============
	Widget::State

	Returns the current control state.
	=============
	*/
	uiControlState_t Widget::State() const
	{
		return m_state;
	}

	/*
	=============
	Widget::SetColorRole

	Assigns the palette role for the widget.
	=============
	*/
	void Widget::SetColorRole(uiColorRole_t role)
	{
		m_colorRole = role;
	}

	/*
	=============
	Widget::ColorRole

	Returns the palette role used by the widget.
	=============
	*/
	uiColorRole_t Widget::ColorRole() const
	{
		return m_colorRole;
	}

	/*
	=============
	Widget::Children

	Returns mutable access to the widget children.
	=============
	*/
	std::vector<std::shared_ptr<Widget>> &Widget::Children()
	{
		return m_children;
	}

	/*
	=============
	ButtonWidget::ButtonWidget

	Builds a button widget with a default label.
	=============
	*/
	ButtonWidget::ButtonWidget(const std::string &identifier)
	: Widget(identifier, WidgetType::Button), m_label("Button")
	{
	}

	/*
	=============
	ButtonWidget::SetLabel

	Sets the label displayed by the button.
	=============
	*/
	void ButtonWidget::SetLabel(std::string label)
	{
		m_label = std::move(label);
	}

	/*
	=============
	ButtonWidget::Label

	Returns the button label.
	=============
	*/
	const std::string &ButtonWidget::Label() const
	{
		return m_label;
	}

	/*
	=============
	ButtonWidget::OnEvent

	Updates control state based on focus and hover events.
	=============
	*/
	void ButtonWidget::OnEvent(const UIEvent &event)
	{
		if (event.Type() == EventType::Focus) {
			SetState(UI_STATE_FOCUSED);
		} else if (event.Type() == EventType::Hover) {
			SetState(UI_STATE_HOVERED);
		} else if (event.Type() == EventType::Activate) {
			SetState(UI_STATE_ACTIVE);
		}
	}

	/*
	=============
	ListWidget::ListWidget

	Builds a list widget with no initial items.
	=============
	*/
	ListWidget::ListWidget(const std::string &identifier)
	: Widget(identifier, WidgetType::List), m_selection(0)
	{
	}

	/*
	=============
	ListWidget::SetItems

	Assigns the set of items displayed by the list.
	=============
	*/
	void ListWidget::SetItems(std::vector<std::string> items)
	{
		m_items = std::move(items);
		if (m_selection >= static_cast<int>(m_items.size())) {
			m_selection = 0;
		}
	}

	/*
	=============
	ListWidget::Items

	Returns the current list contents.
	=============
	*/
	const std::vector<std::string> &ListWidget::Items() const
	{
		return m_items;
	}

	/*
	=============
	ListWidget::OnEvent

	Handles navigation by moving the selection index.
	=============
	*/
	void ListWidget::OnEvent(const UIEvent &event)
	{
		if (m_items.empty()) {
			return;
		}

		if (event.Type() == EventType::Navigate) {
			if (event.Direction() == NavigationDirection::Next || event.Direction() == NavigationDirection::Down) {
				m_selection = (m_selection + 1) % static_cast<int>(m_items.size());
			} else if (event.Direction() == NavigationDirection::Previous || event.Direction() == NavigationDirection::Up) {
				m_selection = (m_selection - 1);
				if (m_selection < 0) {
					m_selection = static_cast<int>(m_items.size()) - 1;
				}
			}
		}
	}

	/*
	=============
	SliderWidget::SliderWidget

	Builds a slider widget with a zeroed range.
	=============
	*/
	SliderWidget::SliderWidget(const std::string &identifier)
	: Widget(identifier, WidgetType::Slider), m_min(0.0f), m_max(1.0f), m_value(0.0f)
	{
	}

	/*
	=============
	SliderWidget::SetRange

	Defines the minimum and maximum slider values.
	=============
	*/
	void SliderWidget::SetRange(float minValue, float maxValue)
	{
		m_min = minValue;
		m_max = maxValue;
		if (m_min > m_max) {
			std::swap(m_min, m_max);
		}
	}

	/*
	=============
	SliderWidget::SetValue

	Sets the current slider value clamped to the range.
	=============
	*/
	void SliderWidget::SetValue(float value)
	{
		m_value = std::clamp(value, m_min, m_max);
	}

	/*
	=============
	SliderWidget::Value

	Returns the current slider value.
	=============
	*/
	float SliderWidget::Value() const
	{
		return m_value;
	}

	/*
	=============
	SliderWidget::OnEvent

	Adjusts the slider value based on pointer or navigation input.
	=============
	*/
	void SliderWidget::OnEvent(const UIEvent &event)
	{
		if (event.Type() == EventType::Navigate) {
			float delta = (event.Direction() == NavigationDirection::Right || event.Direction() == NavigationDirection::Next) ? 0.05f : -0.05f;
			SetValue(m_value + delta * (m_max - m_min));
		} else if (event.Type() == EventType::PointerMove) {
			const vrect_t bounds = ResolvedBounds();
			if (bounds.width > 0) {
				float relative = static_cast<float>(event.Region().x - bounds.x) / static_cast<float>(bounds.width);
				SetValue(m_min + relative * (m_max - m_min));
			}
		}
	}

	/*
	=============
	ModalOverlayWidget::ModalOverlayWidget

	Builds a modal overlay widget.
	=============
	*/
	ModalOverlayWidget::ModalOverlayWidget(const std::string &identifier)
	: Widget(identifier, WidgetType::ModalOverlay), m_blocking(true)
	{
	}

	/*
	=============
	ModalOverlayWidget::SetBlocking

	Sets whether the overlay blocks input to lower layers.
	=============
	*/
	void ModalOverlayWidget::SetBlocking(bool blocking)
	{
		m_blocking = blocking;
	}

	/*
	=============
	ModalOverlayWidget::IsBlocking

	Returns whether the overlay captures interaction exclusively.
	=============
	*/
	bool ModalOverlayWidget::IsBlocking() const
	{
		return m_blocking;
	}

	/*
	=============
	ModalOverlayWidget::OnEvent

	Updates state to reflect focus or hover changes on the overlay.
	=============
	*/
	void ModalOverlayWidget::OnEvent(const UIEvent &event)
	{
		if (event.Type() == EventType::Focus) {
			SetState(UI_STATE_FOCUSED);
		} else if (event.Type() == EventType::Hover) {
			SetState(UI_STATE_HOVERED);
		}
	}

	/*
	=============
	SceneLayer::SceneLayer

	Builds a scene layer with the provided name and z-index.
	=============
	*/
	SceneLayer::SceneLayer(std::string name, int zIndex)
	: m_name(std::move(name)), m_zIndex(zIndex)
	{
	}

	/*
	=============
	SceneLayer::SetRoot

	Assigns the root widget for the layer.
	=============
	*/
	void SceneLayer::SetRoot(const std::shared_ptr<Widget> &root)
	{
		m_root = root;
	}

	/*
	=============
	SceneLayer::Root

	Returns the root widget associated with the layer.
	=============
	*/
	std::shared_ptr<Widget> SceneLayer::Root() const
	{
		return m_root;
	}

	/*
	=============
	SceneLayer::Name

	Returns the layer name.
	=============
	*/
	const std::string &SceneLayer::Name() const
	{
		return m_name;
	}

	/*
	=============
	SceneLayer::ZIndex

	Returns the stacking order for the layer.
	=============
	*/
	int SceneLayer::ZIndex() const
	{
		return m_zIndex;
	}

	/*
	=============
	SceneGraph::AddLayer

	Adds a layer to the scene graph and sorts by z-index.
	=============
	*/
	void SceneGraph::AddLayer(const std::shared_ptr<SceneLayer> &layer)
	{
		m_layers.push_back(layer);
		std::stable_sort(m_layers.begin(), m_layers.end(), [](const std::shared_ptr<SceneLayer> &lhs, const std::shared_ptr<SceneLayer> &rhs) {
			return lhs->ZIndex() < rhs->ZIndex();
		});
	}

	/*
	=============
	SceneGraph::RemoveLayer

	Removes a layer from the graph by name.
	=============
	*/
	void SceneGraph::RemoveLayer(const std::string &name)
	{
		m_layers.erase(std::remove_if(m_layers.begin(), m_layers.end(), [&](const std::shared_ptr<SceneLayer> &layer) {
			return layer && layer->Name() == name;
		}), m_layers.end());
	}

	/*
	=============
	SceneGraph::Clear

	Clears all layers from the graph.
	=============
	*/
	void SceneGraph::Clear()
	{
		m_layers.clear();
	}

	/*
	=============
	SceneGraph::FindLayer

	Searches for a layer by name.
	=============
	*/
	std::shared_ptr<SceneLayer> SceneGraph::FindLayer(const std::string &name) const
	{
		for (const auto &layer : m_layers) {
			if (layer && layer->Name() == name) {
				return layer;
			}
		}

		return nullptr;
	}

	/*
	=============
	SceneGraph::OrderedLayers

	Returns the layers in sorted order.
	=============
	*/
	std::vector<std::shared_ptr<SceneLayer>> SceneGraph::OrderedLayers() const
	{
		return m_layers;
	}

	/*
	=============
	InteractionController::InteractionController

	Builds an interaction controller with empty focus and hover states.
	=============
	*/
	InteractionController::InteractionController()
	{
	}

	/*
	=============
	InteractionController::RouteEvent

	Routes an event through the widget hierarchy starting at the provided root.
	=============
	*/
	void InteractionController::RouteEvent(const UIEvent &event, const std::shared_ptr<Widget> &root)
	{
		Visit(root, event);
	}

	/*
	=============
	InteractionController::Focus

	Marks a widget as focused for future navigation events.
	=============
	*/
	void InteractionController::Focus(const std::shared_ptr<Widget> &widget)
	{
		m_focus = widget;
		if (auto locked = m_focus.lock()) {
			locked->OnEvent(UIEvent(EventType::Focus, locked->ResolvedBounds()));
		}
	}

	/*
	=============
	InteractionController::Hover

	Marks a widget as hovered for pointer-driven interactions.
	=============
	*/
	void InteractionController::Hover(const std::shared_ptr<Widget> &widget)
	{
		m_hover = widget;
		if (auto locked = m_hover.lock()) {
			locked->OnEvent(UIEvent(EventType::Hover, locked->ResolvedBounds()));
		}
	}

	/*
	=============
	InteractionController::Focused

	Returns the currently focused widget, if any.
	=============
	*/
	std::weak_ptr<Widget> InteractionController::Focused() const
	{
		return m_focus;
	}

	/*
	=============
	InteractionController::Hovered

	Returns the currently hovered widget, if any.
	=============
	*/
	std::weak_ptr<Widget> InteractionController::Hovered() const
	{
		return m_hover;
	}

	/*
	=============
	InteractionController::Visit

	Traverses the widget tree to deliver events and update hover state.
	=============
	*/
	void InteractionController::Visit(const std::shared_ptr<Widget> &widget, const UIEvent &event)
	{
		if (!widget) {
			return;
		}

		const vrect_t bounds = widget->ResolvedBounds();
		const vrect_t eventBounds = event.Region();
		const bool inside = eventBounds.x >= bounds.x && eventBounds.x <= bounds.x + bounds.width && eventBounds.y >= bounds.y && eventBounds.y <= bounds.y + bounds.height;
		if (inside && event.Type() == EventType::PointerMove) {
			Hover(widget);
		}

		widget->OnEvent(event);
		for (const auto &child : widget->Children()) {
			Visit(child, event);
		}
	}

	/*
	=============
	AnimationClip::AnimationClip

	Builds an animation clip with an identifier, duration, and applier callback.
	=============
	*/
	AnimationClip::AnimationClip(std::string id, float durationMs, std::function<void(float)> applier)
	: m_id(std::move(id)), m_duration(durationMs), m_elapsed(0.0f), m_apply(std::move(applier))
	{
	}

	/*
	=============
	AnimationClip::Tick

	Advances the clip by the provided delta and returns completion status.
	=============
	*/
	bool AnimationClip::Tick(float deltaMs)
	{
		m_elapsed += deltaMs;
		float alpha = m_duration > 0.0f ? std::clamp(m_elapsed / m_duration, 0.0f, 1.0f) : 1.0f;
		m_apply(alpha);
		return m_elapsed >= m_duration;
	}

	/*
	=============
	AnimationClip::Id

	Returns the clip identifier.
	=============
	*/
	const std::string &AnimationClip::Id() const
	{
		return m_id;
	}

	/*
	=============
	AnimationTimeline::AnimationTimeline

	Builds an empty animation timeline.
	=============
	*/
	AnimationTimeline::AnimationTimeline()
	{
	}

	/*
	=============
	AnimationTimeline::AddClip

	Registers a new animation clip with the timeline.
	=============
	*/
	void AnimationTimeline::AddClip(const AnimationClip &clip)
	{
		m_clips.push_back(clip);
	}

	/*
	=============
	AnimationTimeline::Advance

	Advances all clips and removes any that have finished.
	=============
	*/
	void AnimationTimeline::Advance(float deltaMs)
	{
		for (auto it = m_clips.begin(); it != m_clips.end();) {
			if (it->Tick(deltaMs)) {
				it = m_clips.erase(it);
			} else {
				++it;
			}
		}
	}

	/*
	=============
	ThemeContext::ThemeContext

	Builds a theme context with no palette entries.
	=============
	*/
	ThemeContext::ThemeContext()
	{
	}

/*
=============
ThemeContext::SetPalette

	Stores a palette for later lookups.
	=============
	*/
void ThemeContext::SetPalette(std::vector<uiPaletteEntry_t> palette)
{
	m_palette = std::move(palette);
}

/*
=============
ThemeContext::SetTypography

Stores a typography set for later lookups.
=============
*/
void ThemeContext::SetTypography(uiTypographySet_t typography)
{
	m_typography = std::move(typography);
}

	/*
	=============
	ThemeContext::Resolve

	Returns a palette entry for the requested role or a default value.
	=============
	*/
uiPaletteEntry_t ThemeContext::Resolve(uiColorRole_t role) const
{
if (role >= 0 && static_cast<size_t>(role) < m_palette.size()) {
return m_palette[static_cast<size_t>(role)];
}

		uiPaletteEntry_t fallback{};
		for (int i = 0; i < UI_STATE_COUNT; i++) {
fallback.states[i] = color_white;
}
return fallback;
}

/*
=============
ThemeContext::Typography

Returns the stored typography set.
=============
*/
const uiTypographySet_t &ThemeContext::Typography() const
{
	return m_typography;
}

	/*
	=============
	UIXSystem::UIXSystem

	Builds the unified UIX system with default subsystems.
	=============
	*/
	UIXSystem::UIXSystem()
	: m_layout(), m_graph(), m_interaction(), m_animation(), m_theme()
	{
	}

	/*
	=============
	UIXSystem::UpdateLayout

	Updates layout metrics inside the layout engine.
	=============
	*/
	void UIXSystem::UpdateLayout(const uiLayoutMetrics_t &metrics)
	{
		m_layout.UpdateMetrics(metrics);
	}

	/*
	=============
	UIXSystem::Layout

	Returns the layout engine instance.
	=============
	*/
	LayoutEngine &UIXSystem::Layout()
	{
		return m_layout;
	}

	/*
	=============
	UIXSystem::Graph

	Returns the scene graph.
	=============
	*/
	SceneGraph &UIXSystem::Graph()
	{
		return m_graph;
	}

	/*
	=============
	UIXSystem::Interaction

	Returns the interaction controller.
	=============
	*/
	InteractionController &UIXSystem::Interaction()
	{
		return m_interaction;
	}

	/*
	=============
	UIXSystem::Animations

	Returns the animation timeline.
	=============
	*/
	AnimationTimeline &UIXSystem::Animations()
	{
		return m_animation;
	}

	/*
	=============
	UIXSystem::Theme

	Returns the theme context used by the UI.
	=============
	*/
	ThemeContext &UIXSystem::Theme()
	{
		return m_theme;
	}

	/*
	=============
	GetSystem

	Returns the singleton UIX system, constructing it if necessary.
	=============
	*/
	UIXSystem &GetSystem()
	{
		if (!g_uixSystem) {
			g_uixSystem = std::make_unique<UIXSystem>();
		}

		return *g_uixSystem;
	}

}
