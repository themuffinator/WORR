/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include "shared/shared.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#include "client/client.h"
#include "client/video.h"
#include "renderer/renderer.h"

#if defined(RENDERER_DLL)
#include "renderer/renderer_api.h"
#endif

#if defined(RENDERER_VULKAN)
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#endif

#if USE_X11
#define VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#endif

#if USE_WAYLAND
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <wayland-client.h>
#endif

#include <vulkan/vulkan.h>
#endif

typedef struct vk_swapchain_s {
    VkSwapchainKHR handle;
    VkFormat format;
    VkExtent2D extent;
    VkImage *images;
    VkImageView *views;
    VkFramebuffer *framebuffers;
    VkCommandBuffer *command_buffers;
    uint32_t image_count;
} vk_swapchain_t;

typedef struct vk_context_s {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family;
    VkSurfaceKHR surface;
    VkRenderPass render_pass;
    VkCommandPool command_pool;
    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence in_flight_fence;
    vk_swapchain_t swapchain;
} vk_context_t;

typedef struct vk_state_s {
    bool initialized;
    bool swapchain_dirty;
    vid_native_window_t native_window;
    vk_context_t ctx;
} vk_state_t;

extern vk_state_t vk_state;
