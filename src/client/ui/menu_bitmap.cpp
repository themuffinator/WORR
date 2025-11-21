#include "menu_controls.hpp"

/*
=============
Bitmap_Free

Releases bitmap menu item resources.
=============
*/
void Bitmap_Free(menuBitmap_t *b)
{
	Z_Free(b->generic.status);
	Z_Free(b->cmd);
	Z_Free(b);
}

/*
=============
Bitmap_Init

Initializes layout information for a bitmap menu item.
=============
*/
void Bitmap_Init(menuBitmap_t *b)
{
	b->generic.rect.x = b->generic.x;
	b->generic.rect.y = b->generic.y;
	b->generic.rect.width = b->generic.width;
	b->generic.rect.height = b->generic.height;
}
