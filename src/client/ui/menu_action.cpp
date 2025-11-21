#include "menu_controls.hpp"

/*
=============
Action_Free

Releases memory owned by an action menu item.
=============
*/
void Action_Free(menuAction_t *a)
{
	Z_Free(a->generic.name);
	Z_Free(a->generic.status);
	Z_Free(a->cmd);
	Z_Free(a);
}

/*
=============
Action_Init

Initializes geometry for an action menu item.
=============
*/
void Action_Init(menuAction_t *a)
{
	Q_assert(a->generic.name);

	if ((a->generic.uiFlags & UI_CENTER) != UI_CENTER) {
		a->generic.x += RCOLUMN_OFFSET;
	}

	a->generic.rect.x = a->generic.x;
	a->generic.rect.y = a->generic.y;
	UI_StringDimensions(&a->generic.rect, a->generic.uiFlags, a->generic.name);
}

/*
=============
Savegame_Push

Populates a save or load action with the current slot metadata.
=============
*/
void Savegame_Push(menuAction_t *a)
{
	char *info;

	Z_Free(a->generic.name);

	info = SV_GetSaveInfo(a->cmd);
	if (info) {
		a->generic.name = info;
		a->generic.flags &= ~QMF_GRAYED;
	} else {
		a->generic.name = UI_CopyString("<EMPTY>");
		if (a->generic.type == MTYPE_LOADGAME)
			a->generic.flags |= QMF_GRAYED;
	}

	UI_StringDimensions(&a->generic.rect, a->generic.uiFlags, a->generic.name);
}
