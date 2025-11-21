#include "menu_controls.hpp"

#include <cstring>

/*
=============
Slider_DoSlideInternal

Applies a directional change to the slider value and triggers callbacks.
=============
*/
static menuSound_t Slider_DoSlideInternal(menuSlider_t *s, int dir)
{
	s->modified = true;
	s->curvalue = Q_circ_clipf(s->curvalue + dir * s->step, s->minvalue, s->maxvalue);

	if (s->generic.change) {
		menuSound_t sound = s->generic.change(&s->generic);
		if (sound != QMS_NOTHANDLED) {
			return sound;
		}
	}

	return QMS_SILENT;
}

/*
=============
Slider_Push

Loads the slider value from its cvar and resets modification tracking.
=============
*/
void Slider_Push(menuSlider_t *s)
{
	s->modified = false;
	s->curvalue = Q_circ_clipf(s->cvar->value, s->minvalue, s->maxvalue);
}

/*
=============
Slider_Pop

Writes a modified slider back to its cvar.
=============
*/
void Slider_Pop(menuSlider_t *s)
{
	if (s->modified) {
		float val = Q_circ_clipf(s->curvalue, s->minvalue, s->maxvalue);
		Cvar_SetValue(s->cvar, val, FROM_MENU);
	}
}

/*
=============
Slider_Free

Releases memory owned by a slider widget.
=============
*/
void Slider_Free(menuSlider_t *s)
{
	Z_Free(s->generic.name);
	Z_Free(s->generic.status);
	Z_Free(s);
}

/*
=============
Slider_Init

Initializes layout information for a slider widget.
=============
*/
void Slider_Init(menuSlider_t *s)
{
	int len = strlen(s->generic.name) * CONCHAR_WIDTH;

	s->generic.rect.x = s->generic.x + LCOLUMN_OFFSET - len;
	s->generic.rect.y = s->generic.y;

	s->generic.rect.width = (RCOLUMN_OFFSET - LCOLUMN_OFFSET) +
		len + (SLIDER_RANGE + 2) * CONCHAR_WIDTH;
	s->generic.rect.height = CONCHAR_HEIGHT;
}

/*
=============
Slider_Click

Responds to mouse clicks on the slider track or thumb.
=============
*/
static menuSound_t Slider_Click(menuSlider_t *s)
{
	vrect_t rect;
	float pos;
	int x;

	pos = Q_clipf((s->curvalue - s->minvalue) / (s->maxvalue - s->minvalue), 0, 1);

	x = CONCHAR_WIDTH + (SLIDER_RANGE - 1) * CONCHAR_WIDTH * pos;

	rect.x = s->generic.x + RCOLUMN_OFFSET;
	rect.y = s->generic.y;
	rect.width = x;
	rect.height = CONCHAR_HEIGHT;
	if (UI_CursorInRect(&rect))
		return Slider_DoSlideInternal(s, -1);

	rect.x = s->generic.x + RCOLUMN_OFFSET + x;
	rect.y = s->generic.y;
	rect.width = CONCHAR_WIDTH;
	rect.height = CONCHAR_HEIGHT;
	if (UI_CursorInRect(&rect)) {
		uis.mouseTracker = &s->generic;
		return QMS_SILENT;
	}

	rect.x = s->generic.x + RCOLUMN_OFFSET + x + CONCHAR_WIDTH;
	rect.y = s->generic.y;
	rect.width = (SLIDER_RANGE + 1) * CONCHAR_WIDTH - x;
	rect.height = CONCHAR_HEIGHT;
	if (UI_CursorInRect(&rect))
		return Slider_DoSlideInternal(s, 1);

	return QMS_SILENT;
}

/*
=============
Slider_MouseMove

Tracks mouse movement when dragging the slider thumb.
=============
*/
menuSound_t Slider_MouseMove(menuSlider_t *s)
{
	float pos, value;
	int steps;

	if (uis.mouseTracker != &s->generic)
		return QMS_NOTHANDLED;

	pos = (uis.mouseCoords[0] - (s->generic.x + RCOLUMN_OFFSET + CONCHAR_WIDTH)) * (1.0f / (SLIDER_RANGE * CONCHAR_WIDTH));

	value = Q_clipf(pos, 0, 1) * (s->maxvalue - s->minvalue);
	steps = Q_rint(value / s->step);

	s->modified = true;
	s->curvalue = s->minvalue + steps * s->step;
	return QMS_SILENT;
}

/*
=============
Slider_Key

Handles key input routed to the slider widget.
=============
*/
menuSound_t Slider_Key(menuSlider_t *s, int key)
{
	switch (key) {
	case K_END:
		s->modified = true;
		s->curvalue = s->maxvalue;
		return QMS_MOVE;
	case K_HOME:
		s->modified = true;
		s->curvalue = s->minvalue;
		return QMS_MOVE;
	case K_MOUSE1:
		return Slider_Click(s);
	}

	return QMS_NOTHANDLED;
}

/*
=============
Slider_DoSlide

Public wrapper used by other menu logic to step sliders.
=============
*/
menuSound_t Slider_DoSlide(menuSlider_t *s, int dir)
{
	return Slider_DoSlideInternal(s, dir);
}
