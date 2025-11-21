#include "MenuItem.h"
#include "ui.hpp"

#include <algorithm>
#include <utility>

/*
==============
MenuItem

Constructs a menu item with ownership-aware resources and optional children.
==============
*/
MenuItem::MenuItem(std::string name,
TextureHandle texture,
Callback onActivate,
std::vector<std::unique_ptr<MenuItem>> children)
: m_name(std::move(name)),
m_texture(std::move(texture)),
m_onActivate(std::move(onActivate)),
m_children(std::move(children))
{
}

/*
==============
~MenuItem

Virtual destructor to ensure derived classes release resources cleanly.
==============
*/
MenuItem::~MenuItem() = default;

/*
==============
GetName

Returns the immutable name assigned during construction.
==============
*/
const std::string &MenuItem::GetName() const
{
	return m_name;
}

/*
==============
GetTexture

Provides read-only access to the retained texture handle. The shared pointer
keeps the underlying texture alive for the lifetime of the menu item.
==============
*/
const MenuItem::TextureHandle &MenuItem::GetTexture() const
{
	return m_texture;
}

/*
==============
GetChildren

Exposes owned child items. Callers can traverse the returned vector without
altering ownership.
==============
*/
const std::vector<std::unique_ptr<MenuItem>> &MenuItem::GetChildren() const
{
	return m_children;
}

/*
==============
SetActivateCallback

Updates the activation callback. The std::function allows callers to provide
RAII-friendly functors that manage captured resources safely.
==============
*/
void MenuItem::SetActivateCallback(Callback callback)
{
	m_onActivate = std::move(callback);
}

/*
==============
AddChild

Appends a child to the ownership-tracking collection. Exclusive ownership keeps
the hierarchy well-defined while eliminating manual delete calls.
==============
*/
void MenuItem::AddChild(std::unique_ptr<MenuItem> child)
{
	m_children.emplace_back(std::move(child));
}

/*
==============
RemoveChild

Removes a matching child instance from the container while preserving the
ownership of unrelated entries.
==============
*/
void MenuItem::RemoveChild(const MenuItem *child)
{
	m_children.erase(
		std::remove_if(m_children.begin(), m_children.end(),
		[child](const std::unique_ptr<MenuItem> &candidate)
{
			return candidate.get() == child;
		}),
		m_children.end());
}

/*
==============
ForEachChild

Invokes a visitor for every child entry while maintaining unique ownership.
==============
*/
void MenuItem::ForEachChild(const std::function<void(const MenuItem &)> &visitor) const
{
	for (const auto &child : m_children)
{
		visitor(*child);
}
}

/*
==============
TriggerActivate

Invokes the activation callback when set. The callback owns its captured state
through std::function and follows normal RAII semantics.
==============
*/
void MenuItem::TriggerActivate()
{
	if (m_onActivate)
	{
		m_onActivate(*this);
	}
}

/*
=============
UI_ResolveHandle

Safely unwraps a shared_ptr-backed texture handle for draw routines.
=============
*/
static qhandle_t UI_ResolveHandle(const MenuItem::TextureHandle &handle)
{
	return handle ? *handle : 0;
}

/*
=============
ActionItem::ActionItem

Initializes an actionable menu entry modeled after the legacy menu Action
widget, retaining focus highlights and activation callbacks.
=============
*/
ActionItem::ActionItem(std::string name,
int x,
int y,
int uiFlags,
Callback onActivate,
TextureHandle texture,
bool disabled)
: MenuItem(std::move(name), std::move(texture), std::move(onActivate)),
m_x(x),
m_y(y),
m_uiFlags(uiFlags),
m_hasFocus(false),
m_disabled(disabled)
{
}

/*
=============
ActionItem::~ActionItem

Virtual destructor to satisfy the abstract interface.
=============
*/
ActionItem::~ActionItem() = default;

/*
=============
ActionItem::Draw

Renders the label with the same pulsing cursor behavior used by the procedural
Action_Draw helper.
=============
*/
void ActionItem::Draw() const
{
	int flags = m_uiFlags;
	color_t color = COLOR_WHITE;

	if (m_hasFocus)
	{
		if ((m_uiFlags & UI_CENTER) != UI_CENTER)
		{
			if ((uis.realtime >> 8) & 1)
			{
				UI_DrawChar(m_x - RCOLUMN_OFFSET / 2, m_y, m_uiFlags | UI_RIGHT, color, 13);
			}
		}
		else
		{
			flags |= UI_ALTCOLOR;
			if ((uis.realtime >> 8) & 1)
			{
				UI_DrawChar(m_x - static_cast<int>(GetName().length()) * CONCHAR_WIDTH / 2 - CONCHAR_WIDTH, m_y, flags, color, 13);
			}
		}
	}

	if (m_disabled)
	{
		color = uis.color.disabled;
	}

	UI_DrawString(m_x, m_y, flags, color, GetName().c_str());
}

/*
=============
ActionItem::HandleEvent

Triggers activation on ENTER or primary pointer clicks.
=============
*/
bool ActionItem::HandleEvent(const MenuEvent &event)
{
	if (m_disabled)
	{
		return false;
	}

	switch (event.type)
	{
		case MenuEvent::Type::Key:
		if (event.key == K_ENTER || event.key == K_MOUSE1 || event.key == K_KP_ENTER)
		{
			return Activate();
		}
		break;
		case MenuEvent::Type::Pointer:
		return Activate();
		default:
		break;
	}

	return false;
}

/*
=============
ActionItem::Activate

Runs the supplied callback when enabled.
=============
*/
bool ActionItem::Activate()
{
	if (m_disabled)
	{
		return false;
	}

	TriggerActivate();
	return true;
}

/*
=============
ActionItem::SetFocus

Stores focus state for the pulsing indicator.
=============
*/
void ActionItem::SetFocus(bool hasFocus)
{
	m_hasFocus = hasFocus;
}

/*
=============
ActionItem::HasFocus
=============
*/
bool ActionItem::HasFocus() const
{
	return m_hasFocus;
}

/*
=============
ActionItem::OnAttach
=============
*/
void ActionItem::OnAttach()
{
}

/*
=============
ActionItem::OnDetach
=============
*/
void ActionItem::OnDetach()
{
}

/*
=============
StaticItem::StaticItem

Constructs a static label with optional custom color.
=============
*/
StaticItem::StaticItem(std::string name,
int x,
int y,
int uiFlags,
TextureHandle texture,
color_t color)
: MenuItem(std::move(name), std::move(texture), nullptr),
m_x(x),
m_y(y),
m_uiFlags(uiFlags),
m_color(color)
{
}

/*
=============
StaticItem::~StaticItem
=============
*/
StaticItem::~StaticItem() = default;

/*
=============
StaticItem::Draw

Renders the text at the configured position without interaction.
=============
*/
void StaticItem::Draw() const
{
	UI_DrawString(m_x, m_y, m_uiFlags, m_color, GetName().c_str());
}

/*
=============
StaticItem::HandleEvent
=============
*/
bool StaticItem::HandleEvent(const MenuEvent &event)
{
	(void)event;
	return false;
}

/*
=============
StaticItem::Activate
=============
*/
bool StaticItem::Activate()
{
	return false;
}

/*
=============
StaticItem::SetFocus
=============
*/
void StaticItem::SetFocus(bool hasFocus)
{
	(void)hasFocus;
}

/*
=============
StaticItem::HasFocus
=============
*/
bool StaticItem::HasFocus() const
{
	return false;
}

/*
=============
StaticItem::OnAttach
=============
*/
void StaticItem::OnAttach()
{
}

/*
=============
StaticItem::OnDetach
=============
*/
void StaticItem::OnDetach()
{
}

/*
=============
BitmapItem::BitmapItem

Initializes a clickable bitmap with focus highlight parity to Bitmap_Draw.
=============
*/
BitmapItem::BitmapItem(std::string name,
int x,
int y,
int width,
int height,
TextureHandle defaultPic,
TextureHandle focusPic,
Callback onActivate)
: MenuItem(std::move(name), std::move(defaultPic), std::move(onActivate)),
m_x(x),
m_y(y),
m_width(width),
m_height(height),
m_focusPic(std::move(focusPic)),
m_hasFocus(false)
{
}

/*
=============
BitmapItem::~BitmapItem
=============
*/
BitmapItem::~BitmapItem() = default;

/*
=============
BitmapItem::Draw

Renders the default or focused bitmap along with the cursor overlay when
focused.
=============
*/
void BitmapItem::Draw() const
{
	const color_t color = COLOR_WHITE;
	const qhandle_t basePic = UI_ResolveHandle(GetTexture());
	const qhandle_t focusPic = UI_ResolveHandle(m_focusPic);

	if (m_hasFocus && uis.bitmapCursors[0])
	{
		const unsigned frame = (uis.realtime / 100) % NUM_CURSOR_FRAMES;
		R_DrawPic(m_x - CURSOR_OFFSET, m_y, color, uis.bitmapCursors[frame]);
	}

	if (m_hasFocus && focusPic)
	{
		R_DrawPic(m_x, m_y, color, focusPic);
	}
	else if (basePic)
	{
		R_DrawPic(m_x, m_y, color, basePic);
	}
}

/*
=============
BitmapItem::HandleEvent

Activates on click or confirm keys when focused.
=============
*/
bool BitmapItem::HandleEvent(const MenuEvent &event)
{
	switch (event.type)
	{
		case MenuEvent::Type::Key:
		if (event.key == K_ENTER || event.key == K_MOUSE1 || event.key == K_KP_ENTER)
		{
			return Activate();
		}
		break;
		case MenuEvent::Type::Pointer:
		if (event.x >= m_x && event.x <= m_x + m_width && event.y >= m_y && event.y <= m_y + m_height)
		{
			return Activate();
		}
		break;
		default:
		break;
	}

	return false;
}

/*
=============
BitmapItem::Activate
=============
*/
bool BitmapItem::Activate()
{
	TriggerActivate();
	return true;
}

/*
=============
BitmapItem::SetFocus
=============
*/
void BitmapItem::SetFocus(bool hasFocus)
{
	m_hasFocus = hasFocus;
}

/*
=============
BitmapItem::HasFocus
=============
*/
bool BitmapItem::HasFocus() const
{
	return m_hasFocus;
}

/*
=============
BitmapItem::OnAttach
=============
*/
void BitmapItem::OnAttach()
{
}

/*
=============
BitmapItem::OnDetach
=============
*/
void BitmapItem::OnDetach()
{
}

/*
=============
FieldItem::FieldItem

Wraps an input field with the IF_* helpers to mirror legacy text entry.
=============
*/
FieldItem::FieldItem(std::string name,
int x,
int y,
int uiFlags,
std::string initial,
size_t visibleChars,
size_t maxChars,
Callback onChange,
TextureHandle texture)
: MenuItem(std::move(name), std::move(texture), nullptr),
m_x(x),
m_y(y),
m_uiFlags(uiFlags),
m_onChange(std::move(onChange)),
m_hasFocus(false)
{
	IF_Init(&m_field, visibleChars, maxChars);
	IF_Replace(&m_field, initial.c_str());
}

/*
=============
FieldItem::~FieldItem
=============
*/
FieldItem::~FieldItem() = default;

/*
=============
FieldItem::GetField
=============
*/
const inputField_t &FieldItem::GetField() const
{
	return m_field;
}

/*
=============
FieldItem::GetValue
=============
*/
std::string FieldItem::GetValue() const
{
	return m_field.text;
}

/*
=============
FieldItem::Draw

Renders the label and editable input field with blinking cursor when focused.
=============
*/
void FieldItem::Draw() const
{
	UI_DrawString(m_x + LCOLUMN_OFFSET, m_y, m_uiFlags | UI_RIGHT | UI_ALTCOLOR, COLOR_WHITE, GetName().c_str());
	IF_Draw(&m_field, m_x + RCOLUMN_OFFSET, m_y, m_uiFlags | (m_hasFocus ? UI_BLINK : 0), uis.fontHandle);
}

/*
=============
FieldItem::HandleEvent

Relays key and character input to the underlying input field and invokes the
change callback when the buffer is modified.
=============
*/
bool FieldItem::HandleEvent(const MenuEvent &event)
{
	bool handled = false;

	if (event.type == MenuEvent::Type::Key)
	{
		if (event.key >= K_ASCIIFIRST && event.key <= K_ASCIILAST)
		{
			handled = IF_CharEvent(&m_field, event.key);
		}
		else
		{
			handled = IF_KeyEvent(&m_field, event.key);
		}
	}

	if (handled && m_onChange)
	{
		m_onChange(*this);
	}

	return handled;
}

/*
=============
FieldItem::Activate

Activation simply surfaces the current value through the change callback.
=============
*/
bool FieldItem::Activate()
{
	if (m_onChange)
	{
		m_onChange(*this);
		return true;
	}

	return false;
}

/*
=============
FieldItem::SetFocus
=============
*/
void FieldItem::SetFocus(bool hasFocus)
{
	m_hasFocus = hasFocus;
}

/*
=============
FieldItem::HasFocus
=============
*/
bool FieldItem::HasFocus() const
{
	return m_hasFocus;
}

/*
=============
FieldItem::OnAttach
=============
*/
void FieldItem::OnAttach()
{
}

/*
=============
FieldItem::OnDetach
=============
*/
void FieldItem::OnDetach()
{
}

/*
=============
ListItem::ListItem

Builds a simple multi-row list mirroring MenuList row selection and highlighting.
=============
*/
ListItem::ListItem(std::string name,
int x,
int y,
int width,
int uiFlags,
Rows rows,
SelectionCallback onSelect,
TextureHandle texture)
: MenuItem(std::move(name), std::move(texture), nullptr),
m_x(x),
m_y(y),
m_width(width),
m_uiFlags(uiFlags),
m_rows(std::move(rows)),
m_onSelect(std::move(onSelect)),
m_selectedRow(0),
m_hasFocus(false)
{
}

/*
=============
ListItem::~ListItem
=============
*/
ListItem::~ListItem() = default;

/*
=============
ListItem::GetSelection
=============
*/
int ListItem::GetSelection() const
{
	return m_selectedRow;
}

/*
=============
ListItem::SetSelection

Adjusts the current selection, clamping to valid rows.
=============
*/
void ListItem::SetSelection(int index)
{
	if (m_rows.empty())
	{
		m_selectedRow = 0;
		return;
	}

	if (index < 0)
	{
		index = 0;
	}
	else if (index >= static_cast<int>(m_rows.size()))
	{
		index = static_cast<int>(m_rows.size()) - 1;
	}

	m_selectedRow = index;
}

/*
=============
ListItem::GetRows
=============
*/
const ListItem::Rows &ListItem::GetRows() const
{
	return m_rows;
}

/*
=============
ListItem::Draw

Displays each row with a focus highlight akin to MenuList_Draw.
=============
*/
void ListItem::Draw() const
{
	int yCursor = m_y;
	const int flags = m_uiFlags & ~(UI_LEFT | UI_RIGHT);

	for (size_t rowIndex = 0; rowIndex < m_rows.size(); rowIndex++)
	{
		const bool selected = static_cast<int>(rowIndex) == m_selectedRow;
		color_t color = selected ? uis.color.textHighlight : COLOR_WHITE;
		int xCursor = m_x;

		for (const auto &col : m_rows[rowIndex])
		{
			UI_DrawString(xCursor, yCursor, flags | UI_LEFT | (selected ? UI_ALTCOLOR : 0), color, col.c_str());
			xCursor += m_width / std::max<int>(1, static_cast<int>(m_rows[rowIndex].size()));
		}

		if (selected && m_hasFocus)
		{
			UI_DrawChar(m_x - CURSOR_OFFSET / 2, yCursor, flags | UI_RIGHT, color, 13);
		}

		yCursor += MLIST_SPACING;
	}
}

/*
=============
ListItem::HandleEvent

Supports arrow-key navigation and pointer activation similar to MenuList_Key and
MenuList_MouseMove.
=============
*/
bool ListItem::HandleEvent(const MenuEvent &event)
{
	if (m_rows.empty())
	{
		return false;
	}

	switch (event.type)
	{
		case MenuEvent::Type::Key:
		if (event.key == K_UPARROW || event.key == K_KP_UPARROW)
		{
			SetSelection(m_selectedRow - 1);
			return true;
		}
		if (event.key == K_DOWNARROW || event.key == K_KP_DOWNARROW)
		{
			SetSelection(m_selectedRow + 1);
			return true;
		}
		if (event.key == K_ENTER || event.key == K_MOUSE1 || event.key == K_KP_ENTER)
		{
			return Activate();
		}
		break;
		case MenuEvent::Type::Pointer:
		{
			const int relativeRow = (event.y - m_y) / MLIST_SPACING;
			if (relativeRow >= 0 && relativeRow < static_cast<int>(m_rows.size()))
			{
				SetSelection(relativeRow);
				return Activate();
			}
			break;
		}
		default:
		break;
	}

	return false;
}

/*
=============
ListItem::Activate

Invokes the selection callback for the current row.
=============
*/
bool ListItem::Activate()
{
	if (m_onSelect)
	{
		m_onSelect(*this, m_selectedRow);
		return true;
	}

	return false;
}

/*
=============
ListItem::SetFocus
=============
*/
void ListItem::SetFocus(bool hasFocus)
{
	m_hasFocus = hasFocus;
}

/*
=============
ListItem::HasFocus
=============
*/
bool ListItem::HasFocus() const
{
	return m_hasFocus;
}

/*
=============
ListItem::OnAttach
=============
*/
void ListItem::OnAttach()
{
}

/*
=============
ListItem::OnDetach
=============
*/
void ListItem::OnDetach()
{
}

/*
=============
SliderItem::SliderItem

Creates a slider mirroring Slider_Draw visuals and keyboard interactions.
=============
*/
SliderItem::SliderItem(std::string name,
int x,
int y,
int uiFlags,
float minValue,
float maxValue,
float step,
float currentValue,
Callback onChange,
TextureHandle texture)
: MenuItem(std::move(name), std::move(texture), nullptr),
m_x(x),
m_y(y),
m_uiFlags(uiFlags),
m_minValue(minValue),
m_maxValue(maxValue),
m_step(step),
m_value(currentValue),
m_onChange(std::move(onChange)),
m_hasFocus(false)
{
}

/*
=============
SliderItem::~SliderItem
=============
*/
SliderItem::~SliderItem() = default;

/*
=============
SliderItem::GetValue
=============
*/
float SliderItem::GetValue() const
{
	return m_value;
}

/*
=============
SliderItem::Draw

Renders a text label and slider bar, using the same glyph layout as Slider_Draw.
=============
*/
void SliderItem::Draw() const
{
	int flags = m_uiFlags & ~(UI_LEFT | UI_RIGHT);
	color_t color = COLOR_WHITE;

	if (m_hasFocus)
	{
		if ((uis.realtime >> 8) & 1)
		{
			UI_DrawChar(m_x + RCOLUMN_OFFSET / 2, m_y, m_uiFlags | UI_RIGHT, color, 13);
		}
	}

	UI_DrawString(m_x + LCOLUMN_OFFSET, m_y, flags | UI_RIGHT | UI_ALTCOLOR, color, GetName().c_str());
	UI_DrawChar(m_x + RCOLUMN_OFFSET, m_y, flags | UI_LEFT, color, 128);

	for (int i = 0; i < SLIDER_RANGE; i++)
	{
		UI_DrawChar(RCOLUMN_OFFSET + m_x + i * CONCHAR_WIDTH + CONCHAR_WIDTH, m_y, flags | UI_LEFT, color, 129);
	}

	UI_DrawChar(RCOLUMN_OFFSET + m_x + SLIDER_RANGE * CONCHAR_WIDTH + CONCHAR_WIDTH, m_y, flags | UI_LEFT, color, 130);

	const float pos = Q_clipf((m_value - m_minValue) / (m_maxValue - m_minValue), 0.0f, 1.0f);
	UI_DrawChar(CONCHAR_WIDTH + RCOLUMN_OFFSET + m_x + (SLIDER_RANGE - 1) * CONCHAR_WIDTH * pos, m_y, flags | UI_LEFT, color, 131);
}

/*
=============
SliderItem::HandleEvent

Responds to keyboard and controller movement to adjust the slider.
=============
*/
bool SliderItem::HandleEvent(const MenuEvent &event)
{
	if (event.type == MenuEvent::Type::Key)
	{
		if (event.key == K_LEFTARROW || event.key == K_KP_LEFTARROW)
		{
			NudgeValue(-1);
			return true;
		}
		if (event.key == K_RIGHTARROW || event.key == K_KP_RIGHTARROW)
		{
			NudgeValue(1);
			return true;
		}
	}

	return false;
}

/*
=============
SliderItem::Activate
=============
*/
bool SliderItem::Activate()
{
	return false;
}

/*
=============
SliderItem::SetFocus
=============
*/
void SliderItem::SetFocus(bool hasFocus)
{
	m_hasFocus = hasFocus;
}

/*
=============
SliderItem::HasFocus
=============
*/
bool SliderItem::HasFocus() const
{
	return m_hasFocus;
}

/*
=============
SliderItem::OnAttach
=============
*/
void SliderItem::OnAttach()
{
}

/*
=============
SliderItem::OnDetach
=============
*/
void SliderItem::OnDetach()
{
}

/*
=============
SliderItem::NudgeValue

Steps the slider and fires the change callback.
=============
*/
void SliderItem::NudgeValue(int direction)
{
	m_value += m_step * static_cast<float>(direction);
	m_value = Q_clipf(m_value, m_minValue, m_maxValue);

	if (m_onChange)
	{
		m_onChange(*this);
	}
}
