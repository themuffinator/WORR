#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "common/field.hpp"
#include "shared/types.hpp"

class ActionItem;
class StaticItem;
class BitmapItem;
class FieldItem;
class ListItem;
class SliderItem;

	/*
	==============================================================================
	MenuItem

	Interface describing the ownership and lifecycle expectations for UI menu
	items. Texture handles are shared to keep image resources alive across
	references, while children are owned uniquely to eliminate manual lifetime
	management. Callbacks are supplied via std::function so callers can pass
	lambdas or other RAII-friendly callables without leaking state.
	==============================================================================
	*/
class MenuItem {
public:
using TextureHandle = std::shared_ptr<qhandle_t>;
using Callback = std::function<void(MenuItem &)>;

	struct MenuEvent {
	enum class Type {
	Key,
	Pointer,
	Controller
};

Type type{Type::Key};
int key{0};
int x{0};
int y{0};
};

MenuItem(std::string name,
TextureHandle texture,
Callback onActivate,
std::vector<std::unique_ptr<MenuItem>> children = {});

MenuItem(const MenuItem &) = delete;
MenuItem(MenuItem &&) = delete;
MenuItem &operator=(const MenuItem &) = delete;
MenuItem &operator=(MenuItem &&) = delete;

virtual ~MenuItem();

const std::string &GetName() const;
const TextureHandle &GetTexture() const;
const std::vector<std::unique_ptr<MenuItem>> &GetChildren() const;

void SetActivateCallback(Callback callback);

protected:
void AddChild(std::unique_ptr<MenuItem> child);
void RemoveChild(const MenuItem *child);
void ForEachChild(const std::function<void(const MenuItem &)> &visitor) const;
void TriggerActivate();

virtual void Draw() const = 0;
virtual bool HandleEvent(const MenuEvent &event) = 0;
virtual bool Activate() = 0;
virtual void SetFocus(bool hasFocus) = 0;
virtual bool HasFocus() const = 0;
virtual void OnAttach() = 0;
virtual void OnDetach() = 0;

private:
std::string m_name;
TextureHandle m_texture;
Callback m_onActivate;
std::vector<std::unique_ptr<MenuItem>> m_children;
};

class ActionItem : public MenuItem {
	public:
	ActionItem(std::string name,
	int x,
	int y,
	int uiFlags,
	Callback onActivate,
	TextureHandle texture = nullptr,
	bool disabled = false);

	~ActionItem() override;

	protected:
	void Draw() const override;
	bool HandleEvent(const MenuEvent &event) override;
	bool Activate() override;
	void SetFocus(bool hasFocus) override;
	bool HasFocus() const override;
	void OnAttach() override;
	void OnDetach() override;

	private:
	int m_x;
	int m_y;
	int m_uiFlags;
	bool m_hasFocus;
	bool m_disabled;
};

class StaticItem : public MenuItem {
	public:
	StaticItem(std::string name,
	int x,
	int y,
	int uiFlags,
	TextureHandle texture = nullptr,
	color_t color = COLOR_WHITE);

	~StaticItem() override;

	protected:
	void Draw() const override;
	bool HandleEvent(const MenuEvent &event) override;
	bool Activate() override;
	void SetFocus(bool hasFocus) override;
	bool HasFocus() const override;
	void OnAttach() override;
	void OnDetach() override;

	private:
	int m_x;
	int m_y;
	int m_uiFlags;
	color_t m_color;
};

class BitmapItem : public MenuItem {
	public:
	BitmapItem(std::string name,
	int x,
	int y,
	int width,
	int height,
	TextureHandle defaultPic,
	TextureHandle focusPic,
	Callback onActivate);

	~BitmapItem() override;

	protected:
	void Draw() const override;
	bool HandleEvent(const MenuEvent &event) override;
	bool Activate() override;
	void SetFocus(bool hasFocus) override;
	bool HasFocus() const override;
	void OnAttach() override;
	void OnDetach() override;

	private:
	int m_x;
	int m_y;
	int m_width;
	int m_height;
	TextureHandle m_focusPic;
	bool m_hasFocus;
};

class FieldItem : public MenuItem {
	public:
	FieldItem(std::string name,
	int x,
	int y,
	int uiFlags,
	std::string initial,
	size_t visibleChars,
	size_t maxChars,
	Callback onChange,
	TextureHandle texture = nullptr);

	~FieldItem() override;

const inputField_t &GetField() const;
std::string GetValue() const;
void SetValue(const std::string &value);

	protected:
	void Draw() const override;
	bool HandleEvent(const MenuEvent &event) override;
	bool Activate() override;
	void SetFocus(bool hasFocus) override;
	bool HasFocus() const override;
	void OnAttach() override;
	void OnDetach() override;

	private:
	int m_x;
	int m_y;
	int m_uiFlags;
	inputField_t m_field;
	Callback m_onChange;
	bool m_hasFocus;
};

class ListItem : public MenuItem {
	public:
	using Row = std::vector<std::string>;
	using Rows = std::vector<Row>;
	using SelectionCallback = std::function<void(ListItem &, int)>;

	ListItem(std::string name,
	int x,
	int y,
	int width,
	int uiFlags,
	Rows rows,
	SelectionCallback onSelect,
	TextureHandle texture = nullptr);

	~ListItem() override;

	int GetSelection() const;
	void SetSelection(int index);
	const Rows &GetRows() const;

	protected:
	void Draw() const override;
	bool HandleEvent(const MenuEvent &event) override;
	bool Activate() override;
	void SetFocus(bool hasFocus) override;
	bool HasFocus() const override;
	void OnAttach() override;
	void OnDetach() override;

	private:
	int m_x;
	int m_y;
	int m_width;
	int m_uiFlags;
	Rows m_rows;
	SelectionCallback m_onSelect;
	int m_selectedRow;
	bool m_hasFocus;
};

class SliderItem : public MenuItem {
	public:
	SliderItem(std::string name,
	int x,
	int y,
	int uiFlags,
	float minValue,
	float maxValue,
	float step,
	float currentValue,
	Callback onChange,
	TextureHandle texture = nullptr);

	~SliderItem() override;

float GetValue() const;
void SetValue(float value);

void SetStep(float step);
void SetRange(float minValue, float maxValue);

	protected:
	void Draw() const override;
	bool HandleEvent(const MenuEvent &event) override;
	bool Activate() override;
	void SetFocus(bool hasFocus) override;
	bool HasFocus() const override;
	void OnAttach() override;
	void OnDetach() override;

	private:
	void NudgeValue(int direction);

	int m_x;
	int m_y;
	int m_uiFlags;
	float m_minValue;
	float m_maxValue;
	float m_step;
	float m_value;
	Callback m_onChange;
	bool m_hasFocus;
};
