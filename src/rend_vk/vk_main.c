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

#include "vk_local.h"
#include "vk_entity.h"
#include "vk_ui.h"
#include "vk_world.h"
#include "common/utils.h"
#include "format/pcx.h"

#include <string.h>

vk_state_t vk_state;
refcfg_t r_config;
uint32_t d_8to24table[256];
static cvar_t *vk_draw_world;
static cvar_t *vk_draw_entities;
static cvar_t *vk_draw_ui;

static bool VK_LoadPaletteFromColormap(void)
{
    byte *data = NULL;
    int len = FS_LoadFile("pics/colormap.pcx", (void **)&data);
    if (len < (int)sizeof(dpcx_t) || !data) {
        if (data) {
            FS_FreeFile(data);
        }
        return false;
    }

    const dpcx_t *pcx = (const dpcx_t *)data;
    bool valid = (pcx->manufacturer == 10 &&
                  pcx->version == 5 &&
                  pcx->encoding == 1 &&
                  pcx->bits_per_pixel == 8 &&
                  pcx->color_planes == 1 &&
                  len >= 768);
    if (!valid) {
        FS_FreeFile(data);
        return false;
    }

    const byte *src = data + len - 768;
    for (int i = 0; i < 255; i++, src += 3) {
        d_8to24table[i] = COLOR_U32_RGBA(src[0], src[1], src[2], 255);
    }
    d_8to24table[255] = COLOR_U32_RGBA(src[0], src[1], src[2], 0);

    FS_FreeFile(data);
    return true;
}

static void VK_InitPalette(void)
{
    if (VK_LoadPaletteFromColormap()) {
        return;
    }

    for (int i = 0; i < 255; i++) {
        d_8to24table[i] = COLOR_RGB(i, i, i).u32;
    }
    d_8to24table[255] = COLOR_RGBA(0, 0, 0, 0).u32;
    Com_WPrintf("Vulkan: using grayscale fallback palette; colormap.pcx not available\n");
}

static const char *VK_ResultString(VkResult result)
{
    switch (result) {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "VK_ERROR_VALIDATION_FAILED_EXT";
    default:
        return "VK_UNKNOWN_ERROR";
    }
}

static bool VK_Check(VkResult result, const char *what)
{
    if (result == VK_SUCCESS) {
        return true;
    }

    Com_SetLastError(va("Vulkan %s failed: %s", what, VK_ResultString(result)));
    return false;
}

static uint32_t VK_ClampU32(uint32_t value, uint32_t min_value, uint32_t max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static uint32_t VK_FindMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter,
                                  VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & BIT(i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return UINT32_MAX;
}

static bool VK_IsDepthFormatSupported(VkPhysicalDevice device, VkFormat format)
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(device, format, &props);
    return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

static VkFormat VK_ChooseDepthFormat(VkPhysicalDevice device)
{
    static const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };

    for (size_t i = 0; i < q_countof(candidates); ++i) {
        if (VK_IsDepthFormatSupported(device, candidates[i])) {
            return candidates[i];
        }
    }

    return VK_FORMAT_UNDEFINED;
}

static const char *VK_SurfaceExtensionForPlatform(vid_native_platform_t platform)
{
    switch (platform) {
#if defined(_WIN32)
    case VID_NATIVE_WIN32:
        return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#endif
#if USE_X11
    case VID_NATIVE_X11:
        return VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
#endif
#if USE_WAYLAND
    case VID_NATIVE_WAYLAND:
        return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
#endif
    case VID_NATIVE_SDL:
    default:
        return NULL;
    }
}

static bool VK_QueryNativeWindow(vid_native_window_t *out)
{
    if (!out) {
        Com_SetLastError("Vulkan: native window buffer missing");
        return false;
    }

    if (!vid || !vid->get_native_window) {
        Com_SetLastError("Vulkan: video driver does not expose native window handles");
        return false;
    }

    memset(out, 0, sizeof(*out));
    if (!vid->get_native_window(out)) {
        Com_SetLastError("Vulkan: video driver failed to provide native window handles");
        return false;
    }

    return true;
}

static bool VK_CreateInstance(void)
{
    const char *surface_ext = VK_SurfaceExtensionForPlatform(vk_state.native_window.platform);
    if (!surface_ext) {
        Com_SetLastError("Vulkan: unsupported video driver for surface creation");
        return false;
    }

    uint32_t ext_count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(NULL, &ext_count, NULL);
    if (result != VK_SUCCESS || ext_count == 0) {
        return VK_Check(result, "vkEnumerateInstanceExtensionProperties");
    }

    VkExtensionProperties *exts = Z_TagMallocz(sizeof(*exts) * ext_count, TAG_RENDERER);
    result = vkEnumerateInstanceExtensionProperties(NULL, &ext_count, exts);
    if (result != VK_SUCCESS) {
        Z_Free(exts);
        return VK_Check(result, "vkEnumerateInstanceExtensionProperties");
    }

    bool has_surface = false;
    bool has_platform = false;
    for (uint32_t i = 0; i < ext_count; ++i) {
        if (!strcmp(exts[i].extensionName, VK_KHR_SURFACE_EXTENSION_NAME))
            has_surface = true;
        if (!strcmp(exts[i].extensionName, surface_ext))
            has_platform = true;
    }
    Z_Free(exts);

    if (!has_surface || !has_platform) {
        Com_SetLastError("Vulkan: required instance extensions missing");
        return false;
    }

    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        surface_ext
    };

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = PRODUCT,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "WORR",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = (uint32_t)(sizeof(extensions) / sizeof(extensions[0])),
        .ppEnabledExtensionNames = extensions,
    };

    return VK_Check(vkCreateInstance(&create_info, NULL, &vk_state.ctx.instance),
                    "vkCreateInstance");
}

static bool VK_CreateSurface(void)
{
    const vid_native_window_t *native = &vk_state.native_window;

    switch (native->platform) {
#if defined(_WIN32)
    case VID_NATIVE_WIN32: {
        VkWin32SurfaceCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = (HINSTANCE)native->handle.win32.hinstance,
            .hwnd = (HWND)native->handle.win32.hwnd,
        };
        return VK_Check(vkCreateWin32SurfaceKHR(vk_state.ctx.instance, &create_info, NULL,
                                                &vk_state.ctx.surface),
                        "vkCreateWin32SurfaceKHR");
    }
#endif
#if USE_X11
    case VID_NATIVE_X11: {
        VkXlibSurfaceCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
            .dpy = (Display *)native->handle.x11.display,
            .window = (Window)native->handle.x11.window,
        };
        return VK_Check(vkCreateXlibSurfaceKHR(vk_state.ctx.instance, &create_info, NULL,
                                               &vk_state.ctx.surface),
                        "vkCreateXlibSurfaceKHR");
    }
#endif
#if USE_WAYLAND
    case VID_NATIVE_WAYLAND: {
        VkWaylandSurfaceCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
            .display = (struct wl_display *)native->handle.wayland.display,
            .surface = (struct wl_surface *)native->handle.wayland.surface,
        };
        return VK_Check(vkCreateWaylandSurfaceKHR(vk_state.ctx.instance, &create_info, NULL,
                                                  &vk_state.ctx.surface),
                        "vkCreateWaylandSurfaceKHR");
    }
#endif
    case VID_NATIVE_SDL:
    default:
        Com_SetLastError("Vulkan: SDL native window handles are not supported yet");
        return false;
    }
}

static bool VK_DeviceHasExtension(VkPhysicalDevice device, const char *ext)
{
    uint32_t count = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL);
    if (result != VK_SUCCESS || count == 0) {
        return false;
    }

    VkExtensionProperties *props = Z_TagMallocz(sizeof(*props) * count, TAG_RENDERER);
    result = vkEnumerateDeviceExtensionProperties(device, NULL, &count, props);
    if (result != VK_SUCCESS) {
        Z_Free(props);
        return false;
    }

    bool found = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (!strcmp(props[i].extensionName, ext)) {
            found = true;
            break;
        }
    }

    Z_Free(props);
    return found;
}

static bool VK_FindQueueFamily(VkPhysicalDevice device, uint32_t *out_index)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);
    if (!count)
        return false;

    VkQueueFamilyProperties *props = Z_TagMallocz(sizeof(*props) * count, TAG_RENDERER);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, props);

    bool found = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (!(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            continue;

        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, vk_state.ctx.surface,
                                             &present_supported);
        if (present_supported) {
            *out_index = i;
            found = true;
            break;
        }
    }

    Z_Free(props);
    return found;
}

static bool VK_SelectPhysicalDevice(void)
{
    uint32_t count = 0;
    VkResult result = vkEnumeratePhysicalDevices(vk_state.ctx.instance, &count, NULL);
    if (result != VK_SUCCESS || count == 0) {
        return VK_Check(result, "vkEnumeratePhysicalDevices");
    }

    VkPhysicalDevice *devices = Z_TagMallocz(sizeof(*devices) * count, TAG_RENDERER);
    result = vkEnumeratePhysicalDevices(vk_state.ctx.instance, &count, devices);
    if (result != VK_SUCCESS) {
        Z_Free(devices);
        return VK_Check(result, "vkEnumeratePhysicalDevices");
    }

    VkPhysicalDevice picked = VK_NULL_HANDLE;
    uint32_t queue_family = 0;

    for (uint32_t i = 0; i < count; ++i) {
        VkPhysicalDevice device = devices[i];
        if (!VK_DeviceHasExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
            continue;

        uint32_t family = 0;
        if (!VK_FindQueueFamily(device, &family))
            continue;

        picked = device;
        queue_family = family;
        break;
    }

    if (picked == VK_NULL_HANDLE) {
        Z_Free(devices);
        Com_SetLastError("Vulkan: no compatible physical device found");
        return false;
    }

    vk_state.ctx.physical_device = picked;
    vk_state.ctx.graphics_queue_family = queue_family;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(picked, &props);
    Com_Printf("Vulkan device: %s\n", props.deviceName);

    Z_Free(devices);
    return true;
}

static bool VK_CreateDevice(void)
{
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = vk_state.ctx.graphics_queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    const char *extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = (uint32_t)(sizeof(extensions) / sizeof(extensions[0])),
        .ppEnabledExtensionNames = extensions,
    };

    if (!VK_Check(vkCreateDevice(vk_state.ctx.physical_device, &create_info, NULL,
                                 &vk_state.ctx.device),
                  "vkCreateDevice")) {
        return false;
    }

    vkGetDeviceQueue(vk_state.ctx.device, vk_state.ctx.graphics_queue_family, 0,
                     &vk_state.ctx.graphics_queue);

    return true;
}

static bool VK_CreateCommandPool(vk_context_t *ctx)
{
    if (!ctx || !ctx->device) {
        Com_SetLastError("Vulkan: device unavailable for command pool creation");
        return false;
    }

    if (ctx->command_pool) {
        return true;
    }

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ctx->graphics_queue_family,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };

    VkResult result = vkCreateCommandPool(ctx->device, &pool_info, NULL, &ctx->command_pool);
    return VK_Check(result, "vkCreateCommandPool");
}

static VkSurfaceFormatKHR VK_ChooseSurfaceFormat(const VkSurfaceFormatKHR *formats,
                                                 uint32_t count)
{
    if (count == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        VkSurfaceFormatKHR chosen = {
            .format = VK_FORMAT_B8G8R8A8_UNORM,
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        };
        return chosen;
    }

    for (uint32_t i = 0; i < count; ++i) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return formats[i];
        }
    }

    return formats[0];
}

static VkPresentModeKHR VK_ChoosePresentMode(const VkPresentModeKHR *modes, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        if (modes[i] == VK_PRESENT_MODE_FIFO_KHR)
            return modes[i];
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D VK_ChooseSwapExtent(const VkSurfaceCapabilitiesKHR *caps,
                                      uint32_t width, uint32_t height)
{
    if (caps->currentExtent.width != UINT32_MAX) {
        return caps->currentExtent;
    }

    VkExtent2D extent = {
        .width = VK_ClampU32(width, caps->minImageExtent.width, caps->maxImageExtent.width),
        .height = VK_ClampU32(height, caps->minImageExtent.height, caps->maxImageExtent.height),
    };

    return extent;
}

static void VK_DestroySwapchain(vk_context_t *ctx)
{
    if (!ctx->device)
        return;

    ctx->frame_submitted = false;

    VK_World_DestroySwapchainResources(ctx);
    VK_Entity_DestroySwapchainResources(ctx);
    VK_UI_DestroySwapchainResources(ctx);

    if (ctx->in_flight_fence) {
        vkDestroyFence(ctx->device, ctx->in_flight_fence, NULL);
        ctx->in_flight_fence = VK_NULL_HANDLE;
    }
    if (ctx->render_finished) {
        vkDestroySemaphore(ctx->device, ctx->render_finished, NULL);
        ctx->render_finished = VK_NULL_HANDLE;
    }
    if (ctx->image_available) {
        vkDestroySemaphore(ctx->device, ctx->image_available, NULL);
        ctx->image_available = VK_NULL_HANDLE;
    }

    if (ctx->command_pool && ctx->swapchain.command_buffers && ctx->swapchain.image_count) {
        vkFreeCommandBuffers(ctx->device, ctx->command_pool, ctx->swapchain.image_count,
                             ctx->swapchain.command_buffers);
    }

    if (ctx->swapchain.framebuffers) {
        for (uint32_t i = 0; i < ctx->swapchain.image_count; ++i) {
            if (ctx->swapchain.framebuffers[i]) {
                vkDestroyFramebuffer(ctx->device, ctx->swapchain.framebuffers[i], NULL);
            }
        }
        Z_Free(ctx->swapchain.framebuffers);
        ctx->swapchain.framebuffers = NULL;
    }

    if (ctx->render_pass) {
        vkDestroyRenderPass(ctx->device, ctx->render_pass, NULL);
        ctx->render_pass = VK_NULL_HANDLE;
    }

    if (ctx->swapchain.views) {
        for (uint32_t i = 0; i < ctx->swapchain.image_count; ++i) {
            if (ctx->swapchain.views[i]) {
                vkDestroyImageView(ctx->device, ctx->swapchain.views[i], NULL);
            }
        }
        Z_Free(ctx->swapchain.views);
        ctx->swapchain.views = NULL;
    }

    if (ctx->swapchain.depth_view) {
        vkDestroyImageView(ctx->device, ctx->swapchain.depth_view, NULL);
        ctx->swapchain.depth_view = VK_NULL_HANDLE;
    }

    if (ctx->swapchain.depth_image) {
        vkDestroyImage(ctx->device, ctx->swapchain.depth_image, NULL);
        ctx->swapchain.depth_image = VK_NULL_HANDLE;
    }

    if (ctx->swapchain.depth_memory) {
        vkFreeMemory(ctx->device, ctx->swapchain.depth_memory, NULL);
        ctx->swapchain.depth_memory = VK_NULL_HANDLE;
    }

    if (ctx->swapchain.images) {
        Z_Free(ctx->swapchain.images);
        ctx->swapchain.images = NULL;
    }

    if (ctx->swapchain.handle) {
        vkDestroySwapchainKHR(ctx->device, ctx->swapchain.handle, NULL);
        ctx->swapchain.handle = VK_NULL_HANDLE;
    }

    if (ctx->swapchain.command_buffers) {
        Z_Free(ctx->swapchain.command_buffers);
        ctx->swapchain.command_buffers = NULL;
    }

    ctx->swapchain.image_count = 0;
    ctx->swapchain.format = VK_FORMAT_UNDEFINED;
    ctx->swapchain.depth_format = VK_FORMAT_UNDEFINED;
    memset(&ctx->swapchain.extent, 0, sizeof(ctx->swapchain.extent));
}

static bool VK_CreateSwapchain(uint32_t width, uint32_t height)
{
    vk_context_t *ctx = &vk_state.ctx;

    VkSurfaceCapabilitiesKHR caps;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->physical_device,
                                                                ctx->surface, &caps);
    if (result != VK_SUCCESS) {
        return VK_Check(result, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    }

    uint32_t format_count = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physical_device, ctx->surface,
                                                  &format_count, NULL);
    if (result != VK_SUCCESS || !format_count) {
        return VK_Check(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    }

    VkSurfaceFormatKHR *formats = Z_TagMallocz(sizeof(*formats) * format_count, TAG_RENDERER);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physical_device, ctx->surface,
                                                  &format_count, formats);
    if (result != VK_SUCCESS) {
        Z_Free(formats);
        return VK_Check(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    }
    VkSurfaceFormatKHR chosen_format = VK_ChooseSurfaceFormat(formats, format_count);
    Z_Free(formats);

    uint32_t present_count = 0;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physical_device, ctx->surface,
                                                       &present_count, NULL);
    if (result != VK_SUCCESS || !present_count) {
        return VK_Check(result, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    }

    VkPresentModeKHR *present_modes = Z_TagMallocz(sizeof(*present_modes) * present_count,
                                                   TAG_RENDERER);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physical_device, ctx->surface,
                                                       &present_count, present_modes);
    if (result != VK_SUCCESS) {
        Z_Free(present_modes);
        return VK_Check(result, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    }
    VkPresentModeKHR present_mode = VK_ChoosePresentMode(present_modes, present_count);
    Z_Free(present_modes);

    VkExtent2D extent = VK_ChooseSwapExtent(&caps, width, height);
    if (!extent.width || !extent.height) {
        Com_SetLastError("Vulkan: swapchain extent is zero");
        return false;
    }

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = ctx->surface,
        .minImageCount = image_count,
        .imageFormat = chosen_format.format,
        .imageColorSpace = chosen_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    result = vkCreateSwapchainKHR(ctx->device, &create_info, NULL, &ctx->swapchain.handle);
    if (result != VK_SUCCESS) {
        return VK_Check(result, "vkCreateSwapchainKHR");
    }

    result = vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain.handle, &image_count, NULL);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkGetSwapchainImagesKHR");
    }

    ctx->swapchain.images = Z_TagMallocz(sizeof(VkImage) * image_count, TAG_RENDERER);
    result = vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain.handle, &image_count,
                                     ctx->swapchain.images);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkGetSwapchainImagesKHR");
    }

    ctx->swapchain.image_count = image_count;
    ctx->swapchain.format = chosen_format.format;
    ctx->swapchain.extent = extent;

    ctx->swapchain.views = Z_TagMallocz(sizeof(VkImageView) * image_count, TAG_RENDERER);

    for (uint32_t i = 0; i < image_count; ++i) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = ctx->swapchain.images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = ctx->swapchain.format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        result = vkCreateImageView(ctx->device, &view_info, NULL, &ctx->swapchain.views[i]);
        if (result != VK_SUCCESS) {
            VK_DestroySwapchain(ctx);
            return VK_Check(result, "vkCreateImageView");
        }
    }

    ctx->swapchain.depth_format = VK_ChooseDepthFormat(ctx->physical_device);
    if (ctx->swapchain.depth_format == VK_FORMAT_UNDEFINED) {
        Com_SetLastError("Vulkan: no supported depth format found");
        VK_DestroySwapchain(ctx);
        return false;
    }

    VkImageCreateInfo depth_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = ctx->swapchain.depth_format,
        .extent = {
            .width = ctx->swapchain.extent.width,
            .height = ctx->swapchain.extent.height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    result = vkCreateImage(ctx->device, &depth_image_info, NULL, &ctx->swapchain.depth_image);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkCreateImage(depth)");
    }

    VkMemoryRequirements depth_requirements;
    vkGetImageMemoryRequirements(ctx->device, ctx->swapchain.depth_image, &depth_requirements);

    uint32_t depth_memory_type = VK_FindMemoryType(ctx->physical_device,
                                                   depth_requirements.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (depth_memory_type == UINT32_MAX) {
        Com_SetLastError("Vulkan: no suitable depth memory type found");
        VK_DestroySwapchain(ctx);
        return false;
    }

    VkMemoryAllocateInfo depth_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = depth_requirements.size,
        .memoryTypeIndex = depth_memory_type,
    };

    result = vkAllocateMemory(ctx->device, &depth_alloc_info, NULL, &ctx->swapchain.depth_memory);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkAllocateMemory(depth)");
    }

    result = vkBindImageMemory(ctx->device, ctx->swapchain.depth_image, ctx->swapchain.depth_memory, 0);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkBindImageMemory(depth)");
    }

    VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (ctx->swapchain.depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        ctx->swapchain.depth_format == VK_FORMAT_D24_UNORM_S8_UINT) {
        depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VkImageViewCreateInfo depth_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = ctx->swapchain.depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = ctx->swapchain.depth_format,
        .subresourceRange = {
            .aspectMask = depth_aspect,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    result = vkCreateImageView(ctx->device, &depth_view_info, NULL, &ctx->swapchain.depth_view);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkCreateImageView(depth)");
    }

    VkAttachmentDescription attachments[2] = {
        {
            .format = ctx->swapchain.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        },
        {
            .format = ctx->swapchain.depth_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },
    };

    VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depth_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pDepthStencilAttachment = &depth_ref,
    };

    VkSubpassDependency dependencies[2] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        },
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = 0,
        },
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = q_countof(attachments),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = q_countof(dependencies),
        .pDependencies = dependencies,
    };

    result = vkCreateRenderPass(ctx->device, &render_pass_info, NULL, &ctx->render_pass);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkCreateRenderPass");
    }

    ctx->swapchain.framebuffers = Z_TagMallocz(sizeof(VkFramebuffer) * image_count,
                                               TAG_RENDERER);

    for (uint32_t i = 0; i < image_count; ++i) {
        VkImageView framebuffer_attachments[] = {
            ctx->swapchain.views[i],
            ctx->swapchain.depth_view,
        };
        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = ctx->render_pass,
            .attachmentCount = q_countof(framebuffer_attachments),
            .pAttachments = framebuffer_attachments,
            .width = ctx->swapchain.extent.width,
            .height = ctx->swapchain.extent.height,
            .layers = 1,
        };

        result = vkCreateFramebuffer(ctx->device, &framebuffer_info, NULL,
                                     &ctx->swapchain.framebuffers[i]);
        if (result != VK_SUCCESS) {
            VK_DestroySwapchain(ctx);
            return VK_Check(result, "vkCreateFramebuffer");
        }
    }

    if (!ctx->command_pool) {
        Com_SetLastError("Vulkan: command pool unavailable during swapchain creation");
        VK_DestroySwapchain(ctx);
        return false;
    }

    ctx->swapchain.command_buffers = Z_TagMallocz(sizeof(VkCommandBuffer) * image_count,
                                                  TAG_RENDERER);

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = image_count,
    };

    result = vkAllocateCommandBuffers(ctx->device, &alloc_info, ctx->swapchain.command_buffers);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkAllocateCommandBuffers");
    }

    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    result = vkCreateSemaphore(ctx->device, &sem_info, NULL, &ctx->image_available);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkCreateSemaphore");
    }

    result = vkCreateSemaphore(ctx->device, &sem_info, NULL, &ctx->render_finished);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkCreateSemaphore");
    }

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    result = vkCreateFence(ctx->device, &fence_info, NULL, &ctx->in_flight_fence);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkCreateFence");
    }

    ctx->frame_submitted = false;
    return true;
}

static bool VK_RecreateSwapchain(uint32_t width, uint32_t height)
{
    vk_context_t *ctx = &vk_state.ctx;

    if (!ctx->device)
        return false;

    vkDeviceWaitIdle(ctx->device);
    VK_DestroySwapchain(ctx);

    if (!VK_CreateSwapchain(width, height)) {
        return false;
    }

    if (!VK_World_CreateSwapchainResources(ctx)) {
        VK_DestroySwapchain(ctx);
        return false;
    }

    if (!VK_Entity_CreateSwapchainResources(ctx)) {
        VK_DestroySwapchain(ctx);
        return false;
    }

    if (!VK_UI_CreateSwapchainResources(ctx)) {
        VK_DestroySwapchain(ctx);
        return false;
    }

    vk_state.swapchain_dirty = false;
    return true;
}

static bool VK_RecordCommandBuffer(VkCommandBuffer cmd, uint32_t image_index)
{
    vk_context_t *ctx = &vk_state.ctx;

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };

    VkResult result = vkBeginCommandBuffer(cmd, &begin_info);
    if (result != VK_SUCCESS) {
        return VK_Check(result, "vkBeginCommandBuffer");
    }

    VkClearValue clear_values[2] = {
        {
            .color = { { 0.0f, 0.0f, 0.0f, 1.0f } },
        },
        {
            .depthStencil = { 1.0f, 0 },
        },
    };

    VkRenderPassBeginInfo render_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx->render_pass,
        .framebuffer = ctx->swapchain.framebuffers[image_index],
        .renderArea = {
            .offset = { 0, 0 },
            .extent = ctx->swapchain.extent,
        },
        .clearValueCount = q_countof(clear_values),
        .pClearValues = clear_values,
    };

    vkCmdBeginRenderPass(cmd, &render_info, VK_SUBPASS_CONTENTS_INLINE);
    if (!vk_draw_world || vk_draw_world->integer) {
        VK_World_Record(cmd, &ctx->swapchain.extent);
    }
    if (!vk_draw_entities || vk_draw_entities->integer) {
        VK_Entity_Record(cmd, &ctx->swapchain.extent);
    }
    if (!vk_draw_ui || vk_draw_ui->integer) {
        VK_UI_Record(cmd, &ctx->swapchain.extent);
    }
    vkCmdEndRenderPass(cmd);

    result = vkEndCommandBuffer(cmd);
    return VK_Check(result, "vkEndCommandBuffer");
}

static bool VK_WaitForSubmittedFrame(vk_context_t *ctx, const char *what)
{
    if (!ctx->frame_submitted) {
        return true;
    }

    VkResult result = vkWaitForFences(ctx->device, 1, &ctx->in_flight_fence, VK_TRUE,
                                      UINT64_MAX);
    if (result == VK_SUCCESS) {
        return true;
    }

    ctx->frame_submitted = false;
    return VK_Check(result, what);
}

static bool VK_DrawFrame(void)
{
    vk_context_t *ctx = &vk_state.ctx;

    if (!ctx->swapchain.handle)
        return false;

    VkResult result = VK_SUCCESS;
    if (!VK_WaitForSubmittedFrame(ctx, "vkWaitForFences")) {
        return false;
    }

    uint32_t image_index = 0;
    result = vkAcquireNextImageKHR(ctx->device, ctx->swapchain.handle, UINT64_MAX,
                                   ctx->image_available, VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        vk_state.swapchain_dirty = true;
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return VK_Check(result, "vkAcquireNextImageKHR");
    }

    VkCommandBuffer cmd = ctx->swapchain.command_buffers[image_index];
    vkResetCommandBuffer(cmd, 0);
    if (!VK_RecordCommandBuffer(cmd, image_index))
        return false;

    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &ctx->image_available,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &ctx->render_finished,
    };

    result = vkResetFences(ctx->device, 1, &ctx->in_flight_fence);
    if (result != VK_SUCCESS) {
        ctx->frame_submitted = false;
        return VK_Check(result, "vkResetFences");
    }

    result = vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, ctx->in_flight_fence);
    if (result != VK_SUCCESS) {
        ctx->frame_submitted = false;
        return VK_Check(result, "vkQueueSubmit");
    }
    ctx->frame_submitted = true;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &ctx->render_finished,
        .swapchainCount = 1,
        .pSwapchains = &ctx->swapchain.handle,
        .pImageIndices = &image_index,
    };

    result = vkQueuePresentKHR(ctx->graphics_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        vk_state.swapchain_dirty = true;
        return false;
    }

    return VK_Check(result, "vkQueuePresentKHR");
}

static void VK_DestroyContext(void)
{
    vk_context_t *ctx = &vk_state.ctx;

    if (ctx->device) {
        vkDeviceWaitIdle(ctx->device);
    }

    VK_Entity_Shutdown(ctx);
    VK_World_Shutdown(ctx);
    VK_UI_Shutdown(ctx);
    VK_DestroySwapchain(ctx);

    if (ctx->command_pool) {
        vkDestroyCommandPool(ctx->device, ctx->command_pool, NULL);
        ctx->command_pool = VK_NULL_HANDLE;
    }

    if (ctx->device) {
        vkDestroyDevice(ctx->device, NULL);
        ctx->device = VK_NULL_HANDLE;
    }

    if (ctx->surface) {
        vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);
        ctx->surface = VK_NULL_HANDLE;
    }

    if (ctx->instance) {
        vkDestroyInstance(ctx->instance, NULL);
        ctx->instance = VK_NULL_HANDLE;
    }

    ctx->physical_device = VK_NULL_HANDLE;
    ctx->graphics_queue = VK_NULL_HANDLE;
    ctx->graphics_queue_family = 0;
}

bool R_Init(bool total)
{
    if (!vk_draw_world) {
        vk_draw_world = Cvar_Get("vk_draw_world", "1", 0);
    }
    if (!vk_draw_entities) {
        vk_draw_entities = Cvar_Get("vk_draw_entities", "1", 0);
    }
    if (!vk_draw_ui) {
        vk_draw_ui = Cvar_Get("vk_draw_ui", "1", 0);
    }

    if (!total) {
        vk_state.swapchain_dirty = true;
        return vk_state.initialized;
    }

    if (total && !vk_state.initialized) {
        VK_InitPalette();
    }

    Com_Printf("------- VK_Init -------\n");
    Com_Printf("Using video driver: %s\n", vid->name);

    if (!vid->init()) {
        return false;
    }

    vid_native_window_t native;
    if (!VK_QueryNativeWindow(&native)) {
        vid->shutdown();
        return false;
    }

    vk_state.native_window = native;

    if (!VK_CreateInstance()) {
        vid->shutdown();
        return false;
    }

    if (!VK_CreateSurface()) {
        VK_DestroyContext();
        vid->shutdown();
        return false;
    }

    if (!VK_SelectPhysicalDevice()) {
        VK_DestroyContext();
        vid->shutdown();
        return false;
    }

    if (!VK_CreateDevice()) {
        VK_DestroyContext();
        vid->shutdown();
        return false;
    }

    if (!VK_CreateCommandPool(&vk_state.ctx)) {
        VK_DestroyContext();
        vid->shutdown();
        return false;
    }

    if (!VK_UI_Init(&vk_state.ctx)) {
        VK_DestroyContext();
        vid->shutdown();
        return false;
    }

    if (!VK_World_Init(&vk_state.ctx)) {
        VK_DestroyContext();
        vid->shutdown();
        return false;
    }

    if (!VK_Entity_Init(&vk_state.ctx)) {
        VK_DestroyContext();
        vid->shutdown();
        return false;
    }

    vk_state.initialized = true;
    vk_state.swapchain_dirty = true;

    if (!VK_RecreateSwapchain(r_config.width, r_config.height)) {
        vk_state.swapchain_dirty = true;
    }

    Com_Printf("----------------------\n");

    return true;
}

void R_Shutdown(bool total)
{
    if (!vk_state.initialized) {
        return;
    }

    if (!total) {
        vk_state.swapchain_dirty = true;
        return;
    }

    VK_DestroyContext();
    vid->shutdown();

    memset(&vk_state, 0, sizeof(vk_state));
}

void R_BeginRegistration(const char *map)
{
    VK_Entity_BeginRegistration();
    VK_World_BeginRegistration(map);
}

qhandle_t R_RegisterModel(const char *name)
{
    return VK_Entity_RegisterModel(name);
}

qhandle_t R_RegisterImage(const char *name, imagetype_t type, imageflags_t flags)
{
    return VK_UI_RegisterImage(name, type, flags);
}

qhandle_t R_RegisterRawImage(const char *name, int width, int height, byte *pic,
                             imagetype_t type, imageflags_t flags)
{
    return VK_UI_RegisterRawImage(name, width, height, pic, type, flags);
}

void R_UnregisterImage(qhandle_t handle)
{
    VK_UI_UnregisterImage(handle);
}

void R_SetSky(const char *name, float rotate, bool autorotate, const vec3_t axis)
{
    Com_DPrintf("VK R_SetSky: name=%s rotate=%.2f autorotate=%d axis=(%.2f %.2f %.2f)\n",
                (name && *name) ? name : "<empty>",
                rotate, autorotate ? 1 : 0,
                axis ? axis[0] : 0.0f,
                axis ? axis[1] : 0.0f,
                axis ? axis[2] : 0.0f);
    VK_World_SetSky(name, rotate, autorotate, axis);
}

void R_EndRegistration(void)
{
    VK_World_EndRegistration();
    VK_Entity_EndRegistration();
}

void R_RenderFrame(const refdef_t *fd)
{
    VK_World_RenderFrame(fd);
    VK_Entity_RenderFrame(fd);
}

void R_LightPoint(const vec3_t origin, vec3_t light)
{
    VK_World_LightPoint(origin, light);
}

void R_SetClipRect(const clipRect_t *clip)
{
    VK_UI_SetClipRect(clip);
}

float R_ClampScale(cvar_t *var)
{
    return VK_UI_ClampScale(var);
}

void R_SetScale(float scale)
{
    VK_UI_SetScale(scale);
}

static inline color_t vk_draw_resolve_color(int flags, color_t color)
{
    if (flags & (UI_ALTCOLOR | UI_XORCOLOR)) {
        color_t alt = COLOR_RGB(255, 255, 0);
        alt.a = color.a;
        return alt;
    }
    return color;
}

static inline void vk_draw_char(int x, int y, int w, int h, int flags, int c,
                                color_t color, qhandle_t font)
{
    if ((c & 127) == 32)
        return;

    if (flags & UI_ALTCOLOR)
        c |= 0x80;
    if (flags & UI_XORCOLOR)
        c ^= 0x80;

    if (c >> 7)
        color = COLOR_SETA_U8(COLOR_WHITE, color.a);

    float s = (c & 15) * 0.0625f;
    float t = (c >> 4) * 0.0625f;
    float eps = 1e-5f;

    if ((flags & UI_DROPSHADOW) && c != 0x83) {
        color_t shadow = COLOR_A(color.a);
        VK_UI_DrawStretchSubPic(x + 1, y + 1, w, h,
                                s + eps, t + eps,
                                s + 0.0625f - eps, t + 0.0625f - eps,
                                shadow, font);
    }

    VK_UI_DrawStretchSubPic(x, y, w, h,
                            s + eps, t + eps,
                            s + 0.0625f - eps, t + 0.0625f - eps,
                            color, font);
}

void R_DrawChar(int x, int y, int flags, int ch, color_t color, qhandle_t font)
{
    vk_draw_char(x, y, CONCHAR_WIDTH, CONCHAR_HEIGHT, flags, ch & 255, color, font);
}

void R_DrawStretchChar(int x, int y, int w, int h, int flags, int ch, color_t color, qhandle_t font)
{
    vk_draw_char(x, y, w, h, flags, ch & 255, color, font);
}

int R_DrawStringStretch(int x, int y, int scale, int flags, size_t maxChars,
                        const char *string, color_t color, qhandle_t font)
{
    if (!string)
        return x;

    int sx = x;
    size_t remaining = maxChars;
    bool use_color_codes = Com_HasColorEscape(string, maxChars);
    int draw_flags = flags;
    color_t base_color = color;
    if (use_color_codes) {
        base_color = vk_draw_resolve_color(flags, color);
        draw_flags &= ~(UI_ALTCOLOR | UI_XORCOLOR);
    }
    color_t draw_color = use_color_codes ? base_color : color;

    while (remaining && *string) {
        if (use_color_codes) {
            color_t parsed;
            if (Com_ParseColorEscape(&string, &remaining, base_color, &parsed)) {
                draw_color = parsed;
                continue;
            }
        }

        byte ch = *string++;
        remaining--;

        if ((flags & UI_MULTILINE) && ch == '\n') {
            y += CONCHAR_HEIGHT * max(scale, 1) + 1;
            x = sx;
            continue;
        }

        vk_draw_char(x, y, CONCHAR_WIDTH * max(scale, 1), CONCHAR_HEIGHT * max(scale, 1),
                     draw_flags, ch, draw_color, font);
        x += CONCHAR_WIDTH * max(scale, 1);
    }

    return x;
}

const kfont_char_t *SCR_KFontLookup(const kfont_t *kfont, uint32_t codepoint)
{
    if (!kfont)
        return NULL;

    if (codepoint < KFONT_ASCII_MIN || codepoint > KFONT_ASCII_MAX)
        return NULL;

    const kfont_char_t *ch = &kfont->chars[codepoint - KFONT_ASCII_MIN];

    if (!ch->w)
        return NULL;

    return ch;
}

void SCR_LoadKFont(kfont_t *font, const char *filename)
{
    static cvar_t *cl_debug_fonts;
    if (!cl_debug_fonts)
        cl_debug_fonts = Cvar_Get("cl_debug_fonts", "1", 0);
    const bool debug_fonts = cl_debug_fonts && cl_debug_fonts->integer;

    memset(font, 0, sizeof(*font));

    char *buffer;

    if (FS_LoadFile(filename, (void **) &buffer) < 0) {
        if (debug_fonts) {
            Com_LPrintf(PRINT_ALL, "Font: SCR_LoadKFont \"%s\" failed: %s\n",
                        filename ? filename : "<null>", Com_GetLastError());
        }
        return;
    }

    if (debug_fonts) {
        Com_LPrintf(PRINT_ALL, "Font: SCR_LoadKFont \"%s\"\n",
                    filename ? filename : "<null>");
    }

    const char *data = buffer;

    while (true) {
        const char *token = COM_Parse(&data);

        if (!*token)
            break;

        if (!strcmp(token, "texture")) {
            token = COM_Parse(&data);
            font->pic = R_RegisterFont(va("/%s", token));
            if (debug_fonts) {
                Com_LPrintf(PRINT_ALL, "Font: kfont texture \"%s\" handle=%d\n",
                            token ? token : "<null>", font->pic);
            }
        } else if (!strcmp(token, "unicode")) {
        } else if (!strcmp(token, "mapchar")) {
            token = COM_Parse(&data);

            while (true) {
                token = COM_Parse(&data);

                if (!strcmp(token, "}"))
                    break;

                uint32_t codepoint = strtoul(token, NULL, 10);
                uint32_t x = strtoul(COM_Parse(&data), NULL, 10);
                uint32_t y = strtoul(COM_Parse(&data), NULL, 10);
                uint32_t w = strtoul(COM_Parse(&data), NULL, 10);
                uint32_t h = strtoul(COM_Parse(&data), NULL, 10);
                COM_Parse(&data);

                codepoint -= KFONT_ASCII_MIN;

                if (codepoint < KFONT_ASCII_MAX) {
                    font->chars[codepoint].x = x;
                    font->chars[codepoint].y = y;
                    font->chars[codepoint].w = w;
                    font->chars[codepoint].h = h;

                    font->line_height = max(font->line_height, h);
                }
            }
        }
    }

    int pic_w = 0;
    int pic_h = 0;
    if (font->pic && R_GetPicSize(&pic_w, &pic_h, font->pic) && pic_w > 0 && pic_h > 0) {
        font->sw = 1.0f / pic_w;
        font->sh = 1.0f / pic_h;
    }

    FS_FreeFile(buffer);

    if (debug_fonts) {
        Com_LPrintf(PRINT_ALL, "Font: kfont \"%s\" loaded line_height=%d handle=%d\n",
                    filename ? filename : "<null>", font->line_height, font->pic);
    }
}

int R_DrawKFontChar(int x, int y, int scale, int flags, uint32_t codepoint, color_t color, const kfont_t *kfont)
{
    const kfont_char_t *ch = SCR_KFontLookup(kfont, codepoint);
    if (!ch || !kfont || !kfont->pic || kfont->sw <= 0.0f || kfont->sh <= 0.0f)
        return 0;

    int draw_scale = max(scale, 1);
    int w = ch->w * draw_scale;
    int h = ch->h * draw_scale;

    float s = ch->x * kfont->sw;
    float t = ch->y * kfont->sh;
    float sw = ch->w * kfont->sw;
    float sh = ch->h * kfont->sh;

    if (flags & UI_DROPSHADOW) {
        color_t shadow = COLOR_A(color.a);
        int shadow_offset = draw_scale;
        VK_UI_DrawStretchSubPic(x + shadow_offset, y + shadow_offset, w, h,
                                s, t, s + sw, t + sh,
                                shadow, kfont->pic);
    }

    VK_UI_DrawStretchSubPic(x, y, w, h, s, t, s + sw, t + sh, color, kfont->pic);
    return ch->w * draw_scale;
}

bool R_GetPicSize(int *w, int *h, qhandle_t pic)
{
    return VK_UI_GetPicSize(w, h, pic);
}

void R_DrawPic(int x, int y, color_t color, qhandle_t pic)
{
    VK_UI_DrawPic(x, y, color, pic);
}

void R_DrawStretchPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    VK_UI_DrawStretchPic(x, y, w, h, color, pic);
}

void R_DrawStretchSubPic(int x, int y, int w, int h,
                         float s1, float t1, float s2, float t2,
                         color_t color, qhandle_t pic)
{
    VK_UI_DrawStretchSubPic(x, y, w, h, s1, t1, s2, t2, color, pic);
}

void R_DrawStretchRotatePic(int x, int y, int w, int h, color_t color, float angle,
                            int pivot_x, int pivot_y, qhandle_t pic)
{
    VK_UI_DrawStretchRotatePic(x, y, w, h, color, angle, pivot_x, pivot_y, pic);
}

void R_DrawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    VK_UI_DrawKeepAspectPic(x, y, w, h, color, pic);
}

void R_DrawStretchRaw(int x, int y, int w, int h)
{
    VK_UI_DrawStretchRaw(x, y, w, h);
}

void R_UpdateRawPic(int pic_w, int pic_h, const uint32_t *pic)
{
    VK_UI_UpdateRawPic(pic_w, pic_h, pic);
}

bool R_UpdateImageRGBA(qhandle_t handle, int width, int height, const byte *pic)
{
    return VK_UI_UpdateImageRGBA(handle, width, height, pic);
}

void R_TileClear(int x, int y, int w, int h, qhandle_t pic)
{
    VK_UI_TileClear(x, y, w, h, pic);
}

void R_DrawFill8(int x, int y, int w, int h, int c)
{
    VK_UI_DrawFill8(x, y, w, h, c);
}

void R_DrawFill32(int x, int y, int w, int h, color_t color)
{
    VK_UI_DrawFill32(x, y, w, h, color);
}

void R_BeginFrame(void)
{
    if (!vk_state.initialized)
        return;

    if (vk_state.swapchain_dirty) {
        if (!VK_RecreateSwapchain(r_config.width, r_config.height))
            return;
    }

    if (!VK_WaitForSubmittedFrame(&vk_state.ctx, "vkWaitForFences(begin frame)")) {
        return;
    }

    VK_UI_BeginFrame();
}

void R_EndFrame(void)
{
    if (!vk_state.initialized)
        return;

    if (vk_state.swapchain_dirty) {
        if (!VK_RecreateSwapchain(r_config.width, r_config.height))
            return;
    }

    VK_UI_EndFrame();
    if (!VK_DrawFrame()) {
        const char *error = Com_GetLastError();
        if (error && *error) {
            Com_WPrintf("Vulkan: frame submission failed: %s\n", error);
        }
    }
}

void R_ModeChanged(int width, int height, int flags)
{
    r_config.width = width;
    r_config.height = height;
    r_config.flags = flags;

    if (!vk_state.initialized)
        return;

    if (vk_state.ctx.device) {
        vkDeviceWaitIdle(vk_state.ctx.device);
    }

    VK_DestroySwapchain(&vk_state.ctx);

    if (vk_state.ctx.surface) {
        vkDestroySurfaceKHR(vk_state.ctx.instance, vk_state.ctx.surface, NULL);
        vk_state.ctx.surface = VK_NULL_HANDLE;
    }

    vid_native_window_t native;
    if (!VK_QueryNativeWindow(&native)) {
        vk_state.swapchain_dirty = true;
        return;
    }

    vk_state.native_window = native;

    if (!VK_CreateSurface()) {
        vk_state.swapchain_dirty = true;
        return;
    }

    if (!VK_CreateSwapchain(width, height)) {
        vk_state.swapchain_dirty = true;
        return;
    }

    if (!VK_World_CreateSwapchainResources(&vk_state.ctx)) {
        VK_DestroySwapchain(&vk_state.ctx);
        vk_state.swapchain_dirty = true;
        return;
    }

    if (!VK_Entity_CreateSwapchainResources(&vk_state.ctx)) {
        VK_DestroySwapchain(&vk_state.ctx);
        vk_state.swapchain_dirty = true;
        return;
    }

    if (!VK_UI_CreateSwapchainResources(&vk_state.ctx)) {
        VK_DestroySwapchain(&vk_state.ctx);
        vk_state.swapchain_dirty = true;
        return;
    }

    vk_state.swapchain_dirty = false;
}

bool R_VideoSync(void)
{
    return true;
}

void GL_ExpireDebugObjects(void)
{
}

bool R_SupportsPerPixelLighting(void)
{
    return false;
}

r_opengl_config_t R_GetGLConfig(void)
{
    r_opengl_config_t cfg = {
        .colorbits = 24,
        .depthbits = 24,
        .stencilbits = 8,
        .multisamples = 0,
        .debug = false,
        .profile = QGL_PROFILE_NONE,
        .major_ver = 0,
        .minor_ver = 0,
    };

    return cfg;
}

void R_ClearDebugLines(void)
{
}

void R_AddDebugLine(const vec3_t start, const vec3_t end, color_t color, uint32_t time,
                    qboolean depth_test)
{
    (void)start;
    (void)end;
    (void)color;
    (void)time;
    (void)depth_test;
}

void R_AddDebugPoint(const vec3_t point, float size, color_t color, uint32_t time,
                     qboolean depth_test)
{
    (void)point;
    (void)size;
    (void)color;
    (void)time;
    (void)depth_test;
}

void R_AddDebugAxis(const vec3_t origin, const vec3_t angles, float size, uint32_t time,
                    qboolean depth_test)
{
    (void)origin;
    (void)angles;
    (void)size;
    (void)time;
    (void)depth_test;
}

void R_AddDebugBounds(const vec3_t mins, const vec3_t maxs, color_t color, uint32_t time,
                      qboolean depth_test)
{
    (void)mins;
    (void)maxs;
    (void)color;
    (void)time;
    (void)depth_test;
}

void R_AddDebugSphere(const vec3_t origin, float radius, color_t color, uint32_t time,
                      qboolean depth_test)
{
    (void)origin;
    (void)radius;
    (void)color;
    (void)time;
    (void)depth_test;
}

void R_AddDebugCircle(const vec3_t origin, float radius, color_t color, uint32_t time,
                      qboolean depth_test)
{
    (void)origin;
    (void)radius;
    (void)color;
    (void)time;
    (void)depth_test;
}

void R_AddDebugCylinder(const vec3_t origin, float half_height, float radius, color_t color,
                        uint32_t time, qboolean depth_test)
{
    (void)origin;
    (void)half_height;
    (void)radius;
    (void)color;
    (void)time;
    (void)depth_test;
}

void R_DrawArrowCap(const vec3_t apex, const vec3_t dir, float size, color_t color,
                    uint32_t time, qboolean depth_test)
{
    (void)apex;
    (void)dir;
    (void)size;
    (void)color;
    (void)time;
    (void)depth_test;
}

void R_AddDebugArrow(const vec3_t start, const vec3_t end, float size, color_t line_color,
                     color_t arrow_color, uint32_t time, qboolean depth_test)
{
    (void)start;
    (void)end;
    (void)size;
    (void)line_color;
    (void)arrow_color;
    (void)time;
    (void)depth_test;
}

void R_AddDebugCurveArrow(const vec3_t start, const vec3_t ctrl, const vec3_t end, float size,
                          color_t line_color, color_t arrow_color, uint32_t time, qboolean depth_test)
{
    (void)start;
    (void)ctrl;
    (void)end;
    (void)size;
    (void)line_color;
    (void)arrow_color;
    (void)time;
    (void)depth_test;
}

void R_AddDebugText(const vec3_t origin, const vec3_t angles, const char *text, float size,
                    color_t color, uint32_t time, qboolean depth_test)
{
    (void)origin;
    (void)angles;
    (void)text;
    (void)size;
    (void)color;
    (void)time;
    (void)depth_test;
}
