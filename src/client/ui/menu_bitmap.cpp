#include "menu_controls.hpp"

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
