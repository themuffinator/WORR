/*
Copyright (C) 2026
*/

#include "vk_entity.h"

#include "vk_entity_spv.h"
#include "vk_ui.h"
#include "vk_world.h"
#include "renderer/view_setup.h"
#include "format/md2.h"
#include "format/sp2.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#if USE_MD5
#include <setjmp.h>
#endif

typedef enum {
    VK_MODEL_FREE = 0,
    VK_MODEL_SPRITE,
    VK_MODEL_MD2,
} vk_model_type_t;

typedef struct {
    int width;
    int height;
    int origin_x;
    int origin_y;
    qhandle_t image;
} vk_sprite_frame_t;

typedef struct {
    uint32_t num_frames;
    uint32_t num_vertices;
    uint32_t num_indices;
    float *positions;      // [num_frames][num_vertices][3]
    float *uv;             // [num_vertices][2]
    uint16_t *indices;     // [num_indices]
    qhandle_t *skins;
    uint32_t num_skins;
} vk_md2_t;

#if USE_MD5
enum {
    VK_MD5_MAX_JOINTS = 256,
    VK_MD5_MAX_MESHES = 32,
    VK_MD5_MAX_WEIGHTS = 8192,
    VK_MD5_MAX_FRAMES = 1024,
    VK_MD5_MAX_JOINTNAME = 48,
    VK_MD5_MAX_VERTICES = 65535,
    VK_MD5_MAX_INDICES = VK_MD5_MAX_VERTICES * 3,
};

typedef struct {
    uint16_t start;
    uint16_t count;
} vk_md5_vertex_t;

typedef struct {
    float st[2];
} vk_md5_tc_t;

typedef struct {
    vec3_t pos;
    float bias;
} vk_md5_weight_t;

typedef struct {
    uint32_t num_verts;
    uint32_t num_indices;
    uint32_t num_weights;
    vk_md5_vertex_t *vertices;
    vk_md5_tc_t *tcoords;
    uint16_t *indices;
    vk_md5_weight_t *weights;
    uint8_t *jointnums;
    qhandle_t shader_image;
} vk_md5_mesh_t;

typedef struct {
    vec3_t pos;
    float scale;
    quat_t orient;
    vec3_t axis[3];
} vk_md5_joint_t;

typedef struct {
    bool loaded;
    uint32_t num_meshes;
    uint32_t num_joints;
    uint32_t num_frames;
    vk_md5_mesh_t *meshes;
    vk_md5_joint_t *skeleton_frames; // [num_frames][num_joints]
} vk_md5_t;
#endif

typedef struct {
    vk_model_type_t type;
    char name[MAX_QPATH];
    int registration_sequence;
    union {
        struct {
            vk_sprite_frame_t *frames;
            uint32_t num_frames;
        } sprite;
        vk_md2_t md2;
    };
#if USE_MD5
    vk_md5_t md5;
#endif
} vk_model_t;

typedef struct {
    float pos[3];
    float uv[2];
    uint32_t color;
} vk_vertex_t;

typedef struct {
    uint32_t first_vertex;
    uint32_t vertex_count;
    VkDescriptorSet set;
    bool alpha;
    bool depth_hack;
    bool weapon_model;
} vk_batch_t;

typedef struct {
    vec3_t origin;
    vec3_t axis[3];
    vec3_t scaled_axis[3];
    vec3_t inv_scale;
} vk_entity_transform_t;

typedef struct {
    vk_context_t *ctx;
    bool initialized;
    bool swapchain_ready;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline_opaque;
    VkPipeline pipeline_alpha;
    VkPipeline pipeline_depthhack;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    size_t vertex_buffer_bytes;
    void *vertex_mapped;

    vk_vertex_t *vertices;
    uint32_t vertex_count;
    uint32_t vertex_capacity;

    vk_batch_t *batches;
    uint32_t batch_count;
    uint32_t batch_capacity;

    renderer_view_push_t frame_push;
    renderer_view_push_t frame_push_weapon;
    bool frame_active;
    bool frame_weapon_active;

    vk_model_t models[MAX_MODELS];
    int num_models;
    int registration_sequence;

    qhandle_t white_image;
    VkDescriptorSet white_set;
    qhandle_t particle_image;
    VkDescriptorSet particle_set;
    const bsp_t *bmodel_texture_bsp;
    qhandle_t *bmodel_texture_handles;
    VkDescriptorSet *bmodel_texture_sets;
    vec2_t *bmodel_texture_inv_sizes;
    bool *bmodel_texture_transparent;
    int bmodel_texture_count;

#if USE_MD5
    vk_md5_joint_t *temp_skeleton;
    uint32_t temp_skeleton_capacity;
#endif
} vk_entity_state_t;

static vk_entity_state_t vk_entity;
static cvar_t *vk_drawentities;
static cvar_t *vk_partscale;
#if USE_MD5
static cvar_t *vk_md5_load;
static cvar_t *vk_md5_use;
static cvar_t *vk_md5_distance;
static jmp_buf vk_md5_jmpbuf;
#endif

static VkDescriptorSet VK_Entity_SetForImage(qhandle_t handle);
static bool VK_Entity_EmitTri(const vk_vertex_t *a, const vk_vertex_t *b, const vk_vertex_t *c,
                              VkDescriptorSet set, bool alpha, bool depth_hack, bool weapon_model);
static inline float VK_Entity_Alpha(const entity_t *ent);
static color_t VK_Entity_LitColor(const entity_t *ent, bool fullbright);
static void VK_Entity_BuildTransform(const entity_t *ent, vk_entity_transform_t *out_transform);
static void VK_Entity_TransformPointWithTransform(const vk_entity_transform_t *transform,
                                                  const vec3_t local, vec3_t out);
static void VK_Entity_TransformPointInverseWithTransform(const vk_entity_transform_t *transform,
                                                         const vec3_t world, vec3_t out);
static qhandle_t VK_Entity_SelectMD2Skin(const entity_t *ent, const vk_md2_t *md2);
static qhandle_t VK_Entity_SelectMD5Skin(const entity_t *ent);
static bool VK_Entity_AddBspModel(const entity_t *ent, const refdef_t *fd, const bsp_t *bsp,
                                  bool depth_hack, bool weapon_model);
static bool VK_Entity_AddParticles(const refdef_t *fd, const vec3_t view_axis[3]);
static void VK_Entity_ClearBspTextureCache(void);
static bool VK_Entity_EnsureBspTextureCache(const bsp_t *bsp);
static void VK_Entity_BuildFramePush(const refdef_t *fd, const bsp_t *world_bsp,
                                     renderer_view_push_t *out_push);
static void VK_Entity_BuildWeaponFramePush(const refdef_t *fd, const bsp_t *world_bsp,
                                           renderer_view_push_t *out_push);
static bool VK_Entity_ResolveAnimationFrames(const refdef_t *fd, uint32_t num_frames,
                                             unsigned frame_in, unsigned oldframe_in,
                                             float backlerp_in,
                                             uint32_t *out_frame, uint32_t *out_oldframe,
                                             float *out_backlerp, float *out_frontlerp);

// Keep in sync with legacy GL particle primitive shape.
#define VK_ENTITY_PARTICLE_SIZE 1.70710678f
#define VK_ENTITY_PARTICLE_SCALE (1.0f / (2.0f * VK_ENTITY_PARTICLE_SIZE))
#define VK_ENTITY_PARTICLE_TEX_SIZE 16

static bool VK_Entity_Check(VkResult result, const char *what)
{
    if (result == VK_SUCCESS) {
        return true;
    }
    Com_SetLastError(va("Vulkan entity %s failed: %d", what, (int)result));
    return false;
}

#if USE_MD5
typedef struct {
    int parent;
    uint32_t flags;
    uint32_t start_index;
    char name[VK_MD5_MAX_JOINTNAME];
    bool scale_pos;
} vk_md5_joint_info_t;

typedef struct {
    vec3_t pos;
    quat_t orient;
} vk_md5_base_joint_t;

q_noreturn static void VK_MD5_ParseError(const char *text)
{
    Com_SetLastError(va("MD5 parse line %u: %s", com_linenum, text));
    longjmp(vk_md5_jmpbuf, -1);
}

static void VK_MD5_ParseExpect(const char **buffer, const char *expect)
{
    char *token = COM_Parse(buffer);
    if (strcmp(token, expect)) {
        VK_MD5_ParseError(va("expected \"%s\", got \"%s\"", expect, Com_MakePrintable(token)));
    }
}

static float VK_MD5_ParseFloat(const char **buffer)
{
    char *token = COM_Parse(buffer);
    char *endptr = NULL;

    float v = strtof(token, &endptr);
    if (!endptr || endptr == token || *endptr) {
        VK_MD5_ParseError(va("expected float, got \"%s\"", Com_MakePrintable(token)));
    }
    return v;
}

static uint32_t VK_MD5_ParseUint(const char **buffer, uint32_t min_v, uint32_t max_v)
{
    char *token = COM_Parse(buffer);
    char *endptr = NULL;

    unsigned long v = strtoul(token, &endptr, 10);
    if (!endptr || endptr == token || *endptr) {
        VK_MD5_ParseError(va("expected uint, got \"%s\"", Com_MakePrintable(token)));
    }
    if (v < min_v || v > max_v) {
        VK_MD5_ParseError(va("value out of range: %lu", v));
    }
    return (uint32_t)v;
}

static int32_t VK_MD5_ParseInt(const char **buffer, int32_t min_v, int32_t max_v)
{
    char *token = COM_Parse(buffer);
    char *endptr = NULL;

    long v = strtol(token, &endptr, 10);
    if (!endptr || endptr == token || *endptr) {
        VK_MD5_ParseError(va("expected int, got \"%s\"", Com_MakePrintable(token)));
    }
    if (v < min_v || v > max_v) {
        VK_MD5_ParseError(va("value out of range: %ld", v));
    }
    return (int32_t)v;
}

static void VK_MD5_ParseVector(const char **buffer, vec3_t out_vec)
{
    VK_MD5_ParseExpect(buffer, "(");
    out_vec[0] = VK_MD5_ParseFloat(buffer);
    out_vec[1] = VK_MD5_ParseFloat(buffer);
    out_vec[2] = VK_MD5_ParseFloat(buffer);
    VK_MD5_ParseExpect(buffer, ")");
}

static void VK_MD5_Free(vk_md5_t *md5)
{
    if (!md5) {
        return;
    }

    if (md5->meshes) {
        for (uint32_t i = 0; i < md5->num_meshes; i++) {
            vk_md5_mesh_t *mesh = &md5->meshes[i];
            free(mesh->vertices);
            free(mesh->tcoords);
            free(mesh->indices);
            free(mesh->weights);
            free(mesh->jointnums);
        }
    }

    free(md5->meshes);
    free(md5->skeleton_frames);
    memset(md5, 0, sizeof(*md5));
}

static bool VK_MD5_ParseMesh(vk_md5_t *out_md5, const char *source)
{
    if (!out_md5 || !source) {
        return false;
    }

    vk_md5_base_joint_t base_joints[VK_MD5_MAX_JOINTS];
    memset(base_joints, 0, sizeof(base_joints));

    vk_md5_t parsed = { 0 };
    const char *s = source;
    com_linenum = 1;

    if (setjmp(vk_md5_jmpbuf)) {
        VK_MD5_Free(&parsed);
        return false;
    }

    VK_MD5_ParseExpect(&s, "MD5Version");
    VK_MD5_ParseExpect(&s, "10");

    VK_MD5_ParseExpect(&s, "commandline");
    COM_SkipToken(&s);

    VK_MD5_ParseExpect(&s, "numJoints");
    parsed.num_joints = VK_MD5_ParseUint(&s, 1, VK_MD5_MAX_JOINTS);

    VK_MD5_ParseExpect(&s, "numMeshes");
    parsed.num_meshes = VK_MD5_ParseUint(&s, 1, VK_MD5_MAX_MESHES);

    VK_MD5_ParseExpect(&s, "joints");
    VK_MD5_ParseExpect(&s, "{");
    for (uint32_t i = 0; i < parsed.num_joints; i++) {
        COM_SkipToken(&s); // name
        COM_SkipToken(&s); // parent
        VK_MD5_ParseVector(&s, base_joints[i].pos);
        VK_MD5_ParseVector(&s, base_joints[i].orient);
        Quat_ComputeW(base_joints[i].orient);
    }
    VK_MD5_ParseExpect(&s, "}");

    parsed.meshes = calloc(parsed.num_meshes, sizeof(*parsed.meshes));
    if (!parsed.meshes) {
        VK_MD5_ParseError("out of memory allocating MD5 meshes");
    }

    for (uint32_t i = 0; i < parsed.num_meshes; i++) {
        vk_md5_mesh_t *mesh = &parsed.meshes[i];

        VK_MD5_ParseExpect(&s, "mesh");
        VK_MD5_ParseExpect(&s, "{");

        VK_MD5_ParseExpect(&s, "shader");
        const char *shader_token = COM_Parse(&s);
        if (shader_token && *shader_token) {
            char shader_name[MAX_QPATH];
            Q_strlcpy(shader_name, shader_token, sizeof(shader_name));
            FS_NormalizePath(shader_name);
            mesh->shader_image = VK_UI_RegisterImage(shader_name, IT_SKIN, IF_NONE);
        }

        VK_MD5_ParseExpect(&s, "numverts");
        mesh->num_verts = VK_MD5_ParseUint(&s, 0, VK_MD5_MAX_VERTICES);
        if (mesh->num_verts) {
            mesh->vertices = calloc(mesh->num_verts, sizeof(*mesh->vertices));
            mesh->tcoords = calloc(mesh->num_verts, sizeof(*mesh->tcoords));
            if (!mesh->vertices || !mesh->tcoords) {
                VK_MD5_ParseError("out of memory allocating MD5 vertices");
            }
        }

        for (uint32_t v = 0; v < mesh->num_verts; v++) {
            VK_MD5_ParseExpect(&s, "vert");
            uint32_t vert_index = VK_MD5_ParseUint(&s, 0, mesh->num_verts - 1);

            VK_MD5_ParseExpect(&s, "(");
            mesh->tcoords[vert_index].st[0] = VK_MD5_ParseFloat(&s);
            mesh->tcoords[vert_index].st[1] = VK_MD5_ParseFloat(&s);
            VK_MD5_ParseExpect(&s, ")");

            mesh->vertices[vert_index].start = (uint16_t)VK_MD5_ParseUint(&s, 0, UINT16_MAX);
            mesh->vertices[vert_index].count = (uint16_t)VK_MD5_ParseUint(&s, 0, UINT16_MAX);
        }

        VK_MD5_ParseExpect(&s, "numtris");
        uint32_t num_tris = VK_MD5_ParseUint(&s, 0, VK_MD5_MAX_INDICES / 3);
        if (num_tris && !mesh->num_verts) {
            VK_MD5_ParseError("mesh has triangles but no vertices");
        }
        mesh->num_indices = num_tris * 3;
        if (mesh->num_indices) {
            mesh->indices = calloc(mesh->num_indices, sizeof(*mesh->indices));
            if (!mesh->indices) {
                VK_MD5_ParseError("out of memory allocating MD5 indices");
            }
        }
        for (uint32_t t = 0; t < num_tris; t++) {
            VK_MD5_ParseExpect(&s, "tri");
            uint32_t tri_index = VK_MD5_ParseUint(&s, 0, num_tris - 1);
            for (int k = 0; k < 3; k++) {
                mesh->indices[tri_index * 3 + k] = (uint16_t)VK_MD5_ParseUint(&s, 0, mesh->num_verts ? mesh->num_verts - 1 : 0);
            }
        }

        VK_MD5_ParseExpect(&s, "numweights");
        mesh->num_weights = VK_MD5_ParseUint(&s, 0, VK_MD5_MAX_WEIGHTS);
        if (mesh->num_weights) {
            mesh->weights = calloc(mesh->num_weights, sizeof(*mesh->weights));
            mesh->jointnums = calloc(mesh->num_weights, sizeof(*mesh->jointnums));
            if (!mesh->weights || !mesh->jointnums) {
                VK_MD5_ParseError("out of memory allocating MD5 weights");
            }
        }
        for (uint32_t w = 0; w < mesh->num_weights; w++) {
            VK_MD5_ParseExpect(&s, "weight");
            uint32_t weight_index = VK_MD5_ParseUint(&s, 0, mesh->num_weights - 1);
            mesh->jointnums[weight_index] = (uint8_t)VK_MD5_ParseUint(&s, 0, parsed.num_joints - 1);
            mesh->weights[weight_index].bias = VK_MD5_ParseFloat(&s);
            VK_MD5_ParseVector(&s, mesh->weights[weight_index].pos);
        }

        VK_MD5_ParseExpect(&s, "}");

        for (uint32_t v = 0; v < mesh->num_verts; v++) {
            const vk_md5_vertex_t *vert = &mesh->vertices[v];
            if ((uint32_t)vert->start + (uint32_t)vert->count > mesh->num_weights) {
                VK_MD5_ParseError("invalid MD5 vertex weight span");
            }
        }
    }

    parsed.loaded = true;
    *out_md5 = parsed;
    return true;
}

static void VK_MD5_BuildFrameSkeleton(const vk_md5_joint_info_t *joint_infos,
                                      const vk_md5_base_joint_t *base_frame,
                                      const float *frame_data,
                                      vk_md5_joint_t *out_skeleton,
                                      uint32_t num_joints)
{
    for (uint32_t i = 0; i < num_joints; i++) {
        vec3_t animated_pos;
        quat_t animated_orient;
        VectorCopy(base_frame[i].pos, animated_pos);
        Vector4Copy(base_frame[i].orient, animated_orient);

        int component_index = 0;
        uint32_t flags = joint_infos[i].flags;
        uint32_t start = joint_infos[i].start_index;

        if (flags & BIT(0))
            animated_pos[0] = frame_data[start + component_index++];
        if (flags & BIT(1))
            animated_pos[1] = frame_data[start + component_index++];
        if (flags & BIT(2))
            animated_pos[2] = frame_data[start + component_index++];
        if (flags & BIT(3))
            animated_orient[0] = frame_data[start + component_index++];
        if (flags & BIT(4))
            animated_orient[1] = frame_data[start + component_index++];
        if (flags & BIT(5))
            animated_orient[2] = frame_data[start + component_index++];

        Quat_ComputeW(animated_orient);

        vk_md5_joint_t *joint = &out_skeleton[i];
        if (joint_infos[i].scale_pos) {
            VectorScale(animated_pos, joint->scale, animated_pos);
        }

        if (joint_infos[i].parent < 0) {
            VectorCopy(animated_pos, joint->pos);
            Vector4Copy(animated_orient, joint->orient);
            Quat_ToAxis(joint->orient, joint->axis);
            continue;
        }

        const vk_md5_joint_t *parent = &out_skeleton[joint_infos[i].parent];
        vec3_t rotated_pos;
        Quat_RotatePoint(parent->orient, animated_pos, rotated_pos);
        VectorAdd(parent->pos, rotated_pos, joint->pos);

        Quat_MultiplyQuat(parent->orient, animated_orient, joint->orient);
        Quat_Normalize(joint->orient);
        Quat_ToAxis(joint->orient, joint->axis);
    }
}

static void VK_MD5_LoadScales(vk_md5_t *md5, const char *path, vk_md5_joint_info_t *joint_infos)
{
    if (!md5 || !path || !joint_infos) {
        return;
    }

    jsmn_parser parser;
    jsmntok_t tokens[4096];
    char *data = NULL;
    int len = FS_LoadFile(path, (void **)&data);
    if (!data) {
        if (len != Q_ERR(ENOENT)) {
            Com_EPrintf("Couldn't load %s: %s\n", path, Q_ErrorString(len));
        }
        return;
    }

    jsmn_init(&parser);
    int ret = jsmn_parse(&parser, data, (size_t)len, tokens, q_countof(tokens));
    if (ret < 0) {
        goto fail;
    }
    if (ret == 0) {
        goto skip;
    }

    const jsmntok_t *tok = &tokens[0];
    if (tok->type != JSMN_OBJECT) {
        goto fail;
    }

    const jsmntok_t *end = tokens + ret;
    tok++;

    while (tok < end) {
        if (tok->type != JSMN_STRING) {
            goto fail;
        }

        int joint_id = -1;
        const char *joint_name = data + tok->start;
        data[tok->end] = 0;
        for (uint32_t i = 0; i < md5->num_joints; i++) {
            if (!strcmp(joint_name, joint_infos[i].name)) {
                joint_id = (int)i;
                break;
            }
        }

        if (joint_id == -1) {
            Com_WPrintf("No such joint \"%s\" in %s\n", Com_MakePrintable(joint_name), path);
        }

        if (++tok == end || tok->type != JSMN_OBJECT) {
            goto fail;
        }

        int num_keys = tok->size;
        if (end - ++tok < num_keys * 2) {
            goto fail;
        }

        for (int i = 0; i < num_keys; i++) {
            const jsmntok_t *key = tok++;
            const jsmntok_t *val = tok++;
            if (key->type != JSMN_STRING || val->type != JSMN_PRIMITIVE) {
                goto fail;
            }

            if (joint_id == -1) {
                continue;
            }

            data[key->end] = 0;
            const char *key_text = data + key->start;
            if (!strcmp(key_text, "scale_positions")) {
                joint_infos[joint_id].scale_pos = data[val->start] == 't';
            } else {
                unsigned frame_id = Q_atoi(key_text);
                if (frame_id < md5->num_frames) {
                    md5->skeleton_frames[(size_t)frame_id * md5->num_joints + (uint32_t)joint_id].scale =
                        Q_atof(data + val->start);
                } else {
                    Com_WPrintf("No such frame %u in %s\n", frame_id, path);
                }
            }
        }
    }

skip:
    FS_FreeFile(data);
    return;

fail:
    Com_EPrintf("Couldn't load %s: Invalid JSON data\n", path);
    FS_FreeFile(data);
}

static bool VK_MD5_ParseAnim(vk_md5_t *md5, const char *source, const char *path)
{
    if (!md5 || !source) {
        return false;
    }

    vk_md5_joint_info_t joint_infos[VK_MD5_MAX_JOINTS];
    vk_md5_base_joint_t base_frame[VK_MD5_MAX_JOINTS];
    float anim_frame_data[VK_MD5_MAX_JOINTS * 6];
    memset(joint_infos, 0, sizeof(joint_infos));
    memset(base_frame, 0, sizeof(base_frame));
    memset(anim_frame_data, 0, sizeof(anim_frame_data));

    const char *s = source;
    com_linenum = 1;

    if (setjmp(vk_md5_jmpbuf)) {
        return false;
    }

    VK_MD5_ParseExpect(&s, "MD5Version");
    VK_MD5_ParseExpect(&s, "10");

    VK_MD5_ParseExpect(&s, "commandline");
    COM_SkipToken(&s);

    VK_MD5_ParseExpect(&s, "numFrames");
    md5->num_frames = VK_MD5_ParseUint(&s, 1, VK_MD5_MAX_FRAMES);

    VK_MD5_ParseExpect(&s, "numJoints");
    uint32_t num_joints = VK_MD5_ParseUint(&s, 1, VK_MD5_MAX_JOINTS);
    if (num_joints != md5->num_joints) {
        VK_MD5_ParseError("numJoints mismatch between mesh and animation");
    }

    VK_MD5_ParseExpect(&s, "frameRate");
    COM_SkipToken(&s);

    VK_MD5_ParseExpect(&s, "numAnimatedComponents");
    uint32_t num_animated_components = VK_MD5_ParseUint(&s, 0, md5->num_joints * 6);

    VK_MD5_ParseExpect(&s, "hierarchy");
    VK_MD5_ParseExpect(&s, "{");
    for (uint32_t i = 0; i < md5->num_joints; i++) {
        COM_ParseToken(&s, joint_infos[i].name, sizeof(joint_infos[i].name), PARSE_FLAG_NONE);
        joint_infos[i].parent = VK_MD5_ParseInt(&s, -1, (int32_t)md5->num_joints - 1);
        joint_infos[i].flags = VK_MD5_ParseUint(&s, 0, UINT32_MAX);
        joint_infos[i].start_index = VK_MD5_ParseUint(&s, 0, num_animated_components);
        joint_infos[i].scale_pos = false;

        int num_components = 0;
        for (int c = 0; c < 6; c++) {
            if (joint_infos[i].flags & BIT(c)) {
                num_components++;
            }
        }

        if (joint_infos[i].start_index + (uint32_t)num_components > num_animated_components) {
            VK_MD5_ParseError("invalid hierarchy animated component span");
        }
        if (joint_infos[i].parent >= (int)i) {
            VK_MD5_ParseError("invalid hierarchy parent ordering");
        }
    }
    VK_MD5_ParseExpect(&s, "}");

    VK_MD5_ParseExpect(&s, "bounds");
    VK_MD5_ParseExpect(&s, "{");
    for (uint32_t i = 0; i < md5->num_frames; i++) {
        vec3_t ignore_min;
        vec3_t ignore_max;
        VK_MD5_ParseVector(&s, ignore_min);
        VK_MD5_ParseVector(&s, ignore_max);
    }
    VK_MD5_ParseExpect(&s, "}");

    VK_MD5_ParseExpect(&s, "baseframe");
    VK_MD5_ParseExpect(&s, "{");
    for (uint32_t i = 0; i < md5->num_joints; i++) {
        VK_MD5_ParseVector(&s, base_frame[i].pos);
        VK_MD5_ParseVector(&s, base_frame[i].orient);
        Quat_ComputeW(base_frame[i].orient);
    }
    VK_MD5_ParseExpect(&s, "}");

    md5->skeleton_frames = calloc((size_t)md5->num_frames * (size_t)md5->num_joints,
                                  sizeof(*md5->skeleton_frames));
    if (!md5->skeleton_frames) {
        VK_MD5_ParseError("out of memory allocating MD5 skeleton frames");
    }

    for (uint32_t i = 0; i < md5->num_frames * md5->num_joints; i++) {
        md5->skeleton_frames[i].scale = 1.0f;
    }

    if (path && *path) {
        char scale_path[MAX_QPATH];
        if (COM_StripExtension(scale_path, path, sizeof(scale_path)) < sizeof(scale_path) &&
            Q_strlcat(scale_path, ".md5scale", sizeof(scale_path)) < sizeof(scale_path)) {
            VK_MD5_LoadScales(md5, scale_path, joint_infos);
        } else {
            Com_WPrintf("MD5 scale path too long: %s\n", scale_path);
        }
    }

    for (uint32_t frame = 0; frame < md5->num_frames; frame++) {
        VK_MD5_ParseExpect(&s, "frame");
        uint32_t frame_index = VK_MD5_ParseUint(&s, 0, md5->num_frames - 1);
        VK_MD5_ParseExpect(&s, "{");
        for (uint32_t i = 0; i < num_animated_components; i++) {
            anim_frame_data[i] = VK_MD5_ParseFloat(&s);
        }
        VK_MD5_ParseExpect(&s, "}");

        VK_MD5_BuildFrameSkeleton(joint_infos, base_frame, anim_frame_data,
                                  &md5->skeleton_frames[(size_t)frame_index * md5->num_joints],
                                  md5->num_joints);
    }
    return true;
}

static bool VK_Entity_LoadMD5Replacement(vk_model_t *model)
{
    if (!model || model->type != VK_MODEL_MD2 || !vk_md5_load || !vk_md5_load->integer) {
        return false;
    }

    char model_name[MAX_QPATH];
    char base_path[MAX_QPATH];
    char mesh_path[MAX_QPATH];
    char anim_path[MAX_QPATH];

    COM_SplitPath(model->name, model_name, sizeof(model_name), base_path, sizeof(base_path), true);
    if (Q_concat(mesh_path, sizeof(mesh_path), base_path, "md5/", model_name, ".md5mesh") >= sizeof(mesh_path) ||
        Q_concat(anim_path, sizeof(anim_path), base_path, "md5/", model_name, ".md5anim") >= sizeof(anim_path)) {
        return false;
    }

    if (!FS_FileExists(mesh_path) || !FS_FileExists(anim_path)) {
        return false;
    }

    char *mesh_data = NULL;
    char *anim_data = NULL;
    int mesh_len = FS_LoadFile(mesh_path, (void **)&mesh_data);
    if (!mesh_data || mesh_len < 0) {
        if (mesh_data) {
            FS_FreeFile(mesh_data);
        }
        return false;
    }

    int anim_len = FS_LoadFile(anim_path, (void **)&anim_data);
    if (!anim_data || anim_len < 0) {
        FS_FreeFile(mesh_data);
        if (anim_data) {
            FS_FreeFile(anim_data);
        }
        return false;
    }

    vk_md5_t parsed = { 0 };
    bool ok = VK_MD5_ParseMesh(&parsed, mesh_data) && VK_MD5_ParseAnim(&parsed, anim_data, anim_path);
    FS_FreeFile(mesh_data);
    FS_FreeFile(anim_data);

    if (!ok) {
        VK_MD5_Free(&parsed);
        return false;
    }

    if (model->md2.num_frames && parsed.num_frames < model->md2.num_frames) {
        Com_WPrintf("%s has less frames than %s (%u < %u)\n",
                    anim_path, model->name, parsed.num_frames, model->md2.num_frames);
    }

    VK_MD5_Free(&model->md5);
    model->md5 = parsed;
    model->md5.loaded = true;

    Com_DPrintf("Vulkan MD5 replacement loaded for %s (%u meshes, %u joints, %u frames)\n",
                model->name, model->md5.num_meshes, model->md5.num_joints, model->md5.num_frames);
    return true;
}

static bool VK_Entity_EnsureTempSkeleton(uint32_t num_joints)
{
    if (num_joints <= vk_entity.temp_skeleton_capacity) {
        return true;
    }

    vk_md5_joint_t *new_skeleton = realloc(vk_entity.temp_skeleton, (size_t)num_joints * sizeof(*new_skeleton));
    if (!new_skeleton) {
        Com_SetLastError("Vulkan entity: out of memory for temporary MD5 skeleton");
        return false;
    }

    vk_entity.temp_skeleton = new_skeleton;
    vk_entity.temp_skeleton_capacity = num_joints;
    return true;
}

static const vk_md5_joint_t *VK_Entity_LerpMD5Skeleton(const vk_md5_t *md5,
                                                        uint32_t oldframe, uint32_t frame,
                                                        float backlerp, float frontlerp)
{
    if (!md5 || !md5->skeleton_frames || !md5->num_joints || !md5->num_frames) {
        return NULL;
    }

    oldframe %= md5->num_frames;
    frame %= md5->num_frames;
    if (oldframe == frame) {
        return &md5->skeleton_frames[(size_t)frame * md5->num_joints];
    }

    if (!VK_Entity_EnsureTempSkeleton(md5->num_joints)) {
        return NULL;
    }

    const vk_md5_joint_t *skel_a = &md5->skeleton_frames[(size_t)oldframe * md5->num_joints];
    const vk_md5_joint_t *skel_b = &md5->skeleton_frames[(size_t)frame * md5->num_joints];
    vk_md5_joint_t *out = vk_entity.temp_skeleton;

    for (uint32_t i = 0; i < md5->num_joints; i++) {
        out[i].scale = skel_b[i].scale;
        LerpVector2(skel_a[i].pos, skel_b[i].pos, backlerp, frontlerp, out[i].pos);
        Quat_SLerp(skel_a[i].orient, skel_b[i].orient, backlerp, frontlerp, out[i].orient);
        Quat_ToAxis(out[i].orient, out[i].axis);
    }

    return out;
}

static bool VK_Entity_ShouldUseMD5(const entity_t *ent, const refdef_t *fd, const vk_model_t *model)
{
    if (!ent || !model || !model->md5.loaded || !vk_md5_use || !vk_md5_use->integer) {
        return false;
    }
    if (ent->flags & RF_NO_LOD) {
        return true;
    }
    if (!fd || !vk_md5_distance || vk_md5_distance->value <= 0.0f) {
        return true;
    }
    return Distance(ent->origin, fd->vieworg) <= vk_md5_distance->value;
}

static void VK_Entity_MD5VertexPosition(const vk_md5_mesh_t *mesh,
                                        const vk_md5_joint_t *skeleton,
                                        uint32_t num_joints,
                                        uint32_t vertex_index,
                                        vec3_t out_pos)
{
    VectorClear(out_pos);
    if (!mesh || !skeleton || vertex_index >= mesh->num_verts) {
        return;
    }

    const vk_md5_vertex_t *vert = &mesh->vertices[vertex_index];
    for (uint32_t i = 0; i < vert->count; i++) {
        uint32_t weight_index = (uint32_t)vert->start + i;
        if (weight_index >= mesh->num_weights) {
            break;
        }

        uint32_t joint_index = mesh->jointnums[weight_index];
        if (joint_index >= num_joints) {
            continue;
        }

        const vk_md5_weight_t *weight = &mesh->weights[weight_index];
        const vk_md5_joint_t *joint = &skeleton[joint_index];

        vec3_t rotated;
        vec3_t weighted;
        VectorRotate(weight->pos, joint->axis, rotated);
        VectorMA(joint->pos, joint->scale, rotated, weighted);
        VectorMA(out_pos, weight->bias, weighted, out_pos);
    }
}

static bool VK_Entity_AddMD5(const entity_t *ent, const refdef_t *fd, const vk_model_t *model,
                             bool depth_hack, bool weapon_model)
{
    if (!ent || !model || !model->md5.loaded) {
        return true;
    }

    const vk_md5_t *md5 = &model->md5;
    if (!md5->num_meshes || !md5->num_joints || !md5->num_frames || !md5->meshes || !md5->skeleton_frames) {
        return true;
    }

    uint32_t frame = 0;
    uint32_t oldframe = 0;
    float backlerp = 0.0f;
    float frontlerp = 1.0f;
    uint32_t frame_count = model->md2.num_frames ? model->md2.num_frames : md5->num_frames;
    if (!VK_Entity_ResolveAnimationFrames(fd, frame_count, ent->frame, ent->oldframe,
                                          ent->backlerp,
                                          &frame, &oldframe, &backlerp, &frontlerp)) {
        return true;
    }

    const vk_md5_joint_t *skeleton = VK_Entity_LerpMD5Skeleton(md5, oldframe, frame, backlerp, frontlerp);
    if (!skeleton) {
        return false;
    }

    vk_entity_transform_t transform;
    VK_Entity_BuildTransform(ent, &transform);

    color_t color = VK_Entity_LitColor(ent, false);
    qhandle_t preferred_skin = VK_Entity_SelectMD5Skin(ent);

    for (uint32_t i = 0; i < md5->num_meshes; i++) {
        const vk_md5_mesh_t *mesh = &md5->meshes[i];
        if (!mesh->num_indices || !mesh->vertices || !mesh->indices || !mesh->tcoords ||
            !mesh->weights || !mesh->jointnums) {
            continue;
        }

        qhandle_t skin = preferred_skin ? preferred_skin : mesh->shader_image;
        VkDescriptorSet set = VK_Entity_SetForImage(skin);
        bool alpha = VK_Entity_Alpha(ent) < 1.0f;

        for (uint32_t tri_idx = 0; tri_idx < mesh->num_indices; tri_idx += 3) {
            vk_vertex_t tri[3];
            bool valid_tri = true;

            for (uint32_t j = 0; j < 3; j++) {
                uint32_t idx = mesh->indices[tri_idx + j];
                if (idx >= mesh->num_verts) {
                    valid_tri = false;
                    break;
                }

                vec3_t local_pos;
                VK_Entity_MD5VertexPosition(mesh, skeleton, md5->num_joints, idx, local_pos);
                VK_Entity_TransformPointWithTransform(&transform, local_pos, tri[j].pos);
                tri[j].uv[0] = mesh->tcoords[idx].st[0];
                tri[j].uv[1] = mesh->tcoords[idx].st[1];
                tri[j].color = color.u32;
            }

            if (!valid_tri) {
                continue;
            }

            if (!VK_Entity_EmitTri(&tri[0], &tri[1], &tri[2], set, alpha, depth_hack, weapon_model)) {
                return false;
            }
        }
    }

    return true;
}
#endif

static void VK_Entity_FreeModel(vk_model_t *model)
{
    if (!model) {
        return;
    }
    if (model->type == VK_MODEL_SPRITE) {
        free(model->sprite.frames);
    } else if (model->type == VK_MODEL_MD2) {
        free(model->md2.positions);
        free(model->md2.uv);
        free(model->md2.indices);
        free(model->md2.skins);
    }
#if USE_MD5
    VK_MD5_Free(&model->md5);
#endif
    memset(model, 0, sizeof(*model));
}

static void VK_Entity_FreeAllModels(void)
{
    for (int i = 0; i < vk_entity.num_models; i++) {
        VK_Entity_FreeModel(&vk_entity.models[i]);
    }
    vk_entity.num_models = 0;
}

static uint32_t VK_Entity_FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(vk_entity.ctx->physical_device, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & BIT(i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

static void VK_Entity_DestroyVertexBuffer(void)
{
    if (!vk_entity.ctx || !vk_entity.ctx->device) {
        return;
    }
    VkDevice dev = vk_entity.ctx->device;
    if (vk_entity.vertex_mapped && vk_entity.vertex_memory) {
        vkUnmapMemory(dev, vk_entity.vertex_memory);
        vk_entity.vertex_mapped = NULL;
    }
    if (vk_entity.vertex_buffer) {
        vkDestroyBuffer(dev, vk_entity.vertex_buffer, NULL);
        vk_entity.vertex_buffer = VK_NULL_HANDLE;
    }
    if (vk_entity.vertex_memory) {
        vkFreeMemory(dev, vk_entity.vertex_memory, NULL);
        vk_entity.vertex_memory = VK_NULL_HANDLE;
    }
    vk_entity.vertex_buffer_bytes = 0;
}

static bool VK_Entity_EnsureVertexBuffer(size_t bytes)
{
    if (vk_entity.vertex_buffer && vk_entity.vertex_memory && bytes <= vk_entity.vertex_buffer_bytes) {
        return true;
    }

    VK_Entity_DestroyVertexBuffer();

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bytes,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    if (!VK_Entity_Check(vkCreateBuffer(vk_entity.ctx->device, &buffer_info, NULL, &vk_entity.vertex_buffer),
                         "vkCreateBuffer")) {
        return false;
    }

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(vk_entity.ctx->device, vk_entity.vertex_buffer, &req);
    uint32_t memory_index = VK_Entity_FindMemoryType(req.memoryTypeBits,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memory_index == UINT32_MAX) {
        Com_SetLastError("Vulkan entity: memory type not found");
        VK_Entity_DestroyVertexBuffer();
        return false;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = memory_index,
    };
    if (!VK_Entity_Check(vkAllocateMemory(vk_entity.ctx->device, &alloc_info, NULL, &vk_entity.vertex_memory),
                         "vkAllocateMemory")) {
        VK_Entity_DestroyVertexBuffer();
        return false;
    }

    if (!VK_Entity_Check(vkBindBufferMemory(vk_entity.ctx->device, vk_entity.vertex_buffer,
                                            vk_entity.vertex_memory, 0),
                         "vkBindBufferMemory")) {
        VK_Entity_DestroyVertexBuffer();
        return false;
    }

    vk_entity.vertex_buffer_bytes = bytes;
    if (!VK_Entity_Check(vkMapMemory(vk_entity.ctx->device, vk_entity.vertex_memory,
                                     0, vk_entity.vertex_buffer_bytes, 0,
                                     &vk_entity.vertex_mapped),
                         "vkMapMemory(entity persistent)")) {
        VK_Entity_DestroyVertexBuffer();
        return false;
    }
    return true;
}

static bool VK_Entity_EnsureVertexCapacity(uint32_t needed)
{
    if (needed <= vk_entity.vertex_capacity) {
        return true;
    }
    uint32_t cap = vk_entity.vertex_capacity ? vk_entity.vertex_capacity : 4096;
    while (cap < needed) {
        cap *= 2;
    }
    vk_vertex_t *new_buf = realloc(vk_entity.vertices, (size_t)cap * sizeof(*new_buf));
    if (!new_buf) {
        Com_SetLastError("Vulkan entity: out of memory for vertices");
        return false;
    }
    vk_entity.vertices = new_buf;
    vk_entity.vertex_capacity = cap;
    return true;
}

static bool VK_Entity_EnsureBatchCapacity(uint32_t needed)
{
    if (needed <= vk_entity.batch_capacity) {
        return true;
    }
    uint32_t cap = vk_entity.batch_capacity ? vk_entity.batch_capacity : 512;
    while (cap < needed) {
        cap *= 2;
    }
    vk_batch_t *new_buf = realloc(vk_entity.batches, (size_t)cap * sizeof(*new_buf));
    if (!new_buf) {
        Com_SetLastError("Vulkan entity: out of memory for batches");
        return false;
    }
    vk_entity.batches = new_buf;
    vk_entity.batch_capacity = cap;
    return true;
}

static VkDescriptorSet VK_Entity_SetForImage(qhandle_t handle)
{
    VkDescriptorSet set = VK_UI_GetDescriptorSetForImage(handle);
    return set ? set : vk_entity.white_set;
}

static bool VK_Entity_EmitTri(const vk_vertex_t *a, const vk_vertex_t *b, const vk_vertex_t *c,
                              VkDescriptorSet set, bool alpha, bool depth_hack, bool weapon_model)
{
    if (!set) {
        return true;
    }

    if (!VK_Entity_EnsureVertexCapacity(vk_entity.vertex_count + 3)) {
        return false;
    }

    uint32_t first = vk_entity.vertex_count;
    vk_entity.vertices[vk_entity.vertex_count++] = *a;
    vk_entity.vertices[vk_entity.vertex_count++] = *b;
    vk_entity.vertices[vk_entity.vertex_count++] = *c;

    vk_batch_t *batch = (vk_entity.batch_count > 0) ? &vk_entity.batches[vk_entity.batch_count - 1] : NULL;
    if (!batch || batch->set != set || batch->alpha != alpha ||
        batch->depth_hack != depth_hack || batch->weapon_model != weapon_model) {
        if (!VK_Entity_EnsureBatchCapacity(vk_entity.batch_count + 1)) {
            return false;
        }
        batch = &vk_entity.batches[vk_entity.batch_count++];
        *batch = (vk_batch_t){
            .first_vertex = first,
            .vertex_count = 0,
            .set = set,
            .alpha = alpha,
            .depth_hack = depth_hack,
            .weapon_model = weapon_model,
        };
    }
    batch->vertex_count += 3;
    return true;
}

static inline float VK_Entity_Alpha(const entity_t *ent)
{
    if (!(ent->flags & RF_TRANSLUCENT)) {
        return 1.0f;
    }
    return Q_clipf(ent->alpha, 0.0f, 1.0f);
}

static color_t VK_Entity_LitColor(const entity_t *ent, bool fullbright)
{
    float alpha = VK_Entity_Alpha(ent);
    if (fullbright || (ent->flags & RF_FULLBRIGHT)) {
        return COLOR_RGBA(255, 255, 255, (uint8_t)(alpha * 255.0f + 0.5f));
    }

    vec3_t light;
    VK_World_LightPoint(ent->origin, light);
    if (ent->flags & RF_MINLIGHT) {
        light[0] = max(light[0], 0.1f);
        light[1] = max(light[1], 0.1f);
        light[2] = max(light[2], 0.1f);
    }

    return COLOR_RGBA((uint8_t)min(255, (int)(light[0] * 255.0f + 0.5f)),
                      (uint8_t)min(255, (int)(light[1] * 255.0f + 0.5f)),
                      (uint8_t)min(255, (int)(light[2] * 255.0f + 0.5f)),
                      (uint8_t)(alpha * 255.0f + 0.5f));
}

static void VK_Entity_BuildTransform(const entity_t *ent, vk_entity_transform_t *out_transform)
{
    if (!out_transform) {
        return;
    }

    memset(out_transform, 0, sizeof(*out_transform));
    VectorSet(out_transform->axis[0], 1.0f, 0.0f, 0.0f);
    VectorSet(out_transform->axis[1], 0.0f, 1.0f, 0.0f);
    VectorSet(out_transform->axis[2], 0.0f, 0.0f, 1.0f);
    VectorSet(out_transform->scaled_axis[0], 1.0f, 0.0f, 0.0f);
    VectorSet(out_transform->scaled_axis[1], 0.0f, 1.0f, 0.0f);
    VectorSet(out_transform->scaled_axis[2], 0.0f, 0.0f, 1.0f);
    VectorSet(out_transform->inv_scale, 1.0f, 1.0f, 1.0f);

    if (!ent) {
        return;
    }

    float backlerp = Q_clipf(ent->backlerp, 0.0f, 1.0f);
    float frontlerp = 1.0f - backlerp;
    if (backlerp > 0.0f && !VectorCompare(ent->origin, ent->oldorigin)) {
        LerpVector2(ent->oldorigin, ent->origin, backlerp, frontlerp, out_transform->origin);
    } else {
        VectorCopy(ent->origin, out_transform->origin);
    }

    if (!VectorEmpty(ent->angles)) {
        AnglesToAxis(ent->angles, out_transform->axis);
    }

    for (int i = 0; i < 3; i++) {
        float scale = ent->scale[i] ? ent->scale[i] : 1.0f;
        out_transform->inv_scale[i] = fabsf(scale) > 0.0001f ? (1.0f / scale) : 1.0f;
        VectorScale(out_transform->axis[i], scale, out_transform->scaled_axis[i]);
    }
}

static void VK_Entity_TransformPointWithTransform(const vk_entity_transform_t *transform,
                                                  const vec3_t local, vec3_t out)
{
    if (!transform || !local || !out) {
        return;
    }

    VectorCopy(transform->origin, out);
    VectorMA(out, local[0], transform->scaled_axis[0], out);
    VectorMA(out, local[1], transform->scaled_axis[1], out);
    VectorMA(out, local[2], transform->scaled_axis[2], out);
}

static void VK_Entity_TransformPointInverseWithTransform(const vk_entity_transform_t *transform,
                                                         const vec3_t world, vec3_t out)
{
    if (!transform || !world || !out) {
        return;
    }

    vec3_t rel;
    VectorSubtract(world, transform->origin, rel);
    out[0] = DotProduct(rel, transform->axis[0]) * transform->inv_scale[0];
    out[1] = DotProduct(rel, transform->axis[1]) * transform->inv_scale[1];
    out[2] = DotProduct(rel, transform->axis[2]) * transform->inv_scale[2];
}

static void VK_Entity_ClearBspTextureCache(void)
{
    free(vk_entity.bmodel_texture_handles);
    free(vk_entity.bmodel_texture_sets);
    free(vk_entity.bmodel_texture_inv_sizes);
    free(vk_entity.bmodel_texture_transparent);
    vk_entity.bmodel_texture_handles = NULL;
    vk_entity.bmodel_texture_sets = NULL;
    vk_entity.bmodel_texture_inv_sizes = NULL;
    vk_entity.bmodel_texture_transparent = NULL;
    vk_entity.bmodel_texture_count = 0;
    vk_entity.bmodel_texture_bsp = NULL;
}

static bool VK_Entity_EnsureBspTextureCache(const bsp_t *bsp)
{
    if (!bsp || bsp->numtexinfo <= 0 || !bsp->texinfo) {
        VK_Entity_ClearBspTextureCache();
        return false;
    }

    if (vk_entity.bmodel_texture_bsp == bsp &&
        vk_entity.bmodel_texture_handles &&
        vk_entity.bmodel_texture_sets &&
        vk_entity.bmodel_texture_inv_sizes &&
        vk_entity.bmodel_texture_transparent &&
        vk_entity.bmodel_texture_count == bsp->numtexinfo) {
        return true;
    }

    VK_Entity_ClearBspTextureCache();

    vk_entity.bmodel_texture_handles = calloc((size_t)bsp->numtexinfo, sizeof(*vk_entity.bmodel_texture_handles));
    vk_entity.bmodel_texture_sets = calloc((size_t)bsp->numtexinfo, sizeof(*vk_entity.bmodel_texture_sets));
    vk_entity.bmodel_texture_inv_sizes = calloc((size_t)bsp->numtexinfo, sizeof(*vk_entity.bmodel_texture_inv_sizes));
    vk_entity.bmodel_texture_transparent = calloc((size_t)bsp->numtexinfo, sizeof(*vk_entity.bmodel_texture_transparent));
    if (!vk_entity.bmodel_texture_handles || !vk_entity.bmodel_texture_sets ||
        !vk_entity.bmodel_texture_inv_sizes || !vk_entity.bmodel_texture_transparent) {
        VK_Entity_ClearBspTextureCache();
        Com_SetLastError("Vulkan entity: out of memory for BSP texture cache");
        return false;
    }

    vk_entity.bmodel_texture_bsp = bsp;
    vk_entity.bmodel_texture_count = bsp->numtexinfo;
    return true;
}

static void VK_Entity_BuildFramePush(const refdef_t *fd, const bsp_t *world_bsp,
                                     renderer_view_push_t *out_push)
{
    if (!fd || !out_push) {
        return;
    }

    float znear = 4.0f;
    float zfar = 8192.0f;

    // Keep entity depth projection in sync with VK world pass.
    if (world_bsp && world_bsp->numnodes > 0) {
        vec3_t extents;
        VectorSubtract(world_bsp->nodes[0].maxs, world_bsp->nodes[0].mins, extents);
        float radius = VectorLength(extents) * 0.5f;
        zfar = max(2048.0f, radius * 8.0f);
    }

    R_BuildViewPush(fd, znear, zfar, out_push);
}

static void VK_Entity_BuildWeaponFramePush(const refdef_t *fd, const bsp_t *world_bsp,
                                           renderer_view_push_t *out_push)
{
    if (!fd || !out_push) {
        return;
    }

    float znear = 4.0f;
    float zfar = 8192.0f;

    if (world_bsp && world_bsp->numnodes > 0) {
        vec3_t extents;
        VectorSubtract(world_bsp->nodes[0].maxs, world_bsp->nodes[0].mins, extents);
        float radius = VectorLength(extents) * 0.5f;
        zfar = max(2048.0f, radius * 8.0f);
    }

    float fov_x = fd->fov_x;
    float fov_y = fd->fov_y;
    float reflect_x = 1.0f;
    float gunfov = 0.0f;
    int gun = 0;
    int hand = 0;

    if (Cvar_VariableValue) {
        gunfov = Cvar_VariableValue("cl_gunfov");
    } else if (Cvar_VariableInteger) {
        gunfov = (float)Cvar_VariableInteger("cl_gunfov");
    }

    if (gunfov > 0.0f) {
        fov_x = Q_clipf(gunfov, 30.0f, 160.0f);
        fov_y = V_CalcFov(fov_x, 4.0f, 3.0f);
        if (fd->height > 0 && fd->width > 0) {
            fov_x = V_CalcFov(fov_y, (float)fd->height, (float)fd->width);
        }
    }

    if (Cvar_VariableInteger) {
        gun = Cvar_VariableInteger("cl_gun");
        hand = Cvar_VariableInteger("hand");
    }
    if ((hand == 1 && gun == 1) || gun == 3) {
        reflect_x = -1.0f;
    }

    R_BuildViewPushEx(fd, fov_x, fov_y, reflect_x, znear, zfar, out_push);
}

static bool VK_Entity_ResolveAnimationFrames(const refdef_t *fd, uint32_t num_frames,
                                             unsigned frame_in, unsigned oldframe_in,
                                             float backlerp_in,
                                             uint32_t *out_frame, uint32_t *out_oldframe,
                                             float *out_backlerp, float *out_frontlerp)
{
    if (!out_frame || !out_oldframe || !out_backlerp || !out_frontlerp || !num_frames) {
        return false;
    }

    uint32_t frame = frame_in;
    uint32_t oldframe = oldframe_in;
    if (fd && fd->extended) {
        frame %= num_frames;
        oldframe %= num_frames;
    } else {
        if (frame >= num_frames) {
            frame = 0;
        }
        if (oldframe >= num_frames) {
            oldframe = 0;
        }
    }

    float backlerp = Q_clipf(backlerp_in, 0.0f, 1.0f);
    if (backlerp == 0.0f) {
        oldframe = frame;
    }

    *out_frame = frame;
    *out_oldframe = oldframe;
    *out_backlerp = backlerp;
    *out_frontlerp = 1.0f - backlerp;
    return true;
}

static bool VK_Entity_AddSprite(const entity_t *ent, const vec3_t view_axis[3], const vk_model_t *model,
                                bool depth_hack, bool weapon_model)
{
    if (!model->sprite.num_frames || !model->sprite.frames) {
        return true;
    }

    uint32_t frame = ent->frame % model->sprite.num_frames;
    const vk_sprite_frame_t *sf = &model->sprite.frames[frame];
    if (!sf->image) {
        return true;
    }

    float scale = ent->scale[0] ? ent->scale[0] : 1.0f;
    vec3_t left, right, down, up;
    VectorScale(view_axis[1], sf->origin_x * scale, left);
    VectorScale(view_axis[1], (sf->origin_x - sf->width) * scale, right);
    VectorScale(view_axis[2], -sf->origin_y * scale, down);
    VectorScale(view_axis[2], (sf->height - sf->origin_y) * scale, up);

    color_t color = VK_Entity_LitColor(ent, true);
    VkDescriptorSet set = VK_Entity_SetForImage(sf->image);
    bool alpha = (color.a < 255) || VK_UI_IsImageTransparent(sf->image);

    vk_vertex_t v0 = { .uv = { 0, 1 }, .color = color.u32 };
    vk_vertex_t v1 = { .uv = { 0, 0 }, .color = color.u32 };
    vk_vertex_t v2 = { .uv = { 1, 1 }, .color = color.u32 };
    vk_vertex_t v3 = { .uv = { 1, 0 }, .color = color.u32 };
    VectorAdd3(ent->origin, down, left, v0.pos);
    VectorAdd3(ent->origin, up, left, v1.pos);
    VectorAdd3(ent->origin, down, right, v2.pos);
    VectorAdd3(ent->origin, up, right, v3.pos);

    return VK_Entity_EmitTri(&v0, &v1, &v2, set, alpha, depth_hack, weapon_model) &&
           VK_Entity_EmitTri(&v2, &v1, &v3, set, alpha, depth_hack, weapon_model);
}

static bool VK_Entity_AddBeam(const entity_t *ent, const refdef_t *fd)
{
    vec3_t start, end, dir, to_view, right;
    VectorCopy(ent->origin, start);
    VectorCopy(ent->oldorigin, end);
    VectorSubtract(end, start, dir);
    float len = VectorNormalize(dir);
    if (len <= 0.01f) {
        return true;
    }

    vec3_t mid;
    VectorAdd(start, end, mid);
    VectorScale(mid, 0.5f, mid);
    VectorSubtract(fd->vieworg, mid, to_view);
    if (VectorNormalize(to_view) <= 0.01f) {
        VectorSet(to_view, 0, 0, 1);
    }

    CrossProduct(dir, to_view, right);
    if (VectorNormalize(right) <= 0.01f) {
        return true;
    }

    float half_width = max(0.5f, (float)ent->frame * 0.5f);
    VectorScale(right, half_width, right);

    vec3_t p0, p1, p2, p3;
    VectorAdd(start, right, p0);
    VectorSubtract(start, right, p1);
    VectorAdd(end, right, p2);
    VectorSubtract(end, right, p3);

    color_t color;
    if (ent->skinnum >= 0) {
        extern uint32_t d_8to24table[256];
        color = COLOR_U32(d_8to24table[ent->skinnum & 255]);
        color.a = (uint8_t)(VK_Entity_Alpha(ent) * 255.0f + 0.5f);
    } else {
        color = ent->rgba;
        color.a = (uint8_t)(VK_Entity_Alpha(ent) * 255.0f + 0.5f);
    }

    vk_vertex_t v0 = { .uv = { 0, 0 }, .color = color.u32 };
    vk_vertex_t v1 = { .uv = { 1, 0 }, .color = color.u32 };
    vk_vertex_t v2 = { .uv = { 0, 1 }, .color = color.u32 };
    vk_vertex_t v3 = { .uv = { 1, 1 }, .color = color.u32 };
    VectorCopy(p0, v0.pos);
    VectorCopy(p1, v1.pos);
    VectorCopy(p2, v2.pos);
    VectorCopy(p3, v3.pos);

    bool depth_hack = (ent->flags & (RF_DEPTHHACK | RF_WEAPONMODEL)) != 0;
    return VK_Entity_EmitTri(&v0, &v1, &v2, vk_entity.white_set, true, depth_hack, false) &&
           VK_Entity_EmitTri(&v2, &v1, &v3, vk_entity.white_set, true, depth_hack, false);
}

static inline uint32_t VK_Entity_SurfEdgeVertexIndex(const bsp_t *bsp, const msurfedge_t *surfedge)
{
    const medge_t *edge = &bsp->edges[surfedge->edge];
    return surfedge->vert ? edge->v[1] : edge->v[0];
}

static bool VK_Entity_GetBspFaceTexture(const bsp_t *bsp, const mface_t *face,
                                        VkDescriptorSet *out_set,
                                        float *out_inv_w, float *out_inv_h,
                                        bool *out_is_transparent)
{
    if (out_set) {
        *out_set = VK_NULL_HANDLE;
    }
    if (out_inv_w) {
        *out_inv_w = 1.0f;
    }
    if (out_inv_h) {
        *out_inv_h = 1.0f;
    }
    if (out_is_transparent) {
        *out_is_transparent = false;
    }

    if (!bsp || !face || !face->texinfo || !face->texinfo->name[0] || !out_set) {
        return false;
    }

    if (!VK_Entity_EnsureBspTextureCache(bsp)) {
        return false;
    }

    int tex_index = (int)(face->texinfo - bsp->texinfo);
    if (tex_index < 0 || tex_index >= vk_entity.bmodel_texture_count) {
        return false;
    }

    if (vk_entity.bmodel_texture_handles[tex_index] &&
        vk_entity.bmodel_texture_sets[tex_index]) {
        *out_set = vk_entity.bmodel_texture_sets[tex_index];
        if (out_inv_w) {
            *out_inv_w = vk_entity.bmodel_texture_inv_sizes[tex_index][0];
        }
        if (out_inv_h) {
            *out_inv_h = vk_entity.bmodel_texture_inv_sizes[tex_index][1];
        }
        if (out_is_transparent) {
            *out_is_transparent = vk_entity.bmodel_texture_transparent[tex_index];
        }
        return true;
    }

    imageflags_t flags = IF_REPEAT;
    if (face->texinfo->c.flags & SURF_TRANS_MASK) {
        flags |= IF_TRANSPARENT;
    } else if (!(face->texinfo->c.flags & SURF_WARP)) {
        flags |= IF_OPAQUE;
    }
    if (face->texinfo->c.flags & SURF_WARP) {
        flags |= IF_TURBULENT;
    }

    char path[MAX_QPATH];
    if (Q_concat(path, sizeof(path), "textures/", face->texinfo->name, ".wal") >= sizeof(path)) {
        return false;
    }

    qhandle_t handle = VK_UI_RegisterImage(path, IT_WALL, flags);
    if (!handle) {
        return false;
    }

    VkDescriptorSet set = VK_UI_GetDescriptorSetForImage(handle);
    if (!set) {
        return false;
    }

    int tex_w = 0;
    int tex_h = 0;
    if (!VK_UI_GetPicSize(&tex_w, &tex_h, handle) || tex_w <= 0 || tex_h <= 0) {
        tex_w = 64;
        tex_h = 64;
    }

    if (out_inv_w) {
        *out_inv_w = 1.0f / (float)tex_w;
    }
    if (out_inv_h) {
        *out_inv_h = 1.0f / (float)tex_h;
    }

    vk_entity.bmodel_texture_handles[tex_index] = handle;
    vk_entity.bmodel_texture_sets[tex_index] = set;
    vk_entity.bmodel_texture_inv_sizes[tex_index][0] = 1.0f / (float)tex_w;
    vk_entity.bmodel_texture_inv_sizes[tex_index][1] = 1.0f / (float)tex_h;
    vk_entity.bmodel_texture_transparent[tex_index] = VK_UI_IsImageTransparent(handle);
    if (out_is_transparent) {
        *out_is_transparent = vk_entity.bmodel_texture_transparent[tex_index];
    }
    *out_set = set;
    return true;
}

static bool VK_Entity_AddBspModel(const entity_t *ent, const refdef_t *fd, const bsp_t *bsp,
                                  bool depth_hack, bool weapon_model)
{
    if (!ent || !bsp || !bsp->models || !bsp->faces ||
        !bsp->vertices || !bsp->edges || !bsp->surfedges) {
        return true;
    }

    int model_index = ~ent->model;
    if (model_index < 1 || model_index >= bsp->nummodels) {
        return true;
    }

    const mmodel_t *model = &bsp->models[model_index];
    if (!model->firstface || model->numfaces <= 0) {
        return true;
    }
    if (!VK_Entity_EnsureBspTextureCache(bsp)) {
        return false;
    }

    vk_entity_transform_t transform;
    VK_Entity_BuildTransform(ent, &transform);

    vec3_t view_local = { 0.0f, 0.0f, 0.0f };
    if (fd) {
        VK_Entity_TransformPointInverseWithTransform(&transform, fd->vieworg, view_local);
    }
    color_t entity_color = VK_Entity_LitColor(ent, false);

    for (int i = 0; i < model->numfaces; i++) {
        const mface_t *face = &model->firstface[i];
        if (!face || !face->texinfo || !face->firstsurfedge || face->numsurfedges < 3 || !face->plane) {
            continue;
        }

        surfflags_t surf_flags = face->texinfo->c.flags;
        if (surf_flags & (SURF_NODRAW | SURF_SKY)) {
            continue;
        }

        if (fd) {
            const float dot = PlaneDiffFast(view_local, face->plane);
            if ((face->drawflags & DSURF_PLANEBACK) ? (dot > 0.01f) : (dot < -0.01f)) {
                continue;
            }
        }

        float inv_tex_w = 1.0f;
        float inv_tex_h = 1.0f;
        VkDescriptorSet set = VK_NULL_HANDLE;
        bool texture_transparent = false;
        if (!VK_Entity_GetBspFaceTexture(bsp, face, &set, &inv_tex_w, &inv_tex_h, &texture_transparent)) {
            continue;
        }
        if (!set) {
            continue;
        }

        float alpha_f = VK_Entity_Alpha(ent);
        if (surf_flags & SURF_TRANS33) {
            alpha_f = min(alpha_f, 0.33f);
        } else if (surf_flags & SURF_TRANS66) {
            alpha_f = min(alpha_f, 0.66f);
        }
        uint8_t alpha_u8 = (uint8_t)Q_clipf(alpha_f * 255.0f + 0.5f, 0.0f, 255.0f);

        bool alpha = (alpha_u8 < 255) || (surf_flags & SURF_TRANS_MASK) ||
                     ((surf_flags & SURF_ALPHATEST) == 0 && texture_transparent);

        const msurfedge_t *surfedges = face->firstsurfedge;
        if (surfedges[0].edge >= (uint32_t)bsp->numedges) {
            continue;
        }
        uint32_t i0 = VK_Entity_SurfEdgeVertexIndex(bsp, &surfedges[0]);
        if (i0 >= (uint32_t)bsp->numvertices) {
            continue;
        }
        const mvertex_t *v0 = &bsp->vertices[i0];

        for (int j = 1; j < face->numsurfedges - 1; j++) {
            if (surfedges[j].edge >= (uint32_t)bsp->numedges ||
                surfedges[j + 1].edge >= (uint32_t)bsp->numedges) {
                continue;
            }

            uint32_t i1 = VK_Entity_SurfEdgeVertexIndex(bsp, &surfedges[j]);
            uint32_t i2 = VK_Entity_SurfEdgeVertexIndex(bsp, &surfedges[j + 1]);
            if (i1 >= (uint32_t)bsp->numvertices || i2 >= (uint32_t)bsp->numvertices) {
                continue;
            }

            const mvertex_t *verts[3] = {
                v0,
                &bsp->vertices[i1],
                &bsp->vertices[i2],
            };

            vk_vertex_t tri[3];
            for (int k = 0; k < 3; k++) {
                const vec3_t local = {
                    verts[k]->point[0],
                    verts[k]->point[1],
                    verts[k]->point[2],
                };
                VK_Entity_TransformPointWithTransform(&transform, local, tri[k].pos);

                float u = DotProduct(local, face->texinfo->axis[0]) + face->texinfo->offset[0];
                float v = DotProduct(local, face->texinfo->axis[1]) + face->texinfo->offset[1];
                tri[k].uv[0] = u * inv_tex_w;
                tri[k].uv[1] = v * inv_tex_h;

                color_t color = entity_color;
                color.a = alpha_u8;
                tri[k].color = color.u32;
            }

            if (!VK_Entity_EmitTri(&tri[0], &tri[1], &tri[2], set, alpha, depth_hack, weapon_model)) {
                return false;
            }
        }
    }

    return true;
}

static void VK_Entity_InitParticleTexture(void)
{
    byte pixels[VK_ENTITY_PARTICLE_TEX_SIZE * VK_ENTITY_PARTICLE_TEX_SIZE * 4];
    byte *dst = pixels;

    for (int y = 0; y < VK_ENTITY_PARTICLE_TEX_SIZE; y++) {
        for (int x = 0; x < VK_ENTITY_PARTICLE_TEX_SIZE; x++) {
            float fx = (float)x - (float)VK_ENTITY_PARTICLE_TEX_SIZE * 0.5f + 0.5f;
            float fy = (float)y - (float)VK_ENTITY_PARTICLE_TEX_SIZE * 0.5f + 0.5f;
            float dist = sqrtf(fx * fx + fy * fy);
            float a = 1.0f - dist / ((float)VK_ENTITY_PARTICLE_TEX_SIZE * 0.5f - 0.5f);
            a = Q_clipf(a, 0.0f, 1.0f);

            dst[0] = 255;
            dst[1] = 255;
            dst[2] = 255;
            dst[3] = (byte)(255.0f * a + 0.5f);
            dst += 4;
        }
    }

    vk_entity.particle_image = VK_UI_RegisterRawImage("**vk_entity_particle**",
                                                       VK_ENTITY_PARTICLE_TEX_SIZE,
                                                       VK_ENTITY_PARTICLE_TEX_SIZE,
                                                       pixels,
                                                       IT_SPRITE,
                                                       IF_PERMANENT | IF_TRANSPARENT);
    if (!vk_entity.particle_image) {
        vk_entity.particle_set = vk_entity.white_set;
        return;
    }

    vk_entity.particle_set = VK_UI_GetDescriptorSetForImage(vk_entity.particle_image);
    if (!vk_entity.particle_set) {
        VK_UI_UnregisterImage(vk_entity.particle_image);
        vk_entity.particle_image = 0;
        vk_entity.particle_set = vk_entity.white_set;
    }
}

static bool VK_Entity_AddParticles(const refdef_t *fd, const vec3_t view_axis[3])
{
    if (!fd || !fd->particles || fd->num_particles <= 0) {
        return true;
    }

    VkDescriptorSet set = vk_entity.particle_set ? vk_entity.particle_set : vk_entity.white_set;
    if (!set) {
        return true;
    }

    extern uint32_t d_8to24table[256];
    float partscale = (vk_partscale ? max(vk_partscale->value, 0.0f) : 2.0f);

    for (int i = 0; i < fd->num_particles; i++) {
        const particle_t *p = &fd->particles[i];
        if (!p || p->alpha <= 0.0f || p->scale <= 0.0f) {
            continue;
        }

        vec3_t transformed;
        VectorSubtract(p->origin, fd->vieworg, transformed);
        float dist = DotProduct(transformed, view_axis[0]);

        float scale = 1.0f;
        if (dist > 20.0f) {
            scale += dist * 0.004f;
        }
        scale *= partscale * p->scale;
        float scale2 = scale * VK_ENTITY_PARTICLE_SCALE;

        color_t color;
        if (p->color == -1) {
            color = p->rgba;
        } else {
            color.u32 = d_8to24table[p->color & 0xff];
        }
        if (p->brightness > 0.0f && p->brightness != 1.0f) {
            color.r = (uint8_t)min(255, (int)(color.r * p->brightness + 0.5f));
            color.g = (uint8_t)min(255, (int)(color.g * p->brightness + 0.5f));
            color.b = (uint8_t)min(255, (int)(color.b * p->brightness + 0.5f));
        }
        color.a = (uint8_t)Q_clipf((float)color.a * Q_clipf(p->alpha, 0.0f, 1.0f),
                                   0.0f, 255.0f);
        if (!color.a) {
            continue;
        }

        vk_vertex_t v0 = { .uv = { 0.0f, 0.0f }, .color = color.u32 };
        vk_vertex_t v1 = { .uv = { 0.0f, VK_ENTITY_PARTICLE_SIZE }, .color = color.u32 };
        vk_vertex_t v2 = { .uv = { VK_ENTITY_PARTICLE_SIZE, 0.0f }, .color = color.u32 };

        VectorMA(p->origin, scale2, view_axis[1], v0.pos);
        VectorMA(v0.pos, -scale2, view_axis[2], v0.pos);
        VectorMA(v0.pos, scale, view_axis[2], v1.pos);
        VectorMA(v0.pos, -scale, view_axis[1], v2.pos);

        if (!VK_Entity_EmitTri(&v0, &v1, &v2, set, true, false, false)) {
            return false;
        }
    }

    return true;
}

static qhandle_t VK_Entity_SelectMD2Skin(const entity_t *ent, const vk_md2_t *md2)
{
    if (ent->flags & RF_CUSTOMSKIN) {
        return ent->skin;
    }
    if (ent->skin) {
        return ent->skin;
    }
    if (!md2->skins || md2->num_skins == 0) {
        return 0;
    }
    int skinnum = ent->skinnum;
    if (skinnum < 0 || skinnum >= (int)md2->num_skins || !md2->skins[skinnum]) {
        skinnum = 0;
    }
    return md2->skins[skinnum];
}

static qhandle_t VK_Entity_SelectMD5Skin(const entity_t *ent)
{
    if (ent->flags & RF_CUSTOMSKIN) {
        return ent->skin;
    }
    if (ent->skin) {
        return ent->skin;
    }
    return 0;
}

static bool VK_Entity_AddMD2(const entity_t *ent, const refdef_t *fd, const vk_model_t *model,
                             bool depth_hack, bool weapon_model)
{
    const vk_md2_t *md2 = &model->md2;
    if (!md2->positions || !md2->uv || !md2->indices || !md2->num_frames) {
        return true;
    }

    uint32_t frame = 0;
    uint32_t oldframe = 0;
    float backlerp = 0.0f;
    float frontlerp = 1.0f;
    if (!VK_Entity_ResolveAnimationFrames(fd, md2->num_frames, ent->frame, ent->oldframe,
                                          ent->backlerp,
                                          &frame, &oldframe, &backlerp, &frontlerp)) {
        return true;
    }

    qhandle_t skin = VK_Entity_SelectMD2Skin(ent, md2);
    VkDescriptorSet set = VK_Entity_SetForImage(skin);
    bool alpha = VK_Entity_Alpha(ent) < 1.0f;
    color_t color = VK_Entity_LitColor(ent, false);
    vk_entity_transform_t transform;
    VK_Entity_BuildTransform(ent, &transform);

    for (uint32_t i = 0; i < md2->num_indices; i += 3) {
        vk_vertex_t tri[3];
        bool valid_tri = true;
        for (uint32_t j = 0; j < 3; j++) {
            uint32_t idx = md2->indices[i + j];
            if (idx >= md2->num_vertices) {
                valid_tri = false;
                break;
            }
            const float *p0 = &md2->positions[((size_t)frame * md2->num_vertices + idx) * 3];
            const float *p1 = &md2->positions[((size_t)oldframe * md2->num_vertices + idx) * 3];
            vec3_t local = {
                p0[0] * frontlerp + p1[0] * backlerp,
                p0[1] * frontlerp + p1[1] * backlerp,
                p0[2] * frontlerp + p1[2] * backlerp,
            };
            VK_Entity_TransformPointWithTransform(&transform, local, tri[j].pos);
            tri[j].uv[0] = md2->uv[idx * 2 + 0];
            tri[j].uv[1] = md2->uv[idx * 2 + 1];
            tri[j].color = color.u32;
        }

        if (!valid_tri) {
            continue;
        }

        if (!VK_Entity_EmitTri(&tri[0], &tri[1], &tri[2], set, alpha, depth_hack, weapon_model)) {
            return false;
        }
    }

    return true;
}

static bool VK_Entity_LoadSP2(vk_model_t *model, const byte *raw, size_t len)
{
    if (len < sizeof(dsp2header_t)) {
        return false;
    }

    dsp2header_t hdr;
    memcpy(&hdr, raw, sizeof(hdr));
    hdr.ident = LittleLong(hdr.ident);
    hdr.version = LittleLong(hdr.version);
    hdr.numframes = LittleLong(hdr.numframes);
    if (hdr.ident != SP2_IDENT || hdr.version != SP2_VERSION || hdr.numframes <= 0 || hdr.numframes > SP2_MAX_FRAMES) {
        return false;
    }

    if (sizeof(dsp2header_t) + (size_t)hdr.numframes * sizeof(dsp2frame_t) > len) {
        return false;
    }

    vk_sprite_frame_t *frames = calloc((size_t)hdr.numframes, sizeof(*frames));
    if (!frames) {
        return false;
    }

    const dsp2frame_t *src = (const dsp2frame_t *)(raw + sizeof(dsp2header_t));
    for (int i = 0; i < hdr.numframes; i++) {
        char name[SP2_MAX_FRAMENAME];
        if (Q_memccpy(name, src[i].name, 0, sizeof(name))) {
            FS_NormalizePath(name);
            frames[i].image = VK_UI_RegisterImage(name, IT_SPRITE, IF_NONE);
        }
        frames[i].width = (int)LittleLong(src[i].width);
        frames[i].height = (int)LittleLong(src[i].height);
        frames[i].origin_x = (int)LittleLong(src[i].origin_x);
        frames[i].origin_y = (int)LittleLong(src[i].origin_y);
    }

    model->type = VK_MODEL_SPRITE;
    model->sprite.frames = frames;
    model->sprite.num_frames = (uint32_t)hdr.numframes;
    return true;
}

static bool VK_Entity_LoadMD2(vk_model_t *model, const byte *raw, size_t len)
{
    if (len < sizeof(dmd2header_t)) {
        return false;
    }

    dmd2header_t hdr;
    memcpy(&hdr, raw, sizeof(hdr));
    hdr.ident = LittleLong(hdr.ident);
    hdr.version = LittleLong(hdr.version);
    hdr.skinwidth = LittleLong(hdr.skinwidth);
    hdr.skinheight = LittleLong(hdr.skinheight);
    hdr.framesize = LittleLong(hdr.framesize);
    hdr.num_skins = LittleLong(hdr.num_skins);
    hdr.num_xyz = LittleLong(hdr.num_xyz);
    hdr.num_st = LittleLong(hdr.num_st);
    hdr.num_tris = LittleLong(hdr.num_tris);
    hdr.num_frames = LittleLong(hdr.num_frames);
    hdr.ofs_skins = LittleLong(hdr.ofs_skins);
    hdr.ofs_st = LittleLong(hdr.ofs_st);
    hdr.ofs_tris = LittleLong(hdr.ofs_tris);
    hdr.ofs_frames = LittleLong(hdr.ofs_frames);

    if (hdr.ident != MD2_IDENT || hdr.version != MD2_VERSION || hdr.num_tris <= 0 || hdr.num_frames <= 0 ||
        hdr.num_xyz <= 0 || hdr.num_st <= 0) {
        return false;
    }

    if (hdr.num_tris > MD2_MAX_TRIANGLES || hdr.num_xyz > MD2_MAX_VERTS || hdr.num_frames > MD2_MAX_FRAMES ||
        hdr.num_skins > MD2_MAX_SKINS || hdr.skinwidth < 1 || hdr.skinwidth > MD2_MAX_SKINWIDTH ||
        hdr.skinheight < 1 || hdr.skinheight > MD2_MAX_SKINHEIGHT) {
        return false;
    }

    uint64_t min_frame_size = sizeof(dmd2frame_t) + (uint64_t)(hdr.num_xyz - 1) * sizeof(dmd2trivertx_t);
    if (hdr.framesize < min_frame_size || hdr.framesize > MD2_MAX_FRAMESIZE) {
        return false;
    }

    if ((uint64_t)hdr.ofs_tris + (uint64_t)hdr.num_tris * sizeof(dmd2triangle_t) > len ||
        (uint64_t)hdr.ofs_st + (uint64_t)hdr.num_st * sizeof(dmd2stvert_t) > len ||
        (uint64_t)hdr.ofs_frames + (uint64_t)hdr.num_frames * (uint64_t)hdr.framesize > len ||
        (uint64_t)hdr.ofs_skins + (uint64_t)hdr.num_skins * MD2_MAX_SKINNAME > len) {
        return false;
    }

    uint32_t max_indices = (uint32_t)hdr.num_tris * 3;
    uint32_t num_indices = 0;
    uint32_t num_vertices = 0;
    uint32_t num_frames = (uint32_t)hdr.num_frames;
    float *positions = NULL;
    float *uv = NULL;
    uint16_t *indices = NULL;
    uint16_t *vert_indices = NULL;
    uint16_t *tc_indices = NULL;
    uint16_t *remap = NULL;
    uint16_t *final_indices = NULL;
    qhandle_t *skins = NULL;

    vert_indices = calloc((size_t)max_indices, sizeof(*vert_indices));
    tc_indices = calloc((size_t)max_indices, sizeof(*tc_indices));
    remap = calloc((size_t)max_indices, sizeof(*remap));
    final_indices = calloc((size_t)max_indices, sizeof(*final_indices));
    if (!vert_indices || !tc_indices || !remap || !final_indices) {
        goto fail;
    }

    const dmd2triangle_t *tris = (const dmd2triangle_t *)(raw + hdr.ofs_tris);
    const dmd2stvert_t *st = (const dmd2stvert_t *)(raw + hdr.ofs_st);
    for (uint32_t t = 0; t < (uint32_t)hdr.num_tris; t++) {
        bool good_tri = true;
        for (uint32_t j = 0; j < 3; j++) {
            uint16_t xyz = LittleShort(tris[t].index_xyz[j]);
            uint16_t tc = LittleShort(tris[t].index_st[j]);
            if (xyz >= hdr.num_xyz || tc >= hdr.num_st) {
                good_tri = false;
                break;
            }
            vert_indices[num_indices + j] = xyz;
            tc_indices[num_indices + j] = tc;
        }
        if (good_tri) {
            num_indices += 3;
        }
    }

    if (num_indices < 3) {
        goto fail;
    }
    if (num_indices != max_indices) {
        Com_DPrintf("%s has %u bad triangles\n", model->name, (max_indices - num_indices) / 3);
    }

    for (uint32_t i = 0; i < num_indices; i++) {
        remap[i] = UINT16_MAX;
    }

    for (uint32_t i = 0; i < num_indices; i++) {
        if (remap[i] != UINT16_MAX) {
            continue;
        }
        for (uint32_t j = i + 1; j < num_indices; j++) {
            if (vert_indices[i] == vert_indices[j] &&
                st[tc_indices[i]].s == st[tc_indices[j]].s &&
                st[tc_indices[i]].t == st[tc_indices[j]].t) {
                remap[j] = (uint16_t)i;
                final_indices[j] = (uint16_t)num_vertices;
            }
        }
        if (num_vertices == UINT16_MAX) {
            goto fail;
        }
        remap[i] = (uint16_t)i;
        final_indices[i] = (uint16_t)num_vertices++;
    }

    positions = calloc((size_t)num_frames * num_vertices * 3, sizeof(float));
    uv = calloc((size_t)num_vertices * 2, sizeof(float));
    indices = calloc((size_t)num_indices, sizeof(*indices));
    skins = hdr.num_skins ? calloc((size_t)hdr.num_skins, sizeof(*skins)) : NULL;
    if (!positions || !uv || !indices || (hdr.num_skins && !skins)) {
        goto fail;
    }

    for (uint32_t i = 0; i < num_indices; i++) {
        indices[i] = final_indices[i];
    }
    for (uint32_t i = 0; i < num_indices; i++) {
        if (remap[i] != i) {
            continue;
        }
        uint32_t out = final_indices[i];
        uv[out * 2 + 0] = (float)((int16_t)LittleShort(st[tc_indices[i]].s)) / (float)hdr.skinwidth;
        uv[out * 2 + 1] = (float)((int16_t)LittleShort(st[tc_indices[i]].t)) / (float)hdr.skinheight;
    }

    for (uint32_t f = 0; f < num_frames; f++) {
        const dmd2frame_t *frame = (const dmd2frame_t *)(raw + hdr.ofs_frames + (size_t)f * (size_t)hdr.framesize);
        float scale[3] = {
            LittleFloat(frame->scale[0]),
            LittleFloat(frame->scale[1]),
            LittleFloat(frame->scale[2]),
        };
        float translate[3] = {
            LittleFloat(frame->translate[0]),
            LittleFloat(frame->translate[1]),
            LittleFloat(frame->translate[2]),
        };
        for (uint32_t i = 0; i < num_indices; i++) {
            if (remap[i] != i) {
                continue;
            }
            uint32_t out = final_indices[i];
            const dmd2trivertx_t *src = &frame->verts[vert_indices[i]];
            float *dst = &positions[((size_t)f * num_vertices + out) * 3];
            dst[0] = src->v[0] * scale[0] + translate[0];
            dst[1] = src->v[1] * scale[1] + translate[1];
            dst[2] = src->v[2] * scale[2] + translate[2];
        }
    }

    if (skins) {
        const char *skin_data = (const char *)(raw + hdr.ofs_skins);
        for (int i = 0; i < hdr.num_skins; i++) {
            char name[MD2_MAX_SKINNAME];
            memcpy(name, skin_data + i * MD2_MAX_SKINNAME, MD2_MAX_SKINNAME);
            name[MD2_MAX_SKINNAME - 1] = '\0';
            FS_NormalizePath(name);
            skins[i] = VK_UI_RegisterImage(name, IT_SKIN, IF_NONE);
        }
    }

    free(vert_indices);
    free(tc_indices);
    free(remap);
    free(final_indices);

    model->type = VK_MODEL_MD2;
    model->md2.num_frames = num_frames;
    model->md2.num_vertices = num_vertices;
    model->md2.num_indices = num_indices;
    model->md2.positions = positions;
    model->md2.uv = uv;
    model->md2.indices = indices;
    model->md2.skins = skins;
    model->md2.num_skins = (uint32_t)hdr.num_skins;
    return true;

fail:
    free(positions);
    free(uv);
    free(indices);
    free(vert_indices);
    free(tc_indices);
    free(remap);
    free(final_indices);
    free(skins);
    return false;
}

static bool VK_Entity_CreatePipeline(vk_context_t *ctx, bool alpha, bool depth_hack,
                                     VkPipeline *out_pipeline)
{
    VkShaderModule vert = VK_NULL_HANDLE;
    VkShaderModule frag = VK_NULL_HANDLE;

    VkShaderModuleCreateInfo vert_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vk_entity_vert_spv_size,
        .pCode = vk_entity_vert_spv,
    };
    if (!VK_Entity_Check(vkCreateShaderModule(ctx->device, &vert_info, NULL, &vert),
                         "vkCreateShaderModule(entity vert)")) {
        return false;
    }

    VkShaderModuleCreateInfo frag_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vk_entity_frag_spv_size,
        .pCode = vk_entity_frag_spv,
    };
    if (!VK_Entity_Check(vkCreateShaderModule(ctx->device, &frag_info, NULL, &frag),
                         "vkCreateShaderModule(entity frag)")) {
        vkDestroyShaderModule(ctx->device, vert, NULL);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main" },
    };

    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride = sizeof(vk_vertex_t),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription attrs[3] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(vk_vertex_t, pos) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(vk_vertex_t, uv) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R8G8B8A8_UNORM, .offset = offsetof(vk_vertex_t, color) },
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
    };
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineDepthStencilStateCreateInfo depth = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = (alpha || depth_hack) ? VK_FALSE : VK_TRUE,
        .depthCompareOp = depth_hack ? VK_COMPARE_OP_ALWAYS : VK_COMPARE_OP_LESS_OR_EQUAL,
    };
    VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = alpha ? VK_TRUE : VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };
    VkDynamicState states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = q_countof(states),
        .pDynamicStates = states,
    };

    VkGraphicsPipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = q_countof(stages),
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &raster,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depth,
        .pColorBlendState = &blend,
        .pDynamicState = &dynamic,
        .layout = vk_entity.pipeline_layout,
        .renderPass = ctx->render_pass,
        .subpass = 0,
    };

    bool ok = VK_Entity_Check(vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &info, NULL, out_pipeline),
                              depth_hack ? "vkCreateGraphicsPipelines(entity depthhack)" :
                              (alpha ? "vkCreateGraphicsPipelines(entity alpha)" :
                                       "vkCreateGraphicsPipelines(entity opaque)"));

    vkDestroyShaderModule(ctx->device, vert, NULL);
    vkDestroyShaderModule(ctx->device, frag, NULL);
    return ok;
}

bool VK_Entity_Init(vk_context_t *ctx)
{
    memset(&vk_entity, 0, sizeof(vk_entity));
    if (!ctx) {
        return false;
    }
    vk_entity.ctx = ctx;
    vk_entity.registration_sequence = 1;

    if (!vk_drawentities) {
        vk_drawentities = Cvar_Get("vk_drawentities", "1", 0);
    }
    if (!vk_partscale) {
        vk_partscale = Cvar_Get("vk_partscale", "2", 0);
    }
#if USE_MD5
    if (!vk_md5_load) {
        vk_md5_load = Cvar_Get("vk_md5_load", "1", CVAR_FILES);
    }
    if (!vk_md5_use) {
        vk_md5_use = Cvar_Get("vk_md5_use", "0", 0);
    }
    if (!vk_md5_distance) {
        vk_md5_distance = Cvar_Get("vk_md5_distance", "2048", 0);
    }
#endif

    VkDescriptorSetLayout set_layout = VK_UI_GetDescriptorSetLayout();
    if (!set_layout) {
        Com_SetLastError("Vulkan entity: descriptor set layout unavailable");
        return false;
    }

    VkPushConstantRange push = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(renderer_view_push_t),
    };
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push,
    };
    if (!VK_Entity_Check(vkCreatePipelineLayout(ctx->device, &layout_info, NULL, &vk_entity.pipeline_layout),
                         "vkCreatePipelineLayout(entity)")) {
        return false;
    }

    static byte white_rgba[4] = { 255, 255, 255, 255 };
    vk_entity.white_image = VK_UI_RegisterRawImage("**vk_entity_white**", 1, 1, white_rgba,
                                                    IT_PIC, IF_PERMANENT | IF_OPAQUE);
    vk_entity.white_set = VK_UI_GetDescriptorSetForImage(vk_entity.white_image);
    if (!vk_entity.white_set) {
        Com_SetLastError("Vulkan entity: white descriptor unavailable");
        VK_Entity_Shutdown(ctx);
        return false;
    }
    VK_Entity_InitParticleTexture();

    vk_entity.initialized = true;
    return true;
}

void VK_Entity_DestroySwapchainResources(vk_context_t *ctx)
{
    (void)ctx;
    if (!vk_entity.initialized || !vk_entity.ctx || !vk_entity.ctx->device) {
        return;
    }
    if (vk_entity.pipeline_alpha) {
        vkDestroyPipeline(vk_entity.ctx->device, vk_entity.pipeline_alpha, NULL);
        vk_entity.pipeline_alpha = VK_NULL_HANDLE;
    }
    if (vk_entity.pipeline_depthhack) {
        vkDestroyPipeline(vk_entity.ctx->device, vk_entity.pipeline_depthhack, NULL);
        vk_entity.pipeline_depthhack = VK_NULL_HANDLE;
    }
    if (vk_entity.pipeline_opaque) {
        vkDestroyPipeline(vk_entity.ctx->device, vk_entity.pipeline_opaque, NULL);
        vk_entity.pipeline_opaque = VK_NULL_HANDLE;
    }
    vk_entity.swapchain_ready = false;
}

bool VK_Entity_CreateSwapchainResources(vk_context_t *ctx)
{
    if (!vk_entity.initialized || !ctx || !ctx->render_pass) {
        return false;
    }
    VK_Entity_DestroySwapchainResources(ctx);
    if (!VK_Entity_CreatePipeline(ctx, false, false, &vk_entity.pipeline_opaque)) {
        return false;
    }
    if (!VK_Entity_CreatePipeline(ctx, true, false, &vk_entity.pipeline_alpha)) {
        VK_Entity_DestroySwapchainResources(ctx);
        return false;
    }
    if (!VK_Entity_CreatePipeline(ctx, true, true, &vk_entity.pipeline_depthhack)) {
        VK_Entity_DestroySwapchainResources(ctx);
        return false;
    }
    vk_entity.swapchain_ready = true;
    return true;
}

void VK_Entity_Shutdown(vk_context_t *ctx)
{
    if (!vk_entity.initialized) {
        return;
    }
    if (!ctx) {
        ctx = vk_entity.ctx;
    }
    if (ctx && ctx->device) {
        vkDeviceWaitIdle(ctx->device);
    }

    VK_Entity_DestroySwapchainResources(ctx);
    VK_Entity_DestroyVertexBuffer();
    VK_Entity_FreeAllModels();
    VK_Entity_ClearBspTextureCache();

    if (vk_entity.white_image) {
        VK_UI_UnregisterImage(vk_entity.white_image);
    }
    if (vk_entity.particle_image) {
        VK_UI_UnregisterImage(vk_entity.particle_image);
    }

    if (ctx && ctx->device && vk_entity.pipeline_layout) {
        vkDestroyPipelineLayout(ctx->device, vk_entity.pipeline_layout, NULL);
    }

    free(vk_entity.vertices);
    free(vk_entity.batches);
#if USE_MD5
    free(vk_entity.temp_skeleton);
#endif
    memset(&vk_entity, 0, sizeof(vk_entity));
}

void VK_Entity_BeginRegistration(void)
{
    if (!vk_entity.initialized) {
        return;
    }
    vk_entity.registration_sequence++;
    if (vk_entity.registration_sequence <= 0) {
        vk_entity.registration_sequence = 1;
    }
}

void VK_Entity_EndRegistration(void)
{
    if (!vk_entity.initialized) {
        return;
    }

    for (int i = 0; i < vk_entity.num_models; i++) {
        vk_model_t *model = &vk_entity.models[i];
        if (!model->type) {
            continue;
        }
        if (model->registration_sequence == vk_entity.registration_sequence) {
            continue;
        }
        VK_Entity_FreeModel(model);
    }
}

qhandle_t VK_Entity_RegisterModel(const char *name)
{
    if (!vk_entity.initialized || !name || !*name) {
        return 0;
    }

    if (*name == '*') {
        return ~Q_atoi(name + 1);
    }

    char normalized[MAX_QPATH];
    size_t namelen = FS_NormalizePathBuffer(normalized, name, sizeof(normalized));
    if (namelen == 0 || namelen >= sizeof(normalized)) {
        return 0;
    }

    for (int i = 0; i < vk_entity.num_models; i++) {
        vk_model_t *model = &vk_entity.models[i];
        if (!model->type) {
            continue;
        }
        if (!FS_pathcmp(model->name, normalized)) {
            model->registration_sequence = vk_entity.registration_sequence;
            return (qhandle_t)(i + 1);
        }
    }

    byte *raw = NULL;
    int filelen = FS_LoadFile(normalized, (void **)&raw);
    if (!raw) {
        if (filelen != Q_ERR(ENOENT)) {
            Com_EPrintf("Couldn't load %s: %s\n", normalized, Q_ErrorString(filelen));
        }
        return 0;
    }

    vk_model_t *slot = NULL;
    for (int i = 0; i < vk_entity.num_models; i++) {
        if (!vk_entity.models[i].type) {
            slot = &vk_entity.models[i];
            break;
        }
    }
    if (!slot) {
        if (vk_entity.num_models >= MAX_MODELS) {
            FS_FreeFile(raw);
            return 0;
        }
        slot = &vk_entity.models[vk_entity.num_models++];
    }

    memset(slot, 0, sizeof(*slot));
    Q_strlcpy(slot->name, normalized, sizeof(slot->name));
    slot->registration_sequence = vk_entity.registration_sequence;

    bool loaded = false;
    if (filelen >= 4) {
        uint32_t ident = LittleLong(*(uint32_t *)raw);
        if (ident == SP2_IDENT) {
            loaded = VK_Entity_LoadSP2(slot, raw, (size_t)filelen);
        } else if (ident == MD2_IDENT) {
            loaded = VK_Entity_LoadMD2(slot, raw, (size_t)filelen);
        }
    }

    FS_FreeFile(raw);
    if (!loaded) {
        VK_Entity_FreeModel(slot);
        return 0;
    }

#if USE_MD5
    if (slot->type == VK_MODEL_MD2) {
        VK_Entity_LoadMD5Replacement(slot);
    }
#endif

    return (qhandle_t)((slot - vk_entity.models) + 1);
}

void VK_Entity_RenderFrame(const refdef_t *fd)
{
    vk_entity.frame_active = false;
    vk_entity.frame_weapon_active = false;
    vk_entity.vertex_count = 0;
    vk_entity.batch_count = 0;
    memset(&vk_entity.frame_push, 0, sizeof(vk_entity.frame_push));
    memset(&vk_entity.frame_push_weapon, 0, sizeof(vk_entity.frame_push_weapon));

    if (!vk_entity.initialized || !fd || !vk_drawentities || !vk_drawentities->integer) {
        return;
    }

    vec3_t view_axis[3];
    AnglesToAxis(fd->viewangles, view_axis);
    const bsp_t *world_bsp = VK_World_GetBsp();
    if (world_bsp != vk_entity.bmodel_texture_bsp) {
        VK_Entity_ClearBspTextureCache();
    }

    for (int i = 0; i < fd->num_entities; i++) {
        const entity_t *ent = &fd->entities[i];
        if (ent->flags & RF_VIEWERMODEL) {
            continue;
        }
        bool depth_hack = (ent->flags & (RF_DEPTHHACK | RF_WEAPONMODEL)) != 0;
        bool weapon_model = (ent->flags & RF_WEAPONMODEL) != 0;
        vk_entity.frame_weapon_active |= weapon_model;

        if (ent->flags & RF_BEAM) {
            if (!VK_Entity_AddBeam(ent, fd)) {
                return;
            }
            continue;
        }

        if (ent->model & BIT(31)) {
            if (!VK_Entity_AddBspModel(ent, fd, world_bsp, depth_hack, weapon_model)) {
                return;
            }
            continue;
        }

        if (ent->model <= 0 || ent->model > vk_entity.num_models) {
            continue;
        }

        const vk_model_t *model = &vk_entity.models[ent->model - 1];
        if (!model->type) {
            continue;
        }

        if (model->type == VK_MODEL_SPRITE) {
            if (!VK_Entity_AddSprite(ent, view_axis, model, depth_hack, weapon_model)) {
                return;
            }
        } else if (model->type == VK_MODEL_MD2) {
#if USE_MD5
            if (VK_Entity_ShouldUseMD5(ent, fd, model)) {
                if (!VK_Entity_AddMD5(ent, fd, model, depth_hack, weapon_model)) {
                    return;
                }
                continue;
            }
#endif
            if (!VK_Entity_AddMD2(ent, fd, model, depth_hack, weapon_model)) {
                return;
            }
        }
    }

    if (!VK_Entity_AddParticles(fd, view_axis)) {
        return;
    }

    if (!vk_entity.vertex_count) {
        return;
    }

    size_t bytes = (size_t)vk_entity.vertex_count * sizeof(*vk_entity.vertices);
    if (!VK_Entity_EnsureVertexBuffer(bytes)) {
        return;
    }
    if (!vk_entity.vertex_mapped) {
        Com_SetLastError("Vulkan entity: vertex buffer not mapped");
        return;
    }
    memcpy(vk_entity.vertex_mapped, vk_entity.vertices, bytes);

    VK_Entity_BuildFramePush(fd, world_bsp, &vk_entity.frame_push);
    if (vk_entity.frame_weapon_active) {
        VK_Entity_BuildWeaponFramePush(fd, world_bsp, &vk_entity.frame_push_weapon);
    }
    vk_entity.frame_active = true;
}

void VK_Entity_Record(VkCommandBuffer cmd, const VkExtent2D *extent)
{
    if (!vk_entity.initialized || !vk_entity.swapchain_ready ||
        !vk_entity.pipeline_opaque || !vk_entity.pipeline_alpha || !vk_entity.pipeline_depthhack ||
        !vk_entity.frame_active || !vk_entity.vertex_count || !vk_entity.batch_count ||
        !vk_entity.vertex_buffer || !extent) {
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

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_entity.vertex_buffer, &offset);
    vkCmdPushConstants(cmd, vk_entity.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(vk_entity.frame_push), &vk_entity.frame_push);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    for (int pass = 0; pass < 2; pass++) {
        bool alpha = (pass == 1);
        VkPipeline target_pipeline = alpha ? vk_entity.pipeline_alpha : vk_entity.pipeline_opaque;
        VkPipeline bound_pipeline = VK_NULL_HANDLE;
        VkDescriptorSet last_set = VK_NULL_HANDLE;

        for (uint32_t i = 0; i < vk_entity.batch_count; i++) {
            const vk_batch_t *batch = &vk_entity.batches[i];
            if (!batch->vertex_count || !batch->set || batch->depth_hack || batch->alpha != alpha) {
                continue;
            }

            if (bound_pipeline != target_pipeline) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, target_pipeline);
                bound_pipeline = target_pipeline;
                last_set = VK_NULL_HANDLE;
            }

            if (batch->set != last_set) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        vk_entity.pipeline_layout,
                                        0, 1, &batch->set, 0, NULL);
                last_set = batch->set;
            }

            vkCmdDraw(cmd, batch->vertex_count, 1, batch->first_vertex, 0);
        }
    }

    {
        VkPipeline bound_pipeline = VK_NULL_HANDLE;
        VkDescriptorSet last_set = VK_NULL_HANDLE;
        bool using_weapon_push = false;

        for (uint32_t i = 0; i < vk_entity.batch_count; i++) {
            const vk_batch_t *batch = &vk_entity.batches[i];
            if (!batch->vertex_count || !batch->set || !batch->depth_hack) {
                continue;
            }

            bool use_weapon_push = batch->weapon_model && vk_entity.frame_weapon_active;
            if (use_weapon_push != using_weapon_push) {
                const renderer_view_push_t *push = use_weapon_push
                    ? &vk_entity.frame_push_weapon
                    : &vk_entity.frame_push;
                vkCmdPushConstants(cmd, vk_entity.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
                                   0, sizeof(*push), push);
                using_weapon_push = use_weapon_push;
            }

            if (bound_pipeline != vk_entity.pipeline_depthhack) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_entity.pipeline_depthhack);
                bound_pipeline = vk_entity.pipeline_depthhack;
                last_set = VK_NULL_HANDLE;
            }

            if (batch->set != last_set) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        vk_entity.pipeline_layout,
                                        0, 1, &batch->set, 0, NULL);
                last_set = batch->set;
            }

            vkCmdDraw(cmd, batch->vertex_count, 1, batch->first_vertex, 0);
        }
    }
}
