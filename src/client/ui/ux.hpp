#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ui.hpp"

namespace ui::ux {
	enum class LayoutUnit : uint8_t {
		Pixels,
		Percent
	};

	enum class LayoutFlow : uint8_t {
		Row,
		Column
	};

	enum class WidgetType : uint8_t {
		Container,
		Button,
		List,
		Slider,
		ModalOverlay
	};

		enum class EventType : uint8_t {
			Focus,
			Hover,
			Activate,
			Navigate,
			PointerMove,
			PointerDown,
			PointerUp,
			Scroll,
			TextInput
		};

	enum class NavigationDirection : uint8_t {
		Next,
		Previous,
		Up,
		Down,
		Left,
		Right
	};

	class LayoutValue {
		public:
		LayoutValue();
		LayoutValue(float scalar, LayoutUnit unit);

		static LayoutValue Percent(float percent);
		static LayoutValue Pixels(float pixels);

		float Value() const;
		LayoutUnit Unit() const;

		private:
		float m_value;
		LayoutUnit m_unit;
	};

	class LayoutRect {
		public:
		LayoutRect();
		LayoutRect(LayoutValue x, LayoutValue y, LayoutValue width, LayoutValue height, int padding = 0, int spacing = 0);

		LayoutValue X() const;
		LayoutValue Y() const;
		LayoutValue Width() const;
		LayoutValue Height() const;
		int Padding() const;
		int Spacing() const;

		private:
		LayoutValue m_x;
		LayoutValue m_y;
		LayoutValue m_width;
		LayoutValue m_height;
		int m_padding;
		int m_spacing;
	};

	class FlexConstraint {
		public:
		FlexConstraint();
		FlexConstraint(float grow, float shrink, LayoutValue basis);

		float Grow() const;
		float Shrink() const;
		LayoutValue Basis() const;

		private:
		float m_grow;
		float m_shrink;
		LayoutValue m_basis;
	};

	class LayoutEngine {
		public:
		LayoutEngine();
		explicit LayoutEngine(const uiLayoutMetrics_t &metrics);

		void UpdateMetrics(const uiLayoutMetrics_t &metrics);
		vrect_t ResolveRect(const LayoutRect &rect, const vrect_t &parent) const;
		std::vector<vrect_t> ApplyFlex(const std::span<const LayoutRect> children, const vrect_t &parent, LayoutFlow direction, const std::span<const FlexConstraint> constraints) const;
		std::vector<vrect_t> ApplyGrid(const std::span<const LayoutRect> children, const vrect_t &parent, int columns, int rowSpacing, int columnSpacing) const;

		private:
		int ResolveValue(const LayoutValue &value, int reference) const;
		uiLayoutMetrics_t m_metrics;
	};

	class UIEvent {
		public:
			UIEvent(EventType type, NavigationDirection direction, vrect_t region);
			UIEvent(EventType type, vrect_t region);
			UIEvent(EventType type, int key, vrect_t region);

			EventType Type() const;
			NavigationDirection Direction() const;
			vrect_t Region() const;
			int Key() const;

			private:
			EventType m_type;
			NavigationDirection m_direction;
			vrect_t m_region;
			int m_key;
	};

	class Widget : public std::enable_shared_from_this<Widget> {
		public:
		explicit Widget(std::string identifier, WidgetType type);
		virtual ~Widget() = default;

		virtual void OnLayout(LayoutEngine &engine, const vrect_t &parentBounds);
		virtual void OnEvent(const UIEvent &event);

		void AddChild(const std::shared_ptr<Widget> &child);
		void SetLayout(const LayoutRect &layout);
		const LayoutRect &GetLayout() const;
		const vrect_t &ResolvedBounds() const;
		WidgetType Type() const;
		const std::string &Id() const;
		void SetState(uiControlState_t state);
		uiControlState_t State() const;
		void SetColorRole(uiColorRole_t role);
		uiColorRole_t ColorRole() const;
		std::vector<std::shared_ptr<Widget>> &Children();

		protected:
		LayoutRect m_layout;
		vrect_t m_bounds;
		std::vector<std::shared_ptr<Widget>> m_children;
		WidgetType m_type;
		std::string m_identifier;
		uiControlState_t m_state;
		uiColorRole_t m_colorRole;
	};

	class ButtonWidget : public Widget {
		public:
		explicit ButtonWidget(const std::string &identifier);

		void SetLabel(std::string label);
		const std::string &Label() const;
		void OnEvent(const UIEvent &event) override;

		private:
		std::string m_label;
	};

	class ListWidget : public Widget {
		public:
		explicit ListWidget(const std::string &identifier);

		void SetItems(std::vector<std::string> items);
		const std::vector<std::string> &Items() const;
		void OnEvent(const UIEvent &event) override;

		private:
		std::vector<std::string> m_items;
		int m_selection;
	};

	class SliderWidget : public Widget {
		public:
		explicit SliderWidget(const std::string &identifier);

		void SetRange(float minValue, float maxValue);
		void SetValue(float value);
		float Value() const;
		void OnEvent(const UIEvent &event) override;

		private:
		float m_min;
		float m_max;
		float m_value;
	};

	class ModalOverlayWidget : public Widget {
		public:
		explicit ModalOverlayWidget(const std::string &identifier);

		void SetBlocking(bool blocking);
		bool IsBlocking() const;
		void OnEvent(const UIEvent &event) override;

		private:
		bool m_blocking;
	};

	class SceneLayer {
		public:
		SceneLayer(std::string name, int zIndex);

		void SetRoot(const std::shared_ptr<Widget> &root);
		std::shared_ptr<Widget> Root() const;
		const std::string &Name() const;
		int ZIndex() const;

		private:
		std::string m_name;
		int m_zIndex;
		std::shared_ptr<Widget> m_root;
	};

	class SceneGraph {
		public:
		void AddLayer(const std::shared_ptr<SceneLayer> &layer);
		void RemoveLayer(const std::string &name);
		void Clear();
		std::shared_ptr<SceneLayer> FindLayer(const std::string &name) const;
		std::vector<std::shared_ptr<SceneLayer>> OrderedLayers() const;

		private:
		std::vector<std::shared_ptr<SceneLayer>> m_layers;
	};

	class InteractionController {
		public:
		InteractionController();

		void RouteEvent(const UIEvent &event, const std::shared_ptr<Widget> &root);
		void Focus(const std::shared_ptr<Widget> &widget);
		void Hover(const std::shared_ptr<Widget> &widget);
		std::weak_ptr<Widget> Focused() const;
		std::weak_ptr<Widget> Hovered() const;

		private:
		void Visit(const std::shared_ptr<Widget> &widget, const UIEvent &event);
		std::weak_ptr<Widget> m_focus;
		std::weak_ptr<Widget> m_hover;
	};

	class AnimationClip {
		public:
		AnimationClip(std::string id, float durationMs, std::function<void(float)> applier);

		bool Tick(float deltaMs);
		const std::string &Id() const;

		private:
		std::string m_id;
		float m_duration;
		float m_elapsed;
		std::function<void(float)> m_apply;
	};

	class AnimationTimeline {
		public:
		AnimationTimeline();

		void AddClip(const AnimationClip &clip);
		void Advance(float deltaMs);

		private:
		std::vector<AnimationClip> m_clips;
	};

class ThemeContext {
public:
ThemeContext();

void SetPalette(std::vector<uiPaletteEntry_t> palette);
void SetTypography(uiTypographySet_t typography);
uiPaletteEntry_t Resolve(uiColorRole_t role) const;
const uiTypographySet_t &Typography() const;

private:
std::vector<uiPaletteEntry_t> m_palette;
uiTypographySet_t m_typography;
};

	class UIXSystem {
		public:
		UIXSystem();

		void UpdateLayout(const uiLayoutMetrics_t &metrics);
		LayoutEngine &Layout();
		SceneGraph &Graph();
		InteractionController &Interaction();
		AnimationTimeline &Animations();
		ThemeContext &Theme();

		private:
		LayoutEngine m_layout;
		SceneGraph m_graph;
		InteractionController m_interaction;
		AnimationTimeline m_animation;
		ThemeContext m_theme;
	};

	UIXSystem &GetSystem();
}
