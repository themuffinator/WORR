/*
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include "vk_local.h"

bool VK_UI_Init(vk_context_t *ctx);
void VK_UI_Shutdown(vk_context_t *ctx);
bool VK_UI_CreateSwapchainResources(vk_context_t *ctx);
void VK_UI_DestroySwapchainResources(vk_context_t *ctx);
void VK_UI_BeginFrame(void);
void VK_UI_EndFrame(void);
void VK_UI_Record(VkCommandBuffer cmd, const VkExtent2D *extent);

float VK_UI_ClampScale(cvar_t *var);
void VK_UI_SetScale(float scale);
void VK_UI_SetClipRect(const clipRect_t *clip);

qhandle_t VK_UI_RegisterImage(const char *name, imagetype_t type, imageflags_t flags);
qhandle_t VK_UI_RegisterRawImage(const char *name, int width, int height, byte *pic,
                                 imagetype_t type, imageflags_t flags);
void VK_UI_UnregisterImage(qhandle_t handle);
bool VK_UI_GetPicSize(int *w, int *h, qhandle_t pic);
bool VK_UI_IsImageTransparent(qhandle_t pic);
VkDescriptorSetLayout VK_UI_GetDescriptorSetLayout(void);
VkDescriptorSet VK_UI_GetDescriptorSetForImage(qhandle_t pic);
bool VK_UI_UpdateImageRGBA(qhandle_t handle, int width, int height, const byte *pic);
bool VK_UI_UpdateImageRGBASubRect(qhandle_t handle, int x, int y, int width, int height, const byte *pic);
void VK_UI_UpdateRawPic(int pic_w, int pic_h, const uint32_t *pic);
void VK_UI_DrawStretchRaw(int x, int y, int w, int h);
void VK_UI_DrawPic(int x, int y, color_t color, qhandle_t pic);
void VK_UI_DrawStretchPic(int x, int y, int w, int h, color_t color, qhandle_t pic);
void VK_UI_DrawStretchSubPic(int x, int y, int w, int h,
                             float s1, float t1, float s2, float t2,
                             color_t color, qhandle_t pic);
void VK_UI_DrawStretchRotatePic(int x, int y, int w, int h, color_t color, float angle,
                                int pivot_x, int pivot_y, qhandle_t pic);
void VK_UI_DrawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic);
void VK_UI_TileClear(int x, int y, int w, int h, qhandle_t pic);
void VK_UI_DrawFill32(int x, int y, int w, int h, color_t color);
void VK_UI_DrawFill8(int x, int y, int w, int h, int c);
