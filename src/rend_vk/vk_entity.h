/*
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include "vk_local.h"

bool VK_Entity_Init(vk_context_t *ctx);
void VK_Entity_Shutdown(vk_context_t *ctx);

bool VK_Entity_CreateSwapchainResources(vk_context_t *ctx);
void VK_Entity_DestroySwapchainResources(vk_context_t *ctx);

void VK_Entity_BeginRegistration(void);
void VK_Entity_EndRegistration(void);
qhandle_t VK_Entity_RegisterModel(const char *name);

void VK_Entity_RenderFrame(const refdef_t *fd);
void VK_Entity_Record(VkCommandBuffer cmd, const VkExtent2D *extent);
