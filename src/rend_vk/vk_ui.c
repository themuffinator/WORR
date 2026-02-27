/*
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "vk_ui.h"

#include "renderer/ui_scale.h"
#include "renderer/dds.h"
#include "format/pcx.h"
#include "format/wal.h"
#include "refresh/stb/stb_image.h"
#include "vk_ui2d_spv.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define VK_UI_INITIAL_IMAGE_CAPACITY 128
#define VK_UI_INITIAL_DRAW_CAPACITY 2048
#define VK_UI_INITIAL_VERTEX_CAPACITY (VK_UI_INITIAL_DRAW_CAPACITY * 4)
#define VK_UI_INITIAL_INDEX_CAPACITY (VK_UI_INITIAL_DRAW_CAPACITY * 6)
#define VK_UI_BUFFER_GROWTH_FACTOR 2
#define VK_UI_MAX_TEXTURE_SIZE 4096

typedef struct {
    float pos[2];
    float uv[2];
    uint32_t color;
} vk_ui_vertex_t;

typedef struct {
    bool in_use;
    bool transparent;
    imagetype_t type;
    imageflags_t flags;
    int width;
    int height;
    char name[MAX_QPATH];

    VkImage image;
    VkDeviceMemory image_memory;
    VkImageView view;
    VkDescriptorSet descriptor_set;
} vk_ui_image_t;

typedef struct {
    uint32_t first_index;
    uint32_t index_count;
    VkDescriptorSet descriptor_set;
    VkRect2D scissor;
} vk_ui_draw_t;

typedef struct {
    vk_context_t *ctx;

    bool initialized;
    bool swapchain_ready;

    float base_scale;
    float virtual_width;
    float virtual_height;
    float scale;

    bool clip_enabled;
    clipRect_t clip_pixels;

    int registration_sequence;

    vk_ui_image_t *images;
    uint32_t image_capacity;

    qhandle_t white_image;
    qhandle_t missing_image;
    qhandle_t raw_image;

    VkSampler sampler_repeat;
    VkSampler sampler_clamp;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    void *vertex_mapped;
    size_t vertex_buffer_bytes;

    VkBuffer index_buffer;
    VkDeviceMemory index_memory;
    void *index_mapped;
    size_t index_buffer_bytes;

    vk_ui_vertex_t *vertices;
    uint32_t vertex_count;
    uint32_t vertex_capacity;

    uint32_t *indices;
    uint32_t index_count;
    uint32_t index_capacity;

    vk_ui_draw_t *draws;
    uint32_t draw_count;
    uint32_t draw_capacity;
} vk_ui_state_t;

static vk_ui_state_t vk_ui;
extern uint32_t d_8to24table[256];

static inline bool VK_UI_Check(VkResult result, const char *what)
{
    if (result == VK_SUCCESS) {
        return true;
    }

    Com_SetLastError(va("Vulkan UI %s failed: %d", what, (int)result));
    return false;
}

static inline VkDevice VK_UI_Device(void)
{
    return vk_ui.ctx ? vk_ui.ctx->device : VK_NULL_HANDLE;
}

static void VK_UI_RefreshVirtualMetrics(void)
{
    renderer_ui_scale_t metrics = R_UIScaleCompute(r_config.width, r_config.height);
    vk_ui.base_scale = metrics.base_scale;
    vk_ui.virtual_width = metrics.virtual_width;
    vk_ui.virtual_height = metrics.virtual_height;

    if (vk_ui.scale <= 0.0f) {
        vk_ui.scale = 1.0f;
    }
}

static uint32_t VK_UI_FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(vk_ui.ctx->physical_device, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & BIT(i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return UINT32_MAX;
}

static void VK_UI_DestroyBuffer(VkBuffer *buffer, VkDeviceMemory *memory, void **mapped)
{
    VkDevice device = VK_UI_Device();

    if (mapped && *mapped) {
        vkUnmapMemory(device, *memory);
        *mapped = NULL;
    }

    if (buffer && *buffer) {
        vkDestroyBuffer(device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
    }

    if (memory && *memory) {
        vkFreeMemory(device, *memory, NULL);
        *memory = VK_NULL_HANDLE;
    }
}

static bool VK_UI_CreateBuffer(size_t size, VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties,
                               VkBuffer *out_buffer, VkDeviceMemory *out_memory,
                               void **out_mapped)
{
    VkDevice device = VK_UI_Device();

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (!VK_UI_Check(vkCreateBuffer(device, &buffer_info, NULL, out_buffer),
                     "vkCreateBuffer")) {
        return false;
    }

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, *out_buffer, &requirements);

    uint32_t memory_index = VK_UI_FindMemoryType(requirements.memoryTypeBits, properties);
    if (memory_index == UINT32_MAX) {
        Com_SetLastError("Vulkan UI: suitable buffer memory type not found");
        vkDestroyBuffer(device, *out_buffer, NULL);
        *out_buffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memory_index,
    };

    if (!VK_UI_Check(vkAllocateMemory(device, &alloc_info, NULL, out_memory),
                     "vkAllocateMemory")) {
        vkDestroyBuffer(device, *out_buffer, NULL);
        *out_buffer = VK_NULL_HANDLE;
        return false;
    }

    if (!VK_UI_Check(vkBindBufferMemory(device, *out_buffer, *out_memory, 0),
                     "vkBindBufferMemory")) {
        vkDestroyBuffer(device, *out_buffer, NULL);
        vkFreeMemory(device, *out_memory, NULL);
        *out_buffer = VK_NULL_HANDLE;
        *out_memory = VK_NULL_HANDLE;
        return false;
    }

    if (out_mapped) {
        if (!VK_UI_Check(vkMapMemory(device, *out_memory, 0, size, 0, out_mapped),
                         "vkMapMemory")) {
            vkDestroyBuffer(device, *out_buffer, NULL);
            vkFreeMemory(device, *out_memory, NULL);
            *out_buffer = VK_NULL_HANDLE;
            *out_memory = VK_NULL_HANDLE;
            *out_mapped = NULL;
            return false;
        }
    }

    return true;
}

static bool VK_UI_EnsureDrawCapacity(uint32_t needed_vertices,
                                     uint32_t needed_indices,
                                     uint32_t needed_draws)
{
    if (needed_vertices > vk_ui.vertex_capacity) {
        uint32_t new_capacity = vk_ui.vertex_capacity ? vk_ui.vertex_capacity : VK_UI_INITIAL_VERTEX_CAPACITY;
        while (new_capacity < needed_vertices) {
            new_capacity *= VK_UI_BUFFER_GROWTH_FACTOR;
        }

        void *new_vertices = realloc(vk_ui.vertices, sizeof(*vk_ui.vertices) * new_capacity);
        if (!new_vertices) {
            Com_SetLastError("Vulkan UI: out of memory for vertices");
            return false;
        }

        vk_ui.vertices = new_vertices;
        vk_ui.vertex_capacity = new_capacity;
    }

    if (needed_indices > vk_ui.index_capacity) {
        uint32_t new_capacity = vk_ui.index_capacity ? vk_ui.index_capacity : VK_UI_INITIAL_INDEX_CAPACITY;
        while (new_capacity < needed_indices) {
            new_capacity *= VK_UI_BUFFER_GROWTH_FACTOR;
        }

        void *new_indices = realloc(vk_ui.indices, sizeof(*vk_ui.indices) * new_capacity);
        if (!new_indices) {
            Com_SetLastError("Vulkan UI: out of memory for indices");
            return false;
        }

        vk_ui.indices = new_indices;
        vk_ui.index_capacity = new_capacity;
    }

    if (needed_draws > vk_ui.draw_capacity) {
        uint32_t new_capacity = vk_ui.draw_capacity ? vk_ui.draw_capacity : VK_UI_INITIAL_DRAW_CAPACITY;
        while (new_capacity < needed_draws) {
            new_capacity *= VK_UI_BUFFER_GROWTH_FACTOR;
        }

        void *new_draws = realloc(vk_ui.draws, sizeof(*vk_ui.draws) * new_capacity);
        if (!new_draws) {
            Com_SetLastError("Vulkan UI: out of memory for draw commands");
            return false;
        }

        vk_ui.draws = new_draws;
        vk_ui.draw_capacity = new_capacity;
    }

    return true;
}

static bool VK_UI_EnsureHostBuffers(void)
{
    if (!VK_UI_EnsureDrawCapacity(VK_UI_INITIAL_VERTEX_CAPACITY,
                                  VK_UI_INITIAL_INDEX_CAPACITY,
                                  VK_UI_INITIAL_DRAW_CAPACITY)) {
        return false;
    }

    return true;
}

static bool VK_UI_EnsureGpuBuffers(void)
{
    size_t needed_vertex_bytes = sizeof(*vk_ui.vertices) * vk_ui.vertex_capacity;
    size_t needed_index_bytes = sizeof(*vk_ui.indices) * vk_ui.index_capacity;

    if (needed_vertex_bytes > vk_ui.vertex_buffer_bytes) {
        VK_UI_DestroyBuffer(&vk_ui.vertex_buffer, &vk_ui.vertex_memory, &vk_ui.vertex_mapped);

        if (!VK_UI_CreateBuffer(needed_vertex_bytes,
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &vk_ui.vertex_buffer,
                                &vk_ui.vertex_memory,
                                &vk_ui.vertex_mapped)) {
            return false;
        }

        vk_ui.vertex_buffer_bytes = needed_vertex_bytes;
    }

    if (needed_index_bytes > vk_ui.index_buffer_bytes) {
        VK_UI_DestroyBuffer(&vk_ui.index_buffer, &vk_ui.index_memory, &vk_ui.index_mapped);

        if (!VK_UI_CreateBuffer(needed_index_bytes,
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &vk_ui.index_buffer,
                                &vk_ui.index_memory,
                                &vk_ui.index_mapped)) {
            return false;
        }

        vk_ui.index_buffer_bytes = needed_index_bytes;
    }

    return true;
}

static void VK_UI_DestroyImageResources(vk_ui_image_t *image)
{
    if (!image || !vk_ui.ctx || !vk_ui.ctx->device) {
        return;
    }

    VkDevice device = vk_ui.ctx->device;

    if (vk_ui.descriptor_pool && vk_ui.descriptor_set_layout && image->descriptor_set) {
        vkFreeDescriptorSets(device, vk_ui.descriptor_pool, 1, &image->descriptor_set);
        image->descriptor_set = VK_NULL_HANDLE;
    }

    if (image->view) {
        vkDestroyImageView(device, image->view, NULL);
        image->view = VK_NULL_HANDLE;
    }

    if (image->image) {
        vkDestroyImage(device, image->image, NULL);
        image->image = VK_NULL_HANDLE;
    }

    if (image->image_memory) {
        vkFreeMemory(device, image->image_memory, NULL);
        image->image_memory = VK_NULL_HANDLE;
    }
}

static vk_ui_image_t *VK_UI_ImageForHandle(qhandle_t handle)
{
    if (handle <= 0 || (uint32_t)handle >= vk_ui.image_capacity) {
        return NULL;
    }

    vk_ui_image_t *image = &vk_ui.images[handle];
    if (!image->in_use) {
        return NULL;
    }

    return image;
}

static bool VK_UI_EnsureImageCapacity(uint32_t needed)
{
    if (needed < vk_ui.image_capacity) {
        return true;
    }

    uint32_t new_capacity = vk_ui.image_capacity ? vk_ui.image_capacity : VK_UI_INITIAL_IMAGE_CAPACITY;
    while (new_capacity <= needed) {
        new_capacity *= VK_UI_BUFFER_GROWTH_FACTOR;
    }

    size_t new_size = sizeof(*vk_ui.images) * new_capacity;
    void *new_images = realloc(vk_ui.images, new_size);
    if (!new_images) {
        Com_SetLastError("Vulkan UI: out of memory for image handles");
        return false;
    }

    size_t old_size = sizeof(*vk_ui.images) * vk_ui.image_capacity;
    memset((byte *)new_images + old_size, 0, new_size - old_size);

    vk_ui.images = new_images;
    vk_ui.image_capacity = new_capacity;

    return true;
}

static vk_ui_image_t *VK_UI_AllocImageSlot(qhandle_t *out_handle)
{
    for (uint32_t i = 1; i < vk_ui.image_capacity; ++i) {
        if (!vk_ui.images[i].in_use) {
            vk_ui.images[i].in_use = true;
            if (out_handle) {
                *out_handle = (qhandle_t)i;
            }
            return &vk_ui.images[i];
        }
    }

    uint32_t handle = vk_ui.image_capacity;
    if (!VK_UI_EnsureImageCapacity(handle)) {
        return NULL;
    }

    vk_ui.images[handle].in_use = true;

    if (out_handle) {
        *out_handle = (qhandle_t)handle;
    }

    return &vk_ui.images[handle];
}

static vk_ui_image_t *VK_UI_FindImageByName(const char *name, imagetype_t type)
{
    if (!name || !*name) {
        return NULL;
    }

    for (uint32_t i = 1; i < vk_ui.image_capacity; ++i) {
        vk_ui_image_t *image = &vk_ui.images[i];
        if (!image->in_use) {
            continue;
        }

        if (image->type != type) {
            continue;
        }

        if (!strcmp(image->name, name)) {
            return image;
        }
    }

    return NULL;
}

static bool VK_UI_BeginImmediate(VkCommandBuffer *out_cmd)
{
    if (!vk_ui.ctx || !vk_ui.ctx->command_pool) {
        Com_SetLastError("Vulkan UI: command pool is not available");
        return false;
    }

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk_ui.ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    if (!VK_UI_Check(vkAllocateCommandBuffers(vk_ui.ctx->device, &alloc_info, out_cmd),
                     "vkAllocateCommandBuffers")) {
        return false;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (!VK_UI_Check(vkBeginCommandBuffer(*out_cmd, &begin_info), "vkBeginCommandBuffer")) {
        vkFreeCommandBuffers(vk_ui.ctx->device, vk_ui.ctx->command_pool, 1, out_cmd);
        *out_cmd = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

static bool VK_UI_EndImmediate(VkCommandBuffer cmd)
{
    if (!VK_UI_Check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer")) {
        vkFreeCommandBuffers(vk_ui.ctx->device, vk_ui.ctx->command_pool, 1, &cmd);
        return false;
    }

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };

    if (!VK_UI_Check(vkQueueSubmit(vk_ui.ctx->graphics_queue, 1, &submit_info, VK_NULL_HANDLE),
                     "vkQueueSubmit")) {
        vkFreeCommandBuffers(vk_ui.ctx->device, vk_ui.ctx->command_pool, 1, &cmd);
        return false;
    }

    if (!VK_UI_Check(vkQueueWaitIdle(vk_ui.ctx->graphics_queue), "vkQueueWaitIdle")) {
        vkFreeCommandBuffers(vk_ui.ctx->device, vk_ui.ctx->command_pool, 1, &cmd);
        return false;
    }

    vkFreeCommandBuffers(vk_ui.ctx->device, vk_ui.ctx->command_pool, 1, &cmd);
    return true;
}

static bool VK_UI_CreateImageStorage(vk_ui_image_t *image, int width, int height)
{
    VkDevice device = VK_UI_Device();

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {
            .width = (uint32_t)width,
            .height = (uint32_t)height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (!VK_UI_Check(vkCreateImage(device, &image_info, NULL, &image->image),
                     "vkCreateImage")) {
        return false;
    }

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, image->image, &requirements);

    uint32_t memory_index = VK_UI_FindMemoryType(requirements.memoryTypeBits,
                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_index == UINT32_MAX) {
        Com_SetLastError("Vulkan UI: suitable image memory type not found");
        vkDestroyImage(device, image->image, NULL);
        image->image = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memory_index,
    };

    if (!VK_UI_Check(vkAllocateMemory(device, &alloc_info, NULL, &image->image_memory),
                     "vkAllocateMemory")) {
        vkDestroyImage(device, image->image, NULL);
        image->image = VK_NULL_HANDLE;
        return false;
    }

    if (!VK_UI_Check(vkBindImageMemory(device, image->image, image->image_memory, 0),
                     "vkBindImageMemory")) {
        vkDestroyImage(device, image->image, NULL);
        vkFreeMemory(device, image->image_memory, NULL);
        image->image = VK_NULL_HANDLE;
        image->image_memory = VK_NULL_HANDLE;
        return false;
    }

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    if (!VK_UI_Check(vkCreateImageView(device, &view_info, NULL, &image->view),
                     "vkCreateImageView")) {
        vkDestroyImage(device, image->image, NULL);
        vkFreeMemory(device, image->image_memory, NULL);
        image->image = VK_NULL_HANDLE;
        image->image_memory = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

static bool VK_UI_AllocDescriptorSet(vk_ui_image_t *image)
{
    if (image->descriptor_set) {
        return true;
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vk_ui.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &vk_ui.descriptor_set_layout,
    };

    if (!VK_UI_Check(vkAllocateDescriptorSets(vk_ui.ctx->device, &alloc_info, &image->descriptor_set),
                     "vkAllocateDescriptorSets")) {
        image->descriptor_set = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

static void VK_UI_UpdateDescriptorSet(vk_ui_image_t *image)
{
    VkSampler sampler = (image->flags & IF_REPEAT) ? vk_ui.sampler_repeat : vk_ui.sampler_clamp;
    VkDescriptorImageInfo image_info = {
        .sampler = sampler,
        .imageView = image->view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = image->descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info,
    };

    vkUpdateDescriptorSets(vk_ui.ctx->device, 1, &write, 0, NULL);
}

static bool VK_UI_UploadImageData(vk_ui_image_t *image, int width, int height,
                                  const byte *rgba, VkImageLayout old_layout)
{
    size_t pixel_size = (size_t)width * (size_t)height * 4;

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    void *staging_mapped = NULL;

    if (!VK_UI_CreateBuffer(pixel_size,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            &staging_buffer,
                            &staging_memory,
                            &staging_mapped)) {
        return false;
    }

    memcpy(staging_mapped, rgba, pixel_size);

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (!VK_UI_BeginImmediate(&cmd)) {
        VK_UI_DestroyBuffer(&staging_buffer, &staging_memory, &staging_mapped);
        return false;
    }

    VkImageMemoryBarrier to_transfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        to_transfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else {
        to_transfer.srcAccessMask = 0;
        to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    vkCmdPipelineBarrier(cmd,
                         old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                             ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                             : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0, NULL,
                         0, NULL,
                         1, &to_transfer);

    VkBufferImageCopy copy_region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = {
            .width = (uint32_t)width,
            .height = (uint32_t)height,
            .depth = 1,
        },
    };

    vkCmdCopyBufferToImage(cmd, staging_buffer, image->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &copy_region);

    VkImageMemoryBarrier to_shader = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    };

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0, NULL,
                         0, NULL,
                         1, &to_shader);

    bool ok = VK_UI_EndImmediate(cmd);
    VK_UI_DestroyBuffer(&staging_buffer, &staging_memory, &staging_mapped);

    if (!ok) {
        return false;
    }

    image->width = width;
    image->height = height;

    return true;
}

static bool VK_UI_UploadImageDataSubRect(vk_ui_image_t *image, int x, int y,
                                         int width, int height,
                                         const byte *rgba, VkImageLayout old_layout)
{
    size_t pixel_size = (size_t)width * (size_t)height * 4;

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    void *staging_mapped = NULL;

    if (!VK_UI_CreateBuffer(pixel_size,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            &staging_buffer,
                            &staging_memory,
                            &staging_mapped)) {
        return false;
    }

    memcpy(staging_mapped, rgba, pixel_size);

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (!VK_UI_BeginImmediate(&cmd)) {
        VK_UI_DestroyBuffer(&staging_buffer, &staging_memory, &staging_mapped);
        return false;
    }

    VkImageMemoryBarrier to_transfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        to_transfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else {
        to_transfer.srcAccessMask = 0;
        to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    vkCmdPipelineBarrier(cmd,
                         old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                             ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                             : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0, NULL,
                         0, NULL,
                         1, &to_transfer);

    VkBufferImageCopy copy_region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = { x, y, 0 },
        .imageExtent = {
            .width = (uint32_t)width,
            .height = (uint32_t)height,
            .depth = 1,
        },
    };

    vkCmdCopyBufferToImage(cmd, staging_buffer, image->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &copy_region);

    VkImageMemoryBarrier to_shader = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    };

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0, NULL,
                         0, NULL,
                         1, &to_shader);

    bool ok = VK_UI_EndImmediate(cmd);
    VK_UI_DestroyBuffer(&staging_buffer, &staging_memory, &staging_mapped);
    return ok;
}

static bool VK_UI_SetImagePixels(vk_ui_image_t *image, int width, int height, const byte *rgba)
{
    if (!image || !rgba || width <= 0 || height <= 0) {
        return false;
    }

    bool needs_recreate = (image->image == VK_NULL_HANDLE || image->view == VK_NULL_HANDLE ||
                           image->width != width || image->height != height);

    if (needs_recreate) {
        vkDeviceWaitIdle(vk_ui.ctx->device);
        VK_UI_DestroyImageResources(image);

        if (!VK_UI_CreateImageStorage(image, width, height)) {
            return false;
        }

        if (!VK_UI_AllocDescriptorSet(image)) {
            VK_UI_DestroyImageResources(image);
            return false;
        }

        if (!VK_UI_UploadImageData(image, width, height, rgba, VK_IMAGE_LAYOUT_UNDEFINED)) {
            VK_UI_DestroyImageResources(image);
            return false;
        }

        VK_UI_UpdateDescriptorSet(image);
        return true;
    }

    if (!VK_UI_UploadImageData(image, width, height, rgba, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
        return false;
    }

    VK_UI_UpdateDescriptorSet(image);
    return true;
}

static bool VK_UI_ImageHasTransparency(const byte *rgba, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (rgba[i * 4 + 3] != 255) {
            return true;
        }
    }

    return false;
}

static void VK_UI_Unpack8ToRgba(uint32_t *out, const uint8_t *in, int width, int height)
{
    int x, y, p;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            p = *in;
            if (p == 255) {
                // Transparent index; borrow nearby color to avoid fringes.
                if (y > 0 && *(in - width) != 255) {
                    p = *(in - width);
                } else if (y < height - 1 && *(in + width) != 255) {
                    p = *(in + width);
                } else if (x > 0 && *(in - 1) != 255) {
                    p = *(in - 1);
                } else if (x < width - 1 && *(in + 1) != 255) {
                    p = *(in + 1);
                } else if (y > 0 && x > 0 && *(in - width - 1) != 255) {
                    p = *(in - width - 1);
                } else if (y > 0 && x < width - 1 && *(in - width + 1) != 255) {
                    p = *(in - width + 1);
                } else if (y < height - 1 && x > 0 && *(in + width - 1) != 255) {
                    p = *(in + width - 1);
                } else if (y < height - 1 && x < width - 1 && *(in + width + 1) != 255) {
                    p = *(in + width + 1);
                } else {
                    p = 0;
                }
                *out = d_8to24table[p] & COLOR_U32_RGBA(255, 255, 255, 0);
            } else {
                *out = d_8to24table[p];
            }

            in++;
            out++;
        }
    }
}

static bool VK_UI_LoadPcxRgba(const byte *raw_data, size_t raw_len,
                              int *out_w, int *out_h, byte **out_rgba)
{
    if (!raw_data || raw_len < sizeof(dpcx_t) || !out_w || !out_h || !out_rgba) {
        return false;
    }

    const dpcx_t *pcx = (const dpcx_t *)raw_data;
    if (pcx->manufacturer != 10 || pcx->version != 5 ||
        pcx->encoding != 1 || pcx->bits_per_pixel != 8 || pcx->color_planes != 1) {
        return false;
    }

    int width = (LittleShort(pcx->xmax) - LittleShort(pcx->xmin)) + 1;
    int height = (LittleShort(pcx->ymax) - LittleShort(pcx->ymin)) + 1;
    int scan = LittleShort(pcx->bytes_per_line);

    if (width < 1 || height < 1 || width > 640 || height > 480 || scan < width) {
        return false;
    }

    uint8_t *indexed = malloc((size_t)width * (size_t)height);
    if (!indexed) {
        return false;
    }

    const byte *raw = pcx->data;
    const byte *end = raw_data + raw_len;

    for (int y = 0; y < height; y++) {
        uint8_t *dst = indexed + (size_t)y * (size_t)width;
        for (int x = 0; x < scan;) {
            if (raw >= end) {
                free(indexed);
                return false;
            }

            int data_byte = *raw++;
            int run_length = 1;

            if ((data_byte & 0xC0) == 0xC0) {
                run_length = data_byte & 0x3F;
                if (x + run_length > scan || raw >= end) {
                    free(indexed);
                    return false;
                }
                data_byte = *raw++;
            }

            while (run_length--) {
                if (x < width) {
                    dst[x] = (uint8_t)data_byte;
                }
                x++;
            }
        }
    }

    size_t rgba_bytes = (size_t)width * (size_t)height * sizeof(uint32_t);
    uint32_t *rgba = malloc(rgba_bytes);
    if (!rgba) {
        free(indexed);
        return false;
    }

    VK_UI_Unpack8ToRgba(rgba, indexed, width, height);
    free(indexed);

    *out_w = width;
    *out_h = height;
    *out_rgba = (byte *)rgba;
    return true;
}

static bool VK_UI_LoadWalRgba(const byte *raw_data, size_t raw_len,
                              int *out_w, int *out_h, byte **out_rgba)
{
    if (!raw_data || raw_len < sizeof(miptex_t) || !out_w || !out_h || !out_rgba) {
        return false;
    }

    const miptex_t *mt = (const miptex_t *)raw_data;
    uint32_t width = LittleLong(mt->width);
    uint32_t height = LittleLong(mt->height);

    if (width < 1 || height < 1 || width > VK_UI_MAX_TEXTURE_SIZE || height > VK_UI_MAX_TEXTURE_SIZE) {
        return false;
    }

    uint32_t size = width * height;
    uint32_t offset = LittleLong(mt->offsets[0]);
    if (offset + size < offset || offset + size > raw_len) {
        return false;
    }

    size_t rgba_bytes = (size_t)size * sizeof(uint32_t);
    uint32_t *rgba = malloc(rgba_bytes);
    if (!rgba) {
        return false;
    }

    VK_UI_Unpack8ToRgba(rgba, (const uint8_t *)mt + offset, (int)width, (int)height);

    *out_w = (int)width;
    *out_h = (int)height;
    *out_rgba = (byte *)rgba;
    return true;
}

static const char *VK_UI_PathExtension(const char *path)
{
    if (!path || !*path) {
        return NULL;
    }

    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *sep = slash;
    if (!sep || (backslash && backslash > sep)) {
        sep = backslash;
    }

    if (!dot || (sep && dot < sep)) {
        return NULL;
    }

    return dot;
}

static bool VK_UI_LoadRgbaFromFile(const char *path, int *out_w, int *out_h, byte **out_rgba)
{
    if (!path || !*path || !out_w || !out_h || !out_rgba) {
        return false;
    }

    byte *file_data = NULL;
    int file_len = FS_LoadFile(path, (void **)&file_data);
    if (file_len < 0 || !file_data) {
        return false;
    }

    int width = 0;
    int height = 0;
    byte *native_rgba = NULL;

    const char *ext = VK_UI_PathExtension(path);
    if (ext && !Q_stricmp(ext, ".pcx")) {
        if (VK_UI_LoadPcxRgba(file_data, (size_t)file_len, &width, &height, &native_rgba)) {
            FS_FreeFile(file_data);
            *out_w = width;
            *out_h = height;
            *out_rgba = native_rgba;
            return true;
        }
    } else if (ext && !Q_stricmp(ext, ".wal")) {
        if (VK_UI_LoadWalRgba(file_data, (size_t)file_len, &width, &height, &native_rgba)) {
            FS_FreeFile(file_data);
            *out_w = width;
            *out_h = height;
            *out_rgba = native_rgba;
            return true;
        }
    } else if (ext && !Q_stricmp(ext, ".dds")) {
        bool has_alpha = false;
        int dds_ret = R_DecodeDDS(file_data, (size_t)file_len,
                                  &width, &height, &native_rgba,
                                  &has_alpha, malloc);
        if (dds_ret >= 0) {
            FS_FreeFile(file_data);
            *out_w = width;
            *out_h = height;
            *out_rgba = native_rgba;
            return true;
        }
    }

    int channels = 0;
    byte *decoded = stbi_load_from_memory(file_data, file_len, &width, &height, &channels, 4);
    FS_FreeFile(file_data);

    if (!decoded || width <= 0 || height <= 0) {
        if (decoded) {
            stbi_image_free(decoded);
        }
        return false;
    }

    size_t rgba_bytes = (size_t)width * (size_t)height * 4;
    byte *rgba_copy = malloc(rgba_bytes);
    if (!rgba_copy) {
        stbi_image_free(decoded);
        return false;
    }

    memcpy(rgba_copy, decoded, rgba_bytes);
    stbi_image_free(decoded);

    *out_w = width;
    *out_h = height;
    *out_rgba = rgba_copy;
    return true;
}

static bool VK_UI_NormalizeImagePath(char *out_name, size_t out_size,
                                     const char *name, imagetype_t type)
{
    if (!out_name || !out_size || !name || !*name) {
        return false;
    }

    size_t len = 0;

    if (type == IT_PIC || type == IT_FONT) {
        if (*name == '/' || *name == '\\') {
            len = FS_NormalizePathBuffer(out_name, name + 1, out_size);
        } else if (type == IT_FONT && (strchr(name, '/') || strchr(name, '\\'))) {
            len = FS_NormalizePathBuffer(out_name, name, out_size);
        } else {
            len = Q_concat(out_name, out_size, "pics/", name);
            if (len < out_size) {
                FS_NormalizePath(out_name);
                len = COM_DefaultExtension(out_name, ".pcx", out_size);
            }
        }
    } else {
        if (*name == '/' || *name == '\\') {
            len = FS_NormalizePathBuffer(out_name, name + 1, out_size);
        } else {
            len = FS_NormalizePathBuffer(out_name, name, out_size);
        }
    }

    if (len >= out_size) {
        return false;
    }

    return true;
}

static bool VK_UI_ReplaceExtension(char *path, size_t path_size, const char *ext)
{
    if (!path || !*path || !ext || !*ext) {
        return false;
    }

    char *dot = strrchr(path, '.');
    char *slash = strrchr(path, '/');
    char *backslash = strrchr(path, '\\');
    char *sep = slash;
    if (!sep || (backslash && backslash > sep)) {
        sep = backslash;
    }

    if (!dot || (sep && dot < sep)) {
        size_t len = strlen(path);
        if (len + strlen(ext) >= path_size) {
            return false;
        }
        Q_strlcat(path, ext, path_size);
        return true;
    }

    size_t base_len = (size_t)(dot - path);
    if (base_len + strlen(ext) >= path_size) {
        return false;
    }

    path[base_len] = '\0';
    Q_strlcat(path, ext, path_size);
    return true;
}

static bool VK_UI_LoadImageData(const char *normalized_name,
                                int *out_w, int *out_h, byte **out_rgba)
{
    if (VK_UI_LoadRgbaFromFile(normalized_name, out_w, out_h, out_rgba)) {
        return true;
    }

    const char *fallback_exts[] = {
        ".wal", ".dds", ".png", ".tga", ".jpg", ".jpeg", ".pcx"
    };

    char candidate[MAX_QPATH];
    for (size_t i = 0; i < q_countof(fallback_exts); ++i) {
        Q_strlcpy(candidate, normalized_name, sizeof(candidate));
        if (!VK_UI_ReplaceExtension(candidate, sizeof(candidate), fallback_exts[i])) {
            continue;
        }

        if (VK_UI_LoadRgbaFromFile(candidate, out_w, out_h, out_rgba)) {
            return true;
        }
    }

    return false;
}

static qhandle_t VK_UI_CreateImage(const char *name, imagetype_t type, imageflags_t flags,
                                   int width, int height, const byte *rgba)
{
    qhandle_t handle = 0;
    vk_ui_image_t *image = VK_UI_AllocImageSlot(&handle);
    if (!image) {
        return 0;
    }

    memset(image, 0, sizeof(*image));
    image->in_use = true;
    image->type = type;
    image->flags = flags;
    image->width = width;
    image->height = height;
    image->transparent = VK_UI_ImageHasTransparency(rgba, (size_t)width * (size_t)height);

    if (name) {
        Q_strlcpy(image->name, name, sizeof(image->name));
    }

    if (image->transparent) {
        image->flags |= IF_TRANSPARENT;
    } else {
        image->flags |= IF_OPAQUE;
    }

    if (!VK_UI_SetImagePixels(image, width, height, rgba)) {
        VK_UI_DestroyImageResources(image);
        memset(image, 0, sizeof(*image));
        return 0;
    }

    return handle;
}

static void VK_UI_EnsureDefaultImages(void)
{
    if (vk_ui.white_image && vk_ui.missing_image) {
        return;
    }

    static const byte white_rgba[4] = { 255, 255, 255, 255 };

    static const byte missing_rgba[4 * 4] = {
        255, 0, 255, 255,   0, 0, 0, 255,
        0, 0, 0, 255,       255, 0, 255, 255,
    };

    if (!vk_ui.white_image) {
        vk_ui.white_image = VK_UI_CreateImage("**white**", IT_PIC, IF_PERMANENT | IF_SPECIAL,
                                              1, 1, white_rgba);
    }

    if (!vk_ui.missing_image) {
        vk_ui.missing_image = VK_UI_CreateImage("**missing**", IT_PIC, IF_PERMANENT | IF_SPECIAL,
                                                2, 2, missing_rgba);
    }
}

static VkRect2D VK_UI_DefaultScissor(void)
{
    VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = {
            .width = (uint32_t)r_config.width,
            .height = (uint32_t)r_config.height,
        },
    };

    return scissor;
}

static VkRect2D VK_UI_CurrentScissor(void)
{
    if (!vk_ui.clip_enabled) {
        return VK_UI_DefaultScissor();
    }

    int left = max(0, vk_ui.clip_pixels.left);
    int top = max(0, vk_ui.clip_pixels.top);
    int right = min(r_config.width, vk_ui.clip_pixels.right);
    int bottom = min(r_config.height, vk_ui.clip_pixels.bottom);

    if (right <= left || bottom <= top) {
        VkRect2D empty = { .offset = { 0, 0 }, .extent = { 0, 0 } };
        return empty;
    }

    VkRect2D scissor = {
        .offset = { left, top },
        .extent = {
            .width = (uint32_t)(right - left),
            .height = (uint32_t)(bottom - top),
        },
    };

    return scissor;
}

static inline float VK_UI_VirtualWidthScaled(void)
{
    float width = vk_ui.virtual_width * vk_ui.scale;
    if (width <= 0.0f) {
        width = 1.0f;
    }
    return width;
}

static inline float VK_UI_VirtualHeightScaled(void)
{
    float height = vk_ui.virtual_height * vk_ui.scale;
    if (height <= 0.0f) {
        height = 1.0f;
    }
    return height;
}

static inline void VK_UI_ToNdc(float x, float y, float *out_x, float *out_y)
{
    float vw = VK_UI_VirtualWidthScaled();
    float vh = VK_UI_VirtualHeightScaled();

    *out_x = (x / vw) * 2.0f - 1.0f;
    *out_y = 1.0f - (y / vh) * 2.0f;
}

static void VK_UI_ResolvePic(qhandle_t *inout_pic)
{
    if (*inout_pic == 0) {
        *inout_pic = vk_ui.white_image;
    }

    vk_ui_image_t *image = VK_UI_ImageForHandle(*inout_pic);
    if (!image || !image->descriptor_set) {
        *inout_pic = vk_ui.missing_image ? vk_ui.missing_image : vk_ui.white_image;
    }
}

static bool VK_UI_EnqueueQuad(float x, float y, float w, float h,
                              float s1, float t1, float s2, float t2,
                              color_t color, qhandle_t pic)
{
    if (w == 0.0f || h == 0.0f) {
        return true;
    }

    VK_UI_EnsureDefaultImages();
    VK_UI_ResolvePic(&pic);

    vk_ui_image_t *image = VK_UI_ImageForHandle(pic);
    if (!image || !image->descriptor_set) {
        return false;
    }

    if (!VK_UI_EnsureDrawCapacity(vk_ui.vertex_count + 4,
                                  vk_ui.index_count + 6,
                                  vk_ui.draw_count + 1)) {
        return false;
    }

    uint32_t base_vertex = vk_ui.vertex_count;
    uint32_t base_index = vk_ui.index_count;

    float x0, y0, x1, y1;
    VK_UI_ToNdc(x, y, &x0, &y0);
    VK_UI_ToNdc(x + w, y + h, &x1, &y1);

    vk_ui.vertices[vk_ui.vertex_count + 0] = (vk_ui_vertex_t){ .pos = { x0, y0 }, .uv = { s1, t1 }, .color = color.u32 };
    vk_ui.vertices[vk_ui.vertex_count + 1] = (vk_ui_vertex_t){ .pos = { x1, y0 }, .uv = { s2, t1 }, .color = color.u32 };
    vk_ui.vertices[vk_ui.vertex_count + 2] = (vk_ui_vertex_t){ .pos = { x1, y1 }, .uv = { s2, t2 }, .color = color.u32 };
    vk_ui.vertices[vk_ui.vertex_count + 3] = (vk_ui_vertex_t){ .pos = { x0, y1 }, .uv = { s1, t2 }, .color = color.u32 };

    vk_ui.indices[vk_ui.index_count + 0] = base_vertex + 0;
    vk_ui.indices[vk_ui.index_count + 1] = base_vertex + 2;
    vk_ui.indices[vk_ui.index_count + 2] = base_vertex + 3;
    vk_ui.indices[vk_ui.index_count + 3] = base_vertex + 0;
    vk_ui.indices[vk_ui.index_count + 4] = base_vertex + 1;
    vk_ui.indices[vk_ui.index_count + 5] = base_vertex + 2;

    vk_ui.vertex_count += 4;
    vk_ui.index_count += 6;

    VkRect2D scissor = VK_UI_CurrentScissor();

    if (vk_ui.draw_count > 0) {
        vk_ui_draw_t *prev = &vk_ui.draws[vk_ui.draw_count - 1];
        if (prev->descriptor_set == image->descriptor_set &&
            prev->scissor.offset.x == scissor.offset.x &&
            prev->scissor.offset.y == scissor.offset.y &&
            prev->scissor.extent.width == scissor.extent.width &&
            prev->scissor.extent.height == scissor.extent.height &&
            prev->first_index + prev->index_count == base_index) {
            prev->index_count += 6;
            return true;
        }
    }

    vk_ui_draw_t *draw = &vk_ui.draws[vk_ui.draw_count++];
    draw->first_index = base_index;
    draw->index_count = 6;
    draw->descriptor_set = image->descriptor_set;
    draw->scissor = scissor;

    return true;
}

static bool VK_UI_EnqueueRotatedQuad(float x, float y, float w, float h,
                                     float s1, float t1, float s2, float t2,
                                     float angle, float pivot_x, float pivot_y,
                                     color_t color, qhandle_t pic)
{
    if (w == 0.0f || h == 0.0f) {
        return true;
    }

    VK_UI_EnsureDefaultImages();
    VK_UI_ResolvePic(&pic);

    vk_ui_image_t *image = VK_UI_ImageForHandle(pic);
    if (!image || !image->descriptor_set) {
        return false;
    }

    if (!VK_UI_EnsureDrawCapacity(vk_ui.vertex_count + 4,
                                  vk_ui.index_count + 6,
                                  vk_ui.draw_count + 1)) {
        return false;
    }

    float hw = w * 0.5f;
    float hh = h * 0.5f;

    float local_x[4] = {
        -hw + pivot_x,
         hw + pivot_x,
         hw + pivot_x,
        -hw + pivot_x,
    };

    float local_y[4] = {
        -hh + pivot_y,
        -hh + pivot_y,
         hh + pivot_y,
         hh + pivot_y,
    };

    float u[4] = { s1, s2, s2, s1 };
    float v[4] = { t1, t1, t2, t2 };

    float s = sinf(angle);
    float c = cosf(angle);

    uint32_t base_vertex = vk_ui.vertex_count;
    uint32_t base_index = vk_ui.index_count;

    for (int i = 0; i < 4; ++i) {
        float rx = local_x[i] * c - local_y[i] * s;
        float ry = local_x[i] * s + local_y[i] * c;

        float ndc_x, ndc_y;
        VK_UI_ToNdc(rx + x, ry + y, &ndc_x, &ndc_y);

        vk_ui.vertices[vk_ui.vertex_count + i] = (vk_ui_vertex_t){
            .pos = { ndc_x, ndc_y },
            .uv = { u[i], v[i] },
            .color = color.u32,
        };
    }

    vk_ui.indices[vk_ui.index_count + 0] = base_vertex + 0;
    vk_ui.indices[vk_ui.index_count + 1] = base_vertex + 2;
    vk_ui.indices[vk_ui.index_count + 2] = base_vertex + 3;
    vk_ui.indices[vk_ui.index_count + 3] = base_vertex + 0;
    vk_ui.indices[vk_ui.index_count + 4] = base_vertex + 1;
    vk_ui.indices[vk_ui.index_count + 5] = base_vertex + 2;

    vk_ui.vertex_count += 4;
    vk_ui.index_count += 6;

    VkRect2D scissor = VK_UI_CurrentScissor();

    if (vk_ui.draw_count > 0) {
        vk_ui_draw_t *prev = &vk_ui.draws[vk_ui.draw_count - 1];
        if (prev->descriptor_set == image->descriptor_set &&
            prev->scissor.offset.x == scissor.offset.x &&
            prev->scissor.offset.y == scissor.offset.y &&
            prev->scissor.extent.width == scissor.extent.width &&
            prev->scissor.extent.height == scissor.extent.height &&
            prev->first_index + prev->index_count == base_index) {
            prev->index_count += 6;
            return true;
        }
    }

    vk_ui_draw_t *draw = &vk_ui.draws[vk_ui.draw_count++];
    draw->first_index = base_index;
    draw->index_count = 6;
    draw->descriptor_set = image->descriptor_set;
    draw->scissor = scissor;

    return true;
}

bool VK_UI_Init(vk_context_t *ctx)
{
    memset(&vk_ui, 0, sizeof(vk_ui));

    if (!ctx) {
        Com_SetLastError("Vulkan UI: context is missing");
        return false;
    }

    vk_ui.ctx = ctx;
    vk_ui.initialized = true;
    vk_ui.scale = 1.0f;
    vk_ui.registration_sequence = 1;

    if (!VK_UI_EnsureImageCapacity(VK_UI_INITIAL_IMAGE_CAPACITY)) {
        goto fail;
    }

    if (!VK_UI_EnsureHostBuffers()) {
        goto fail;
    }

    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        .unnormalizedCoordinates = VK_FALSE,
    };

    if (!VK_UI_Check(vkCreateSampler(ctx->device, &sampler_info, NULL, &vk_ui.sampler_repeat),
                     "vkCreateSampler(repeat)")) {
        goto fail;
    }

    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (!VK_UI_Check(vkCreateSampler(ctx->device, &sampler_info, NULL, &vk_ui.sampler_clamp),
                     "vkCreateSampler(clamp)")) {
        goto fail;
    }

    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };

    if (!VK_UI_Check(vkCreateDescriptorSetLayout(ctx->device, &layout_info, NULL,
                                                 &vk_ui.descriptor_set_layout),
                     "vkCreateDescriptorSetLayout")) {
        goto fail;
    }

    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 8192,
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 8192,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    if (!VK_UI_Check(vkCreateDescriptorPool(ctx->device, &pool_info, NULL, &vk_ui.descriptor_pool),
                     "vkCreateDescriptorPool")) {
        goto fail;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &vk_ui.descriptor_set_layout,
    };

    if (!VK_UI_Check(vkCreatePipelineLayout(ctx->device, &pipeline_layout_info, NULL,
                                            &vk_ui.pipeline_layout),
                     "vkCreatePipelineLayout")) {
        goto fail;
    }

    VK_UI_RefreshVirtualMetrics();
    return true;

fail:
    VK_UI_Shutdown(ctx);
    return false;
}

void VK_UI_DestroySwapchainResources(vk_context_t *ctx)
{
    (void)ctx;

    if (!vk_ui.initialized || !vk_ui.ctx || !vk_ui.ctx->device) {
        return;
    }

    if (vk_ui.pipeline) {
        vkDestroyPipeline(vk_ui.ctx->device, vk_ui.pipeline, NULL);
        vk_ui.pipeline = VK_NULL_HANDLE;
    }

    vk_ui.swapchain_ready = false;
}

bool VK_UI_CreateSwapchainResources(vk_context_t *ctx)
{
    if (!vk_ui.initialized || !ctx || !ctx->render_pass) {
        return false;
    }

    VK_UI_DestroySwapchainResources(ctx);

    VkShaderModule vert_shader = VK_NULL_HANDLE;
    VkShaderModule frag_shader = VK_NULL_HANDLE;

    VkShaderModuleCreateInfo vert_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vk_ui2d_vert_spv_size,
        .pCode = vk_ui2d_vert_spv,
    };

    if (!VK_UI_Check(vkCreateShaderModule(ctx->device, &vert_info, NULL, &vert_shader),
                     "vkCreateShaderModule(vert)")) {
        return false;
    }

    VkShaderModuleCreateInfo frag_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vk_ui2d_frag_spv_size,
        .pCode = vk_ui2d_frag_spv,
    };

    if (!VK_UI_Check(vkCreateShaderModule(ctx->device, &frag_info, NULL, &frag_shader),
                     "vkCreateShaderModule(frag)")) {
        vkDestroyShaderModule(ctx->device, vert_shader, NULL);
        return false;
    }

    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_shader,
            .pName = "main",
        },
    };

    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(vk_ui_vertex_t),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attribs[3] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(vk_ui_vertex_t, pos),
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(vk_ui_vertex_t, uv),
        },
        {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .offset = offsetof(vk_ui_vertex_t, color),
        },
    };

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_desc,
        .vertexAttributeDescriptionCount = q_countof(attribs),
        .pVertexAttributeDescriptions = attribs,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                          VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    VkDynamicState dynamic_states[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = q_countof(dynamic_states),
        .pDynamicStates = dynamic_states,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = q_countof(shader_stages),
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &raster,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &blend,
        .pDynamicState = &dynamic,
        .layout = vk_ui.pipeline_layout,
        .renderPass = ctx->render_pass,
        .subpass = 0,
    };

    bool ok = VK_UI_Check(vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1,
                                                    &pipeline_info, NULL, &vk_ui.pipeline),
                          "vkCreateGraphicsPipelines");

    vkDestroyShaderModule(ctx->device, vert_shader, NULL);
    vkDestroyShaderModule(ctx->device, frag_shader, NULL);

    if (!ok) {
        return false;
    }

    vk_ui.swapchain_ready = true;
    return true;
}

void VK_UI_Shutdown(vk_context_t *ctx)
{
    if (!vk_ui.initialized) {
        return;
    }

    if (!ctx) {
        ctx = vk_ui.ctx;
    }

    if (ctx && ctx->device) {
        vkDeviceWaitIdle(ctx->device);
    }

    VK_UI_DestroySwapchainResources(ctx);

    for (uint32_t i = 1; i < vk_ui.image_capacity; ++i) {
        if (!vk_ui.images[i].in_use) {
            continue;
        }
        VK_UI_DestroyImageResources(&vk_ui.images[i]);
    }

    if (ctx && ctx->device) {
        if (vk_ui.pipeline_layout) {
            vkDestroyPipelineLayout(ctx->device, vk_ui.pipeline_layout, NULL);
            vk_ui.pipeline_layout = VK_NULL_HANDLE;
        }

        if (vk_ui.descriptor_pool) {
            vkDestroyDescriptorPool(ctx->device, vk_ui.descriptor_pool, NULL);
            vk_ui.descriptor_pool = VK_NULL_HANDLE;
        }

        if (vk_ui.descriptor_set_layout) {
            vkDestroyDescriptorSetLayout(ctx->device, vk_ui.descriptor_set_layout, NULL);
            vk_ui.descriptor_set_layout = VK_NULL_HANDLE;
        }

        if (vk_ui.sampler_clamp) {
            vkDestroySampler(ctx->device, vk_ui.sampler_clamp, NULL);
            vk_ui.sampler_clamp = VK_NULL_HANDLE;
        }

        if (vk_ui.sampler_repeat) {
            vkDestroySampler(ctx->device, vk_ui.sampler_repeat, NULL);
            vk_ui.sampler_repeat = VK_NULL_HANDLE;
        }

        VK_UI_DestroyBuffer(&vk_ui.vertex_buffer, &vk_ui.vertex_memory, &vk_ui.vertex_mapped);
        vk_ui.vertex_buffer_bytes = 0;

        VK_UI_DestroyBuffer(&vk_ui.index_buffer, &vk_ui.index_memory, &vk_ui.index_mapped);
        vk_ui.index_buffer_bytes = 0;
    }

    free(vk_ui.images);
    free(vk_ui.vertices);
    free(vk_ui.indices);
    free(vk_ui.draws);

    memset(&vk_ui, 0, sizeof(vk_ui));
}

void VK_UI_BeginFrame(void)
{
    if (!vk_ui.initialized) {
        return;
    }

    VK_UI_RefreshVirtualMetrics();
    VK_UI_EnsureDefaultImages();

    vk_ui.draw_count = 0;
    vk_ui.vertex_count = 0;
    vk_ui.index_count = 0;
    vk_ui.clip_enabled = false;
    vk_ui.scale = 1.0f;
}

void VK_UI_EndFrame(void)
{
}

void VK_UI_Record(VkCommandBuffer cmd, const VkExtent2D *extent)
{
    if (!vk_ui.initialized || !vk_ui.swapchain_ready || !vk_ui.pipeline ||
        !extent || !vk_ui.draw_count || !vk_ui.vertex_count || !vk_ui.index_count) {
        return;
    }

    if (!VK_UI_EnsureGpuBuffers()) {
        return;
    }

    size_t vertex_bytes = sizeof(*vk_ui.vertices) * vk_ui.vertex_count;
    size_t index_bytes = sizeof(*vk_ui.indices) * vk_ui.index_count;

    memcpy(vk_ui.vertex_mapped, vk_ui.vertices, vertex_bytes);
    memcpy(vk_ui.index_mapped, vk_ui.indices, index_bytes);

    VkViewport viewport = {
        .x = 0.0f,
        .y = (float)extent->height,
        .width = (float)extent->width,
        .height = -(float)extent->height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_ui.pipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_ui.vertex_buffer, &offset);
    vkCmdBindIndexBuffer(cmd, vk_ui.index_buffer, 0, VK_INDEX_TYPE_UINT32);

    for (uint32_t i = 0; i < vk_ui.draw_count; ++i) {
        const vk_ui_draw_t *draw = &vk_ui.draws[i];
        if (!draw->index_count || !draw->descriptor_set ||
            draw->scissor.extent.width == 0 || draw->scissor.extent.height == 0) {
            continue;
        }

        if (draw->first_index >= vk_ui.index_count ||
            draw->index_count > (vk_ui.index_count - draw->first_index)) {
            Com_WPrintf("Vulkan UI: skipping invalid draw range first=%u count=%u total=%u\n",
                        draw->first_index, draw->index_count, vk_ui.index_count);
            continue;
        }

        vkCmdSetScissor(cmd, 0, 1, &draw->scissor);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_ui.pipeline_layout,
                                0, 1, &draw->descriptor_set,
                                0, NULL);
        vkCmdDrawIndexed(cmd, draw->index_count, 1, draw->first_index, 0, 0);
    }
}

float VK_UI_ClampScale(cvar_t *var)
{
    return R_UIScaleClamp(r_config.width, r_config.height, var);
}

void VK_UI_SetScale(float scale)
{
    if (scale <= 0.0f) {
        scale = 1.0f;
    }

    vk_ui.scale = scale;
}

void VK_UI_SetClipRect(const clipRect_t *clip)
{
    if (!clip) {
        vk_ui.clip_enabled = false;
        memset(&vk_ui.clip_pixels, 0, sizeof(vk_ui.clip_pixels));
        return;
    }

    clipRect_t pixel_clip;
    if (!R_UIScaleClipToPixels(clip, vk_ui.base_scale, vk_ui.scale,
                               r_config.width, r_config.height, &pixel_clip)) {
        vk_ui.clip_enabled = false;
        memset(&vk_ui.clip_pixels, 0, sizeof(vk_ui.clip_pixels));
        return;
    }

    vk_ui.clip_pixels = pixel_clip;
    vk_ui.clip_enabled = true;
}

qhandle_t VK_UI_RegisterImage(const char *name, imagetype_t type, imageflags_t flags)
{
    if (!vk_ui.initialized || !name || !*name) {
        return 0;
    }

    VK_UI_EnsureDefaultImages();

    char normalized[MAX_QPATH];
    if (!VK_UI_NormalizeImagePath(normalized, sizeof(normalized), name, type)) {
        return (flags & IF_OPTIONAL) ? 0 : vk_ui.missing_image;
    }

    vk_ui_image_t *existing = VK_UI_FindImageByName(normalized, type);
    if (existing) {
        existing->flags |= (flags & IF_PERMANENT);
        return (qhandle_t)(existing - vk_ui.images);
    }

    int width = 0;
    int height = 0;
    byte *rgba = NULL;

    if (!VK_UI_LoadImageData(normalized, &width, &height, &rgba)) {
        if (flags & IF_OPTIONAL) {
            return 0;
        }
        return vk_ui.missing_image;
    }

    qhandle_t handle = VK_UI_CreateImage(normalized, type, flags, width, height, rgba);
    free(rgba);

    if (!handle) {
        return vk_ui.missing_image;
    }

    return handle;
}

qhandle_t VK_UI_RegisterRawImage(const char *name, int width, int height, byte *pic,
                                 imagetype_t type, imageflags_t flags)
{
    if (!vk_ui.initialized || width <= 0 || height <= 0 || !pic) {
        return 0;
    }

    VK_UI_EnsureDefaultImages();

    const char *resolved_name = name ? name : "**raw**";
    vk_ui_image_t *existing = VK_UI_FindImageByName(resolved_name, type);
    if (existing) {
        if (!VK_UI_SetImagePixels(existing, width, height, pic)) {
            return 0;
        }

        existing->flags = flags;
        existing->transparent = VK_UI_ImageHasTransparency(pic, (size_t)width * (size_t)height);
        if (existing->transparent) {
            existing->flags |= IF_TRANSPARENT;
        } else {
            existing->flags |= IF_OPAQUE;
        }

        return (qhandle_t)(existing - vk_ui.images);
    }

    return VK_UI_CreateImage(resolved_name, type, flags, width, height, pic);
}

void VK_UI_UnregisterImage(qhandle_t handle)
{
    vk_ui_image_t *image = VK_UI_ImageForHandle(handle);
    if (!image) {
        return;
    }

    if (handle == vk_ui.white_image || handle == vk_ui.missing_image) {
        return;
    }

    if (handle == vk_ui.raw_image) {
        vk_ui.raw_image = 0;
    }

    VK_UI_DestroyImageResources(image);
    memset(image, 0, sizeof(*image));
}

bool VK_UI_GetPicSize(int *w, int *h, qhandle_t pic)
{
    vk_ui_image_t *image = VK_UI_ImageForHandle(pic);
    if (!image) {
        if (w) {
            *w = 0;
        }
        if (h) {
            *h = 0;
        }
        return false;
    }

    if (w) {
        *w = image->width;
    }

    if (h) {
        *h = image->height;
    }

    return image->transparent;
}

bool VK_UI_IsImageTransparent(qhandle_t pic)
{
    vk_ui_image_t *image = VK_UI_ImageForHandle(pic);
    if (!image) {
        return false;
    }

    return image->transparent;
}

VkDescriptorSetLayout VK_UI_GetDescriptorSetLayout(void)
{
    if (!vk_ui.initialized) {
        return VK_NULL_HANDLE;
    }

    return vk_ui.descriptor_set_layout;
}

VkDescriptorSet VK_UI_GetDescriptorSetForImage(qhandle_t pic)
{
    if (!vk_ui.initialized) {
        return VK_NULL_HANDLE;
    }

    VK_UI_EnsureDefaultImages();
    VK_UI_ResolvePic(&pic);

    vk_ui_image_t *image = VK_UI_ImageForHandle(pic);
    if (!image) {
        return VK_NULL_HANDLE;
    }

    return image->descriptor_set;
}

bool VK_UI_UpdateImageRGBA(qhandle_t handle, int width, int height, const byte *pic)
{
    vk_ui_image_t *image = VK_UI_ImageForHandle(handle);
    if (!image || !pic || width <= 0 || height <= 0) {
        return false;
    }

    if (!VK_UI_SetImagePixels(image, width, height, pic)) {
        return false;
    }

    image->transparent = VK_UI_ImageHasTransparency(pic, (size_t)width * (size_t)height);
    if (image->transparent) {
        image->flags |= IF_TRANSPARENT;
        image->flags &= ~IF_OPAQUE;
    } else {
        image->flags |= IF_OPAQUE;
        image->flags &= ~IF_TRANSPARENT;
    }

    return true;
}

bool VK_UI_UpdateImageRGBASubRect(qhandle_t handle, int x, int y, int width, int height, const byte *pic)
{
    vk_ui_image_t *image = VK_UI_ImageForHandle(handle);
    if (!image || !image->image || !image->view || !pic || width <= 0 || height <= 0) {
        return false;
    }

    if (x < 0 || y < 0 || x + width > image->width || y + height > image->height) {
        Com_SetLastError("Vulkan UI: sub-rect update out of bounds");
        return false;
    }

    return VK_UI_UploadImageDataSubRect(image, x, y, width, height, pic,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VK_UI_UpdateRawPic(int pic_w, int pic_h, const uint32_t *pic)
{
    if (!pic || pic_w <= 0 || pic_h <= 0) {
        return;
    }

    if (vk_ui.raw_image) {
        if (VK_UI_UpdateImageRGBA(vk_ui.raw_image, pic_w, pic_h, (const byte *)pic)) {
            return;
        }

        VK_UI_UnregisterImage(vk_ui.raw_image);
        vk_ui.raw_image = 0;
    }

    vk_ui.raw_image = VK_UI_RegisterRawImage("**rawpic**", pic_w, pic_h,
                                             (byte *)pic, IT_PIC, IF_NONE);
}

void VK_UI_DrawStretchRaw(int x, int y, int w, int h)
{
    if (!vk_ui.raw_image) {
        return;
    }

    VK_UI_DrawStretchPic(x, y, w, h, COLOR_WHITE, vk_ui.raw_image);
}

void VK_UI_DrawPic(int x, int y, color_t color, qhandle_t pic)
{
    int w = 0;
    int h = 0;
    if (!VK_UI_GetPicSize(&w, &h, pic) || w <= 0 || h <= 0) {
        return;
    }

    VK_UI_DrawStretchPic(x, y, w, h, color, pic);
}

void VK_UI_DrawStretchPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    VK_UI_DrawStretchSubPic(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color, pic);
}

void VK_UI_DrawStretchSubPic(int x, int y, int w, int h,
                             float s1, float t1, float s2, float t2,
                             color_t color, qhandle_t pic)
{
    VK_UI_EnqueueQuad((float)x, (float)y, (float)w, (float)h,
                      s1, t1, s2, t2,
                      color, pic);
}

void VK_UI_DrawStretchRotatePic(int x, int y, int w, int h, color_t color, float angle,
                                int pivot_x, int pivot_y, qhandle_t pic)
{
    VK_UI_EnqueueRotatedQuad((float)x, (float)y, (float)w, (float)h,
                             0.0f, 0.0f, 1.0f, 1.0f,
                             angle,
                             (float)pivot_x, (float)pivot_y,
                             color, pic);
}

void VK_UI_DrawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    vk_ui_image_t *image = VK_UI_ImageForHandle(pic);
    if (!image || image->width <= 0 || image->height <= 0) {
        return;
    }

    float aspect = (float)image->width / (float)image->height;

    float scale_w = (float)w;
    float scale_h = (float)h * aspect;
    float scale = max(scale_w, scale_h);

    float s = (1.0f - scale_w / scale) * 0.5f;
    float t = (1.0f - scale_h / scale) * 0.5f;

    VK_UI_DrawStretchSubPic(x, y, w, h, s, t, 1.0f - s, 1.0f - t, color, pic);
}

void VK_UI_TileClear(int x, int y, int w, int h, qhandle_t pic)
{
    const float div64 = 1.0f / 64.0f;

    VK_UI_DrawStretchSubPic(x, y, w, h,
                            x * div64,
                            y * div64,
                            (x + w) * div64,
                            (y + h) * div64,
                            COLOR_WHITE,
                            pic);
}

void VK_UI_DrawFill32(int x, int y, int w, int h, color_t color)
{
    if (!w || !h) {
        return;
    }

    VK_UI_DrawStretchPic(x, y, w, h, color, vk_ui.white_image);
}

void VK_UI_DrawFill8(int x, int y, int w, int h, int c)
{
    if (!w || !h) {
        return;
    }

    VK_UI_DrawFill32(x, y, w, h, COLOR_U32(d_8to24table[c & 0xff]));
}
