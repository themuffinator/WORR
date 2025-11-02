#pragma once

#include "images.hpp"

#if USE_FREETYPE

bool Draw_LoadFreeTypeFont(image_t *image, const char *filename);
void Draw_FreeFreeTypeFont(image_t *image);
void Draw_InitFreeTypeFonts(void);
void Draw_ShutdownFreeTypeFonts(void);

#else

inline bool Draw_LoadFreeTypeFont(image_t *, const char *)
{
    return false;
}

inline void Draw_FreeFreeTypeFont(image_t *)
{
}

inline void Draw_InitFreeTypeFonts(void)
{
}

inline void Draw_ShutdownFreeTypeFonts(void)
{
}

#endif
