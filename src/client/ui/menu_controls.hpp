#pragma once

#include "ui.hpp"

void Action_Init(menuAction_t *a);
void Savegame_Push(menuAction_t *a);

void Static_Init(menuStatic_t *s);

void Bitmap_Init(menuBitmap_t *b);

void Field_Init(menuField_t *f);
void Field_Push(menuField_t *f);
void Field_Pop(menuField_t *f);
int Field_Key(menuField_t *f, int key);
int Field_Char(menuField_t *f, int key);

void Slider_Init(menuSlider_t *s);
void Slider_Push(menuSlider_t *s);
void Slider_Pop(menuSlider_t *s);
menuSound_t Slider_Key(menuSlider_t *s, int key);
menuSound_t Slider_MouseMove(menuSlider_t *s);
menuSound_t Slider_DoSlide(menuSlider_t *s, int dir);
