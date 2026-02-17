/*
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include "vk_local.h"

bool VK_World_Init(vk_context_t *ctx);
void VK_World_Shutdown(vk_context_t *ctx);

bool VK_World_CreateSwapchainResources(vk_context_t *ctx);
void VK_World_DestroySwapchainResources(vk_context_t *ctx);

void VK_World_BeginRegistration(const char *map);
void VK_World_EndRegistration(void);
void VK_World_SetSky(const char *name, float rotate, bool autorotate, const vec3_t axis);

void VK_World_RenderFrame(const refdef_t *fd);
void VK_World_Record(VkCommandBuffer cmd, const VkExtent2D *extent);

void VK_World_LightPoint(const vec3_t origin, vec3_t light);
const bsp_t *VK_World_GetBsp(void);
