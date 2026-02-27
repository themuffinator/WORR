/*
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "vk_world.h"

#include "vk_ui.h"
#include "vk_world_spv.h"
#include "renderer/view_setup.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float pos[3];
    float uv[2];
    float lm_uv[2];
    uint32_t color;
    float base_uv[2];
    uint8_t base_alpha;
    uint8_t flags;
    uint16_t reserved;
} vk_world_vertex_t;

typedef struct {
    uint32_t first_vertex;
    uint32_t vertex_count;
    VkDescriptorSet descriptor_set;
    uint32_t flags;
} vk_world_batch_t;

typedef struct vk_world_face_lightmap_s vk_world_face_lightmap_t;

enum {
    VK_WORLD_VERTEX_WARP = BIT(0),
    VK_WORLD_VERTEX_FULLBRIGHT = BIT(1),
    VK_WORLD_VERTEX_ALPHATEST = BIT(2),
    VK_WORLD_VERTEX_FLOWING = BIT(3),
};

enum {
    VK_WORLD_BATCH_ALPHA = BIT(0),
    VK_WORLD_BATCH_SKY = BIT(1),
};

enum {
    VK_WORLD_SKY_FACE_COUNT = 6,
    VK_WORLD_SKY_VERTS_PER_FACE = 6,
    VK_WORLD_SKY_TOTAL_VERTS = VK_WORLD_SKY_FACE_COUNT * VK_WORLD_SKY_VERTS_PER_FACE,
};

// 1 = s, 2 = t, 3 = size
static const int8_t vk_world_sky_st_to_vec[VK_WORLD_SKY_FACE_COUNT][3] = {
    { 3, -1, 2 },
    { -3, 1, 2 },
    { 1, 3, 2 },
    { -1, -3, 2 },
    { -2, -1, 3 },
    { 2, -1, -3 },
};

typedef struct {
    vk_context_t *ctx;
    bool initialized;
    bool swapchain_ready;

    bsp_t *bsp;
    char map_name[MAX_QPATH];

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline_opaque;
    VkPipeline pipeline_alpha;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    size_t vertex_buffer_bytes;
    void *vertex_mapped;
    uint32_t vertex_count;
    vk_world_vertex_t *cpu_vertices;
    vk_world_batch_t *batches;
    uint32_t batch_count;
    qhandle_t lightmap_handle;
    VkDescriptorSet lightmap_descriptor_set;
    qhandle_t sky_lightmap_white_image;
    VkDescriptorSet sky_lightmap_white_set;
    vk_world_face_lightmap_t *face_lms;
    byte *lightmap_rgba;
    int lightmap_atlas_size;
    float style_cache[MAX_LIGHTSTYLES];
    bool style_cache_valid;

    qhandle_t sky_images[VK_WORLD_SKY_FACE_COUNT];
    VkDescriptorSet sky_descriptor_sets[VK_WORLD_SKY_FACE_COUNT];
    vec3_t sky_axis;
    float sky_rotate;
    bool sky_autorotate;
    bool sky_enabled;

    VkBuffer sky_vertex_buffer;
    VkDeviceMemory sky_vertex_memory;
    size_t sky_vertex_buffer_bytes;
    uint32_t sky_vertex_count;
    float sky_world_size;

    const refdef_t *current_fd;
    renderer_view_push_t frame_push;
    bool frame_active;
    bool first_draw_logged;
    bool has_warp_vertices;
    bool vertex_dynamic_dirty;
    byte *world_face_mask;
} vk_world_state_t;

static vk_world_state_t vk_world;
static cvar_t *vk_lightmap_debug;
static cvar_t *vk_drawsky;
static cvar_t *vk_shaders;

static inline bool VK_World_Check(VkResult result, const char *what)
{
    if (result == VK_SUCCESS) {
        return true;
    }

    Com_SetLastError(va("Vulkan world %s failed: %d", what, (int)result));
    return false;
}

static void VK_World_ClearWorldFaceMask(void)
{
    free(vk_world.world_face_mask);
    vk_world.world_face_mask = NULL;
}

static bool VK_World_BuildWorldFaceMask(const bsp_t *bsp)
{
    VK_World_ClearWorldFaceMask();

    if (!bsp || bsp->numfaces <= 0 || bsp->nummodels < 1 || !bsp->models || !bsp->faces) {
        return false;
    }

    byte *mask = calloc((size_t)bsp->numfaces, sizeof(*mask));
    if (!mask) {
        Com_SetLastError("Vulkan world: out of memory for world-face mask");
        return false;
    }

    const mmodel_t *world_model = &bsp->models[0];
    bool world_range_valid = false;
    if (world_model->firstface && world_model->numfaces > 0) {
        ptrdiff_t first = world_model->firstface - bsp->faces;
        if (first >= 0 && first < bsp->numfaces) {
            int count = min(world_model->numfaces, bsp->numfaces - (int)first);
            if (count > 0) {
                memset(mask + first, 1, (size_t)count);
                world_range_valid = true;
            }
        }
    }

    if (!world_range_valid) {
        memset(mask, 1, (size_t)bsp->numfaces);
    }

    int inline_faces_cleared = 0;
    for (int i = 1; i < bsp->nummodels; i++) {
        const mmodel_t *inline_model = &bsp->models[i];
        if (!inline_model->firstface || inline_model->numfaces <= 0) {
            continue;
        }

        ptrdiff_t first = inline_model->firstface - bsp->faces;
        if (first < 0 || first >= bsp->numfaces) {
            continue;
        }

        int count = min(inline_model->numfaces, bsp->numfaces - (int)first);
        if (count <= 0) {
            continue;
        }
        for (int j = 0; j < count; j++) {
            if (mask[first + j]) {
                inline_faces_cleared++;
            }
        }
        memset(mask + first, 0, (size_t)count);
    }

    int world_faces = 0;
    for (int i = 0; i < bsp->numfaces; i++) {
        if (mask[i]) {
            world_faces++;
        }
    }

    Com_DPrintf("VK_World_BuildWorldFaceMask: total=%d world=%d inline_cleared=%d world_range_valid=%d\n",
                bsp->numfaces, world_faces, inline_faces_cleared, world_range_valid ? 1 : 0);

    vk_world.world_face_mask = mask;
    return true;
}

static inline bool VK_World_IsWorldFaceIndex(const bsp_t *bsp, int face_index)
{
    if (!bsp || face_index < 0 || face_index >= bsp->numfaces) {
        return false;
    }
    if (!vk_world.world_face_mask) {
        return true;
    }
    return vk_world.world_face_mask[face_index] != 0;
}

static uint32_t VK_World_FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(vk_world.ctx->physical_device, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & BIT(i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return UINT32_MAX;
}

static void VK_World_DestroyVertexBuffer(void)
{
    if (!vk_world.ctx || !vk_world.ctx->device) {
        return;
    }

    VkDevice device = vk_world.ctx->device;

    if (vk_world.vertex_mapped && vk_world.vertex_memory) {
        vkUnmapMemory(device, vk_world.vertex_memory);
        vk_world.vertex_mapped = NULL;
    }

    if (vk_world.vertex_buffer) {
        vkDestroyBuffer(device, vk_world.vertex_buffer, NULL);
        vk_world.vertex_buffer = VK_NULL_HANDLE;
    }

    if (vk_world.vertex_memory) {
        vkFreeMemory(device, vk_world.vertex_memory, NULL);
        vk_world.vertex_memory = VK_NULL_HANDLE;
    }

    vk_world.vertex_buffer_bytes = 0;
    vk_world.vertex_count = 0;
}

static void VK_World_DestroySkyVertexBuffer(void)
{
    if (!vk_world.ctx || !vk_world.ctx->device) {
        return;
    }

    VkDevice device = vk_world.ctx->device;

    if (vk_world.sky_vertex_buffer) {
        vkDestroyBuffer(device, vk_world.sky_vertex_buffer, NULL);
        vk_world.sky_vertex_buffer = VK_NULL_HANDLE;
    }

    if (vk_world.sky_vertex_memory) {
        vkFreeMemory(device, vk_world.sky_vertex_memory, NULL);
        vk_world.sky_vertex_memory = VK_NULL_HANDLE;
    }

    vk_world.sky_vertex_buffer_bytes = 0;
    vk_world.sky_vertex_count = 0;
}

static bool VK_World_CreateVertexBuffer(size_t size)
{
    VkDevice device = vk_world.ctx->device;

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (!VK_World_Check(vkCreateBuffer(device, &buffer_info, NULL, &vk_world.vertex_buffer),
                        "vkCreateBuffer")) {
        return false;
    }

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, vk_world.vertex_buffer, &requirements);

    uint32_t memory_index = VK_World_FindMemoryType(requirements.memoryTypeBits,
                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memory_index == UINT32_MAX) {
        Com_SetLastError("Vulkan world: suitable vertex memory type not found");
        vkDestroyBuffer(device, vk_world.vertex_buffer, NULL);
        vk_world.vertex_buffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memory_index,
    };

    if (!VK_World_Check(vkAllocateMemory(device, &alloc_info, NULL, &vk_world.vertex_memory),
                        "vkAllocateMemory")) {
        vkDestroyBuffer(device, vk_world.vertex_buffer, NULL);
        vk_world.vertex_buffer = VK_NULL_HANDLE;
        return false;
    }

    if (!VK_World_Check(vkBindBufferMemory(device, vk_world.vertex_buffer, vk_world.vertex_memory, 0),
                        "vkBindBufferMemory")) {
        vkDestroyBuffer(device, vk_world.vertex_buffer, NULL);
        vkFreeMemory(device, vk_world.vertex_memory, NULL);
        vk_world.vertex_buffer = VK_NULL_HANDLE;
        vk_world.vertex_memory = VK_NULL_HANDLE;
        return false;
    }

    vk_world.vertex_buffer_bytes = size;
    if (!VK_World_Check(vkMapMemory(device, vk_world.vertex_memory, 0, vk_world.vertex_buffer_bytes, 0,
                                    &vk_world.vertex_mapped),
                        "vkMapMemory(world persistent)")) {
        VK_World_DestroyVertexBuffer();
        return false;
    }
    return true;
}

static bool VK_World_UploadVertices(const vk_world_vertex_t *vertices, uint32_t vertex_count)
{
    VK_World_DestroyVertexBuffer();

    if (!vertices || !vertex_count) {
        return true;
    }

    size_t bytes = (size_t)vertex_count * sizeof(*vertices);
    if (!VK_World_CreateVertexBuffer(bytes)) {
        return false;
    }

    if (!vk_world.vertex_mapped) {
        Com_SetLastError("Vulkan world: vertex buffer not mapped");
        VK_World_DestroyVertexBuffer();
        return false;
    }

    memcpy(vk_world.vertex_mapped, vertices, bytes);

    vk_world.vertex_count = vertex_count;
    return true;
}

static bool VK_World_UploadSkyVertices(const vk_world_vertex_t *vertices, uint32_t vertex_count)
{
    VK_World_DestroySkyVertexBuffer();

    if (!vertices || !vertex_count) {
        return true;
    }

    VkDevice device = vk_world.ctx->device;
    size_t bytes = (size_t)vertex_count * sizeof(*vertices);

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bytes,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (!VK_World_Check(vkCreateBuffer(device, &buffer_info, NULL, &vk_world.sky_vertex_buffer),
                        "vkCreateBuffer(sky)")) {
        return false;
    }

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, vk_world.sky_vertex_buffer, &requirements);

    uint32_t memory_index = VK_World_FindMemoryType(requirements.memoryTypeBits,
                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memory_index == UINT32_MAX) {
        Com_SetLastError("Vulkan world: suitable sky vertex memory type not found");
        VK_World_DestroySkyVertexBuffer();
        return false;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memory_index,
    };
    if (!VK_World_Check(vkAllocateMemory(device, &alloc_info, NULL, &vk_world.sky_vertex_memory),
                        "vkAllocateMemory(sky)")) {
        VK_World_DestroySkyVertexBuffer();
        return false;
    }

    if (!VK_World_Check(vkBindBufferMemory(device, vk_world.sky_vertex_buffer,
                                           vk_world.sky_vertex_memory, 0),
                        "vkBindBufferMemory(sky)")) {
        VK_World_DestroySkyVertexBuffer();
        return false;
    }

    void *mapped = NULL;
    if (!VK_World_Check(vkMapMemory(device, vk_world.sky_vertex_memory, 0, bytes, 0, &mapped),
                        "vkMapMemory(sky)")) {
        VK_World_DestroySkyVertexBuffer();
        return false;
    }

    memcpy(mapped, vertices, bytes);
    vkUnmapMemory(device, vk_world.sky_vertex_memory);

    vk_world.sky_vertex_buffer_bytes = bytes;
    vk_world.sky_vertex_count = vertex_count;
    return true;
}

static void VK_World_ClearCpuMesh(void)
{
    free(vk_world.cpu_vertices);
    vk_world.cpu_vertices = NULL;
}

static void VK_World_ClearBatches(void)
{
    free(vk_world.batches);
    vk_world.batches = NULL;
    vk_world.batch_count = 0;
}

static void VK_World_ClearLightmap(void)
{
    if (vk_world.lightmap_handle) {
        VK_UI_UnregisterImage(vk_world.lightmap_handle);
        vk_world.lightmap_handle = 0;
    }

    free(vk_world.face_lms);
    vk_world.face_lms = NULL;
    free(vk_world.lightmap_rgba);
    vk_world.lightmap_rgba = NULL;
    vk_world.lightmap_atlas_size = 0;
    vk_world.lightmap_descriptor_set = VK_NULL_HANDLE;
    vk_world.style_cache_valid = false;
}

static void VK_World_UnsetSky(void)
{
    for (int i = 0; i < VK_WORLD_SKY_FACE_COUNT; i++) {
        if (vk_world.sky_images[i]) {
            VK_UI_UnregisterImage(vk_world.sky_images[i]);
            vk_world.sky_images[i] = 0;
        }
        vk_world.sky_descriptor_sets[i] = VK_NULL_HANDLE;
    }

    vk_world.sky_enabled = false;
    vk_world.sky_rotate = 0.0f;
    vk_world.sky_autorotate = false;
    VectorSet(vk_world.sky_axis, 0.0f, 0.0f, 1.0f);
    VK_World_DestroySkyVertexBuffer();
}

static inline float VK_World_SkyU(float s)
{
    return Q_clipf((s + 1.0f) * 0.5f, 1.0f / 512.0f, 511.0f / 512.0f);
}

static inline float VK_World_SkyV(float t)
{
    return 1.0f - VK_World_SkyU(t);
}

static void VK_World_BuildSkyFaceVertex(float s, float t, int axis, float size,
                                        const vec3_t vieworg, const vec3_t sky_matrix[3],
                                        bool rotated, float lm_u, float lm_v,
                                        vk_world_vertex_t *out)
{
    vec3_t b = { s * size, t * size, size };
    vec3_t v;

    for (int j = 0; j < 3; j++) {
        int k = vk_world_sky_st_to_vec[axis][j];
        v[j] = (k < 0) ? -b[-k - 1] : b[k - 1];
    }

    vec3_t pos;
    if (rotated) {
        pos[0] = DotProduct(sky_matrix[0], v) + vieworg[0];
        pos[1] = DotProduct(sky_matrix[1], v) + vieworg[1];
        pos[2] = DotProduct(sky_matrix[2], v) + vieworg[2];
    } else {
        VectorAdd(v, vieworg, pos);
    }

    *out = (vk_world_vertex_t){
        .pos = { pos[0], pos[1], pos[2] },
        .uv = { VK_World_SkyU(s), VK_World_SkyV(t) },
        .lm_uv = { lm_u, lm_v },
        .color = COLOR_RGBA(255, 255, 255, 255).u32,
        .base_uv = { VK_World_SkyU(s), VK_World_SkyV(t) },
        .base_alpha = 255,
        .flags = VK_WORLD_VERTEX_FULLBRIGHT,
    };
}

static bool VK_World_UpdateSkyGeometry(const refdef_t *fd)
{
    vk_world.sky_vertex_count = 0;

    if (!vk_world.sky_enabled || !fd || vk_world.lightmap_atlas_size <= 0) {
        return true;
    }

    vk_world_vertex_t verts[VK_WORLD_SKY_TOTAL_VERTS];
    const float lm_u = 0.5f / (float)vk_world.lightmap_atlas_size;
    const float lm_v = 0.5f / (float)vk_world.lightmap_atlas_size;

    vec3_t sky_matrix[3];
    bool rotated = false;
    if (vk_world.sky_rotate != 0.0f) {
        float degrees = vk_world.sky_autorotate
            ? fd->time * vk_world.sky_rotate
            : vk_world.sky_rotate;
        SetupRotationMatrix(sky_matrix, vk_world.sky_axis, degrees);
        rotated = true;
    }

    const float size = max(512.0f, vk_world.sky_world_size);
    uint32_t out = 0;
    for (int face = 0; face < VK_WORLD_SKY_FACE_COUNT; face++) {
        VK_World_BuildSkyFaceVertex(1.0f, -1.0f, face, size, fd->vieworg, sky_matrix, rotated,
                                    lm_u, lm_v, &verts[out + 0]);
        VK_World_BuildSkyFaceVertex(-1.0f, -1.0f, face, size, fd->vieworg, sky_matrix, rotated,
                                    lm_u, lm_v, &verts[out + 1]);
        VK_World_BuildSkyFaceVertex(1.0f, 1.0f, face, size, fd->vieworg, sky_matrix, rotated,
                                    lm_u, lm_v, &verts[out + 2]);
        VK_World_BuildSkyFaceVertex(1.0f, 1.0f, face, size, fd->vieworg, sky_matrix, rotated,
                                    lm_u, lm_v, &verts[out + 3]);
        VK_World_BuildSkyFaceVertex(-1.0f, -1.0f, face, size, fd->vieworg, sky_matrix, rotated,
                                    lm_u, lm_v, &verts[out + 4]);
        VK_World_BuildSkyFaceVertex(-1.0f, 1.0f, face, size, fd->vieworg, sky_matrix, rotated,
                                    lm_u, lm_v, &verts[out + 5]);
        out += VK_WORLD_SKY_VERTS_PER_FACE;
    }

    if (!VK_World_UploadSkyVertices(verts, out)) {
        return false;
    }

    return true;
}

static inline uint32_t VK_World_SurfEdgeVertexIndex(const bsp_t *bsp, const msurfedge_t *surfedge)
{
    const medge_t *edge = &bsp->edges[surfedge->edge];
    return surfedge->vert ? edge->v[1] : edge->v[0];
}

static inline byte VK_World_ColorChannel(float v)
{
    float c = fabsf(v);
    if (c > 1.0f)
        c = 1.0f;
    c = c * 0.75f + 0.25f;
    if (c < 0.0f)
        c = 0.0f;
    if (c > 1.0f)
        c = 1.0f;
    return (byte)(c * 255.0f);
}

static inline float VK_World_LightStyleWhiteForRefdef(const refdef_t *fd, byte style)
{
    if (style == 255)
        return 0.0f;

    if (!fd || !fd->lightstyles)
        return 1.0f;

    return fd->lightstyles[style].white;
}

static inline float VK_World_LightStyleWhite(byte style)
{
    return VK_World_LightStyleWhiteForRefdef(vk_world.current_fd, style);
}

static inline void VK_World_ShiftLightmapBytes(const byte in[3], vec3_t out)
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
}

static inline void VK_World_AdjustLightColor(vec3_t color)
{
    VectorScale(color, 1.0f / 255.0f, color);
}

static inline color_t VK_World_ColorFromLight(const vec3_t light)
{
    int r = (int)(light[0] * 255.0f + 0.5f);
    int g = (int)(light[1] * 255.0f + 0.5f);
    int b = (int)(light[2] * 255.0f + 0.5f);
    r = max(0, min(255, r));
    g = max(0, min(255, g));
    b = max(0, min(255, b));
    return COLOR_RGBA(r, g, b, 255);
}

static bool VK_World_SampleFaceLightmap(const mface_t *face, float s, float t, vec3_t out_color)
{
    if (!face || !face->lightmap || !out_color || face->lm_width < 1 || face->lm_height < 1) {
        return false;
    }

    int smax = face->lm_width;
    int tmax = face->lm_height;

    if (s < 0.0f || t < 0.0f || s > (float)(smax - 1) || t > (float)(tmax - 1)) {
        return false;
    }

    if (smax < 2 || tmax < 2) {
        const byte *lightmap = face->lightmap;
        VectorClear(out_color);
        for (int i = 0; i < face->numstyles; i++) {
            if (face->styles[i] == 255)
                break;

            vec3_t sample;
            VK_World_ShiftLightmapBytes(lightmap, sample);
            VectorMA(out_color, VK_World_LightStyleWhite(face->styles[i]), sample, out_color);
            lightmap += smax * tmax * 3;
        }
        VK_World_AdjustLightColor(out_color);
        return true;
    }

    int si = (int)s;
    int ti = (int)t;
    float fracu = s - si;
    float fracv = t - ti;

    si = max(0, min(smax - 2, si));
    ti = max(0, min(tmax - 2, ti));

    float w1 = (1.0f - fracu) * (1.0f - fracv);
    float w2 = fracu * (1.0f - fracv);
    float w3 = fracu * fracv;
    float w4 = (1.0f - fracu) * fracv;

    int style_bytes = smax * tmax * 3;
    const byte *lightmap = face->lightmap;

    VectorClear(out_color);

    for (int i = 0; i < face->numstyles; i++) {
        if (face->styles[i] == 255)
            break;

        const byte *b1 = &lightmap[3 * ((ti + 0) * smax + (si + 0))];
        const byte *b2 = &lightmap[3 * ((ti + 0) * smax + (si + 1))];
        const byte *b3 = &lightmap[3 * ((ti + 1) * smax + (si + 1))];
        const byte *b4 = &lightmap[3 * ((ti + 1) * smax + (si + 0))];

        vec3_t c1, c2, c3, c4, blended;
        VK_World_ShiftLightmapBytes(b1, c1);
        VK_World_ShiftLightmapBytes(b2, c2);
        VK_World_ShiftLightmapBytes(b3, c3);
        VK_World_ShiftLightmapBytes(b4, c4);

        blended[0] = w1 * c1[0] + w2 * c2[0] + w3 * c3[0] + w4 * c4[0];
        blended[1] = w1 * c1[1] + w2 * c2[1] + w3 * c3[1] + w4 * c4[1];
        blended[2] = w1 * c1[2] + w2 * c2[2] + w3 * c3[2] + w4 * c4[2];

        VectorMA(out_color, VK_World_LightStyleWhite(face->styles[i]), blended, out_color);
        lightmap += style_bytes;
    }

    VK_World_AdjustLightColor(out_color);
    return true;
}

static bool VK_World_SampleSurfacePointLight(const mface_t *face, const vec3_t point, vec3_t out_color)
{
    if (!face || !point || !out_color || !face->lightmap) {
        return false;
    }

    float s = DotProduct(face->lm_axis[0], point) + face->lm_offset[0];
    float t = DotProduct(face->lm_axis[1], point) + face->lm_offset[1];

    return VK_World_SampleFaceLightmap(face, s, t, out_color);
}

static color_t VK_World_ColorForFace(const mface_t *face)
{
    vec3_t normal;
    VectorCopy(face->plane->normal, normal);

    if (face->drawflags & DSURF_PLANEBACK) {
        VectorNegate(normal, normal);
    }

    color_t color = COLOR_RGBA(VK_World_ColorChannel(normal[0]),
                               VK_World_ColorChannel(normal[1]),
                               VK_World_ColorChannel(normal[2]),
                               255);

    if (face->texinfo && (face->texinfo->c.flags & SURF_WARP)) {
        color = COLOR_RGBA(color.r / 2, color.g / 2, min(255, (int)color.b + 64), 255);
    }

    return color;
}

static bool VK_World_DrawableFace(const mface_t *face)
{
    if (!face || !face->plane || !face->firstsurfedge)
        return false;

    if (face->numsurfedges < 3)
        return false;

    if (!face->texinfo)
        return false;

    surfflags_t flags = face->texinfo->c.flags;
    if (flags & SURF_NODRAW)
        return false;

    return true;
}

struct vk_world_face_lightmap_s {
    bool has_lightmap;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
};

static bool VK_World_AllocAtlasBlock(int atlas_size, int *inuse, int width, int height, int *out_x, int *out_y)
{
    int best = atlas_size;
    int best_x = -1;

    for (int i = 0; i <= atlas_size - width; i++) {
        int best2 = 0;
        int j;
        for (j = 0; j < width; j++) {
            if (inuse[i + j] >= best) {
                break;
            }
            if (inuse[i + j] > best2) {
                best2 = inuse[i + j];
            }
        }

        if (j == width) {
            best = best2;
            best_x = i;
        }
    }

    if (best_x < 0 || best + height > atlas_size) {
        return false;
    }

    for (int i = 0; i < width; i++) {
        inuse[best_x + i] = best + height;
    }

    *out_x = best_x;
    *out_y = best;
    return true;
}

static void VK_World_InitLightmapAtlasPixels(byte *rgba, int atlas_size)
{
    memset(rgba, 0, (size_t)atlas_size * (size_t)atlas_size * 4);
    rgba[0] = 255;
    rgba[1] = 255;
    rgba[2] = 255;
    rgba[3] = 255;
}

static void VK_World_BuildFaceLightmapPixels(const mface_t *face,
                                             const vk_world_face_lightmap_t *face_lm,
                                             int atlas_size, const refdef_t *fd,
                                             byte *rgba)
{
    if (!face || !face_lm || !face_lm->has_lightmap || !face->lightmap || !rgba ||
        atlas_size < 1 || face_lm->width < 1 || face_lm->height < 1) {
        return;
    }

    int w = face_lm->width;
    int h = face_lm->height;
    int x = (int)face_lm->x - 1;
    int y = (int)face_lm->y - 1;

    const int style_bytes = w * h * 3;
    for (int ty = 0; ty < h; ty++) {
        for (int tx = 0; tx < w; tx++) {
            vec3_t light;
            VectorClear(light);
            const byte *style_map = face->lightmap;
            const int pixel_index = 3 * (ty * w + tx);

            for (int s = 0; s < face->numstyles; s++) {
                if (face->styles[s] == 255) {
                    break;
                }

                vec3_t sample;
                VK_World_ShiftLightmapBytes(style_map + pixel_index, sample);
                VectorMA(light, VK_World_LightStyleWhiteForRefdef(fd, face->styles[s]), sample, light);
                style_map += style_bytes;
            }

            VK_World_AdjustLightColor(light);
            light[0] = max(0.0f, min(8.0f, light[0]));
            light[1] = max(0.0f, min(8.0f, light[1]));
            light[2] = max(0.0f, min(8.0f, light[2]));

            byte *dst = rgba + (((size_t)(y + 1 + ty) * (size_t)atlas_size + (size_t)(x + 1 + tx)) * 4);
            dst[0] = (byte)min(255, (int)(light[0] * 255.0f + 0.5f));
            dst[1] = (byte)min(255, (int)(light[1] * 255.0f + 0.5f));
            dst[2] = (byte)min(255, (int)(light[2] * 255.0f + 0.5f));
            dst[3] = 255;
        }
    }

    for (int tx = 0; tx < w; tx++) {
        byte *dst_top = rgba + (((size_t)y * (size_t)atlas_size + (size_t)(x + 1 + tx)) * 4);
        byte *src_top = rgba + (((size_t)(y + 1) * (size_t)atlas_size + (size_t)(x + 1 + tx)) * 4);
        byte *dst_bottom = rgba + (((size_t)(y + h + 1) * (size_t)atlas_size + (size_t)(x + 1 + tx)) * 4);
        byte *src_bottom = rgba + (((size_t)(y + h) * (size_t)atlas_size + (size_t)(x + 1 + tx)) * 4);
        memcpy(dst_top, src_top, 4);
        memcpy(dst_bottom, src_bottom, 4);
    }

    for (int ty = 0; ty < h; ty++) {
        byte *dst_left = rgba + (((size_t)(y + 1 + ty) * (size_t)atlas_size + (size_t)x) * 4);
        byte *src_left = rgba + (((size_t)(y + 1 + ty) * (size_t)atlas_size + (size_t)(x + 1)) * 4);
        byte *dst_right = rgba + (((size_t)(y + 1 + ty) * (size_t)atlas_size + (size_t)(x + w + 1)) * 4);
        byte *src_right = rgba + (((size_t)(y + 1 + ty) * (size_t)atlas_size + (size_t)(x + w)) * 4);
        memcpy(dst_left, src_left, 4);
        memcpy(dst_right, src_right, 4);
    }

    byte *dst_tl = rgba + (((size_t)y * (size_t)atlas_size + (size_t)x) * 4);
    byte *src_tl = rgba + (((size_t)(y + 1) * (size_t)atlas_size + (size_t)(x + 1)) * 4);
    byte *dst_tr = rgba + (((size_t)y * (size_t)atlas_size + (size_t)(x + w + 1)) * 4);
    byte *src_tr = rgba + (((size_t)(y + 1) * (size_t)atlas_size + (size_t)(x + w)) * 4);
    byte *dst_bl = rgba + (((size_t)(y + h + 1) * (size_t)atlas_size + (size_t)x) * 4);
    byte *src_bl = rgba + (((size_t)(y + h) * (size_t)atlas_size + (size_t)(x + 1)) * 4);
    byte *dst_br = rgba + (((size_t)(y + h + 1) * (size_t)atlas_size + (size_t)(x + w + 1)) * 4);
    byte *src_br = rgba + (((size_t)(y + h) * (size_t)atlas_size + (size_t)(x + w)) * 4);
    memcpy(dst_tl, src_tl, 4);
    memcpy(dst_tr, src_tr, 4);
    memcpy(dst_bl, src_bl, 4);
    memcpy(dst_br, src_br, 4);
}

static void VK_World_RebuildLightmapAtlasPixels(const bsp_t *bsp,
                                                const vk_world_face_lightmap_t *face_lms,
                                                int atlas_size, const refdef_t *fd,
                                                byte *rgba)
{
    if (!bsp || !face_lms || atlas_size < 1 || !rgba) {
        return;
    }

    VK_World_InitLightmapAtlasPixels(rgba, atlas_size);

    for (int i = 0; i < bsp->numfaces; i++) {
        if (!VK_World_IsWorldFaceIndex(bsp, i)) {
            continue;
        }
        const mface_t *face = &bsp->faces[i];

        if (!VK_World_DrawableFace(face)) {
            continue;
        }

        const vk_world_face_lightmap_t *face_lm = &face_lms[i];
        if (!face_lm->has_lightmap) {
            continue;
        }

        VK_World_BuildFaceLightmapPixels(face, face_lm, atlas_size, fd, rgba);
    }
}

static void VK_World_CacheLightStyles(const refdef_t *fd)
{
    for (int i = 0; i < MAX_LIGHTSTYLES; i++) {
        vk_world.style_cache[i] = VK_World_LightStyleWhiteForRefdef(fd, (byte)i);
    }

    vk_world.style_cache_valid = true;
}

static int VK_World_CollectChangedStyles(const refdef_t *fd, bool changed_styles[MAX_LIGHTSTYLES])
{
    int changed_count = 0;
    memset(changed_styles, 0, sizeof(bool) * MAX_LIGHTSTYLES);

    if (!vk_world.style_cache_valid) {
        for (int i = 0; i < MAX_LIGHTSTYLES; i++) {
            changed_styles[i] = true;
        }
        return MAX_LIGHTSTYLES;
    }

    for (int i = 0; i < MAX_LIGHTSTYLES; i++) {
        float white = VK_World_LightStyleWhiteForRefdef(fd, (byte)i);
        if (fabsf(white - vk_world.style_cache[i]) > 0.0001f) {
            changed_styles[i] = true;
            changed_count++;
        }
    }

    return changed_count;
}

static bool VK_World_FaceUsesChangedStyle(const mface_t *face,
                                          const bool changed_styles[MAX_LIGHTSTYLES])
{
    if (!face || !changed_styles) {
        return false;
    }

    for (int i = 0; i < face->numstyles; i++) {
        byte style = face->styles[i];
        if (style == 255) {
            break;
        }

        if (changed_styles[style]) {
            return true;
        }
    }

    return false;
}

static bool VK_World_UploadLightmapDirtyRect(int x, int y, int w, int h)
{
    if (!vk_world.lightmap_rgba || w <= 0 || h <= 0 || x < 0 || y < 0 ||
        x + w > vk_world.lightmap_atlas_size || y + h > vk_world.lightmap_atlas_size) {
        return false;
    }

    size_t bytes = (size_t)w * (size_t)h * 4;
    byte *rect_rgba = malloc(bytes);
    if (!rect_rgba) {
        Com_SetLastError("Vulkan world: out of memory for lightmap dirty rect upload");
        return false;
    }

    for (int row = 0; row < h; row++) {
        const byte *src = vk_world.lightmap_rgba +
                          (((size_t)(y + row) * (size_t)vk_world.lightmap_atlas_size + (size_t)x) * 4);
        byte *dst = rect_rgba + ((size_t)row * (size_t)w * 4);
        memcpy(dst, src, (size_t)w * 4);
    }

    bool ok = VK_UI_UpdateImageRGBASubRect(vk_world.lightmap_handle, x, y, w, h, rect_rgba);
    free(rect_rgba);
    if (!ok) {
        Com_SetLastError("Vulkan world: failed to upload lightmap dirty rect");
        return false;
    }

    return true;
}

static bool VK_World_UpdateLightmapStyles(const refdef_t *fd)
{
    if (!vk_world.bsp || !vk_world.lightmap_handle || !vk_world.face_lms ||
        !vk_world.lightmap_rgba || vk_world.lightmap_atlas_size < 1) {
        return true;
    }

    bool changed_styles[MAX_LIGHTSTYLES];
    int changed_style_count = VK_World_CollectChangedStyles(fd, changed_styles);
    if (!changed_style_count) {
        return true;
    }

    int dirty_min_x = vk_world.lightmap_atlas_size;
    int dirty_min_y = vk_world.lightmap_atlas_size;
    int dirty_max_x = 0;
    int dirty_max_y = 0;
    int updated_faces = 0;

    for (int i = 0; i < vk_world.bsp->numfaces; i++) {
        if (!VK_World_IsWorldFaceIndex(vk_world.bsp, i)) {
            continue;
        }
        const mface_t *face = &vk_world.bsp->faces[i];

        if (!VK_World_DrawableFace(face)) {
            continue;
        }

        const vk_world_face_lightmap_t *face_lm = &vk_world.face_lms[i];
        if (!face_lm->has_lightmap || !VK_World_FaceUsesChangedStyle(face, changed_styles)) {
            continue;
        }

        VK_World_BuildFaceLightmapPixels(face, face_lm, vk_world.lightmap_atlas_size, fd,
                                         vk_world.lightmap_rgba);

        int face_x = (int)face_lm->x - 1;
        int face_y = (int)face_lm->y - 1;
        int face_w = (int)face_lm->width + 2;
        int face_h = (int)face_lm->height + 2;

        dirty_min_x = min(dirty_min_x, face_x);
        dirty_min_y = min(dirty_min_y, face_y);
        dirty_max_x = max(dirty_max_x, face_x + face_w);
        dirty_max_y = max(dirty_max_y, face_y + face_h);
        updated_faces++;
    }

    if (updated_faces > 0) {
        dirty_min_x = max(0, min(vk_world.lightmap_atlas_size, dirty_min_x));
        dirty_min_y = max(0, min(vk_world.lightmap_atlas_size, dirty_min_y));
        dirty_max_x = max(0, min(vk_world.lightmap_atlas_size, dirty_max_x));
        dirty_max_y = max(0, min(vk_world.lightmap_atlas_size, dirty_max_y));

        int dirty_w = dirty_max_x - dirty_min_x;
        int dirty_h = dirty_max_y - dirty_min_y;
        if (dirty_w > 0 && dirty_h > 0) {
            if (!VK_World_UploadLightmapDirtyRect(dirty_min_x, dirty_min_y, dirty_w, dirty_h)) {
                return false;
            }
        }

        if (vk_lightmap_debug && vk_lightmap_debug->integer > 0) {
            int atlas_area = vk_world.lightmap_atlas_size * vk_world.lightmap_atlas_size;
            int rect_area = max(1, dirty_w * dirty_h);
            float coverage = (100.0f * (float)rect_area) / (float)max(1, atlas_area);
            Com_Printf("vk_lightmap_debug: styles=%d faces=%d rect=%dx%d@%d,%d (%.2f%% atlas)\n",
                       changed_style_count, updated_faces, dirty_w, dirty_h,
                       dirty_min_x, dirty_min_y, coverage);
        }
    } else if (vk_lightmap_debug && vk_lightmap_debug->integer > 1) {
        Com_Printf("vk_lightmap_debug: styles changed (%d) but no world faces referenced them\n",
                   changed_style_count);
    }

    VK_World_CacheLightStyles(fd);
    return true;
}

static bool VK_World_BuildLightmapAtlas(const bsp_t *bsp,
                                        const refdef_t *fd,
                                        vk_world_face_lightmap_t **out_faces,
                                        int *out_atlas_size,
                                        byte **out_rgba)
{
    if (!bsp || !out_faces || !out_atlas_size || !out_rgba) {
        return false;
    }

    vk_world_face_lightmap_t *face_lms = calloc((size_t)bsp->numfaces, sizeof(*face_lms));
    if (!face_lms) {
        Com_SetLastError("Vulkan world: out of memory for face lightmap table");
        return false;
    }

    static const int atlas_candidates[] = { 1024, 2048, 4096 };

    for (size_t cand = 0; cand < q_countof(atlas_candidates); cand++) {
        const int atlas_size = atlas_candidates[cand];
        int *inuse = calloc((size_t)atlas_size, sizeof(*inuse));
        byte *rgba = calloc((size_t)atlas_size * (size_t)atlas_size, 4);
        if (!inuse || !rgba) {
            free(inuse);
            free(rgba);
            free(face_lms);
            Com_SetLastError("Vulkan world: out of memory building lightmap atlas");
            return false;
        }

        memset(face_lms, 0, (size_t)bsp->numfaces * sizeof(*face_lms));

        // Reserve (0,0) as fullbright texel for surfaces without lightmaps.
        inuse[0] = 1;

        bool failed = false;

        for (int i = 0; i < bsp->numfaces; i++) {
            if (!VK_World_IsWorldFaceIndex(bsp, i)) {
                continue;
            }
            const mface_t *face = &bsp->faces[i];

            if (!VK_World_DrawableFace(face) || !face->lightmap || face->lm_width < 1 || face->lm_height < 1) {
                continue;
            }

            int w = face->lm_width;
            int h = face->lm_height;
            int block_w = w + 2;
            int block_h = h + 2;

            if (block_w > atlas_size || block_h > atlas_size) {
                failed = true;
                break;
            }

            int x = 0;
            int y = 0;
            if (!VK_World_AllocAtlasBlock(atlas_size, inuse, block_w, block_h, &x, &y)) {
                failed = true;
                break;
            }

            face_lms[i].has_lightmap = true;
            face_lms[i].x = (uint16_t)(x + 1);
            face_lms[i].y = (uint16_t)(y + 1);
            face_lms[i].width = (uint16_t)w;
            face_lms[i].height = (uint16_t)h;
        }

        free(inuse);

        if (!failed) {
            VK_World_RebuildLightmapAtlasPixels(bsp, face_lms, atlas_size, fd, rgba);
            *out_faces = face_lms;
            *out_atlas_size = atlas_size;
            *out_rgba = rgba;
            return true;
        }

        free(rgba);
    }

    free(face_lms);
    Com_SetLastError("Vulkan world: failed to pack lightmap atlas");
    return false;
}

static qhandle_t VK_World_GetFaceTexture(const bsp_t *bsp, const mface_t *face,
                                         qhandle_t *texture_handles, vec2_t *texture_inv_sizes)
{
    if (!bsp || !face || !face->texinfo || !texture_handles || !texture_inv_sizes) {
        return 0;
    }

    int tex_index = (int)(face->texinfo - bsp->texinfo);
    if (tex_index < 0 || tex_index >= bsp->numtexinfo) {
        return 0;
    }

    if (texture_handles[tex_index]) {
        return texture_handles[tex_index];
    }

    char path[MAX_QPATH];
    imageflags_t flags = IF_REPEAT;
    if (face->texinfo->c.flags & SURF_WARP) {
        flags |= IF_TURBULENT;
    }

    Q_concat(path, sizeof(path), "textures/", face->texinfo->name, ".wal");
    qhandle_t handle = VK_UI_RegisterImage(path, IT_WALL, flags);
    if (!handle) {
        return 0;
    }

    int tex_w = 0;
    int tex_h = 0;
    VK_UI_GetPicSize(&tex_w, &tex_h, handle);
    if (tex_w <= 0 || tex_h <= 0) {
        tex_w = 64;
        tex_h = 64;
    }

    texture_handles[tex_index] = handle;
    texture_inv_sizes[tex_index][0] = 1.0f / (float)tex_w;
    texture_inv_sizes[tex_index][1] = 1.0f / (float)tex_h;
    return handle;
}

static bool VK_World_BuildMesh(vk_world_vertex_t **out_vertices,
                               uint32_t *out_vertex_count,
                               vk_world_batch_t **out_batches, uint32_t *out_batch_count,
                               const vk_world_face_lightmap_t *face_lms,
                               int atlas_size)
{
    if (!out_vertices || !out_vertex_count || !out_batches || !out_batch_count ||
        !face_lms || atlas_size < 1 || !vk_world.bsp) {
        return false;
    }

    const bsp_t *bsp = vk_world.bsp;
    if (bsp->numtexinfo <= 0) {
        Com_SetLastError("Vulkan world: map has no texinfo");
        return false;
    }

    uint64_t triangle_count = 0;
    for (int i = 0; i < bsp->numfaces; ++i) {
        if (!VK_World_IsWorldFaceIndex(bsp, i)) {
            continue;
        }
        const mface_t *face = &bsp->faces[i];

        if (!VK_World_DrawableFace(face)) {
            continue;
        }

        triangle_count += (uint64_t)(face->numsurfedges - 2);
    }

    if (!triangle_count) {
        *out_vertices = NULL;
        *out_vertex_count = 0;
        *out_batches = NULL;
        *out_batch_count = 0;
        return true;
    }

    if (triangle_count > UINT32_MAX / 3) {
        Com_SetLastError("Vulkan world: triangle count overflow");
        return false;
    }

    uint32_t vertex_count = (uint32_t)triangle_count * 3;
    vk_world_vertex_t *vertices = malloc((size_t)vertex_count * sizeof(*vertices));
    if (!vertices) {
        Com_SetLastError("Vulkan world: out of memory building mesh");
        return false;
    }

    vk_world_batch_t *batches = malloc((size_t)triangle_count * sizeof(*batches));
    if (!batches) {
        free(vertices);
        Com_SetLastError("Vulkan world: out of memory building draw batches");
        return false;
    }

    qhandle_t *texture_handles = calloc((size_t)bsp->numtexinfo, sizeof(*texture_handles));
    vec2_t *texture_inv_sizes = calloc((size_t)bsp->numtexinfo, sizeof(*texture_inv_sizes));
    if (!texture_handles || !texture_inv_sizes) {
        free(texture_handles);
        free(texture_inv_sizes);
        free(batches);
        free(vertices);
        Com_SetLastError("Vulkan world: out of memory building texture map");
        return false;
    }

    uint32_t out = 0;
    uint32_t batch_out = 0;
    uint32_t lightmapped_vertices = 0;

    for (int i = 0; i < bsp->numfaces; ++i) {
        if (!VK_World_IsWorldFaceIndex(bsp, i)) {
            continue;
        }
        const mface_t *face = &bsp->faces[i];

        if (!VK_World_DrawableFace(face)) {
            continue;
        }

        qhandle_t texture_handle =
            VK_World_GetFaceTexture(bsp, face, texture_handles, texture_inv_sizes);
        VkDescriptorSet descriptor_set = VK_UI_GetDescriptorSetForImage(texture_handle);
        if (!descriptor_set) {
            continue;
        }

        int tex_index = (int)(face->texinfo - bsp->texinfo);
        float inv_tex_w = 1.0f;
        float inv_tex_h = 1.0f;
        if (tex_index >= 0 && tex_index < bsp->numtexinfo) {
            inv_tex_w = texture_inv_sizes[tex_index][0];
            inv_tex_h = texture_inv_sizes[tex_index][1];
        }

        surfflags_t surf_flags = face->texinfo->c.flags;
        uint32_t batch_flags = 0;
        uint8_t vertex_flags = 0;

        if (surf_flags & SURF_SKY) {
            batch_flags |= VK_WORLD_BATCH_SKY;
            vertex_flags |= VK_WORLD_VERTEX_FULLBRIGHT;
        }

        if (surf_flags & SURF_WARP) {
            vertex_flags |= VK_WORLD_VERTEX_WARP;
        }
        if (surf_flags & SURF_ALPHATEST) {
            vertex_flags |= VK_WORLD_VERTEX_ALPHATEST;
        }
        if (surf_flags & SURF_FLOWING) {
            vertex_flags |= VK_WORLD_VERTEX_FLOWING;
        }

        float alpha = 1.0f;
        if (surf_flags & SURF_TRANS33) {
            alpha = 0.33f;
            batch_flags |= VK_WORLD_BATCH_ALPHA;
        } else if (surf_flags & SURF_TRANS66) {
            alpha = 0.66f;
            batch_flags |= VK_WORLD_BATCH_ALPHA;
        }

        uint8_t base_alpha = (uint8_t)Q_clipf(alpha * 255.0f + 0.5f, 0.0f, 255.0f);
        color_t modulate_color = COLOR_RGBA(255, 255, 255, base_alpha);

        const vk_world_face_lightmap_t *face_lm = &face_lms[i];
        float fallback_lm_u = 0.5f / (float)atlas_size;
        float fallback_lm_v = 0.5f / (float)atlas_size;

        const msurfedge_t *surfedges = face->firstsurfedge;
        uint32_t i0 = VK_World_SurfEdgeVertexIndex(bsp, &surfedges[0]);
        const mvertex_t *v0 = &bsp->vertices[i0];

        for (int j = 1; j < face->numsurfedges - 1; ++j) {
            uint32_t i1 = VK_World_SurfEdgeVertexIndex(bsp, &surfedges[j]);
            uint32_t i2 = VK_World_SurfEdgeVertexIndex(bsp, &surfedges[j + 1]);
            const mvertex_t *v1 = &bsp->vertices[i1];
            const mvertex_t *v2 = &bsp->vertices[i2];

            if (batch_out == 0 ||
                batches[batch_out - 1].descriptor_set != descriptor_set ||
                batches[batch_out - 1].flags != batch_flags) {
                batches[batch_out++] = (vk_world_batch_t){
                    .first_vertex = out,
                    .vertex_count = 0,
                    .descriptor_set = descriptor_set,
                    .flags = batch_flags,
                };
            }

            float u0 = DotProduct(v0->point, face->texinfo->axis[0]) + face->texinfo->offset[0];
            float t0 = DotProduct(v0->point, face->texinfo->axis[1]) + face->texinfo->offset[1];
            float u1 = DotProduct(v1->point, face->texinfo->axis[0]) + face->texinfo->offset[0];
            float t1 = DotProduct(v1->point, face->texinfo->axis[1]) + face->texinfo->offset[1];
            float u2 = DotProduct(v2->point, face->texinfo->axis[0]) + face->texinfo->offset[0];
            float t2 = DotProduct(v2->point, face->texinfo->axis[1]) + face->texinfo->offset[1];

            float uv0[2] = { u0 * inv_tex_w, t0 * inv_tex_h };
            float uv1[2] = { u1 * inv_tex_w, t1 * inv_tex_h };
            float uv2[2] = { u2 * inv_tex_w, t2 * inv_tex_h };

            float lm_u0 = fallback_lm_u;
            float lm_v0 = fallback_lm_v;
            float lm_u1 = fallback_lm_u;
            float lm_v1 = fallback_lm_v;
            float lm_u2 = fallback_lm_u;
            float lm_v2 = fallback_lm_v;

            if (!(batch_flags & VK_WORLD_BATCH_SKY) && face_lm->has_lightmap) {
                float s0 = DotProduct(face->lm_axis[0], v0->point) + face->lm_offset[0];
                float lt0 = DotProduct(face->lm_axis[1], v0->point) + face->lm_offset[1];
                float s1 = DotProduct(face->lm_axis[0], v1->point) + face->lm_offset[0];
                float lt1 = DotProduct(face->lm_axis[1], v1->point) + face->lm_offset[1];
                float s2 = DotProduct(face->lm_axis[0], v2->point) + face->lm_offset[0];
                float lt2 = DotProduct(face->lm_axis[1], v2->point) + face->lm_offset[1];

                s0 = max(0.0f, min((float)(face_lm->width - 1), s0));
                lt0 = max(0.0f, min((float)(face_lm->height - 1), lt0));
                s1 = max(0.0f, min((float)(face_lm->width - 1), s1));
                lt1 = max(0.0f, min((float)(face_lm->height - 1), lt1));
                s2 = max(0.0f, min((float)(face_lm->width - 1), s2));
                lt2 = max(0.0f, min((float)(face_lm->height - 1), lt2));

                lm_u0 = ((float)face_lm->x + s0 + 0.5f) / (float)atlas_size;
                lm_v0 = ((float)face_lm->y + lt0 + 0.5f) / (float)atlas_size;
                lm_u1 = ((float)face_lm->x + s1 + 0.5f) / (float)atlas_size;
                lm_v1 = ((float)face_lm->y + lt1 + 0.5f) / (float)atlas_size;
                lm_u2 = ((float)face_lm->x + s2 + 0.5f) / (float)atlas_size;
                lm_v2 = ((float)face_lm->y + lt2 + 0.5f) / (float)atlas_size;
            }

            vertices[out++] = (vk_world_vertex_t){
                .pos = { v0->point[0], v0->point[1], v0->point[2] },
                .uv = { uv0[0], uv0[1] },
                .lm_uv = { lm_u0, lm_v0 },
                .color = modulate_color.u32,
                .base_uv = { uv0[0], uv0[1] },
                .base_alpha = base_alpha,
                .flags = vertex_flags,
            };
            vertices[out++] = (vk_world_vertex_t){
                .pos = { v1->point[0], v1->point[1], v1->point[2] },
                .uv = { uv1[0], uv1[1] },
                .lm_uv = { lm_u1, lm_v1 },
                .color = modulate_color.u32,
                .base_uv = { uv1[0], uv1[1] },
                .base_alpha = base_alpha,
                .flags = vertex_flags,
            };
            vertices[out++] = (vk_world_vertex_t){
                .pos = { v2->point[0], v2->point[1], v2->point[2] },
                .uv = { uv2[0], uv2[1] },
                .lm_uv = { lm_u2, lm_v2 },
                .color = modulate_color.u32,
                .base_uv = { uv2[0], uv2[1] },
                .base_alpha = base_alpha,
                .flags = vertex_flags,
            };

            if (!(batch_flags & VK_WORLD_BATCH_SKY) && face_lm->has_lightmap) {
                lightmapped_vertices += 3;
            }

            batches[batch_out - 1].vertex_count += 3;
        }
    }

    free(texture_handles);
    free(texture_inv_sizes);

    if (out == 0) {
        free(vertices);
        vertices = NULL;
    } else if (out < vertex_count) {
        vk_world_vertex_t *shrunk_vertices =
            realloc(vertices, (size_t)out * sizeof(*vertices));
        if (shrunk_vertices) {
            vertices = shrunk_vertices;
        }
    }

    if (batch_out == 0) {
        free(batches);
        batches = NULL;
    } else if (batch_out < triangle_count) {
        vk_world_batch_t *shrunk_batches =
            realloc(batches, (size_t)batch_out * sizeof(*batches));
        if (shrunk_batches) {
            batches = shrunk_batches;
        }
    }

    *out_vertices = vertices;
    *out_vertex_count = out;
    *out_batches = batches;
    *out_batch_count = batch_out;
    Com_DPrintf("VK_World_BuildMesh: vertices=%u batches=%u lightmapped=%u\n",
                out, batch_out, lightmapped_vertices);
    return true;
}

static bool VK_World_HasWarpVertices(const vk_world_vertex_t *vertices, uint32_t vertex_count)
{
    if (!vertices || !vertex_count) {
        return false;
    }

    for (uint32_t i = 0; i < vertex_count; i++) {
        if (vertices[i].flags & VK_WORLD_VERTEX_WARP) {
            return true;
        }
    }

    return false;
}

static void VK_World_FreeBSP(void)
{
    VK_World_ClearWorldFaceMask();

    if (vk_world.bsp) {
        BSP_Free(vk_world.bsp);
        vk_world.bsp = NULL;
    }

    vk_world.map_name[0] = '\0';
}

static inline void VK_World_AddDynamicLightsAtPoint(const vec3_t origin, vec3_t light)
{
    if (!vk_world.current_fd || !vk_world.current_fd->dlights) {
        return;
    }

    for (int i = 0; i < vk_world.current_fd->num_dlights; i++) {
        const dlight_t *dl = vk_world.current_fd->dlights + i;
        float f = dl->radius - DLIGHT_CUTOFF - Distance(dl->origin, origin);
        if (f > 0.0f) {
            f *= (1.0f / 255.0f);
            VectorMA(light, f * dl->intensity, dl->color, light);
        }
    }
}

static inline void VK_World_ClampLight(vec3_t light)
{
    light[0] = max(0.0f, min(8.0f, light[0]));
    light[1] = max(0.0f, min(8.0f, light[1]));
    light[2] = max(0.0f, min(8.0f, light[2]));
}

static bool VK_World_UpdateVertexLighting(void)
{
    if (!vk_world.bsp || !vk_world.cpu_vertices ||
        !vk_world.vertex_count || !vk_world.vertex_buffer || !vk_world.vertex_memory) {
        return false;
    }

    const float time = (vk_world.current_fd ? vk_world.current_fd->time : 0.0f);
    const bool enable_shader_effects = !vk_shaders || (vk_shaders->integer != 0);
    const bool animate_warp = enable_shader_effects && vk_world.has_warp_vertices;
    const bool has_dlights = vk_world.current_fd && vk_world.current_fd->dlights &&
                             vk_world.current_fd->num_dlights > 0;

    if (!animate_warp && !has_dlights && !vk_world.vertex_dynamic_dirty) {
        return true;
    }

    for (uint32_t i = 0; i < vk_world.vertex_count; i++) {
        vk_world_vertex_t *vertex = &vk_world.cpu_vertices[i];

        float base_u = vertex->base_uv[0];
        float base_v = vertex->base_uv[1];
        if (vertex->flags & VK_WORLD_VERTEX_FLOWING) {
            float scroll_speed = (vertex->flags & VK_WORLD_VERTEX_WARP) ? 0.5f : 1.6f;
            base_u += -scroll_speed * time;
        }

        if ((vertex->flags & VK_WORLD_VERTEX_WARP) && animate_warp) {
            // Match GL shader path: tc += w_amp * sin(tc.ts * w_phase + time), w_amp=0.0625, w_phase=4.
            vertex->uv[0] = base_u + 0.0625f * sinf(base_v * 4.0f + time);
            vertex->uv[1] = base_v + 0.0625f * sinf(base_u * 4.0f + time);
        } else if (vk_world.vertex_dynamic_dirty || !animate_warp || (vertex->flags & VK_WORLD_VERTEX_FLOWING)) {
            vertex->uv[0] = base_u;
            vertex->uv[1] = base_v;
        }

        if (has_dlights) {
            color_t color;
            if (vertex->flags & VK_WORLD_VERTEX_FULLBRIGHT) {
                color = COLOR_RGBA(255, 255, 255, vertex->base_alpha);
            } else {
                vec3_t light = { 1.0f, 1.0f, 1.0f };
                VK_World_AddDynamicLightsAtPoint(vertex->pos, light);
                VK_World_ClampLight(light);
                color = VK_World_ColorFromLight(light);
                color.a = vertex->base_alpha;
            }
            vertex->color = color.u32;
        } else if (vk_world.vertex_dynamic_dirty) {
            vertex->color = COLOR_RGBA(255, 255, 255, vertex->base_alpha).u32;
        }
    }

    size_t bytes = (size_t)vk_world.vertex_count * sizeof(*vk_world.cpu_vertices);
    if (!vk_world.vertex_mapped) {
        Com_SetLastError("Vulkan world: vertex buffer not mapped");
        return false;
    }

    memcpy(vk_world.vertex_mapped, vk_world.cpu_vertices, bytes);
    vk_world.vertex_dynamic_dirty = has_dlights || animate_warp;
    return true;
}

static void VK_World_ClearFrame(void)
{
    vk_world.current_fd = NULL;
    memset(&vk_world.frame_push, 0, sizeof(vk_world.frame_push));
    vk_world.frame_active = false;
}

static bool VK_World_CreatePipelineVariant(vk_context_t *ctx, bool alpha_blend, VkPipeline *out_pipeline)
{
    VkShaderModule vert_shader = VK_NULL_HANDLE;
    VkShaderModule frag_shader = VK_NULL_HANDLE;

    VkShaderModuleCreateInfo vert_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vk_world_vert_spv_size,
        .pCode = vk_world_vert_spv,
    };

    if (!VK_World_Check(vkCreateShaderModule(ctx->device, &vert_info, NULL, &vert_shader),
                        "vkCreateShaderModule(world vert)")) {
        return false;
    }

    VkShaderModuleCreateInfo frag_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vk_world_frag_spv_size,
        .pCode = vk_world_frag_spv,
    };

    if (!VK_World_Check(vkCreateShaderModule(ctx->device, &frag_info, NULL, &frag_shader),
                        "vkCreateShaderModule(world frag)")) {
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

    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride = sizeof(vk_world_vertex_t),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attrs[5] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(vk_world_vertex_t, pos),
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(vk_world_vertex_t, uv),
        },
        {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(vk_world_vertex_t, lm_uv),
        },
        {
            .location = 3,
            .binding = 0,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .offset = offsetof(vk_world_vertex_t, color),
        },
        {
            .location = 4,
            .binding = 0,
            .format = VK_FORMAT_R8_UINT,
            .offset = offsetof(vk_world_vertex_t, flags),
        },
    };

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = q_countof(attrs),
        .pVertexAttributeDescriptions = attrs,
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

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = alpha_blend ? VK_FALSE : VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = alpha_blend ? VK_TRUE : VK_FALSE,
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

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = q_countof(dynamic_states),
        .pDynamicStates = dynamic_states,
    };

    VkGraphicsPipelineCreateInfo info = {
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
        .layout = vk_world.pipeline_layout,
        .renderPass = ctx->render_pass,
        .subpass = 0,
    };

    bool ok = VK_World_Check(vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1,
                                                       &info, NULL, out_pipeline),
                             alpha_blend ?
                             "vkCreateGraphicsPipelines(world alpha)" :
                             "vkCreateGraphicsPipelines(world opaque)");

    vkDestroyShaderModule(ctx->device, vert_shader, NULL);
    vkDestroyShaderModule(ctx->device, frag_shader, NULL);

    return ok;
}

bool VK_World_Init(vk_context_t *ctx)
{
    memset(&vk_world, 0, sizeof(vk_world));

    if (!vk_lightmap_debug) {
        vk_lightmap_debug = Cvar_Get("vk_lightmap_debug", "0", 0);
    }
    if (!vk_drawsky) {
        vk_drawsky = Cvar_Get("vk_drawsky", "1", 0);
    }
    if (!vk_shaders) {
        vk_shaders = Cvar_Get("vk_shaders", "1", 0);
    }

    if (!ctx) {
        Com_SetLastError("Vulkan world: context is missing");
        return false;
    }

    vk_world.ctx = ctx;

    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(renderer_view_push_t),
    };

    VkDescriptorSetLayout ui_set_layout = VK_UI_GetDescriptorSetLayout();
    if (!ui_set_layout) {
        Com_SetLastError("Vulkan world: UI descriptor set layout is unavailable");
        return false;
    }

    VkDescriptorSetLayout set_layouts[2] = {
        ui_set_layout,
        ui_set_layout,
    };

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = q_countof(set_layouts),
        .pSetLayouts = set_layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    };

    if (!VK_World_Check(vkCreatePipelineLayout(ctx->device, &layout_info, NULL,
                                               &vk_world.pipeline_layout),
                        "vkCreatePipelineLayout(world)")) {
        return false;
    }

    static byte white_rgba[4] = { 255, 255, 255, 255 };
    vk_world.sky_lightmap_white_image = VK_UI_RegisterRawImage("**vk_world_sky_lm_white**",
                                                                1, 1, white_rgba,
                                                                IT_PIC, IF_PERMANENT | IF_OPAQUE);
    vk_world.sky_lightmap_white_set = VK_UI_GetDescriptorSetForImage(vk_world.sky_lightmap_white_image);
    if (!vk_world.sky_lightmap_white_set) {
        Com_SetLastError("Vulkan world: sky white lightmap descriptor unavailable");
        VK_World_Shutdown(ctx);
        return false;
    }

    VectorSet(vk_world.sky_axis, 0.0f, 0.0f, 1.0f);
    vk_world.sky_world_size = 4096.0f;
    vk_world.initialized = true;
    VK_World_ClearFrame();
    return true;
}

void VK_World_DestroySwapchainResources(vk_context_t *ctx)
{
    (void)ctx;

    if (!vk_world.initialized || !vk_world.ctx || !vk_world.ctx->device) {
        return;
    }

    if (vk_world.pipeline_alpha) {
        vkDestroyPipeline(vk_world.ctx->device, vk_world.pipeline_alpha, NULL);
        vk_world.pipeline_alpha = VK_NULL_HANDLE;
    }

    if (vk_world.pipeline_opaque) {
        vkDestroyPipeline(vk_world.ctx->device, vk_world.pipeline_opaque, NULL);
        vk_world.pipeline_opaque = VK_NULL_HANDLE;
    }

    vk_world.swapchain_ready = false;
}

bool VK_World_CreateSwapchainResources(vk_context_t *ctx)
{
    if (!vk_world.initialized || !ctx || !ctx->render_pass) {
        return false;
    }

    VK_World_DestroySwapchainResources(ctx);

    if (!VK_World_CreatePipelineVariant(ctx, false, &vk_world.pipeline_opaque)) {
        return false;
    }

    if (!VK_World_CreatePipelineVariant(ctx, true, &vk_world.pipeline_alpha)) {
        VK_World_DestroySwapchainResources(ctx);
        return false;
    }

    vk_world.swapchain_ready = true;
    return true;
}

void VK_World_Shutdown(vk_context_t *ctx)
{
    if (!vk_world.initialized) {
        return;
    }

    if (!ctx)
        ctx = vk_world.ctx;

    if (ctx && ctx->device)
        vkDeviceWaitIdle(ctx->device);

    VK_World_DestroySwapchainResources(ctx);
    VK_World_DestroyVertexBuffer();
    VK_World_DestroySkyVertexBuffer();
    VK_World_ClearCpuMesh();
    VK_World_ClearBatches();
    VK_World_ClearLightmap();
    VK_World_UnsetSky();
    VK_World_FreeBSP();

    if (vk_world.sky_lightmap_white_image) {
        VK_UI_UnregisterImage(vk_world.sky_lightmap_white_image);
        vk_world.sky_lightmap_white_image = 0;
        vk_world.sky_lightmap_white_set = VK_NULL_HANDLE;
    }

    if (ctx && ctx->device && vk_world.pipeline_layout) {
        vkDestroyPipelineLayout(ctx->device, vk_world.pipeline_layout, NULL);
        vk_world.pipeline_layout = VK_NULL_HANDLE;
    }

    memset(&vk_world, 0, sizeof(vk_world));
}

void VK_World_BeginRegistration(const char *map)
{
    if (!vk_world.initialized || !map || !*map) {
        return;
    }

    char bsp_path[MAX_QPATH];
    Q_concat(bsp_path, sizeof(bsp_path), "maps/", map, ".bsp");

    bsp_t *loaded = NULL;
    int ret = BSP_Load(bsp_path, &loaded);
    if (!loaded) {
        Com_Error(ERR_DROP, "VK_World_BeginRegistration: couldn't load %s: %s",
                  bsp_path, BSP_ErrorString(ret));
        return;
    }

    if (vk_world.bsp == loaded) {
        loaded->refcount--;
        return;
    }

    vkDeviceWaitIdle(vk_world.ctx->device);

    VK_World_DestroyVertexBuffer();
    VK_World_ClearCpuMesh();
    VK_World_ClearBatches();
    VK_World_ClearLightmap();
    VK_World_FreeBSP();
    vk_world.bsp = loaded;
    if (!VK_World_BuildWorldFaceMask(vk_world.bsp)) {
        VK_World_FreeBSP();
        Com_Error(ERR_DROP, "VK_World_BeginRegistration: failed to build world face mask for %s", map);
        return;
    }

    Q_strlcpy(vk_world.map_name, map, sizeof(vk_world.map_name));
    vk_world.sky_world_size = 4096.0f;
    if (vk_world.bsp->numnodes > 0) {
        vec3_t extents;
        VectorSubtract(vk_world.bsp->nodes[0].maxs, vk_world.bsp->nodes[0].mins, extents);
        float radius = VectorLength(extents) * 0.5f;
        vk_world.sky_world_size = max(1024.0f, radius * 2.0f);
    }

    vk_world_face_lightmap_t *face_lms = NULL;
    byte *atlas_rgba = NULL;
    int atlas_size = 0;
    if (!VK_World_BuildLightmapAtlas(vk_world.bsp, NULL, &face_lms, &atlas_size, &atlas_rgba)) {
        free(face_lms);
        free(atlas_rgba);
        Com_Error(ERR_DROP, "VK_World_BeginRegistration: failed to build world lightmaps for %s", map);
        return;
    }

    qhandle_t lightmap_handle = VK_UI_RegisterRawImage("**world_lightmap**",
                                                        atlas_size, atlas_size, atlas_rgba,
                                                        IT_WALL, IF_REPEAT);
    if (!lightmap_handle) {
        free(face_lms);
        free(atlas_rgba);
        Com_Error(ERR_DROP, "VK_World_BeginRegistration: failed to register world lightmaps for %s", map);
        return;
    }

    VkDescriptorSet lightmap_set = VK_UI_GetDescriptorSetForImage(lightmap_handle);
    if (!lightmap_set) {
        VK_UI_UnregisterImage(lightmap_handle);
        free(face_lms);
        free(atlas_rgba);
        Com_Error(ERR_DROP, "VK_World_BeginRegistration: failed to bind world lightmaps for %s", map);
        return;
    }

    vk_world_vertex_t *vertices = NULL;
    vk_world_batch_t *batches = NULL;
    uint32_t vertex_count = 0;
    uint32_t batch_count = 0;
    if (!VK_World_BuildMesh(&vertices, &vertex_count, &batches, &batch_count, face_lms, atlas_size)) {
        VK_UI_UnregisterImage(lightmap_handle);
        free(face_lms);
        free(atlas_rgba);
        free(vertices);
        free(batches);
        Com_Error(ERR_DROP, "VK_World_BeginRegistration: failed to build world mesh for %s", map);
        return;
    }

    if (!VK_World_UploadVertices(vertices, vertex_count)) {
        VK_UI_UnregisterImage(lightmap_handle);
        free(face_lms);
        free(atlas_rgba);
        free(vertices);
        free(batches);
        Com_Error(ERR_DROP, "VK_World_BeginRegistration: failed to upload world mesh for %s", map);
        return;
    }

    VK_World_ClearCpuMesh();
    vk_world.cpu_vertices = vertices;

    VK_World_ClearBatches();
    vk_world.batches = batches;
    vk_world.batch_count = batch_count;
    vk_world.lightmap_handle = lightmap_handle;
    vk_world.lightmap_descriptor_set = lightmap_set;
    vk_world.face_lms = face_lms;
    vk_world.lightmap_rgba = atlas_rgba;
    vk_world.lightmap_atlas_size = atlas_size;
    vk_world.style_cache_valid = false;
    vk_world.has_warp_vertices = VK_World_HasWarpVertices(vertices, vertex_count);
    vk_world.vertex_dynamic_dirty = false;

    vk_world.first_draw_logged = false;
    Com_DPrintf("VK_World_BeginRegistration: map=%s vertices=%u batches=%u\n",
                map, vertex_count, batch_count);
}

void VK_World_EndRegistration(void)
{
}

void VK_World_SetSky(const char *name, float rotate, bool autorotate, const vec3_t axis)
{
    if (!vk_world.initialized) {
        return;
    }

    bool had_sky_images = false;
    for (int i = 0; i < VK_WORLD_SKY_FACE_COUNT; i++) {
        if (vk_world.sky_images[i]) {
            had_sky_images = true;
            break;
        }
    }

    if (!name || !*name || (vk_drawsky && !vk_drawsky->integer)) {
        if (had_sky_images && vk_world.ctx && vk_world.ctx->device) {
            vkDeviceWaitIdle(vk_world.ctx->device);
        }
        VK_World_UnsetSky();
        return;
    }

    vec3_t normalized_axis = { 0.0f, 0.0f, 1.0f };
    if (axis && VectorNormalize2(axis, normalized_axis) >= 0.001f) {
        VectorCopy(normalized_axis, vk_world.sky_axis);
    } else {
        rotate = 0.0f;
        VectorSet(vk_world.sky_axis, 0.0f, 0.0f, 1.0f);
    }

    if (!rotate) {
        autorotate = false;
    }

    if (had_sky_images && vk_world.ctx && vk_world.ctx->device) {
        vkDeviceWaitIdle(vk_world.ctx->device);
    }
    VK_World_UnsetSky();

    qhandle_t new_images[VK_WORLD_SKY_FACE_COUNT] = { 0 };
    VkDescriptorSet new_sets[VK_WORLD_SKY_FACE_COUNT] = { VK_NULL_HANDLE };

    for (int i = 0; i < VK_WORLD_SKY_FACE_COUNT; i++) {
        char pathname[MAX_QPATH];
        if (Q_concat(pathname, sizeof(pathname), "env/", name, com_env_suf[i], ".tga") >= sizeof(pathname)) {
            for (int k = 0; k < i; k++) {
                if (new_images[k]) {
                    VK_UI_UnregisterImage(new_images[k]);
                }
            }
            Com_WPrintf("Vulkan sky path too long for %s\n", name);
            return;
        }

        new_images[i] = VK_UI_RegisterImage(pathname, IT_SKY, IF_NONE);
        if (!new_images[i]) {
            for (int k = 0; k <= i; k++) {
                if (new_images[k]) {
                    VK_UI_UnregisterImage(new_images[k]);
                }
            }
            Com_WPrintf("Vulkan failed to load sky face %s\n", pathname);
            return;
        }

        new_sets[i] = VK_UI_GetDescriptorSetForImage(new_images[i]);
        if (!new_sets[i]) {
            for (int k = 0; k <= i; k++) {
                if (new_images[k]) {
                    VK_UI_UnregisterImage(new_images[k]);
                }
            }
            Com_WPrintf("Vulkan failed to bind sky face %s\n", pathname);
            return;
        }
    }

    for (int i = 0; i < VK_WORLD_SKY_FACE_COUNT; i++) {
        vk_world.sky_images[i] = new_images[i];
        vk_world.sky_descriptor_sets[i] = new_sets[i];
    }

    vk_world.sky_rotate = rotate;
    vk_world.sky_autorotate = autorotate;
    vk_world.sky_enabled = true;

    Com_DPrintf("VK_World_SetSky: %s rotate=%.2f autorotate=%d axis=(%.2f %.2f %.2f)\n",
                name, rotate, autorotate ? 1 : 0,
                vk_world.sky_axis[0], vk_world.sky_axis[1], vk_world.sky_axis[2]);
}

static void VK_World_ComputeFramePush(const refdef_t *fd, renderer_view_push_t *out_push)
{
    float znear = 4.0f;
    float zfar = 8192.0f;

    if (vk_world.bsp && vk_world.bsp->numnodes > 0) {
        vec3_t extents;
        VectorSubtract(vk_world.bsp->nodes[0].maxs, vk_world.bsp->nodes[0].mins, extents);
        float radius = VectorLength(extents) * 0.5f;
        zfar = max(2048.0f, radius * 8.0f);
    }

    R_BuildViewPush(fd, znear, zfar, out_push);
}

void VK_World_RenderFrame(const refdef_t *fd)
{
    VK_World_ClearFrame();

    if (!vk_world.initialized || !fd) {
        return;
    }

    vk_world.current_fd = fd;

    if (!vk_world.bsp || !vk_world.vertex_count) {
        return;
    }

    if (fd->rdflags & RDF_NOWORLDMODEL) {
        return;
    }

    if (!VK_World_UpdateLightmapStyles(fd)) {
        return;
    }

    if (!VK_World_UpdateVertexLighting()) {
        return;
    }

    if (!VK_World_UpdateSkyGeometry(fd)) {
        return;
    }

    VK_World_ComputeFramePush(fd, &vk_world.frame_push);
    vk_world.frame_active = true;
}

void VK_World_Record(VkCommandBuffer cmd, const VkExtent2D *extent)
{
    if (!vk_world.initialized || !vk_world.swapchain_ready ||
        !vk_world.pipeline_opaque || !vk_world.pipeline_alpha ||
        !vk_world.frame_active || !vk_world.vertex_count || !vk_world.batch_count ||
        !vk_world.vertex_buffer || !vk_world.lightmap_descriptor_set || !extent) {
        return;
    }

    VkViewport viewport = {
        .x = 0.0f,
        .y = (float)extent->height,
        .width = (float)extent->width,
        .height = -(float)extent->height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = *extent,
    };

    const bool draw_sky = !vk_drawsky || (vk_drawsky->integer != 0);
    const bool render_skybox = draw_sky && vk_world.sky_enabled &&
                               vk_world.sky_vertex_count == VK_WORLD_SKY_TOTAL_VERTS &&
                               vk_world.sky_vertex_buffer;

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_world.vertex_buffer, &offset);
    vkCmdPushConstants(cmd, vk_world.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(vk_world.frame_push), &vk_world.frame_push);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vk_world.pipeline_layout,
                            1, 1, &vk_world.lightmap_descriptor_set,
                            0, NULL);

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    if (render_skybox) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_world.pipeline_opaque);

        VkDescriptorSet sky_lightmap_set = vk_world.sky_lightmap_white_set ?
            vk_world.sky_lightmap_white_set : vk_world.lightmap_descriptor_set;
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_world.pipeline_layout,
                                1, 1, &sky_lightmap_set,
                                0, NULL);

        VkDeviceSize sky_offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vk_world.sky_vertex_buffer, &sky_offset);

        for (int face = 0; face < VK_WORLD_SKY_FACE_COUNT; face++) {
            VkDescriptorSet sky_set = vk_world.sky_descriptor_sets[face];
            if (!sky_set) {
                continue;
            }

            vkCmdBindDescriptorSets(cmd,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    vk_world.pipeline_layout,
                                    0, 1, &sky_set,
                                    0, NULL);
            vkCmdDraw(cmd, VK_WORLD_SKY_VERTS_PER_FACE, 1,
                      face * VK_WORLD_SKY_VERTS_PER_FACE, 0);
        }

        offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vk_world.vertex_buffer, &offset);
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_world.pipeline_layout,
                                1, 1, &vk_world.lightmap_descriptor_set,
                                0, NULL);
    }

    for (int pass = 0; pass < 2; pass++) {
        const bool alpha_pass = (pass == 1);
        VkPipeline target_pipeline = alpha_pass ? vk_world.pipeline_alpha : vk_world.pipeline_opaque;
        VkPipeline current_pipeline = VK_NULL_HANDLE;
        VkDescriptorSet last_set = VK_NULL_HANDLE;

        for (uint32_t i = 0; i < vk_world.batch_count; i++) {
            const vk_world_batch_t *batch = &vk_world.batches[i];
            if (!batch->vertex_count || !batch->descriptor_set) {
                continue;
            }

            const bool is_alpha = (batch->flags & VK_WORLD_BATCH_ALPHA) != 0;
            const bool is_sky = (batch->flags & VK_WORLD_BATCH_SKY) != 0;
            if (is_sky) {
                if (!draw_sky || render_skybox) {
                    continue;
                }
            }
            if (is_alpha != alpha_pass) {
                continue;
            }

            if (current_pipeline != target_pipeline) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, target_pipeline);
                current_pipeline = target_pipeline;
                last_set = VK_NULL_HANDLE;
            }

            if (batch->descriptor_set != last_set) {
                vkCmdBindDescriptorSets(cmd,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        vk_world.pipeline_layout,
                                        0, 1, &batch->descriptor_set,
                                        0, NULL);
                last_set = batch->descriptor_set;
            }

            vkCmdDraw(cmd, batch->vertex_count, 1, batch->first_vertex, 0);
        }
    }

    if (!vk_world.first_draw_logged) {
        vk_world.first_draw_logged = true;
        Com_DPrintf("VK_World_Record: rendered map=%s vertices=%u batches=%u\n",
                    vk_world.map_name[0] ? vk_world.map_name : "<unknown>",
                    vk_world.vertex_count, vk_world.batch_count);
    }
}

void VK_World_LightPoint(const vec3_t origin, vec3_t light)
{
    if (!light)
        return;

    VectorSet(light, 1.0f, 1.0f, 1.0f);

    if (!origin || !vk_world.bsp) {
        return;
    }

    const bsp_t *bsp = vk_world.bsp;
    bool has_light = false;

    if (bsp->lightgrid.numleafs) {
        vec3_t point = {
            (origin[0] - bsp->lightgrid.mins[0]) * bsp->lightgrid.scale[0],
            (origin[1] - bsp->lightgrid.mins[1]) * bsp->lightgrid.scale[1],
            (origin[2] - bsp->lightgrid.mins[2]) * bsp->lightgrid.scale[2],
        };

        uint32_t point_i[3] = {
            (uint32_t)point[0],
            (uint32_t)point[1],
            (uint32_t)point[2],
        };

        vec3_t samples[8];
        vec3_t avg;
        int mask = 0;
        int numsamples = 0;
        VectorClear(avg);

        for (int i = 0; i < 8; i++) {
            uint32_t p[3] = {
                point_i[0] + ((i >> 0) & 1),
                point_i[1] + ((i >> 1) & 1),
                point_i[2] + ((i >> 2) & 1),
            };

            const lightgrid_sample_t *s = BSP_LookupLightgrid(&bsp->lightgrid, p);
            if (!s)
                continue;

            VectorClear(samples[i]);
            for (int j = 0; j < (int)bsp->lightgrid.numstyles && s->style != 255; j++, s++) {
                vec3_t shifted;
                VK_World_ShiftLightmapBytes(s->rgb, shifted);
                VectorMA(samples[i], VK_World_LightStyleWhite(s->style), shifted, samples[i]);
            }

            mask |= BIT(i);
            VectorAdd(avg, samples[i], avg);
            numsamples++;
        }

        if (mask) {
            has_light = true;
            if (mask != 255) {
                VectorScale(avg, 1.0f / max(numsamples, 1), avg);
                for (int i = 0; i < 8; i++) {
                    if (!(mask & BIT(i))) {
                        VectorCopy(avg, samples[i]);
                    }
                }
            }

            float fx = point[0] - point_i[0];
            float fy = point[1] - point_i[1];
            float fz = point[2] - point_i[2];
            float bx = 1.0f - fx;
            float by = 1.0f - fy;
            float bz = 1.0f - fz;
            vec3_t lerp_x[4];
            vec3_t lerp_y[2];

            LerpVector2(samples[0], samples[1], bx, fx, lerp_x[0]);
            LerpVector2(samples[2], samples[3], bx, fx, lerp_x[1]);
            LerpVector2(samples[4], samples[5], bx, fx, lerp_x[2]);
            LerpVector2(samples[6], samples[7], bx, fx, lerp_x[3]);

            LerpVector2(lerp_x[0], lerp_x[1], by, fy, lerp_y[0]);
            LerpVector2(lerp_x[2], lerp_x[3], by, fy, lerp_y[1]);

            LerpVector2(lerp_y[0], lerp_y[1], bz, fz, light);
            VK_World_AdjustLightColor(light);
        }
    }

    if (!has_light && bsp->lightmap) {
        vec3_t end = { origin[0], origin[1], origin[2] - 8192.0f };
        int nolm_mask = (bsp->has_bspx ? SURF_NOLM_MASK_REMASTER : SURF_NOLM_MASK_DEFAULT) |
                        SURF_TRANS_MASK;
        lightpoint_t point;
        BSP_LightPoint(&point, origin, end, bsp->nodes, nolm_mask);
        if (point.surf && VK_World_SampleFaceLightmap(point.surf, point.s, point.t, light)) {
            has_light = true;
        }
    }

    if (!has_light) {
        VectorSet(light, 0.75f, 0.75f, 0.75f);
    }

    VK_World_AddDynamicLightsAtPoint(origin, light);
    VK_World_ClampLight(light);
}

const bsp_t *VK_World_GetBsp(void)
{
    return vk_world.bsp;
}
