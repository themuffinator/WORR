#include "menu_controls.hpp"

/*
=============
Static_Init

Initializes layout information for a static text menu item.
=============
*/
void Static_Init(menuStatic_t *s)
{
	Q_assert(s->generic.name);

	if (!s->maxChars) {
		s->maxChars = MAX_STRING_CHARS;
	}

	s->generic.rect.x = s->generic.x;
	s->generic.rect.y = s->generic.y;

	UI_StringDimensions(&s->generic.rect,
		s->generic.uiFlags, s->generic.name);
}
