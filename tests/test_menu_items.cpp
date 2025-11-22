#define cursor cursorPos
#define textHighlight selection

#include "src/client/ui/MenuItem.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

uiStatic_t uis{};
list_t ui_menus{};
cvar_t *ui_debug = nullptr;

static int g_drawStringCount = 0;
static int g_drawCharCount = 0;
static int g_drawPicCount = 0;
static int g_fieldDrawCount = 0;

/*
=============
UI_DrawString
=============
*/
void UI_DrawString(int x, int y, int flags, color_t color, const char *string)
{
	(void)x;
	(void)y;
	(void)flags;
	(void)color;
	(void)string;
	g_drawStringCount++;
}

/*
=============
UI_DrawChar
=============
*/
void UI_DrawChar(int x, int y, int flags, color_t color, int ch)
{
	(void)x;
	(void)y;
	(void)flags;
	(void)color;
	(void)ch;
	g_drawCharCount++;
}

/*
=============
R_DrawPic
=============
*/
void R_DrawPic(int x, int y, color_t color, qhandle_t h)
{
	(void)x;
	(void)y;
	(void)color;
	(void)h;
	g_drawPicCount++;
}

/*
=============
IF_Init
=============
*/
void IF_Init(inputField_t *field, size_t visibleChars, size_t maxChars)
{
	std::memset(field, 0, sizeof(*field));
	field->maxChars = std::min(maxChars, sizeof(field->text) - 1);
	field->visibleChars = std::min(visibleChars, field->maxChars);
}

/*
=============
IF_Clear
=============
*/
void IF_Clear(inputField_t *field)
{
	std::memset(field->text, 0, sizeof(field->text));
	field->cursor = 0;
}

/*
=============
IF_Replace
=============
*/
void IF_Replace(inputField_t *field, const char *text)
{
	if (!text)
	{
		IF_Clear(field);
		return;
	}

	std::strncpy(field->text, text, field->maxChars);
	field->text[field->maxChars] = '\0';
	field->cursor = std::min(strlen(field->text), field->maxChars);
}

/*
=============
IF_KeyEvent
=============
*/
bool IF_KeyEvent(inputField_t *field, int key)
{
	const size_t len = std::strlen(field->text);

	if (key == K_LEFTARROW && field->cursor > 0)
	{
		field->cursor--;
		return true;
	}

	if (key == K_RIGHTARROW && field->cursor < len)
	{
		field->cursor++;
		return true;
	}

	if (key == K_BACKSPACE && field->cursor > 0)
	{
		std::memmove(field->text + field->cursor - 1, field->text + field->cursor, len - field->cursor + 1);
		field->cursor--;
		return true;
	}

	return false;
}

/*
=============
IF_CharEvent
=============
*/
bool IF_CharEvent(inputField_t *field, int key)
{
	if (key < K_ASCIIFIRST || key > K_ASCIILAST)
	{
		return false;
	}

	const char ch = static_cast<char>(key);
	const size_t len = std::strlen(field->text);

	if (len >= field->maxChars)
	{
		return false;
	}

	std::memmove(field->text + field->cursor + 1, field->text + field->cursor, len - field->cursor + 1);
	field->text[field->cursor] = ch;
	field->cursor++;
	return true;
}

/*
=============
IF_Draw
=============
*/
int IF_Draw(const inputField_t *field, int x, int y, int flags, qhandle_t font)
{
	(void)field;
	(void)x;
	(void)y;
	(void)flags;
	(void)font;
	g_fieldDrawCount++;
	return 0;
}

/*
=============
ResetDrawCounters
=============
*/
static void ResetDrawCounters()
{
	g_drawStringCount = 0;
	g_drawCharCount = 0;
	g_drawPicCount = 0;
	g_fieldDrawCount = 0;
}

/*
=============
TestActionAndStaticItems
=============
*/
static bool TestActionAndStaticItems()
{
	ResetDrawCounters();

	auto texture = std::make_shared<qhandle_t>(7);
	std::weak_ptr<qhandle_t> textureRef = texture;

	int activationCount = 0;
	{
		ActionItem action("play", 10, 20, UI_LEFT, [&activationCount](MenuItem &)
		{
			activationCount++;
		}, texture);

		action.SetFocus(true);
		action.Draw();

		MenuItem::MenuEvent event{};
		event.type = MenuItem::MenuEvent::Type::Key;
		event.key = K_ENTER;

		if (!action.HandleEvent(event) || activationCount != 1)
		{
			return false;
		}
	}

	if (textureRef.use_count() != 1 || textureRef.expired())
	{
		return false;
	}

	StaticItem label("static", 0, 0, UI_LEFT);
	label.Draw();

	return g_drawStringCount > 0;
}

/*
=============
TestBitmapItem
=============
*/
static bool TestBitmapItem()
{
	ResetDrawCounters();

	auto base = std::make_shared<qhandle_t>(1);
	auto focus = std::make_shared<qhandle_t>(2);
	std::weak_ptr<qhandle_t> baseRef = base;

	int activated = 0;
	{
		BitmapItem bitmap("btn", 5, 5, 32, 32, base, focus, [&activated](MenuItem &)
		{
			activated++;
		});

		bitmap.SetFocus(true);
		bitmap.Draw();

		MenuItem::MenuEvent pointer{};
		pointer.type = MenuItem::MenuEvent::Type::Pointer;
		pointer.x = 10;
		pointer.y = 10;

		if (!bitmap.HandleEvent(pointer) || activated != 1)
		{
			return false;
		}
	}

	return baseRef.use_count() == 1 && !baseRef.expired() && g_drawPicCount > 0;
}

/*
=============
TestFieldItem
=============
*/
static bool TestFieldItem()
{
	ResetDrawCounters();

	std::vector<std::string> changes;
	FieldItem field("name", 0, 0, UI_LEFT, "hi", 8, 16, [&changes](MenuItem &item)
	{
		changes.push_back(static_cast<FieldItem &>(item).GetValue());
	});

	field.SetFocus(true);
	field.Draw();

	MenuItem::MenuEvent letter{};
	letter.type = MenuItem::MenuEvent::Type::Key;
	letter.key = 'o';
	field.HandleEvent(letter);

	MenuItem::MenuEvent backspace{};
	backspace.type = MenuItem::MenuEvent::Type::Key;
	backspace.key = K_BACKSPACE;
	field.HandleEvent(backspace);

	field.SetValue("abc");
	field.Activate();

	return g_fieldDrawCount > 0 && !changes.empty() && changes.back() == "abc" && field.GetValue() == "abc";
}

/*
=============
TestListItem
=============
*/
static bool TestListItem()
{
	ResetDrawCounters();

	ListItem::Rows rows{{"one", "two"}, {"three", "four"}};
	int lastSelection = -1;
	ListItem list("menu", 0, 0, 200, UI_LEFT, rows, [&lastSelection](ListItem &item, int selected)
	{
		lastSelection = selected;
		item.SetSelection(selected);
	});

	list.SetFocus(true);
	list.Draw();

	MenuItem::MenuEvent down{};
	down.type = MenuItem::MenuEvent::Type::Key;
	down.key = K_DOWNARROW;
	list.HandleEvent(down);

	MenuItem::MenuEvent click{};
	click.type = MenuItem::MenuEvent::Type::Pointer;
	click.y = MLIST_SPACING;
	list.HandleEvent(click);

	return g_drawStringCount > 0 && g_drawCharCount > 0 && lastSelection == 1 && list.GetSelection() == 1;
}

/*
=============
TestSliderItem
=============
*/
static bool TestSliderItem()
{
	ResetDrawCounters();

	std::vector<float> values;
	SliderItem slider("volume", 0, 0, UI_LEFT, 0.0f, 10.0f, 1.0f, 5.0f, [&values](MenuItem &item)
	{
		values.push_back(static_cast<SliderItem &>(item).GetValue());
	});

	slider.SetFocus(true);
	slider.Draw();

	MenuItem::MenuEvent left{};
	left.type = MenuItem::MenuEvent::Type::Key;
	left.key = K_LEFTARROW;
	slider.HandleEvent(left);

	MenuItem::MenuEvent right{};
	right.type = MenuItem::MenuEvent::Type::Key;
	right.key = K_RIGHTARROW;
	slider.HandleEvent(right);
	slider.HandleEvent(right);

	const float value = slider.GetValue();
	const bool adjusted = std::find(values.begin(), values.end(), value) != values.end();

	return g_drawCharCount > 0 && adjusted && value <= 10.0f && value >= 0.0f;
}

/*
=============
main
=============
*/
int main()
{
	uis.color.background.u32 = COLOR_WHITE.u32;
	uis.color.normal.u32 = COLOR_WHITE.u32;
	uis.color.active.u32 = COLOR_WHITE.u32;
	uis.color.selection.u32 = COLOR_WHITE.u32;
	uis.color.disabled.u32 = COLOR_WHITE.u32;
	uis.realtime = 100;

	if (!TestActionAndStaticItems())
	{
		return 1;
	}

	if (!TestBitmapItem())
	{
		return 2;
	}

	if (!TestFieldItem())
	{
		return 3;
	}

	if (!TestListItem())
	{
		return 4;
	}

	if (!TestSliderItem())
	{
		return 5;
	}

	return 0;
}
