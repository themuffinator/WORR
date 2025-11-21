#include "menu_controls.hpp"

/*
=============
Field_Push

Loads the field widget from its backing cvar.
=============
*/
void Field_Push(menuField_t *f)
{
	IF_Init(&f->field, f->width, f->width);
	IF_Replace(&f->field, f->cvar->string);
}

/*
=============
Field_Pop

Writes the field contents back into its cvar.
=============
*/
void Field_Pop(menuField_t *f)
{
	Cvar_SetByVar(f->cvar, f->field.text, FROM_MENU);
}

/*
=============
Field_Free

Releases memory owned by a field widget.
=============
*/
void Field_Free(menuField_t *f)
{
	Z_Free(f->generic.name);
	Z_Free(f->generic.status);
	Z_Free(f);
}

/*
=============
Field_Init

Initializes geometry for a field widget.
=============
*/
void Field_Init(menuField_t *f)
{
	int w = f->width * CONCHAR_WIDTH;

	f->generic.uiFlags &= ~(UI_LEFT | UI_RIGHT);

	if (f->generic.name) {
		f->generic.rect.x = f->generic.x + LCOLUMN_OFFSET;
		f->generic.rect.y = f->generic.y;
		UI_StringDimensions(&f->generic.rect,
			f->generic.uiFlags | UI_RIGHT, f->generic.name);
		f->generic.rect.width += (RCOLUMN_OFFSET - LCOLUMN_OFFSET) + w;
	} else {
		f->generic.rect.x = f->generic.x - w / 2;
		f->generic.rect.y = f->generic.y;
		f->generic.rect.width = w;
		f->generic.rect.height = CONCHAR_HEIGHT;
	}
}

/*
=============
Field_TestKey

Validates whether the supplied key is acceptable for the field type.
=============
*/
static bool Field_TestKey(menuField_t *f, int key)
{
	if (f->generic.flags & QMF_NUMBERSONLY) {
		return Q_isdigit(key) || key == '+' || key == '-' || key == '.';
	}

	return Q_isprint(key);
}

/*
=============
Field_Key

Handles key presses routed to the field widget.
=============
*/
int Field_Key(menuField_t *f, int key)
{
	if (IF_KeyEvent(&f->field, key)) {
		return QMS_SILENT;
	}

	if (Field_TestKey(f, key)) {
		return QMS_SILENT;
	}

	return QMS_NOTHANDLED;
}

/*
=============
Field_Char

Handles character input routed to the field widget.
=============
*/
int Field_Char(menuField_t *f, int key)
{
	bool ret;

	if (!Field_TestKey(f, key)) {
		return QMS_BEEP;
	}

	ret = IF_CharEvent(&f->field, key);
	if (f->generic.change) {
		f->generic.change(&f->generic);
	}

	return ret ? QMS_SILENT : QMS_NOTHANDLED;
}
