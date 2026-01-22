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

#include <string.h>

vk_state_t vk_state;
refcfg_t r_config;
uint32_t d_8to24table[256];

static clipRect_t vk_cliprect;
static float vk_scale = 1.0f;
static qhandle_t vk_next_model = 1;
static qhandle_t vk_next_image = 1;

static void VK_InitPalette(void)
{
    for (int i = 0; i < 255; i++)
        d_8to24table[i] = COLOR_RGB(i, i, i).u32;

    d_8to24table[255] = COLOR_RGBA(0, 0, 0, 0).u32;
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

    if (ctx->command_pool) {
        vkDestroyCommandPool(ctx->device, ctx->command_pool, NULL);
        ctx->command_pool = VK_NULL_HANDLE;
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

    VkAttachmentDescription color_attachment = {
        .format = ctx->swapchain.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    result = vkCreateRenderPass(ctx->device, &render_pass_info, NULL, &ctx->render_pass);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkCreateRenderPass");
    }

    ctx->swapchain.framebuffers = Z_TagMallocz(sizeof(VkFramebuffer) * image_count,
                                               TAG_RENDERER);

    for (uint32_t i = 0; i < image_count; ++i) {
        VkImageView attachments[] = { ctx->swapchain.views[i] };
        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = ctx->render_pass,
            .attachmentCount = 1,
            .pAttachments = attachments,
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

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ctx->graphics_queue_family,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };

    result = vkCreateCommandPool(ctx->device, &pool_info, NULL, &ctx->command_pool);
    if (result != VK_SUCCESS) {
        VK_DestroySwapchain(ctx);
        return VK_Check(result, "vkCreateCommandPool");
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

    VkClearValue clear_value = {
        .color = { { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    VkRenderPassBeginInfo render_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx->render_pass,
        .framebuffer = ctx->swapchain.framebuffers[image_index],
        .renderArea = {
            .offset = { 0, 0 },
            .extent = ctx->swapchain.extent,
        },
        .clearValueCount = 1,
        .pClearValues = &clear_value,
    };

    vkCmdBeginRenderPass(cmd, &render_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(cmd);

    result = vkEndCommandBuffer(cmd);
    return VK_Check(result, "vkEndCommandBuffer");
}

static bool VK_DrawFrame(void)
{
    vk_context_t *ctx = &vk_state.ctx;

    if (!ctx->swapchain.handle)
        return false;

    VkResult result = vkWaitForFences(ctx->device, 1, &ctx->in_flight_fence, VK_TRUE,
                                      UINT64_MAX);
    if (result != VK_SUCCESS) {
        return VK_Check(result, "vkWaitForFences");
    }

    result = vkResetFences(ctx->device, 1, &ctx->in_flight_fence);
    if (result != VK_SUCCESS) {
        return VK_Check(result, "vkResetFences");
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

    result = vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, ctx->in_flight_fence);
    if (result != VK_SUCCESS) {
        return VK_Check(result, "vkQueueSubmit");
    }

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

    VK_DestroySwapchain(ctx);

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
    (void)map;
}

qhandle_t R_RegisterModel(const char *name)
{
    (void)name;
    return vk_next_model++;
}

qhandle_t R_RegisterImage(const char *name, imagetype_t type, imageflags_t flags)
{
    (void)name;
    (void)type;
    (void)flags;
    return vk_next_image++;
}

void R_SetSky(const char *name, float rotate, bool autorotate, const vec3_t axis)
{
    (void)name;
    (void)rotate;
    (void)autorotate;
    (void)axis;
}

void R_EndRegistration(void)
{
}

void R_RenderFrame(const refdef_t *fd)
{
    (void)fd;
}

void R_LightPoint(const vec3_t origin, vec3_t light)
{
    VectorClear(light);
    (void)origin;
}

void R_SetClipRect(const clipRect_t *clip)
{
    if (clip) {
        vk_cliprect = *clip;
    } else {
        memset(&vk_cliprect, 0, sizeof(vk_cliprect));
    }
}

float R_ClampScale(cvar_t *var)
{
    if (!var) {
        return 1.0f;
    }

    if (var->value) {
        return 1.0f / Cvar_ClampValue(var, 1.0f, 10.0f);
    }

    return 1.0f;
}

void R_SetScale(float scale)
{
    vk_scale = scale;
}

static inline bool vk_is_color_escape(char c)
{
    if (c >= '0' && c <= '7')
        return true;
    if (c >= 'a' && c <= 'z')
        return true;
    if (c >= 'A' && c <= 'Z')
        return true;
    return false;
}

static size_t vk_strlen_no_color(const char *string, size_t maxlen)
{
    if (!string || !*string)
        return 0;

    size_t remaining = maxlen;
    size_t len = 0;
    const char *s = string;
    while (remaining && *s) {
        if (remaining >= 2 && *s == '^' && vk_is_color_escape(s[1])) {
            s += 2;
            remaining -= 2;
            continue;
        }
        ++s;
        --remaining;
        ++len;
    }

    return len;
}

void R_DrawChar(int x, int y, int flags, int ch, color_t color, qhandle_t font)
{
    (void)x;
    (void)y;
    (void)flags;
    (void)ch;
    (void)color;
    (void)font;
}

void R_DrawStretchChar(int x, int y, int w, int h, int flags, int ch, color_t color, qhandle_t font)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)flags;
    (void)ch;
    (void)color;
    (void)font;
}

int R_DrawStringStretch(int x, int y, int scale, int flags, size_t maxChars,
                        const char *string, color_t color, qhandle_t font)
{
    (void)y;
    (void)flags;
    (void)color;
    (void)font;

    if (!string) {
        return x;
    }

    size_t len = strlen(string);
    if (maxChars && len > maxChars)
        len = maxChars;

    size_t visible_len = vk_strlen_no_color(string, len);
    return x + (int)(visible_len * 8 * max(scale, 1));
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
    static cvar_t *cl_debugFonts;
    if (!cl_debugFonts)
        cl_debugFonts = Cvar_Get("cl_debugFonts", "1", 0);
    const bool debug_fonts = cl_debugFonts && cl_debugFonts->integer;

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

    FS_FreeFile(buffer);

    if (debug_fonts) {
        Com_LPrintf(PRINT_ALL, "Font: kfont \"%s\" loaded line_height=%d handle=%d\n",
                    filename ? filename : "<null>", font->line_height, font->pic);
    }
}

int R_DrawKFontChar(int x, int y, int scale, int flags, uint32_t codepoint, color_t color, const kfont_t *kfont)
{
    (void)x;
    (void)y;
    (void)flags;
    (void)color;

    const kfont_char_t *ch = SCR_KFontLookup(kfont, codepoint);
    if (!ch)
        return 0;

    return ch->w * max(scale, 1);
}

bool R_GetPicSize(int *w, int *h, qhandle_t pic)
{
    (void)pic;
    if (w)
        *w = 0;
    if (h)
        *h = 0;
    return false;
}

void R_DrawPic(int x, int y, color_t color, qhandle_t pic)
{
    (void)x;
    (void)y;
    (void)color;
    (void)pic;
}

void R_DrawStretchPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)color;
    (void)pic;
}

void R_DrawStretchRotatePic(int x, int y, int w, int h, color_t color, float angle,
                            int pivot_x, int pivot_y, qhandle_t pic)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)color;
    (void)angle;
    (void)pivot_x;
    (void)pivot_y;
    (void)pic;
}

void R_DrawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)color;
    (void)pic;
}

void R_DrawStretchRaw(int x, int y, int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

void R_UpdateRawPic(int pic_w, int pic_h, const uint32_t *pic)
{
    (void)pic_w;
    (void)pic_h;
    (void)pic;
}

void R_TileClear(int x, int y, int w, int h, qhandle_t pic)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)pic;
}

void R_DrawFill8(int x, int y, int w, int h, int c)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)c;
}

void R_DrawFill32(int x, int y, int w, int h, color_t color)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)color;
}

void R_BeginFrame(void)
{
    if (!vk_state.initialized)
        return;

    if (vk_state.swapchain_dirty) {
        VK_RecreateSwapchain(r_config.width, r_config.height);
    }
}

void R_EndFrame(void)
{
    if (!vk_state.initialized)
        return;

    if (vk_state.swapchain_dirty) {
        if (!VK_RecreateSwapchain(r_config.width, r_config.height))
            return;
    }

    VK_DrawFrame();
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
