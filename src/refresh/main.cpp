/*
Copyright (C) 2003-2006 Andrey Nazarov

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

/*
 * gl_main.c
 *
 */

#include "gl.hpp"
#include "postprocess/bloom.hpp"
#include "postprocess/crt.hpp"
#include "postprocess/hdr_luminance.hpp"
#include "font_freetype.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {
constexpr float MOTION_BLUR_FOV_EPSILON = 0.01f;
constexpr float MOTION_BLUR_MAX_FRAME_TIME = 0.25f;
}

static void R_ClearMotionBlurHistory(void);
static void R_BindMotionHistoryTextures(void);
static void R_StoreMotionBlurHistory(void);

glRefdef_t glr;
glStatic_t gl_static;
glConfig_t gl_config;
statCounters_t  c;

entity_t gl_world;

refcfg_t r_config;

unsigned r_registration_sequence;

// regular variables
cvar_t *gl_partscale;
cvar_t *gl_partstyle;
cvar_t *gl_beamstyle;
cvar_t *gl_celshading;
cvar_t *gl_dotshading;
cvar_t *gl_shadows;
cvar_t *gl_modulate;
cvar_t *gl_modulate_world;
cvar_t *gl_coloredlightmaps;
cvar_t *gl_lightmap_bits;
cvar_t *gl_brightness;
cvar_t *gl_dynamic;
cvar_t *gl_dlight_falloff;
cvar_t *gl_modulate_entities;
cvar_t *gl_glowmap_intensity;
cvar_t *gl_flarespeed;
cvar_t *gl_fontshadow;
cvar_t *gl_shadow_filter;
cvar_t *gl_shadow_filter_radius;
cvar_t *gl_shaders;
#if USE_MD5
cvar_t *gl_md5_load;
cvar_t *gl_md5_use;
cvar_t *gl_md5_distance;
#endif
cvar_t *gl_damageblend_frac;
cvar_t *r_skipUnderWaterFX;
cvar_t *r_enablefog;
cvar_t *r_shadows;
cvar_t *r_staticshadows;
cvar_t *r_postProcessing;
cvar_t *r_fbo;
cvar_t *r_bloom;
cvar_t *r_bloomBlurRadius;
cvar_t *r_bloomBlurFalloff;
cvar_t *r_bloomBrightThreshold;
cvar_t *r_bloomKnee;
cvar_t *r_bloomIntensity;
cvar_t *r_bloomScale;
cvar_t *r_bloomKernel;
cvar_t *r_bloomBlurScale;
cvar_t *r_bloomPasses;
cvar_t *r_bloomSaturation;
cvar_t *r_bloomSceneIntensity;
cvar_t *r_bloomSceneSaturation;
cvar_t *r_colorCorrect;
static cvar_t *r_bloomThresholdLegacy;
cvar_t *r_hdr;
cvar_t *r_hdr_mode;
cvar_t *r_tonemap;
cvar_t *r_exposure_auto;
cvar_t *r_exposure_key;
cvar_t *r_exposure_speed_up;
cvar_t *r_exposure_speed_down;
cvar_t *r_exposure_ev_min;
cvar_t *r_exposure_ev_max;
cvar_t *r_output_paper_white;
cvar_t *r_output_peak_white;
cvar_t *r_dither;
cvar_t *r_motionBlur;
cvar_t *r_motionBlurShutterSpeed;
cvar_t *r_motionBlurMinVelocity;
cvar_t *r_motionBlurMinVelocityPixels;
cvar_t *r_ui_sdr_style;
cvar_t *r_debug_histogram;
cvar_t *r_debug_tonemap;
cvar_t *r_crtmode;
cvar_t *r_crt_hardScan;
cvar_t *r_crt_hardPix;
cvar_t *r_crt_maskDark;
cvar_t *r_crt_maskLight;
cvar_t *r_crt_scaleInLinearGamma;
cvar_t *r_crt_shadowMask;
cvar_t *r_crt_brightBoost;
cvar_t *r_crt_warpX;
cvar_t *r_crt_warpY;
cvar_t *gl_dof;
cvar_t *gl_dof_quality;
cvar_t *gl_swapinterval;

// development variables
cvar_t *gl_znear;
cvar_t *gl_drawworld;
cvar_t *gl_drawentities;
cvar_t *gl_drawsky;
cvar_t *gl_draworder;
cvar_t *gl_showtris;
cvar_t *gl_showorigins;
cvar_t *gl_showtearing;
cvar_t *gl_showbloom;
#if USE_DEBUG
cvar_t *gl_showscrap;
cvar_t *gl_nobind;
cvar_t *gl_novbo;
cvar_t *gl_test;
#endif
cvar_t *gl_cull_nodes;
cvar_t *gl_cull_models;
cvar_t *gl_showcull;
cvar_t *gl_clear;
cvar_t *gl_clearcolor;
cvar_t *gl_finish;
cvar_t *gl_novis;
cvar_t *gl_lockpvs;
cvar_t *gl_lightmap;
cvar_t *gl_fullbright;
cvar_t *gl_vertexlight;
cvar_t *gl_lightgrid;
cvar_t *gl_polyblend;
cvar_t *gl_showerrors;

int32_t gl_shaders_modified;

enum hdr_mode_t {
    HDR_MODE_SDR = 0,
    HDR_MODE_HDR10 = 1,
    HDR_MODE_SCRGB = 2,
    HDR_MODE_AUTO = 3,
};

enum tonemap_mode_t {
    TONEMAP_ACES = 0,
    TONEMAP_HABLE = 1,
    TONEMAP_REINHARD = 2,
    TONEMAP_LINEAR = 3,
};

struct hdrStateLocal_t {
    bool gpu_reduce_supported = false;
    bool legacy_auto_supported = false;
    bool auto_supported = false;
    float noise_seed = 0.0f;
    std::vector<float> histogram_scratch;
};

static hdrStateLocal_t hdr_state_local;
static bool hdr_warned_auto_exposure_stall = false;
static int32_t r_hdr_modified = 0;
static int32_t r_hdr_mode_modified = 0;
static int32_t r_exposure_auto_modified = 0;
bool gl_flare_occlusion_disabled = false;

// ==============================================================================

/*
=============
GL_SetFramebufferDrawBuffers

Sets the active draw buffers for the currently bound framebuffer, falling back to
single-target glDrawBuffer when glDrawBuffers is unavailable. When the fallback is
used only the first attachment is written to, so multi-render-target features must
remain guarded on draw-buffer support.
=============
*/
static void GL_SetFramebufferDrawBuffers(GLsizei count, const GLenum *buffers)
{
	if (!buffers || count <= 0) {
		if (qglDrawBuffers) {
			qglDrawBuffers(0, nullptr);
		} else if (qglDrawBuffer) {
			qglDrawBuffer(GL_NONE);
		}
		return;
	}

	if (qglDrawBuffers) {
		qglDrawBuffers(count, buffers);
		return;
	}

	if (qglDrawBuffer) {
		qglDrawBuffer(buffers[0]);
	}
}

static void GL_SetupFrustum(void)
{
    vec_t angle, sf, cf;
    vec3_t forward, left, up;
    cplane_t *p;
    int i;

    // right/left
    angle = DEG2RAD(glr.fd.fov_x / 2);
    sf = sinf(angle);
    cf = cosf(angle);

    VectorScale(glr.viewaxis[0], sf, forward);
    VectorScale(glr.viewaxis[1], cf, left);

    VectorAdd(forward, left, glr.frustumPlanes[0].normal);
    VectorSubtract(forward, left, glr.frustumPlanes[1].normal);

    // top/bottom
    angle = DEG2RAD(glr.fd.fov_y / 2);
    sf = sinf(angle);
    cf = cosf(angle);

    VectorScale(glr.viewaxis[0], sf, forward);
    VectorScale(glr.viewaxis[2], cf, up);

    VectorAdd(forward, up, glr.frustumPlanes[2].normal);
    VectorSubtract(forward, up, glr.frustumPlanes[3].normal);

    for (i = 0, p = glr.frustumPlanes; i < 4; i++, p++) {
        p->dist = DotProduct(glr.fd.vieworg, p->normal);
        p->type = PLANE_NON_AXIAL;
        SetPlaneSignbits(p);
    }
}

glCullResult_t GL_CullBox(const vec3_t bounds[2])
{
    box_plane_t bits;
    glCullResult_t cull;

    if (!gl_cull_models->integer)
        return CULL_IN;

    cull = CULL_IN;
    for (int i = 0; i < 4; i++) {
        bits = BoxOnPlaneSide(bounds[0], bounds[1], &glr.frustumPlanes[i]);
        if (bits == BOX_BEHIND)
            return CULL_OUT;
        if (bits != BOX_INFRONT)
            cull = CULL_CLIP;
    }

    return cull;
}

glCullResult_t GL_CullSphere(const vec3_t origin, float radius)
{
    float dist;
    const cplane_t *p;
    glCullResult_t cull;
    int i;

    if (!gl_cull_models->integer)
        return CULL_IN;

    radius *= glr.entscale;

    cull = CULL_IN;
    for (i = 0, p = glr.frustumPlanes; i < 4; i++, p++) {
        dist = PlaneDiff(origin, p);
        if (dist < -radius)
            return CULL_OUT;
        if (dist <= radius)
            cull = CULL_CLIP;
    }

    return cull;
}

glCullResult_t GL_CullLocalBox(const vec3_t origin, const vec3_t bounds[2])
{
    std::array<vec3_t, 8> points{};
    const cplane_t *p;
    int i, j;
    vec_t dot;
    bool infront;
    glCullResult_t cull;

    if (!gl_cull_models->integer)
        return CULL_IN;

    for (i = 0; i < 8; i++) {
        VectorCopy(origin, points[i]);
        VectorMA(points[i], bounds[(i >> 0) & 1][0], glr.entaxis[0], points[i]);
        VectorMA(points[i], bounds[(i >> 1) & 1][1], glr.entaxis[1], points[i]);
        VectorMA(points[i], bounds[(i >> 2) & 1][2], glr.entaxis[2], points[i]);
    }

    cull = CULL_IN;
    for (i = 0, p = glr.frustumPlanes; i < 4; i++, p++) {
        infront = false;
        for (j = 0; j < 8; j++) {
            dot = DotProduct(points[j], p->normal);
            if (dot >= p->dist) {
                infront = true;
                if (cull == CULL_CLIP)
                    break;
            } else {
                cull = CULL_CLIP;
                if (infront)
                    break;
            }
        }
        if (!infront)
            return CULL_OUT;
    }

    return cull;
}

// shared between lightmap and scrap allocators
bool GL_AllocBlock(int width, int height, uint16_t *inuse,
                   int w, int h, int *s, int *t)
{
    int i, j, k, x, y, max_inuse, min_inuse;

    x = 0; y = height;
    min_inuse = height;
    for (i = 0; i < width - w; i++) {
        max_inuse = 0;
        for (j = 0; j < w; j++) {
            k = inuse[i + j];
            if (k >= min_inuse)
                break;
            if (max_inuse < k)
                max_inuse = k;
        }
        if (j == w) {
            x = i;
            y = min_inuse = max_inuse;
        }
    }

    if (y + h > height)
        return false;

    for (i = 0; i < w; i++)
        inuse[x + i] = y + h;

    *s = x;
    *t = y;
    return true;
}

// P = A * B
void GL_MultMatrix(GLfloat *restrict p, const GLfloat *restrict a, const GLfloat *restrict b)
{
    int i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            p[i * 4 + j] =
                a[0 * 4 + j] * b[i * 4 + 0] +
                a[1 * 4 + j] * b[i * 4 + 1] +
                a[2 * 4 + j] * b[i * 4 + 2] +
                a[3 * 4 + j] * b[i * 4 + 3];
        }
    }
}

void GL_SetEntityAxis(void)
{
    const entity_t *e = glr.ent;

    glr.entrotated = false;
    glr.entscale = 1;

    if (VectorEmpty(e->angles)) {
        VectorSet(glr.entaxis[0], 1, 0, 0);
        VectorSet(glr.entaxis[1], 0, 1, 0);
        VectorSet(glr.entaxis[2], 0, 0, 1);
    } else {
        AnglesToAxis(e->angles, glr.entaxis);
        glr.entrotated = true;
    }

    if ((e->scale[0] && e->scale[0] != 1) ||
        (e->scale[1] && e->scale[1] != 1) ||
        (e->scale[2] && e->scale[2] != 1)) {
        VectorScale(glr.entaxis[0], e->scale[0], glr.entaxis[0]);
        VectorScale(glr.entaxis[1], e->scale[1], glr.entaxis[1]);
        VectorScale(glr.entaxis[2], e->scale[2], glr.entaxis[2]);
        glr.entrotated = true;
        glr.entscale = max(e->scale[0], max(e->scale[1], e->scale[2]));
    }
}

void GL_RotationMatrix(GLfloat *matrix)
{
    matrix[ 0] = glr.entaxis[0][0];
    matrix[ 4] = glr.entaxis[1][0];
    matrix[ 8] = glr.entaxis[2][0];
    matrix[12] = glr.ent->origin[0];

    matrix[ 1] = glr.entaxis[0][1];
    matrix[ 5] = glr.entaxis[1][1];
    matrix[ 9] = glr.entaxis[2][1];
    matrix[13] = glr.ent->origin[1];

    matrix[ 2] = glr.entaxis[0][2];
    matrix[ 6] = glr.entaxis[1][2];
    matrix[10] = glr.entaxis[2][2];
    matrix[14] = glr.ent->origin[2];

    matrix[ 3] = 0;
    matrix[ 7] = 0;
    matrix[11] = 0;
    matrix[15] = 1;
}

void GL_RotateForEntity(void)
{
    bool skies = false;

    if (glr.ent == &gl_world)
        skies = gl_static.use_cubemaps;
    else if (glr.ent->model & BIT(31))
        skies = gl_static.use_bmodel_skies;

    GL_RotationMatrix(glr.entmatrix);
    if (skies) {
        GL_MultMatrix(gls.u_block.m_sky[0], glr.skymatrix[0], glr.entmatrix);
        GL_MultMatrix(gls.u_block.m_sky[1], glr.skymatrix[1], glr.entmatrix);
    }
    GL_ForceMatrix(glr.entmatrix, glr.viewmatrix);
}

static void GL_DrawSpriteModel(const model_t *model)
{
    const entity_t *e = glr.ent;
    const mspriteframe_t *frame = &model->spriteframes[e->frame % model->numframes];
    const image_t *image = frame->image;
    const float alpha = (e->flags & RF_TRANSLUCENT) ? e->alpha : 1.0f;
    const float scale = e->scale[0] ? e->scale[0] : 1.0f;
    glStateBits_t bits = GLS_DEPTHMASK_FALSE | glr.fog_bits;
    vec3_t up, down, left, right;

    if (alpha == 1.0f) {
        if (image->flags & IF_TRANSPARENT) {
            if (image->flags & IF_PALETTED)
                bits |= GLS_ALPHATEST_ENABLE;
            else
                bits |= GLS_BLEND_BLEND;
        }
    } else {
        bits |= GLS_BLEND_BLEND;
    }

    GL_LoadMatrix(gl_identity, glr.viewmatrix);
    GL_LoadUniforms();
    GL_BindTexture(TMU_TEXTURE, image->texnum);
    GL_BindArrays(VA_SPRITE);
    GL_StateBits(bits);
    GL_ArrayBits(GLA_VERTEX | GLA_TC);
    GL_Color(1, 1, 1, alpha);

    VectorScale(glr.viewaxis[1], frame->origin_x * scale, left);
    VectorScale(glr.viewaxis[1], (frame->origin_x - frame->width) * scale, right);
    VectorScale(glr.viewaxis[2], -frame->origin_y * scale, down);
    VectorScale(glr.viewaxis[2], (frame->height - frame->origin_y) * scale, up);

    VectorAdd3(e->origin, down, left,  tess.vertices);
    VectorAdd3(e->origin, up,   left,  tess.vertices +  5);
    VectorAdd3(e->origin, down, right, tess.vertices + 10);
    VectorAdd3(e->origin, up,   right, tess.vertices + 15);

    tess.vertices[ 3] = 0; tess.vertices[ 4] = 1;
    tess.vertices[ 8] = 0; tess.vertices[ 9] = 0;
    tess.vertices[13] = 1; tess.vertices[14] = 1;
    tess.vertices[18] = 1; tess.vertices[19] = 0;

    GL_LockArrays(4);
    qglDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GL_UnlockArrays();
}

static void GL_DrawNullModel(void)
{
    const entity_t *e = glr.ent;

    if (e->flags & RF_WEAPONMODEL)
        return;

    VectorCopy(e->origin, tess.vertices +  0);
    VectorCopy(e->origin, tess.vertices +  8);
    VectorCopy(e->origin, tess.vertices + 16);

    VectorMA(e->origin, 16, glr.entaxis[0], tess.vertices +  4);
    VectorMA(e->origin, 16, glr.entaxis[1], tess.vertices + 12);
    VectorMA(e->origin, 16, glr.entaxis[2], tess.vertices + 20);

    WN32(tess.vertices +  3, COLOR_RED.u32);
    WN32(tess.vertices +  7, COLOR_RED.u32);

    WN32(tess.vertices + 11, COLOR_GREEN.u32);
    WN32(tess.vertices + 15, COLOR_GREEN.u32);

    WN32(tess.vertices + 19, COLOR_BLUE.u32);
    WN32(tess.vertices + 23, COLOR_BLUE.u32);

    GL_LoadMatrix(glr.entmatrix, glr.viewmatrix);
    GL_LoadUniforms();
    GL_BindTexture(TMU_TEXTURE, TEXNUM_WHITE);
    GL_BindArrays(VA_NULLMODEL);
    GL_StateBits(GLS_DEFAULT);
    GL_ArrayBits(GLA_VERTEX | GLA_COLOR);

    GL_LockArrays(6);
    qglDrawArrays(GL_LINES, 0, 6);
    GL_UnlockArrays();
}

static void make_flare_quad(const vec3_t origin, float scale)
{
    vec3_t up, down, left, right;

    VectorScale(glr.viewaxis[1],  scale, left);
    VectorScale(glr.viewaxis[1], -scale, right);
    VectorScale(glr.viewaxis[2], -scale, down);
    VectorScale(glr.viewaxis[2],  scale, up);

    VectorAdd3(origin, down, left,  tess.vertices + 0);
    VectorAdd3(origin, up,   left,  tess.vertices + 3);
    VectorAdd3(origin, down, right, tess.vertices + 6);
    VectorAdd3(origin, up,   right, tess.vertices + 9);
}

/*
=============
GL_OccludeFlares

Submit occlusion queries for flare entities when supported.
=============
*/
static void GL_OccludeFlares(void)
{
	const bsp_t *bsp = gl_static.world.cache;
	const entity_t *ent;
	glquery_t *q;
	vec3_t dir, org;
	float scale, dist;
	bool set = false;
	int i;

	if (!gl_static.queries)
		return;

	if (gl_flare_occlusion_disabled)
		return;

	for (ent = glr.ents.flares; ent; ent = ent->next) {
		q = HashMap_Lookup(glquery_t, gl_static.queries, &ent->skinnum);

		for (i = 0; i < 4; i++)
			if (PlaneDiff(ent->origin, &glr.frustumPlanes[i]) < -2.5f)
				break;
		if (i != 4) {
			if (q)
				q->pending = q->visible = false;
			continue;   // not visible
		}

		if (q) {
			// reset visibility if entity disappeared
			if (com_eventTime - q->timestamp >= 2500) {
				q->pending = q->visible = false;
				q->frac = 0;
			} else {
				if (q->pending)
					continue;
				if (com_eventTime - q->timestamp <= 33)
					continue;
			}
		} else {
			glquery_t new_query{};
			uint32_t map_size = HashMap_Size(gl_static.queries);
			Q_assert(map_size < MAX_EDICTS);
			qglGenQueries(1, &new_query.query);
			if (!new_query.query) {
				if (!gl_flare_occlusion_disabled)
					Com_WPrintf("%s: failed to allocate occlusion query, disabling flare occlusion.\n", __func__);
				gl_flare_occlusion_disabled = true;
				if (set)
					qglColorMask(1, 1, 1, 1);
				return;
			}
			HashMap_Insert(gl_static.queries, &ent->skinnum, &new_query);
			q = HashMap_GetValue(glquery_t, gl_static.queries, map_size);
		}

		if (!set) {
			GL_LoadMatrix(gl_identity, glr.viewmatrix);
			GL_LoadUniforms();
			GL_BindTexture(TMU_TEXTURE, TEXNUM_WHITE);
			GL_BindArrays(VA_OCCLUDE);
			GL_StateBits(GLS_DEPTHMASK_FALSE);
			GL_ArrayBits(GLA_VERTEX);
			qglColorMask(0, 0, 0, 0);
			set = true;
		}

		VectorSubtract(ent->origin, glr.fd.vieworg, dir);
		dist = DotProduct(dir, glr.viewaxis[0]);

		scale = 2.5f;
		if (dist > 20)
			scale += dist * 0.004f;

		if (bsp && BSP_PointLeaf(bsp->nodes, ent->origin)->contents[0] & CONTENTS_SOLID) {
			VectorNormalize(dir);
			VectorMA(ent->origin, -5.0f, dir, org);
			make_flare_quad(org, scale);
		} else
			make_flare_quad(ent->origin, scale);

		if (!q->query) {
			q->visible = true;
			q->pending = false;
			q->frac = 1.0f;
			q->timestamp = com_eventTime;
			continue;
		}

		GL_LockArrays(4);
		qglBeginQuery(gl_static.samples_passed, q->query);
		qglDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		qglEndQuery(gl_static.samples_passed);
		GL_UnlockArrays();

		q->timestamp = com_eventTime;
		q->pending = true;

		c.occlusionQueries++;
	}

	if (set)
		qglColorMask(1, 1, 1, 1);
}

void GL_ClassifyEntities(void)
{
    entity_t *ent;
    int i;

    memset(&glr.ents, 0, sizeof(glr.ents));

    if (!gl_drawentities->integer)
        return;

    for (i = 0, ent = glr.fd.entities; i < glr.fd.num_entities; i++, ent++) {
        if (ent->flags & RF_BEAM) {
            if (ent->frame) {
                ent->next = glr.ents.beams;
                glr.ents.beams = ent;
            }
            continue;
        }

        if (ent->flags & RF_FLARE) {
            if (gl_static.queries) {
                ent->next = glr.ents.flares;
                glr.ents.flares = ent;
            }
            continue;
        }

        if (ent->model & BIT(31)) {
            ent->next = glr.ents.bmodels;
            glr.ents.bmodels = ent;
            continue;
        }

        if (!(ent->flags & RF_TRANSLUCENT)) {
            ent->next = glr.ents.opaque;
            glr.ents.opaque = ent;
            continue;
        }

        if ((ent->flags & RF_WEAPONMODEL) || ent->alpha <= gl_draworder->value) {
            ent->next = glr.ents.alpha_front;
            glr.ents.alpha_front = ent;
            continue;
        }

        ent->next = glr.ents.alpha_back;
        glr.ents.alpha_back = ent;
    }
}

void GL_DrawEntities(entity_t *ent)
{
    model_t *model;

    for (; ent; ent = ent->next) {
        glr.ent = ent;

        // convert angles to axis
        GL_SetEntityAxis();

        // inline BSP model
        if (ent->model & BIT(31)) {
            const bsp_t *bsp = gl_static.world.cache;
            int index = ~ent->model;

            if (!bsp)
                Com_Error(ERR_DROP, "%s: inline model without world",
                          __func__);

            if (index < 1 || index >= bsp->nummodels)
                Com_Error(ERR_DROP, "%s: inline model %d out of range",
                          __func__, index);

            GL_DrawBspModel(&bsp->models[index]);
            continue;
        }

        model = MOD_ForHandle(ent->model);
        if (!model) {
            GL_DrawNullModel();
            continue;
        }

        switch (model->type) {
        case MOD_ALIAS:
            GL_DrawAliasModel(model);
            break;
        case MOD_SPRITE:
            GL_DrawSpriteModel(model);
            break;
        case MOD_EMPTY:
            break;
        default:
            Q_assert(!"bad model type");
        }

        if (gl_showorigins->integer)
            GL_DrawNullModel();
    }
}

static void GL_DrawTearing(void)
{
    static int i;

    // alternate colors to make tearing obvious
    i++;
    if (i & 1)
        qglClearColor(1, 1, 1, 1);
    else
        qglClearColor(1, 0, 0, 0);

    qglClear(GL_COLOR_BUFFER_BIT);
    qglClearColor(Vector4Unpack(gl_static.clearcolor));
}

static const char *GL_ErrorString(GLenum err)
{
    switch (err) {
#define E(x) case GL_##x: return "GL_"#x;
        E(NO_ERROR)
        E(INVALID_ENUM)
        E(INVALID_VALUE)
        E(INVALID_OPERATION)
        E(STACK_OVERFLOW)
        E(STACK_UNDERFLOW)
        E(OUT_OF_MEMORY)
#undef E
    }

    return "UNKNOWN ERROR";
}

void GL_ClearErrors(void)
{
    GLenum err;

    while ((err = qglGetError()) != GL_NO_ERROR)
        ;
}

bool GL_ShowErrors(const char *func)
{
    GLenum err = qglGetError();

    if (err == GL_NO_ERROR)
        return false;

    do {
        if (gl_showerrors->integer)
            Com_EPrintf("%s: %s\n", func, GL_ErrorString(err));
    } while ((err = qglGetError()) != GL_NO_ERROR);

    return true;
}

static void HDR_ResetState(void)
{
    gl_static.hdr.exposure = 1.0f;
    gl_static.hdr.target_exposure = 1.0f;
    gl_static.hdr.average_luminance = 0.18f;
    gl_static.hdr.max_mip_level = 0;
    gl_static.hdr.histogram.fill(0.0f);
    gl_static.hdr.histogram_scale = 1.0f;
    gl_static.hdr.graph_max_input = 16.0f;
}

static void HDR_InitializeCapabilities(void)
{
    const bool float_textures = (gl_config.ver_gl >= QGL_VER(3, 0)) || (gl_config.ver_es >= QGL_VER(3, 0));
    gl_static.hdr.supported = float_textures;
    hdr_state_local.gpu_reduce_supported = gl_static.use_shaders;
    hdr_state_local.legacy_auto_supported = qglGetTexImage != nullptr;
    hdr_state_local.auto_supported = hdr_state_local.gpu_reduce_supported || hdr_state_local.legacy_auto_supported;
}

static void HDR_UpdateConfig(void)
{
    gl_static.hdr.bloom_intensity = max(r_bloomIntensity->value, 0.0f);
    gl_static.hdr.paper_white = max(r_output_paper_white->value, 10.0f);
    gl_static.hdr.peak_white = max(gl_static.hdr.paper_white, r_output_peak_white->value);
    gl_static.hdr.exposure_key = max(r_exposure_key->value, 0.0001f);
    gl_static.hdr.exposure_speed_up = max(r_exposure_speed_up->value, 0.0f);
    gl_static.hdr.exposure_speed_down = max(r_exposure_speed_down->value, 0.0f);
    gl_static.hdr.exposure_ev_min = r_exposure_ev_min->value;
    gl_static.hdr.exposure_ev_max = r_exposure_ev_max->value;
    gl_static.hdr.dither_strength = r_dither->integer ? 1.0f / 255.0f : 0.0f;
    gl_static.hdr.ui_sdr_mix = r_ui_sdr_style->integer ? 1.0f : 0.0f;
    gl_static.hdr.debug_histogram = r_debug_histogram->integer != 0;
    gl_static.hdr.debug_tonemap = r_debug_tonemap->integer != 0;
    gl_static.hdr.tonemap = Q_bound<int>(TONEMAP_ACES, r_tonemap->integer, TONEMAP_LINEAR);

    int hdr_mode = Q_bound<int>(HDR_MODE_SDR, r_hdr_mode->integer, HDR_MODE_AUTO);
    if (hdr_mode == HDR_MODE_AUTO)
        hdr_mode = gl_static.hdr.supported ? HDR_MODE_HDR10 : HDR_MODE_SDR;
    gl_static.hdr.mode = hdr_mode;

    gl_static.hdr.auto_exposure = hdr_state_local.auto_supported && r_exposure_auto->integer;
    gl_static.hdr.graph_max_input = powf(2.0f, gl_static.hdr.exposure_ev_max);
}

static void HDR_UpdatePostprocessFormats(void)
{
    if (gl_static.hdr.active) {
        gl_static.postprocess_internal_format = GL_RGBA16F;
        gl_static.postprocess_format = GL_RGBA;
        gl_static.postprocess_type = GL_HALF_FLOAT;
    } else {
        gl_static.postprocess_internal_format = GL_RGBA;
        gl_static.postprocess_format = GL_RGBA;
        gl_static.postprocess_type = GL_UNSIGNED_BYTE;
    }
}

static void HDR_ComputeHistogram(int width, int height)
{
    if (!gl_static.hdr.debug_histogram)
        return;

    if (!qglReadPixels)
        return;

    const int sample_limit = 128;
    int sample_w = min(width, sample_limit);
    int sample_h = min(height, sample_limit);
    float *scratch = nullptr;

    bool have_samples = false;
    if (hdr_state_local.gpu_reduce_supported && g_hdr_luminance.available()) {
        int reduce_w = 0;
        int reduce_h = 0;
        if (g_hdr_luminance.readbackHistogram(sample_limit, hdr_state_local.histogram_scratch, reduce_w, reduce_h)) {
            sample_w = reduce_w;
            sample_h = reduce_h;
            scratch = hdr_state_local.histogram_scratch.data();
            have_samples = true;
        }
    }

    if (!have_samples) {
        if (!qglReadPixels)
            return;

        sample_w = min(width, sample_limit);
        sample_h = min(height, sample_limit);
        if (sample_w <= 0 || sample_h <= 0)
            return;

        hdr_state_local.histogram_scratch.resize(static_cast<size_t>(sample_w) * sample_h * 4);
        scratch = hdr_state_local.histogram_scratch.data();

        GLint prev_fbo = 0;
        GLint prev_read_buffer = 0;
        qglGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        if (qglReadBuffer)
            qglGetIntegerv(GL_READ_BUFFER, &prev_read_buffer);

        qglBindFramebuffer(GL_FRAMEBUFFER, FBO_SCENE);
        if (qglReadBuffer)
            qglReadBuffer(GL_COLOR_ATTACHMENT0);
        qglReadPixels(0, 0, sample_w, sample_h, GL_RGBA, GL_FLOAT, scratch);
        qglBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
        if (qglReadBuffer)
            qglReadBuffer(prev_read_buffer);
        have_samples = true;
    }

    if (!have_samples || !scratch)
        return;

    constexpr int bins = 64;
    std::array<float, bins> counts{};
    counts.fill(0.0f);

    const float ev_min = gl_static.hdr.exposure_ev_min;
    const float ev_max = max(gl_static.hdr.exposure_ev_max, ev_min + 0.0001f);
    const float ev_span = ev_max - ev_min;

    const size_t total_pixels = static_cast<size_t>(sample_w) * sample_h;
    for (size_t i = 0; i < total_pixels; ++i) {
        const float r = scratch[i * 4 + 0];
        const float g = scratch[i * 4 + 1];
        const float b = scratch[i * 4 + 2];
        const float luminance = max(0.0f, r * 0.2126f + g * 0.7152f + b * 0.0722f);
        const float ev = std::log2(max(luminance, 1e-5f));
        const float norm = (ev - ev_min) / ev_span;
        int bin = static_cast<int>(norm * bins);
        bin = Q_bound(0, bin, bins - 1);
        counts[bin] += 1.0f;
    }

    float peak = 1.0f;
    for (float value : counts)
        peak = max(peak, value);

    gl_static.hdr.histogram_scale = (peak > 0.0f) ? (1.0f / peak) : 1.0f;
    for (int i = 0; i < bins; ++i)
        gl_static.hdr.histogram[i] = counts[i];
}

/*
=============
HDR_UpdateExposure

Updates automatic HDR exposure state using GPU reduction or a legacy fallback.
=============
*/
static void HDR_UpdateExposure(int width, int height)
{
	if (!gl_static.hdr.active)
		return;

	if (!glr.framebuffer_ok)
		return;

	if (width <= 0 || height <= 0)
		return;

	hdr_state_local.noise_seed = glr.fd.time;

	const glTmu_t prevActiveTmu = gls.server_tmu;
	GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);

	const bool need_reduction = hdr_state_local.gpu_reduce_supported &&
		(gl_static.hdr.auto_exposure || gl_static.hdr.debug_histogram || gl_static.hdr.debug_tonemap);
	bool reduction_ok = false;
	if (need_reduction)
		reduction_ok = g_hdr_luminance.reduce(TEXNUM_PP_SCENE, width, height);

	if (need_reduction && !reduction_ok) {
		hdr_state_local.gpu_reduce_supported = false;
		if (gl_showerrors->integer)
			Com_EPrintf("HDR exposure: GPU reduction failed for %dx%d, forcing fallback\n", width, height);
	}

	const int max_dim = max(width, height);
	const int fallback_mip = max(0, static_cast<int>(std::floor(std::log2(static_cast<float>(max_dim)))));
	bool use_fallback = !reduction_ok;
	int target_mip = use_fallback ? fallback_mip : 0;

	if (use_fallback && qglGenerateMipmap)
		qglGenerateMipmap(GL_TEXTURE_2D);

	float pixel[4] = { gl_static.hdr.exposure_key, gl_static.hdr.exposure_key, gl_static.hdr.exposure_key, 1.0f };
	bool have_samples = false;
	if (reduction_ok) {
		if (g_hdr_luminance.readbackAverage(pixel)) {
			have_samples = true;
		} else {
			reduction_ok = false;
			hdr_state_local.gpu_reduce_supported = false;
			if (!use_fallback && qglGenerateMipmap)
				qglGenerateMipmap(GL_TEXTURE_2D);
			use_fallback = true;
			target_mip = fallback_mip;
		}
	}

	gl_static.hdr.max_mip_level = target_mip;

	bool fallback_read_ok = !use_fallback;
	if (use_fallback && hdr_state_local.legacy_auto_supported && qglGetTexImage) {
		GL_ClearErrors();
		qglGetTexImage(GL_TEXTURE_2D, gl_static.hdr.max_mip_level, GL_RGBA, GL_FLOAT, pixel);
		fallback_read_ok = !GL_ShowErrors("HDR exposure fallback readback");
		if (!fallback_read_ok) {
			hdr_state_local.legacy_auto_supported = false;
			hdr_state_local.auto_supported = hdr_state_local.gpu_reduce_supported || hdr_state_local.legacy_auto_supported;
			if (gl_showerrors->integer)
				Com_EPrintf("HDR exposure: disabling legacy fallback after failed readback\n");
		} else {
			have_samples = true;
		}
	}

	if (!use_fallback || fallback_read_ok)
		hdr_state_local.auto_supported = hdr_state_local.gpu_reduce_supported || hdr_state_local.legacy_auto_supported;

	if (have_samples && hdr_warned_auto_exposure_stall)
		hdr_warned_auto_exposure_stall = false;

	if (!have_samples) {
		hdr_state_local.auto_supported = hdr_state_local.gpu_reduce_supported || hdr_state_local.legacy_auto_supported;
		if (!hdr_warned_auto_exposure_stall && gl_showerrors->integer) {
			hdr_warned_auto_exposure_stall = true;
			Com_EPrintf("HDR exposure: no valid samples available, auto-exposure frozen\n");
		}
		GL_ActiveTexture(prevActiveTmu);
		return;
	}

	const float luminance = max(1e-5f, pixel[0] * 0.2126f + pixel[1] * 0.7152f + pixel[2] * 0.0722f);
	gl_static.hdr.average_luminance = luminance;

	float target_ev;
	if (gl_static.hdr.auto_exposure)
		target_ev = std::log2(gl_static.hdr.exposure_key / luminance);
	else
		target_ev = gl_static.hdr.exposure_key;

	target_ev = Q_bound(gl_static.hdr.exposure_ev_min, target_ev, gl_static.hdr.exposure_ev_max);
	const float target_exposure = powf(2.0f, target_ev);
	gl_static.hdr.target_exposure = target_exposure;

	float exposure = gl_static.hdr.exposure;
	const float delta = target_exposure - exposure;
	const float speed = (delta > 0.0f) ? gl_static.hdr.exposure_speed_up : gl_static.hdr.exposure_speed_down;

	if (speed <= 0.0f) {
		exposure = target_exposure;
	} else {
		const float step = speed * glr.fd.frametime;
		if (fabsf(delta) <= step)
			exposure = target_exposure;
		else
			exposure += (delta > 0.0f) ? step : -step;
	}

	gl_static.hdr.exposure = exposure;

	HDR_ComputeHistogram(width, height);

	GL_ActiveTexture(prevActiveTmu);

	if (gl_showerrors->integer > 1) {
		Com_DPrintf("HDR exposure: reduction_ok=%d fallback=%d mip=%d gpu_supported=%d legacy_supported=%d auto_supported=%d luminance=%g exposure=%g\n",
			reduction_ok, use_fallback, gl_static.hdr.max_mip_level,
			hdr_state_local.gpu_reduce_supported, hdr_state_local.legacy_auto_supported,
			hdr_state_local.auto_supported, luminance, exposure);
	}
}

void R_HDRUpdateUniforms(void)
{
    const float inv_width = glr.fd.width > 0 ? 1.0f / glr.fd.width : 0.0f;
    const float inv_height = glr.fd.height > 0 ? 1.0f / glr.fd.height : 0.0f;

    gls.u_block.hdr_exposure[0] = gl_static.hdr.exposure;
    gls.u_block.hdr_exposure[1] = gl_static.hdr.average_luminance;
    gls.u_block.hdr_exposure[2] = gl_static.hdr.target_exposure;
    gls.u_block.hdr_exposure[3] = hdr_state_local.noise_seed;

    gls.u_block.hdr_params0[0] = static_cast<float>(gl_static.hdr.tonemap);
    gls.u_block.hdr_params0[1] = gl_static.hdr.paper_white;
    gls.u_block.hdr_params0[2] = gl_static.hdr.peak_white;
    gls.u_block.hdr_params0[3] = gl_static.hdr.bloom_intensity;

    gls.u_block.hdr_params1[0] = static_cast<float>(gl_static.hdr.mode);
    gls.u_block.hdr_params1[1] = gl_static.hdr.exposure_key;
    gls.u_block.hdr_params1[2] = gl_static.hdr.exposure_ev_min;
    gls.u_block.hdr_params1[3] = gl_static.hdr.exposure_ev_max;

    gls.u_block.hdr_params2[0] = inv_width;
    gls.u_block.hdr_params2[1] = inv_height;
    gls.u_block.hdr_params2[2] = gl_static.hdr.graph_max_input;
    gls.u_block.hdr_params2[3] = gl_static.hdr.histogram_scale;

    gls.u_block.hdr_params3[0] = gl_static.hdr.dither_strength;
    gls.u_block.hdr_params3[1] = gl_static.hdr.debug_histogram ? 1.0f : 0.0f;
    gls.u_block.hdr_params3[2] = gl_static.hdr.debug_tonemap ? 1.0f : 0.0f;
    gls.u_block.hdr_params3[3] = gl_static.hdr.ui_sdr_mix;

    gls.u_block.bloom_params[0] = (std::max)(r_bloomSceneIntensity->value, 0.0f);
    gls.u_block.bloom_params[1] = (std::max)(r_bloomSceneSaturation->value, 0.0f);
    gls.u_block.bloom_params[2] = (std::max)(r_bloomSaturation->value, 0.0f);
    gls.u_block.bloom_params[3] = 0.0f;

    const float color_correct_strength = (std::max)(r_colorCorrect->value, 0.0f);
    gls.u_block.color_correction[0] = color_correct_strength;
    gls.u_block.color_correction[1] = 0.95f;
    gls.u_block.color_correction[2] = 1.0f;
    gls.u_block.color_correction[3] = 1.05f;

    for (int i = 0; i < 16; ++i)
        for (int j = 0; j < 4; ++j)
            gls.u_block.hdr_histogram[i][j] = gl_static.hdr.histogram[i * 4 + j];

    gls.u_block_dirty = true;
}

/*
=============
GL_PostProcess

Renders a screen-aligned quad for the active post-process stage using the
provided rectangle while clamping UVs to the populated scene region.
=============
*/
void GL_PostProcess(glStateBits_t bits, int x, int y, int w, int h)
{
	GL_BindArrays(VA_POSTPROCESS);
	GL_StateBits(GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_FALSE |
			GLS_CULL_DISABLE | GLS_TEXTURE_REPLACE | bits);
	GL_ArrayBits(GLA_VERTEX | GLA_TC);
	gl_backend->load_uniforms();

	float u_min = 0.0f;
	float v_min = 0.0f;
	float u_max = 1.0f;
	float v_max = 1.0f;
	if (glr.framebuffer_width > 0 && glr.fd.width > 0) {
		const float ratio_w = static_cast<float>(glr.fd.width) / static_cast<float>(glr.framebuffer_width);
		u_max = (std::min)(ratio_w, 1.0f);
	}
	if (glr.framebuffer_height > 0 && glr.fd.height > 0) {
		const float ratio_h = static_cast<float>(glr.fd.height) / static_cast<float>(glr.framebuffer_height);
		v_max = (std::min)(ratio_h, 1.0f);
	}

	Vector4Set(tess.vertices,      x,     y,     u_min, v_max);
	Vector4Set(tess.vertices +  4, x,     y + h, u_min, v_min);
	Vector4Set(tess.vertices +  8, x + w, y,     u_max, v_max);
	Vector4Set(tess.vertices + 12, x + w, y + h, u_max, v_min);

	GL_LockArrays(4);
	qglDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GL_UnlockArrays();
}


/*
=============
R_GetFinalCompositeRect

Calculates the backbuffer-space rectangle for the final post-process
composite.
=============
*/
static void R_GetFinalCompositeRect(int *x, int *y, int *w, int *h)
{
	if (!x || !y || !w || !h)
		return;

	*x = glr.fd.x;
	*y = glr.fd.y;
	*w = glr.fd.width;
	*h = glr.fd.height;
}

static void GL_BokehViewport(int w, int h)
{
    qglViewport(0, 0, w, h);
    GL_Ortho(0, w, h, 0, -1, 1);
}

static void GL_BokehSetScreen(int target_w, int target_h, int source_w, int source_h)
{
    const float ratio_w = (target_w > 0 && source_w > 0) ? static_cast<float>(source_w) / static_cast<float>(target_w) : 0.0f;
    const float ratio_h = (target_h > 0 && source_h > 0) ? static_cast<float>(source_h) / static_cast<float>(target_h) : 0.0f;
    const float inv_source_w = source_w > 0 ? 1.0f / static_cast<float>(source_w) : 0.0f;
    const float inv_source_h = source_h > 0 ? 1.0f / static_cast<float>(source_h) : 0.0f;
    Vector4Set(gls.u_block.dof_screen, ratio_w, ratio_h, inv_source_w, inv_source_h);
    gls.u_block_dirty = true;
}

static void GL_BokehCoCPass(int target_w, int target_h, int source_w, int source_h)
{
    GL_BokehViewport(target_w, target_h);
    GL_BokehSetScreen(target_w, target_h, source_w, source_h);
    GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_DEPTH);
    qglBindFramebuffer(GL_FRAMEBUFFER, FBO_BOKEH_COC);
    GL_PostProcess(GLS_BOKEH_COC, 0, 0, target_w, target_h);
}

static void GL_BokehInitialBlurPass(int target_w, int target_h, int source_w, int source_h)
{
    GL_BokehViewport(target_w, target_h);
    GL_BokehSetScreen(target_w, target_h, source_w, source_h);
    GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
    GL_ForceTexture(TMU_LIGHTMAP, TEXNUM_PP_DOF_COC);
    qglBindFramebuffer(GL_FRAMEBUFFER, FBO_BOKEH_RESULT);
    GL_PostProcess(GLS_BOKEH_INITIAL, 0, 0, target_w, target_h);
}

static void GL_BokehDownsamplePass(int target_w, int target_h, int source_w, int source_h)
{
    GL_BokehViewport(target_w, target_h);
    GL_BokehSetScreen(target_w, target_h, source_w, source_h);
    GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_DOF_RESULT);
    GL_ForceTexture(TMU_LIGHTMAP, TEXNUM_PP_DOF_COC);
    qglBindFramebuffer(GL_FRAMEBUFFER, FBO_BOKEH_HALF);
    GL_PostProcess(GLS_BOKEH_DOWNSAMPLE, 0, 0, target_w, target_h);
}

static void GL_BokehGatherPass(int target_w, int target_h, int source_w, int source_h)
{
    GL_BokehViewport(target_w, target_h);
    GL_BokehSetScreen(target_w, target_h, source_w, source_h);
    GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_DOF_HALF);
    qglBindFramebuffer(GL_FRAMEBUFFER, FBO_BOKEH_GATHER);
    GL_PostProcess(GLS_BOKEH_GATHER, 0, 0, target_w, target_h);
}

/*
=============
GL_BokehCombinePass

Combines the depth of field gather result with the scene at the result resolution.
=============
*/
static void GL_BokehCombinePass(int target_w, int target_h, int source_w, int source_h)
{
	const int viewport_w = (gl_static.dof.result_width > 0) ? (std::min)(target_w, gl_static.dof.result_width) : target_w;
	const int viewport_h = (gl_static.dof.result_height > 0) ? (std::min)(target_h, gl_static.dof.result_height) : target_h;

	GL_BokehViewport(viewport_w, viewport_h);
	GL_BokehSetScreen(viewport_w, viewport_h, source_w, source_h);
	GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
	GL_ForceTexture(TMU_LIGHTMAP, TEXNUM_PP_DOF_HALF);
	GL_ForceTexture(TMU_GLOWMAP, TEXNUM_PP_DOF_GATHER);
	qglBindFramebuffer(GL_FRAMEBUFFER, FBO_BOKEH_RESULT);
	GL_PostProcess(GLS_BOKEH_COMBINE, 0, 0, viewport_w, viewport_h);
}

/*
=============
GL_RunDepthOfField

Executes the depth of field pipeline for the current frame.
=============
*/
static void GL_RunDepthOfField(void)
{
	const int full_w = gl_static.dof.full_width;
	const int full_h = gl_static.dof.full_height;
	const int result_w = gl_static.dof.result_width;
	const int result_h = gl_static.dof.result_height;
	const int half_w = gl_static.dof.half_width;
	const int half_h = gl_static.dof.half_height;

	if (full_w <= 0 || full_h <= 0 || result_w <= 0 || result_h <= 0 || half_w <= 0 || half_h <= 0)
		return;

	GL_Setup2D();

	GL_BokehCoCPass(full_w, full_h, full_w, full_h);
	GL_BokehInitialBlurPass(result_w, result_h, full_w, full_h);
	GL_BokehDownsamplePass(half_w, half_h, result_w, result_h);
	GL_BokehGatherPass(half_w, half_h, half_w, half_h);
	GL_BokehCombinePass(result_w, result_h, half_w, half_h);

	qglBindFramebuffer(GL_FRAMEBUFFER, 0);
}

typedef enum {
    PP_NONE      = 0,
    PP_WATERWARP = BIT(0),
    PP_BLOOM     = BIT(1),
    PP_DEPTH_OF_FIELD = BIT(2),
    PP_HDR       = BIT(3),
    PP_CRT       = BIT(4),
    PP_MOTION_BLUR = BIT(5),
} pp_flags_t;

constexpr pp_flags_t operator|(pp_flags_t lhs, pp_flags_t rhs) noexcept
{
    using U = std::underlying_type_t<pp_flags_t>;
    return static_cast<pp_flags_t>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

constexpr pp_flags_t operator&(pp_flags_t lhs, pp_flags_t rhs) noexcept
{
    using U = std::underlying_type_t<pp_flags_t>;
    return static_cast<pp_flags_t>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

constexpr pp_flags_t operator^(pp_flags_t lhs, pp_flags_t rhs) noexcept
{
    using U = std::underlying_type_t<pp_flags_t>;
    return static_cast<pp_flags_t>(static_cast<U>(lhs) ^ static_cast<U>(rhs));
}

constexpr pp_flags_t operator~(pp_flags_t value) noexcept
{
    using U = std::underlying_type_t<pp_flags_t>;
    return static_cast<pp_flags_t>(~static_cast<U>(value));
}

constexpr pp_flags_t &operator|=(pp_flags_t &lhs, pp_flags_t rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr pp_flags_t &operator&=(pp_flags_t &lhs, pp_flags_t rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

constexpr pp_flags_t &operator^=(pp_flags_t &lhs, pp_flags_t rhs) noexcept
{
    lhs = lhs ^ rhs;
    return lhs;
}

/*
=============
GL_DrawBloom

Runs the bloom pipeline and composites the scene to the backbuffer.
=============
*/
static void GL_DrawBloom(pp_flags_t flags)
{
	const bool waterwarp = (flags & PP_WATERWARP) != 0;
	const bool depth_of_field = (flags & PP_DEPTH_OF_FIELD) != 0;
	const bool show_bloom = q_unlikely(gl_showbloom->integer);

	int composite_x = 0;
	int composite_y = 0;
	int composite_w = 0;
	int composite_h = 0;
	R_GetFinalCompositeRect(&composite_x, &composite_y, &composite_w, &composite_h);

	BloomRenderContext context{
		.sceneTexture = TEXNUM_PP_SCENE,
		.bloomTexture = TEXNUM_PP_BLOOM,
		.dofTexture = TEXNUM_PP_DOF_RESULT,
		.depthTexture = TEXNUM_PP_DEPTH,
		.viewportX = composite_x,
		.viewportY = composite_y,
		.viewportWidth = composite_w,
		.viewportHeight = composite_h,
		.waterwarp = waterwarp,
		.depthOfField = depth_of_field,
		.showDebug = show_bloom,
		.tonemap = (flags & PP_HDR) != 0,
		.motionBlurReady = glr.motion_blur_ready,
		.updateHdrUniforms = R_HDRUpdateUniforms,
		.runDepthOfField = depth_of_field ? GL_RunDepthOfField : nullptr,
	};

	if (context.motionBlurReady)
		R_BindMotionHistoryTextures();

	g_bloom_effect.render(context);
}

static void R_ClearMotionBlurHistory(void)
{
    glr.motion_blur_history_count = 0;
    glr.motion_blur_history_index = 0;
    for (int i = 0; i < R_MOTION_BLUR_HISTORY_FRAMES; ++i) {
        glr.motion_history_valid[i] = false;
        for (int j = 0; j < 16; ++j)
            glr.motion_history_view_proj[i][j] = gl_identity[j];
    }
}

/*
=============
R_BindMotionHistoryTextures

Binds the motion history textures and restores the previously active texture unit.
=============
*/
static void R_BindMotionHistoryTextures(void)
{
	const glTmu_t prevActiveTmu = gls.server_tmu;

	if ((r_fbo && !r_fbo->integer) || !glr.motion_history_textures_ready) {
		for (int i = 0; i < R_MOTION_BLUR_HISTORY_FRAMES; ++i) {
			glTmu_t tmu = static_cast<glTmu_t>(TMU_HISTORY0 + i);
			GL_ForceTexture(tmu, TEXNUM_BLACK);
		}
		GL_ActiveTexture(prevActiveTmu);
		return;
	}

	for (int i = 0; i < R_MOTION_BLUR_HISTORY_FRAMES; ++i) {
		glTmu_t tmu = static_cast<glTmu_t>(TMU_HISTORY0 + i);
		GLuint tex = TEXNUM_BLACK;
		if (i < glr.motion_blur_history_count) {
			int slot = (glr.motion_blur_history_index + R_MOTION_BLUR_HISTORY_FRAMES - glr.motion_blur_history_count + i) %
				R_MOTION_BLUR_HISTORY_FRAMES;
			if (glr.motion_history_valid[slot])
				tex = TEXNUM_PP_MOTION_HISTORY(slot);
		}
		GL_ForceTexture(tmu, tex);
	}

	GL_ActiveTexture(prevActiveTmu);
}

/*
=============
R_StoreMotionBlurHistory

Copies the current scene into the motion blur history buffer using the active viewport resolution.
=============
*/
static void R_StoreMotionBlurHistory(void)
{
	if (r_fbo && !r_fbo->integer)
		return;
	if (!glr.motion_blur_enabled || !glr.view_proj_valid)
		return;
	if (!glr.motion_history_textures_ready)
		return;

	int composite_x = 0;
	int composite_y = 0;
	int composite_w = 0;
	int composite_h = 0;
	R_GetFinalCompositeRect(&composite_x, &composite_y, &composite_w, &composite_h);
	if (composite_w <= 0 || composite_h <= 0)
		return;

	const int target_index = glr.motion_blur_history_index;

	qglBindFramebuffer(GL_FRAMEBUFFER, FBO_MOTION_HISTORY(target_index));
	qglViewport(0, 0, composite_w, composite_h);
	GL_Ortho(0, composite_w, composite_h, 0, -1, 1);
	GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
	GL_PostProcess(GLS_DEFAULT, 0, 0, composite_w, composite_h);
	qglBindFramebuffer(GL_FRAMEBUFFER, 0);
	GL_Setup2D();

	(void)composite_x;
	(void)composite_y;

	for (int i = 0; i < 16; ++i)
		glr.motion_history_view_proj[target_index][i] = glr.view_proj_matrix[i];
	glr.motion_history_valid[target_index] = true;
	glr.motion_blur_history_index = (target_index + 1) % R_MOTION_BLUR_HISTORY_FRAMES;
	if (glr.motion_blur_history_count < R_MOTION_BLUR_HISTORY_FRAMES)
		glr.motion_blur_history_count++;
}

static void GL_DrawDepthOfField(pp_flags_t flags)
{
	const bool waterwarp = (flags & PP_WATERWARP) != 0;

	GL_RunDepthOfField();

	GL_Setup2D();

	glStateBits_t bits = GLS_DEFAULT;
	if (waterwarp)
		bits |= GLS_WARP_ENABLE;
	R_HDRUpdateUniforms();
	if (flags & PP_HDR)
		bits |= GLS_TONEMAP_ENABLE;

	int composite_x = 0;
	int composite_y = 0;
	int composite_w = 0;
	int composite_h = 0;
	R_GetFinalCompositeRect(&composite_x, &composite_y, &composite_w, &composite_h);

	bits = R_CRTPrepare(bits, composite_w, composite_h);

	GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_DOF_RESULT);
	if (glr.motion_blur_ready) {
		bits |= GLS_MOTION_BLUR;
		GL_ForceTexture(TMU_GLOWMAP, TEXNUM_PP_DEPTH);
		R_BindMotionHistoryTextures();
	}

	qglBindFramebuffer(GL_FRAMEBUFFER, 0);
	GL_PostProcess(bits, composite_x, composite_y, composite_w, composite_h);
}
static int32_t r_skipUnderWaterFX_modified = 0;
static int32_t r_bloom_modified = 0;
static int32_t r_bloomScale_modified = 0;
static int32_t r_bloomKernel_modified = 0;
static int32_t gl_dof_modified = 0;
static int32_t gl_dof_quality_modified = 0;
static int32_t r_motionBlur_modified = 0;

/*
=============
GL_ClearBloomStateFlags

Resets bloom tracking counters so a future enablement rebuilds required
framebuffer attachments.
=============
*/
static void GL_ClearBloomStateFlags(void)
{
	r_bloom_modified = -1;
	r_bloomScale_modified = -1;
	r_bloomKernel_modified = -1;
}

/*
=============
GL_UpdateBloomEffect

Ensures the bloom effect matches the requested enable state and framebuffer size.
Returns true when bloom remains enabled after the resize attempt.
=============
*/
static bool GL_UpdateBloomEffect(bool bloom_enabled, int target_width, int target_height)
{
	if (bloom_enabled) {
		if (!g_bloom_effect.resize(target_width, target_height)) {
			g_bloom_effect.resize(0, 0);
			return false;
		}
		return true;
	}

	g_bloom_effect.resize(0, 0);
	return false;
}

/*
=============
HDR_DisableFramebufferResources

Disables HDR state and releases reduction resources when the post-process framebuffer is unavailable.
=============
*/
static void HDR_DisableFramebufferResources(void)
{
	gl_static.hdr.active = false;
	g_hdr_luminance.shutdown();
	HDR_ResetState();
	glr.framebuffer_ok = false;
	glr.framebuffer_bound = false;
	glr.framebuffer_width = 0;
	glr.framebuffer_height = 0;
	glr.motion_history_textures_ready = false;
}
/*
=============
GL_BindFramebuffer

Initializes or binds the appropriate framebuffer configuration for the current
frame while updating dependent post-process resources.
=============
*/
static pp_flags_t GL_BindFramebuffer(void)
{
	pp_flags_t flags = PP_NONE;
	bool resized = false;
	const bool world_visible = !(glr.fd.rdflags & RDF_NOWORLDMODEL);
	const bool dof_active = world_visible && gl_dof->integer && glr.fd.depth_of_field;
	const bool post_processing_enabled = r_postProcessing && r_postProcessing->integer;
	const bool fbo_enabled = !r_fbo || r_fbo->integer;
	const bool fbo_disabled = r_fbo && !r_fbo->integer;
	const bool post_processing_requested = gl_static.use_shaders && post_processing_enabled && fbo_enabled;
	const bool post_processing_disabled = !post_processing_requested;
	const bool post_processing_paused = post_processing_requested && !world_visible;
	const bool had_framebuffer = glr.framebuffer_ok;
	const bool had_framebuffer_resources = had_framebuffer || glr.framebuffer_width > 0 || glr.framebuffer_height > 0 || glr.motion_history_textures_ready;
	const GLenum prev_internal_format = gl_static.postprocess_internal_format;
	const GLenum prev_format = gl_static.postprocess_format;
	const GLenum prev_type = gl_static.postprocess_type;

	const int drawable_w = (r_config.width > 0) ? r_config.width : 0;
	const int drawable_h = (r_config.height > 0) ? r_config.height : 0;
	const int viewport_w = (drawable_w > 0 && glr.fd.width > 0) ? (std::min)(glr.fd.width, drawable_w) : 0;
	const int viewport_h = (drawable_h > 0 && glr.fd.height > 0) ? (std::min)(glr.fd.height, drawable_h) : 0;
	const int scene_target_w = viewport_w;
	const int scene_target_h = viewport_h;
	const bool motion_blur_requested = post_processing_requested && r_motionBlur->integer && world_visible &&
		scene_target_w > 0 && scene_target_h > 0;
	const bool motion_blur_enabled = motion_blur_requested;

	if (post_processing_disabled) {
		glr.motion_blur_enabled = false;
		GL_UpdateBloomEffect(false, scene_target_w, scene_target_h);
		HDR_DisableFramebufferResources();
		HDR_UpdatePostprocessFormats();
		GL_ClearBloomStateFlags();
		if (had_framebuffer_resources)
			GL_ReleaseFramebufferResources();
		return PP_NONE;
	}

	HDR_UpdateConfig();

	const bool hdr_prev = gl_static.hdr.active;
	const bool hdr_requested = gl_static.hdr.supported && gl_static.use_shaders && r_hdr->integer;

	if (r_hdr->modified_count != r_hdr_modified) {
		HDR_ResetState();
		r_hdr_modified = r_hdr->modified_count;
	}

	if (r_hdr_mode->modified_count != r_hdr_mode_modified)
		r_hdr_mode_modified = r_hdr_mode->modified_count;

	if (r_exposure_auto->modified_count != r_exposure_auto_modified) {
		HDR_ResetState();
		r_exposure_auto_modified = r_exposure_auto->modified_count;
	}

	gl_static.hdr.active = hdr_requested;

	HDR_UpdatePostprocessFormats();

	const bool postprocess_format_changed = prev_internal_format != gl_static.postprocess_internal_format ||
		prev_format != gl_static.postprocess_format ||
		prev_type != gl_static.postprocess_type;

	if ((glr.fd.rdflags & RDF_UNDERWATER) && !r_skipUnderWaterFX->integer)
		flags |= PP_WATERWARP;

	if (r_bloom->integer && (gl_config.caps & QGL_CAP_DRAW_BUFFERS) && qglDrawBuffers)
		flags |= PP_BLOOM;

	if (dof_active)
		flags |= PP_DEPTH_OF_FIELD;

	if (gl_static.hdr.active)
		flags |= PP_HDR;

	if (R_CRTEnabled())
		flags |= PP_CRT;

	if (motion_blur_enabled)
		flags |= PP_MOTION_BLUR;

	if (postprocess_format_changed) {
		const bool bloom_active = (flags & PP_BLOOM) && scene_target_w > 0 && scene_target_h > 0;
		if (!GL_UpdateBloomEffect(bloom_active, scene_target_w, scene_target_h))
			flags = static_cast<pp_flags_t>(flags & ~PP_BLOOM);
	}

	if (flags)
		resized = scene_target_w != glr.framebuffer_width || scene_target_h != glr.framebuffer_height;

	glr.motion_blur_enabled = motion_blur_enabled;

	if (resized || r_skipUnderWaterFX->modified_count != r_skipUnderWaterFX_modified ||
		r_bloom->modified_count != r_bloom_modified ||
		r_bloomScale->modified_count != r_bloomScale_modified ||
		r_bloomKernel->modified_count != r_bloomKernel_modified ||
		gl_dof->modified_count != gl_dof_modified ||
		gl_dof_quality->modified_count != gl_dof_quality_modified ||
		r_motionBlur->modified_count != r_motionBlur_modified ||
		hdr_prev != gl_static.hdr.active) {
		glr.framebuffer_ok = GL_InitFramebuffers();
		if (glr.framebuffer_ok) {
			glr.framebuffer_width  = scene_target_w;
			glr.framebuffer_height = scene_target_h;
		} else {
			glr.framebuffer_width  = 0;
			glr.framebuffer_height = 0;
		}
		r_skipUnderWaterFX_modified = r_skipUnderWaterFX->modified_count;
		r_bloom_modified = r_bloom->modified_count;
		r_bloomScale_modified = r_bloomScale->modified_count;
		r_bloomKernel_modified = r_bloomKernel->modified_count;
		gl_dof_modified = gl_dof->modified_count;
		gl_dof_quality_modified = gl_dof_quality->modified_count;
		r_motionBlur_modified = r_motionBlur->modified_count;
		if (glr.framebuffer_ok && (flags & PP_BLOOM)) {
			const bool bloom_ready = scene_target_w > 0 && scene_target_h > 0;
			if (!GL_UpdateBloomEffect(bloom_ready, scene_target_w, scene_target_h))
				flags = static_cast<pp_flags_t>(flags & ~PP_BLOOM);
		}
		if (glr.framebuffer_ok) {
			if (flags & PP_BLOOM)
				gl_backend->update_blur();
		} else {
			if (flags & PP_HDR) {
				const bool formats_changed = gl_static.hdr.active;
				gl_static.hdr.active = false;
				if (formats_changed)
					HDR_UpdatePostprocessFormats();
			}

			flags = PP_NONE;
		}
	}

	if (!world_visible) {
		glr.framebuffer_bound = false;
		return PP_NONE;
	}

	if (!flags || !glr.framebuffer_ok) {
		glr.motion_blur_enabled = false;
		if (!post_processing_paused || !glr.framebuffer_ok) {
			HDR_DisableFramebufferResources();
			HDR_UpdatePostprocessFormats();
			GL_UpdateBloomEffect(false, scene_target_w, scene_target_h);
			GL_ClearBloomStateFlags();
			if (had_framebuffer_resources)
				GL_ReleaseFramebufferResources();
		}
		return PP_NONE;
	}

	qglBindFramebuffer(GL_FRAMEBUFFER, FBO_SCENE);
	glr.framebuffer_bound = true;

	static const GLenum scene_draw_buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	const GLsizei scene_draw_buffer_count = (flags & PP_BLOOM) ? 2 : 1;
	GL_SetFramebufferDrawBuffers(scene_draw_buffer_count, scene_draw_buffers);
	if (qglReadBuffer)
		qglReadBuffer(GL_COLOR_ATTACHMENT0);

	if (gl_clear->integer) {
		if (flags & PP_BLOOM) {
			static const GLenum buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
			static const vec4_t black = { 0, 0, 0, 1 };
			GL_SetFramebufferDrawBuffers(2, buffers);
			qglClearBufferfv(GL_COLOR, 0, gl_static.clearcolor);
			qglClearBufferfv(GL_COLOR, 1, black);
			GL_SetFramebufferDrawBuffers(scene_draw_buffer_count, scene_draw_buffers);
		} else {
			qglClear(GL_COLOR_BUFFER_BIT);
		}
	}

	return flags;
}



void R_RenderFrame(const refdef_t *fd)
{
    GL_Flush2D();

    Q_assert(gl_static.world.cache || (fd->rdflags & RDF_NOWORLDMODEL));

    glr.drawframe++;

	glr.fd = *fd;

	if (r_shadows && r_shadows->integer > 0)
		R_ShadowAtlasBeginFrame();
	if (gl_static.use_shaders && r_shadows && r_shadows->integer > 0)
		R_RenderShadowViews();


    const bool viewport_changed = glr.motion_blur_viewport_width != glr.fd.width ||
        glr.motion_blur_viewport_height != glr.fd.height;
    const bool fov_changed = std::fabs(glr.motion_blur_fov_x - glr.fd.fov_x) > MOTION_BLUR_FOV_EPSILON ||
        std::fabs(glr.motion_blur_fov_y - glr.fd.fov_y) > MOTION_BLUR_FOV_EPSILON;

    if (viewport_changed || fov_changed) {
        glr.prev_view_proj_valid = false;
        R_ClearMotionBlurHistory();
    }

    glr.motion_blur_viewport_width = glr.fd.width;
    glr.motion_blur_viewport_height = glr.fd.height;
    glr.motion_blur_fov_x = glr.fd.fov_x;
    glr.motion_blur_fov_y = glr.fd.fov_y;
    glr.ppl_bits  = 0;

	const bool fbo_active = !r_fbo || r_fbo->integer;
	const bool post_processing_enabled = r_postProcessing && r_postProcessing->integer && fbo_active;

	glr.motion_blur_enabled = gl_static.use_shaders && post_processing_enabled && r_motionBlur->integer &&
        !(glr.fd.rdflags & RDF_NOWORLDMODEL) && glr.fd.width > 0 && glr.fd.height > 0;

    float motion_blur_scale = 0.0f;
    if (glr.motion_blur_enabled) {
        const float shutter_speed = (std::max)(r_motionBlurShutterSpeed->value, 0.0001f);
        const float frame_time = (std::max)(glr.fd.frametime, 1.0e-6f);
        if (frame_time <= MOTION_BLUR_MAX_FRAME_TIME) {
            const float exposure = 1.0f / shutter_speed;
            motion_blur_scale = Q_bound(0.0f, exposure / frame_time, 1.0f);
        } else {
            glr.prev_view_proj_valid = false;
            R_ClearMotionBlurHistory();
        }
    } else {
        glr.prev_view_proj_valid = false;
        R_ClearMotionBlurHistory();
    }

    glr.motion_blur_min_velocity = (std::max)(r_motionBlurMinVelocity->value, 0.0f);
    glr.motion_blur_min_velocity_pixels = (std::max)(r_motionBlurMinVelocityPixels->value, 0.0f);
    glr.motion_blur_scale = motion_blur_scale;
    glr.motion_blur_ready = false;
    glr.view_proj_valid = false;

    if (gl_dynamic->integer != 1 || gl_vertexlight->integer)
        glr.fd.num_dlights = 0;

    glr.fog_bits = glr.fog_bits_sky = 0;

    if (gl_static.use_shaders) {
        if (r_enablefog->integer > 0) {
            if (glr.fd.fog.density > 0)
                glr.fog_bits |= GLS_FOG_GLOBAL;
            if (glr.fd.heightfog.density > 0 && glr.fd.heightfog.falloff > 0)
                glr.fog_bits |= GLS_FOG_HEIGHT;
            if (glr.fd.fog.sky_factor > 0)
                glr.fog_bits_sky |= GLS_FOG_SKY;
        }

        if (gl_per_pixel_lighting->integer > 0)
            glr.ppl_bits |= GLS_DYNAMIC_LIGHTS;
    }

    if (lm.dirty) {
        GL_RebuildLighting();
        lm.dirty = false;
    }

    pp_flags_t pp_flags = GL_BindFramebuffer();

    GL_Setup3D();

    GL_SetupFrustum();

    if (!(glr.fd.rdflags & RDF_NOWORLDMODEL) && gl_drawworld->integer)
        GL_DrawWorld();

    GL_ClassifyEntities();

    GL_DrawEntities(glr.ents.bmodels);

    GL_DrawEntities(glr.ents.opaque);

    GL_DrawEntities(glr.ents.alpha_back);

    GL_DrawAlphaFaces();

    GL_DrawBeams();

    GL_DrawParticles();

    GL_OccludeFlares();

    GL_DrawFlares();

    GL_DrawEntities(glr.ents.alpha_front);

    GL_DrawDebugObjects();

    if (glr.framebuffer_bound) {
        qglBindFramebuffer(GL_FRAMEBUFFER, 0);
        glr.framebuffer_bound = false;
    }

	HDR_UpdateExposure(glr.framebuffer_width, glr.framebuffer_height);

    tess.dlight_bits = 0;

    // go back into 2D mode
    GL_Setup2D();

    if (pp_flags & PP_DEPTH_OF_FIELD) {
        Vector4Set(gls.u_block.dof_params, glr.fd.dof_blur_range, glr.fd.dof_focus_distance, glr.fd.dof_focus_range, glr.fd.dof_luma_strength);
        Vector4Set(gls.u_block.dof_depth, glr.view_znear, glr.view_zfar, 0.0f, 0.0f);
        gls.u_block_dirty = true;
    }

	int composite_x = 0;
	int composite_y = 0;
	int composite_w = 0;
	int composite_h = 0;
	R_GetFinalCompositeRect(&composite_x, &composite_y, &composite_w, &composite_h);

	const bool world_visible = !(glr.fd.rdflags & RDF_NOWORLDMODEL);

	if (world_visible && (pp_flags & PP_BLOOM)) {
		GL_DrawBloom(pp_flags);
	} else if (world_visible && (pp_flags & PP_DEPTH_OF_FIELD)) {
		GL_DrawDepthOfField(pp_flags);
	} else if (world_visible && (pp_flags & (PP_WATERWARP | PP_HDR | PP_MOTION_BLUR))) {
		glStateBits_t bits = GLS_DEFAULT;
		if (pp_flags & PP_WATERWARP)
			bits |= GLS_WARP_ENABLE;
		if (pp_flags & PP_HDR) {
			R_HDRUpdateUniforms();
			bits |= GLS_TONEMAP_ENABLE;
		}

		if (pp_flags & PP_CRT)
			bits = R_CRTPrepare(bits, composite_w, composite_h);

		GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
		if (glr.motion_blur_ready) {
			bits |= GLS_MOTION_BLUR;
			GL_ForceTexture(TMU_GLOWMAP, TEXNUM_PP_DEPTH);
			R_BindMotionHistoryTextures();
		}

		qglBindFramebuffer(GL_FRAMEBUFFER, 0);
		GL_PostProcess(bits, composite_x, composite_y, composite_w, composite_h);
	} else if (world_visible && (pp_flags & PP_HDR)) {
		glStateBits_t bits = GLS_TONEMAP_ENABLE;
		GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
		R_HDRUpdateUniforms();
		if (pp_flags & PP_CRT)
			bits = R_CRTPrepare(bits, composite_w, composite_h);
		GL_PostProcess(bits, composite_x, composite_y, composite_w, composite_h);
	} else if (world_visible && (pp_flags & PP_CRT)) {
		glStateBits_t bits = GLS_DEFAULT;
		GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
		bits = R_CRTPrepare(bits, composite_w, composite_h);
		GL_PostProcess(bits, composite_x, composite_y, composite_w, composite_h);
	}


    if (glr.motion_blur_enabled)
        R_StoreMotionBlurHistory();

    if (glr.motion_blur_enabled && glr.view_proj_valid) {
        for (int i = 0; i < 16; ++i)
            glr.prev_view_proj_matrix[i] = glr.view_proj_matrix[i];
        glr.prev_view_proj_valid = true;
    } else if (!glr.motion_blur_enabled || !glr.view_proj_valid) {
        glr.prev_view_proj_valid = false;
        R_ClearMotionBlurHistory();
    }

    if (gl_polyblend->integer)
        GL_Blend();

#if USE_DEBUG
    if (gl_lightmap->integer > 1)
        Draw_Lightmaps();
#endif

    if (gl_showerrors->integer > 1)
        GL_ShowErrors(__func__);
}

bool R_SupportsPerPixelLighting(void)
{
    return gl_backend->use_per_pixel_lighting();
}

void R_BeginFrame(void)
{
    memset(&c, 0, sizeof(c));

    if (gl_finish->integer)
        qglFinish();

    GL_Setup2D();

    if (gl_clear->integer)
        qglClear(GL_COLOR_BUFFER_BIT);

    if (gl_showerrors->integer > 1)
        GL_ShowErrors(__func__);
}

void R_EndFrame(void)
{
    extern cvar_t *cl_async;

    if (SCR_StatActive()) {
        GL_Flush2D();
        SCR_DrawStats();
    }

#if USE_DEBUG
    if (gl_showscrap->integer)
        Draw_Scrap();
#endif
    GL_Flush2D();

    if (gl_showtearing->integer)
        GL_DrawTearing();

    if (gl_showerrors->integer > 1)
        GL_ShowErrors(__func__);

    vid->swap_buffers();

    if (qglFenceSync && cl_async->integer > 1 && !gl_static.sync)
        gl_static.sync = qglFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

bool R_VideoSync(void)
{
    if (!gl_static.sync)
        return true;

    if (qglClientWaitSync(gl_static.sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0) == GL_TIMEOUT_EXPIRED)
        return false;

    qglDeleteSync(gl_static.sync);
    gl_static.sync = 0;

    return true;
}

// ==============================================================================

static void GL_Strings_f(void)
{
    GLint integer = 0;

    Com_Printf("GL_VENDOR: %s\n", qglGetString(GL_VENDOR));
    Com_Printf("GL_RENDERER: %s\n", qglGetString(GL_RENDERER));
    Com_Printf("GL_VERSION: %s\n", qglGetString(GL_VERSION));

    if (gl_config.ver_sl) {
        Com_Printf("GL_SHADING_LANGUAGE_VERSION: %s\n", qglGetString(GL_SHADING_LANGUAGE_VERSION));
    }

    if (Cmd_Argc() > 1) {
        Com_Printf("GL_EXTENSIONS: ");
        if (qglGetStringi) {
            qglGetIntegerv(GL_NUM_EXTENSIONS, &integer);
            for (int i = 0; i < integer; i++)
                Com_Printf("%s ", qglGetStringi(GL_EXTENSIONS, i));
        } else {
            const char *s = (const char *)qglGetString(GL_EXTENSIONS);
            if (s) {
                while (*s) {
                    Com_Printf("%s", s);
                    s += min(strlen(s), MAXPRINTMSG - 1);
                }
            }
        }
        Com_Printf("\n");
    }

    qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &integer);
    Com_Printf("GL_MAX_TEXTURE_SIZE: %d\n", integer);

    if (qglClientActiveTexture) {
        qglGetIntegerv(GL_MAX_TEXTURE_UNITS, &integer);
        Com_Printf("GL_MAX_TEXTURE_UNITS: %d\n", integer);
    }

    if (gl_config.caps & QGL_CAP_TEXTURE_ANISOTROPY) {
        Com_Printf("GL_MAX_TEXTURE_MAX_ANISOTROPY: %.f\n", gl_config.max_anisotropy);
    }

    Com_Printf("GL_PFD: color(%d-bit) Z(%d-bit) stencil(%d-bit)\n",
               gl_config.colorbits, gl_config.depthbits, gl_config.stencilbits);
}

#if USE_DEBUG

static size_t GL_ViewCluster_m(char *buffer, size_t size)
{
    return Q_snprintf(buffer, size, "%d", glr.viewcluster1);
}

static size_t GL_ViewLeaf_m(char *buffer, size_t size)
{
    const bsp_t *bsp = gl_static.world.cache;

    if (bsp) {
        const mleaf_t *leaf = BSP_PointLeaf(bsp->nodes, glr.fd.vieworg);
        return Q_snprintf(buffer, size, "%td %d %d %d %#x", leaf - bsp->leafs,
                          leaf->cluster, leaf->numleafbrushes, leaf->numleaffaces,
                          leaf->contents[0]);
    }

    return Q_strlcpy(buffer, "", size);
}

#endif

static void gl_lightmap_changed(cvar_t *self)
{
    lm.scale = Cvar_ClampValue(gl_coloredlightmaps, 0, 1);
    lm.comp = !(gl_config.caps & QGL_CAP_TEXTURE_BITS) ? GL_RGBA : lm.scale ? GL_RGB : GL_LUMINANCE;
    lm.add = 255 * Cvar_ClampValue(gl_brightness, -1, 1);
    lm.modulate = Cvar_ClampValue(gl_modulate, 0, 1e6f);
    lm.modulate *= Cvar_ClampValue(gl_modulate_world, 0, 1e6f);
    if (gl_static.use_shaders && (self == gl_brightness || self == gl_modulate || self == gl_modulate_world) && !gl_vertexlight->integer)
        return;
    lm.dirty = true; // rebuild all lightmaps next frame
}

static void gl_modulate_entities_changed(cvar_t *self)
{
    gl_static.entity_modulate = Cvar_ClampValue(gl_modulate, 0, 1e6f);
    gl_static.entity_modulate *= Cvar_ClampValue(gl_modulate_entities, 0, 1e6f);
}

static void gl_modulate_changed(cvar_t *self)
{
    gl_lightmap_changed(self);
    gl_modulate_entities_changed(self);
}

static void r_bloom_threshold_legacy_changed(cvar_t *self)
{
    (void)self;

    if (!r_bloomBrightThreshold || !r_bloomThresholdLegacy)
        return;

    Cvar_SetValue(r_bloomBrightThreshold, r_bloomThresholdLegacy->value, FROM_CODE);
}

// ugly hack to reset sky
static void gl_drawsky_changed(cvar_t *self)
{
    if (gl_static.world.cache)
        CL_SetSky();
}

/*
=============
gl_znear_changed

Clamps the near clip plane cvar to a supported minimum.
=============
*/
static void gl_znear_changed(cvar_t *self)
{
	const float clamped = (std::max)(self->value, GL_MINIMUM_ZNEAR);
	if (clamped != self->value)
		Cvar_SetValue(self, clamped, FROM_CODE);
}


static void gl_novis_changed(cvar_t *self)
{
    glr.viewcluster1 = glr.viewcluster2 = -2;
}

static void gl_swapinterval_changed(cvar_t *self)
{
    if (vid && vid->swap_interval)
        vid->swap_interval(self->integer);
}

static void gl_clearcolor_changed(cvar_t *self)
{
    color_t color;

    if (!SCR_ParseColor(self->string, &color)) {
        Com_WPrintf("Invalid value '%s' for '%s'\n", self->string, self->name);
        Cvar_Reset(self);
        color.u32 = COLOR_U32_BLACK;
    }

    gl_static.clearcolor[0] = color.u8[0] / 255.0f;
    gl_static.clearcolor[1] = color.u8[1] / 255.0f;
    gl_static.clearcolor[2] = color.u8[2] / 255.0f;
    gl_static.clearcolor[3] = color.u8[3] / 255.0f;

    if (qglClearColor)
        qglClearColor(Vector4Unpack(gl_static.clearcolor));
}

static void GL_Register(void)
{
    // regular variables
    gl_partscale = Cvar_Get("gl_partscale", "2", 0);
    gl_partstyle = Cvar_Get("gl_partstyle", "0", 0);
    gl_beamstyle = Cvar_Get("gl_beamstyle", "0", 0);
    gl_celshading = Cvar_Get("gl_celshading", "0", 0);
    gl_dotshading = Cvar_Get("gl_dotshading", "0", 0);
    gl_shadows = Cvar_Get("gl_shadows", "2", CVAR_ARCHIVE);
    gl_modulate = Cvar_Get("gl_modulate", "2", CVAR_ARCHIVE);
    gl_modulate->changed = gl_modulate_changed;
    gl_modulate_world = Cvar_Get("gl_modulate_world", "1", 0);
    gl_modulate_world->changed = gl_lightmap_changed;
    gl_coloredlightmaps = Cvar_Get("gl_coloredlightmaps", "1", 0);
    gl_coloredlightmaps->changed = gl_lightmap_changed;
    gl_lightmap_bits = Cvar_Get("gl_lightmap_bits", "0", 0);
    gl_lightmap_bits->changed = gl_lightmap_changed;
    gl_brightness = Cvar_Get("gl_brightness", "0", 0);
    gl_brightness->changed = gl_lightmap_changed;
    gl_dynamic = Cvar_Get("gl_dynamic", "1", 0);
    gl_dynamic->changed = gl_lightmap_changed;
    gl_dlight_falloff = Cvar_Get("gl_dlight_falloff", "1", 0);
    gl_modulate_entities = Cvar_Get("gl_modulate_entities", "1", 0);
    gl_modulate_entities->changed = gl_modulate_entities_changed;
	gl_glowmap_intensity = Cvar_Get("gl_glowmap_intensity", "1", 0);
	gl_flarespeed = Cvar_Get("gl_flarespeed", "8", 0);
	gl_fontshadow = Cvar_Get("gl_fontshadow", "0", 0);
	gl_shadow_filter = Cvar_Get("gl_shadow_filter", "1", CVAR_ARCHIVE);
	gl_shadow_filter_radius = Cvar_Get("gl_shadow_filter_radius", "1.5", CVAR_ARCHIVE);
	gl_shaders = Cvar_Get("gl_shaders", "1", CVAR_FILES);
#if USE_MD5
    gl_md5_load = Cvar_Get("gl_md5_load", "1", CVAR_FILES);
    gl_md5_use = Cvar_Get("gl_md5_use", "1", 0);
    gl_md5_distance = Cvar_Get("gl_md5_distance", "2048", 0);
#endif
    gl_damageblend_frac = Cvar_Get("gl_damageblend_frac", "0.2", 0);
	r_skipUnderWaterFX = Cvar_Get("r_skipUnderWaterFX", "0", CVAR_ARCHIVE);
	r_enablefog = Cvar_Get("r_enablefog", "1", 0);
	r_shadows = Cvar_Get("r_shadows", "1", CVAR_ARCHIVE);
	r_staticshadows = Cvar_Get("r_staticshadows", "1", CVAR_ARCHIVE);
	r_postProcessing = Cvar_Get("r_postProcessing", "1", CVAR_ARCHIVE);
	r_fbo = Cvar_Get("r_fbo", "1", CVAR_ARCHIVE);
	r_bloom = Cvar_Get("r_bloom", "1", CVAR_ARCHIVE);
    r_bloomBlurRadius = Cvar_Get("r_bloomBlurRadius", "12", CVAR_ARCHIVE);
    r_bloomBlurFalloff = Cvar_Get("r_bloomBlurFalloff", "0.75", CVAR_ARCHIVE);
	r_bloomBrightThreshold = Cvar_Get("r_bloomBrightThreshold", "0.75", CVAR_ARCHIVE);
	r_bloomKnee = Cvar_Get("r_bloomKnee", "0.25", CVAR_ARCHIVE);
    r_bloomIntensity = Cvar_Get("r_bloomIntensity", "0.05", CVAR_ARCHIVE);
    r_bloomScale = Cvar_Get("r_bloomScale", "4.0", CVAR_ARCHIVE);
    r_bloomKernel = Cvar_Get("r_bloomKernel", "0", CVAR_ARCHIVE);
    r_bloomBlurScale = Cvar_Get("r_bloomBlurScale", "1.0", CVAR_ARCHIVE);
    r_bloomPasses = Cvar_Get("r_bloomPasses", "1", CVAR_ARCHIVE);
    r_bloomSaturation = Cvar_Get("r_bloomSaturation", "1.0", CVAR_ARCHIVE);
    r_bloomSceneIntensity = Cvar_Get("r_bloomSceneIntensity", "1.0", CVAR_ARCHIVE);
    r_bloomSceneSaturation = Cvar_Get("r_bloomSceneSaturation", "1.0", CVAR_ARCHIVE);
    r_colorCorrect = Cvar_Get("r_colorCorrect", "1.0", CVAR_ARCHIVE);
    r_bloomThresholdLegacy = Cvar_Get("r_bloomThreshold", r_bloomBrightThreshold->string, CVAR_ARCHIVE);
    if (r_bloomThresholdLegacy && r_bloomBrightThreshold &&
        r_bloomThresholdLegacy->value != r_bloomBrightThreshold->value)
        Cvar_SetValue(r_bloomBrightThreshold, r_bloomThresholdLegacy->value, FROM_CODE);
    if (r_bloomThresholdLegacy)
        r_bloomThresholdLegacy->changed = r_bloom_threshold_legacy_changed;
    r_hdr = Cvar_Get("r_hdr", "0", CVAR_ARCHIVE);
    r_hdr_mode = Cvar_Get("r_hdr_mode", "3", CVAR_ARCHIVE);
    r_tonemap = Cvar_Get("r_tonemap", "0", CVAR_ARCHIVE);
    r_exposure_auto = Cvar_Get("r_exposure_auto", "1", CVAR_ARCHIVE);
    r_exposure_key = Cvar_Get("r_exposure_key", "0.18", CVAR_ARCHIVE);
    r_exposure_speed_up = Cvar_Get("r_exposure_speed_up", "2.5", CVAR_ARCHIVE);
    r_exposure_speed_down = Cvar_Get("r_exposure_speed_down", "1.2", CVAR_ARCHIVE);
    r_exposure_ev_min = Cvar_Get("r_exposure_ev_min", "-6", CVAR_ARCHIVE);
    r_exposure_ev_max = Cvar_Get("r_exposure_ev_max", "6", CVAR_ARCHIVE);
    r_output_paper_white = Cvar_Get("r_output_paper_white", "200", CVAR_ARCHIVE);
    r_output_peak_white = Cvar_Get("r_output_peak_white", "1000", CVAR_ARCHIVE);
    r_dither = Cvar_Get("r_dither", "1", CVAR_ARCHIVE);
    r_motionBlur = Cvar_Get("r_motionBlur", "0", CVAR_ARCHIVE);
    r_motionBlurShutterSpeed = Cvar_Get("r_motionBlurShutterSpeed", "250.0", CVAR_ARCHIVE);
    r_motionBlurMinVelocity = Cvar_Get("r_motionBlurMinVelocity", "0.0005", CVAR_ARCHIVE);
    r_motionBlurMinVelocityPixels = Cvar_Get("r_motionBlurMinVelocityPixels", "0.5", CVAR_ARCHIVE);
    r_ui_sdr_style = Cvar_Get("r_ui_sdr_style", "1", CVAR_ARCHIVE);
    r_debug_histogram = Cvar_Get("r_debug_histogram", "0", CVAR_CHEAT);
    r_debug_tonemap = Cvar_Get("r_debug_tonemap", "0", CVAR_CHEAT);
    r_crtmode = Cvar_Get("r_crtmode", "0", CVAR_ARCHIVE);
    r_crt_hardScan = Cvar_Get("r_crt_hardScan", "-8.0", CVAR_ARCHIVE);
    r_crt_hardPix = Cvar_Get("r_crt_hardPix", "-3.0", CVAR_ARCHIVE);
    r_crt_maskDark = Cvar_Get("r_crt_maskDark", "0.5", CVAR_ARCHIVE);
    r_crt_maskLight = Cvar_Get("r_crt_maskLight", "1.5", CVAR_ARCHIVE);
    r_crt_scaleInLinearGamma = Cvar_Get("r_crt_scaleInLinearGamma", "1", CVAR_ARCHIVE);
    r_crt_shadowMask = Cvar_Get("r_crt_shadowMask", "3", CVAR_ARCHIVE);
    r_crt_brightBoost = Cvar_Get("r_crt_brightBoost", "1.0", CVAR_ARCHIVE);
    r_crt_warpX = Cvar_Get("r_crt_warpX", "0.031", CVAR_ARCHIVE);
    r_crt_warpY = Cvar_Get("r_crt_warpY", "0.041", CVAR_ARCHIVE);
    gl_dof = Cvar_Get("gl_dof", "1", CVAR_ARCHIVE);
    gl_dof_quality = Cvar_Get("gl_dof_quality", "1", CVAR_ARCHIVE);
    gl_swapinterval = Cvar_Get("gl_swapinterval", "1", CVAR_ARCHIVE);
    gl_swapinterval->changed = gl_swapinterval_changed;

    // development variables
    gl_znear = Cvar_Get("gl_znear", "2", CVAR_CHEAT);
    gl_znear->changed = gl_znear_changed;
    gl_drawworld = Cvar_Get("gl_drawworld", "1", CVAR_CHEAT);
    gl_drawentities = Cvar_Get("gl_drawentities", "1", CVAR_CHEAT);
    gl_drawsky = Cvar_Get("gl_drawsky", "1", 0);
    gl_drawsky->changed = gl_drawsky_changed;
    gl_draworder = Cvar_Get("gl_draworder", "1", 0);
    gl_showtris = Cvar_Get("gl_showtris", "0", CVAR_CHEAT);
    gl_showorigins = Cvar_Get("gl_showorigins", "0", CVAR_CHEAT);
    gl_showtearing = Cvar_Get("gl_showtearing", "0", CVAR_CHEAT);
    gl_showbloom = Cvar_Get("gl_showbloom", "0", CVAR_CHEAT);
#if USE_DEBUG
    gl_showscrap = Cvar_Get("gl_showscrap", "0", 0);
    gl_nobind = Cvar_Get("gl_nobind", "0", CVAR_CHEAT);
    gl_novbo = Cvar_Get("gl_novbo", "0", CVAR_FILES);
    gl_test = Cvar_Get("gl_test", "0", 0);
#endif
    gl_cull_nodes = Cvar_Get("gl_cull_nodes", "1", 0);
    gl_cull_models = Cvar_Get("gl_cull_models", "1", 0);
    gl_showcull = Cvar_Get("gl_showcull", "0", CVAR_CHEAT);
    gl_clear = Cvar_Get("gl_clear", "0", 0);
    gl_clearcolor = Cvar_Get("gl_clearcolor", "black", 0);
    gl_clearcolor->changed = gl_clearcolor_changed;
    gl_clearcolor->generator = Com_Color_g;
    gl_finish = Cvar_Get("gl_finish", "0", 0);
    gl_novis = Cvar_Get("gl_novis", "0", 0);
    gl_novis->changed = gl_novis_changed;
    gl_lockpvs = Cvar_Get("gl_lockpvs", "0", CVAR_CHEAT);
    gl_lightmap = Cvar_Get("gl_lightmap", "0", CVAR_CHEAT);
    gl_fullbright = Cvar_Get("r_fullbright", "0", CVAR_CHEAT);
    gl_fullbright->changed = gl_lightmap_changed;
    gl_vertexlight = Cvar_Get("gl_vertexlight", "0", 0);
    gl_vertexlight->changed = gl_lightmap_changed;
    gl_lightgrid = Cvar_Get("gl_lightgrid", "1", 0);
    gl_polyblend = Cvar_Get("gl_polyblend", "1", 0);
    gl_showerrors = Cvar_Get("gl_showerrors", "1", 0);
    gl_damageblend_frac = Cvar_Get("gl_damageblend_frac", "0.2", 0);

    gl_lightmap_changed(NULL);
    gl_modulate_entities_changed(NULL);
    gl_swapinterval_changed(gl_swapinterval);
    gl_clearcolor_changed(gl_clearcolor);

    r_hdr_modified = r_hdr->modified_count;
    r_hdr_mode_modified = r_hdr_mode->modified_count;
    r_exposure_auto_modified = r_exposure_auto->modified_count;
    r_motionBlur_modified = r_motionBlur->modified_count;

    Cmd_AddCommand("strings", GL_Strings_f);

#if USE_DEBUG
    Cmd_AddMacro("gl_viewcluster", GL_ViewCluster_m);
    Cmd_AddMacro("gl_viewleaf", GL_ViewLeaf_m);
#endif
}

static void GL_Unregister(void)
{
    Cmd_RemoveCommand("strings");
}

static void APIENTRY myDebugProc(GLenum source, GLenum type, GLuint id, GLenum severity,
                                 GLsizei length, const GLchar *message, const void *userParam)
{
    print_type_t level = PRINT_DEVELOPER;

    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:   level = PRINT_ERROR;   break;
        case GL_DEBUG_SEVERITY_MEDIUM: level = PRINT_WARNING; break;
        case GL_DEBUG_SEVERITY_LOW:    level = PRINT_ALL;     break;
    }

    Com_LPrintf(level, "%s\n", message);
}

static void GL_SetupConfig(void)
{
    GLint integer = 0;

    qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &integer);
    gl_config.max_texture_size_log2 = Q_log2(min(integer, MAX_TEXTURE_SIZE));
    gl_config.max_texture_size = 1U << gl_config.max_texture_size_log2;

    if (gl_config.caps & QGL_CAP_CLIENT_VA) {
        qglGetIntegerv(GL_RED_BITS, &integer);
        gl_config.colorbits = integer;
        qglGetIntegerv(GL_GREEN_BITS, &integer);
        gl_config.colorbits += integer;
        qglGetIntegerv(GL_BLUE_BITS, &integer);
        gl_config.colorbits += integer;

        qglGetIntegerv(GL_DEPTH_BITS, &integer);
        gl_config.depthbits = integer;

        qglGetIntegerv(GL_STENCIL_BITS, &integer);
        gl_config.stencilbits = integer;
    } else if (qglGetFramebufferAttachmentParameteriv) {
        GLenum backbuf = gl_config.ver_es ? GL_BACK : GL_BACK_LEFT;

        qglGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, backbuf, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &integer);
        gl_config.colorbits = integer;
        qglGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, backbuf, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &integer);
        gl_config.colorbits += integer;
        qglGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, backbuf, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, &integer);
        gl_config.colorbits += integer;

        qglGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &integer);
        gl_config.depthbits = integer;

        qglGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &integer);
        gl_config.stencilbits = integer;
    }

    if (qglDebugMessageCallback && qglIsEnabled(GL_DEBUG_OUTPUT)) {
        Com_Printf("Enabling GL debug output.\n");
        qglEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        if (Cvar_VariableInteger("gl_debug") < 2)
            qglDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
        qglDebugMessageCallback(myDebugProc, NULL);
    }

    if (gl_config.caps & QGL_CAP_SHADER_STORAGE) {
        integer = 0;
        qglGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &integer);
        if (integer < 2) {
            Com_DPrintf("Not enough shader storage blocks available\n");
            gl_config.caps &= ~QGL_CAP_SHADER_STORAGE;
        } else {
            integer = 1;
            qglGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &integer);
            if (integer & (integer - 1))
                integer = Q_npot32(integer);
            Com_DPrintf("SSBO alignment: %d\n", integer);
            gl_config.ssbo_align = integer;
        }
    }

    if (gl_config.caps & QGL_CAP_BUFFER_TEXTURE) {
        integer = 0;
        qglGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &integer);
        if (integer < MOD_MAXSIZE_GPU) {
            Com_DPrintf("Not enough buffer texture size available\n");
            gl_config.caps &= ~QGL_CAP_BUFFER_TEXTURE;
        }
    }

    if (gl_config.caps & QGL_CAP_TEXTURE_ANISOTROPY) {
        qglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &gl_config.max_anisotropy);
    }

    GL_ShowErrors(__func__);
}

static void GL_InitTables(void)
{
    for (int i = 0; i < NUMVERTEXNORMALS; i++) {
        const vec_t *v = bytedirs[i];
        float lat = acosf(v[2]);
        float lng = atan2f(v[1], v[0]);

        gl_static.latlngtab[i][0] = (int)(lat * (255 / (2 * M_PIf))) & 255;
        gl_static.latlngtab[i][1] = (int)(lng * (255 / (2 * M_PIf))) & 255;
    }

    for (int i = 0; i < 256; i++)
        gl_static.sintab[i] = sinf(i * (2 * M_PIf / 255));
}

static void GL_PostInit(void)
{
    r_registration_sequence = 1;

    if (gl_shaders->modified_count != gl_shaders_modified) {
        GL_ShutdownState();
        GL_InitState();
        gl_shaders_modified = gl_shaders->modified_count;
    }
    GL_ClearState();
    R_ClearMotionBlurHistory();
    GL_InitImages();
    GL_InitQueries();
    MOD_Init();
}

void GL_InitQueries(void)
{
    if (!qglBeginQuery)
        return;

    gl_static.samples_passed = GL_SAMPLES_PASSED;
    if (gl_config.ver_gl >= QGL_VER(3, 3) || gl_config.ver_es >= QGL_VER(3, 0))
        gl_static.samples_passed = GL_ANY_SAMPLES_PASSED;

    Q_assert(!gl_static.queries);
    gl_static.queries = HashMap_TagCreate(int, glquery_t, HashInt32, NULL, TAG_RENDERER);
	gl_flare_occlusion_disabled = false;
}

void GL_DeleteQueries(void)
{
    if (!gl_static.queries)
        return;

    uint32_t map_size = HashMap_Size(gl_static.queries);
    for (int i = 0; i < map_size; i++) {
        glquery_t *q = HashMap_GetValue(glquery_t, gl_static.queries, i);
        qglDeleteQueries(1, &q->query);
    }

    if (map_size)
        Com_DPrintf("%s: %u queries deleted\n", __func__, map_size);

    HashMap_Destroy(gl_static.queries);
    gl_static.queries = NULL;
}

// ==============================================================================

static void Draw_Stats_s(void)
{
    SCR_StatKeyValuei("Nodes visible", glr.nodes_visible);
    SCR_StatKeyValuei("Nodes culled", c.nodesCulled);
    SCR_StatKeyValuei("Nodes drawn", c.nodesDrawn);
    SCR_StatKeyValuei("Leaves drawn", c.leavesDrawn);
    SCR_StatKeyValuei("Faces drawn", c.facesDrawn);
    SCR_StatKeyValuei("Faces culled", c.facesCulled);
    SCR_StatKeyValuei("Boxes culled", c.boxesCulled);
    SCR_StatKeyValuei("Spheres culled", c.spheresCulled);
    SCR_StatKeyValuei("RtBoxes culled", c.rotatedBoxesCulled);
    SCR_StatKeyValuei("Shadows culled", c.shadowsCulled);
    SCR_StatKeyValuei("Tris drawn", c.trisDrawn);
    SCR_StatKeyValuei("Tex switches", c.texSwitches);
    SCR_StatKeyValuei("Tex uploads", c.texUploads);
    SCR_StatKeyValuei("LM texels", c.lightTexels);
    SCR_StatKeyValuei("Batches drawn", c.batchesDrawn);
    SCR_StatKeyValuef("Faces / batch", c.batchesDrawn ? (float)c.facesDrawn / c.batchesDrawn : 0.0f);
    SCR_StatKeyValuef("Tris / batch", c.batchesDrawn ? (float)c.facesTris / c.batchesDrawn : 0.0f);
    SCR_StatKeyValuei("2D batches", c.batchesDrawn2D);
    SCR_StatKeyValuei("Total entities", glr.fd.num_entities);
    SCR_StatKeyValuei("Total dlights", glr.fd.num_dlights);
    SCR_StatKeyValuei("Total particles", glr.fd.num_particles);
    SCR_StatKeyValuei("Uniform uploads", c.uniformUploads);
    SCR_StatKeyValuei("Array binds", c.vertexArrayBinds);
    SCR_StatKeyValuei("Occl. queries", c.occlusionQueries);
    SCR_StatKeyValuei("Total dlights", c.dlightsTotal);
    SCR_StatKeyValuei("Used dlights", c.dlightsUsed);
    SCR_StatKeyValuei("Dlights entculled", c.dlightsNotUsed);
    SCR_StatKeyValuei("Dlight uploads", c.dlightUploads);
    SCR_StatKeyValuei("Dlight frustum culled", c.dlightsCulled);
    SCR_StatKeyValuei("Dlight pre-culled", c.dlightsEntCulled);
}

/*
===============
R_Init
===============
*/
bool R_Init(bool total)
{
    Com_DPrintf("GL_Init( %i )\n", total);

    if (!total) {
        GL_PostInit();
        return true;
    }

    Com_Printf("------- R_Init -------\n");
    Com_Printf("Using video driver: %s\n", vid->name);

    // initialize OS-specific parts of OpenGL
    // create the window and set up the context
    if (!vid->init())
        return false;

    // initialize our QGL dynamic bindings
    if (!QGL_Init())
        goto fail;

    // get various limits from OpenGL
    GL_SetupConfig();

    // register our variables
    GL_Register();

    HDR_InitializeCapabilities();
    HDR_ResetState();
    HDR_UpdateConfig();

    GL_InitArrays();

    GL_InitState();

    GL_InitTables();

    GL_InitDebugDraw();

    GL_PostInit();

    Draw_InitFreeTypeFonts();

    GL_ShowErrors(__func__);

    SCR_RegisterStat("refresh", Draw_Stats_s);

    Com_Printf("----------------------\n");

    return true;

fail:
    memset(&gl_static, 0, sizeof(gl_static));
    memset(&gl_config, 0, sizeof(gl_config));
    QGL_Shutdown();
    vid->shutdown();
    return false;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown(bool total)
{
    Com_DPrintf("GL_Shutdown( %i )\n", total);

    GL_FreeWorld();
    GL_DeleteQueries();
    GL_ShutdownImages();

    MOD_Shutdown();

    if (!total)
        return;

    Draw_ShutdownFreeTypeFonts();

    if (gl_static.sync) {
        qglDeleteSync(gl_static.sync);
        gl_static.sync = 0;
    }

    GL_ShutdownDebugDraw();

    GL_ShutdownState();

    GL_ShutdownArrays();

    // shutdown our QGL subsystem
    QGL_Shutdown();

    // shut down OS specific OpenGL stuff like contexts, etc.
    vid->shutdown();

    GL_Unregister();

    GL_ShutdownDebugDraw();

    SCR_UnregisterStat("refresh");

    memset(&gl_static, 0, sizeof(gl_static));
    memset(&gl_config, 0, sizeof(gl_config));
}

/*
===============
R_GetGLConfig
===============
*/
r_opengl_config_t R_GetGLConfig(void)
{
#define GET_CVAR(name, def, min, max) \
    Cvar_ClampInteger(Cvar_Get(name, def, CVAR_REFRESH), min, max)

    r_opengl_config_t cfg{};
    cfg.colorbits = GET_CVAR("gl_colorbits", "0", 0, 32);
    cfg.depthbits = GET_CVAR("gl_depthbits", "0", 0, 32);
    cfg.stencilbits = GET_CVAR("gl_stencilbits", "8", 0, 8);
    cfg.multisamples = GET_CVAR("gl_multisamples", "0", 0, 32);
    cfg.debug = GET_CVAR("gl_debug", "0", 0, 2);

    if (cfg.colorbits == 0)
        cfg.colorbits = 24;

    if (cfg.depthbits == 0)
        cfg.depthbits = cfg.colorbits > 16 ? 24 : 16;

    if (cfg.depthbits < 24)
        cfg.stencilbits = 0;

    if (cfg.multisamples < 2)
        cfg.multisamples = 0;

    const char *s = Cvar_Get("gl_profile", DEFGLPROFILE, CVAR_REFRESH)->string;

    if (!Q_stricmpn(s, "gl", 2))
        cfg.profile = QGL_PROFILE_CORE;
    else if (!Q_stricmpn(s, "es", 2))
        cfg.profile = QGL_PROFILE_ES;

    if (cfg.profile) {
        int major = 0, minor = 0;

        sscanf(s + 2, "%d.%d", &major, &minor);
        if (major >= 1 && minor >= 0) {
            cfg.major_ver = major;
            cfg.minor_ver = minor;
        } else if (cfg.profile == QGL_PROFILE_CORE) {
            cfg.major_ver = 3;
            cfg.minor_ver = 2;
        } else if (cfg.profile == QGL_PROFILE_ES) {
            cfg.major_ver = 3;
            cfg.minor_ver = 0;
        }
    }

    return cfg;
}

/*
===============
R_BeginRegistration
===============
*/
void R_BeginRegistration(const char *name)
{
    gl_static.registering = true;
    r_registration_sequence++;

    memset(&glr, 0, sizeof(glr));
    glr.viewcluster1 = glr.viewcluster2 = -2;

    GL_LoadWorld(name);

    R_ClearDebugLines();
}

/*
===============
R_EndRegistration
===============
*/
void R_EndRegistration(void)
{
    IMG_FreeUnused();
    MOD_FreeUnused();
    Scrap_Upload();
    gl_static.registering = false;
}

/*
===============
R_ModeChanged
===============
*/
void R_ModeChanged(int width, int height, int flags)
{
    vidFlags_t vid_flags = static_cast<vidFlags_t>(flags);
    if (qglFenceSync)
        vid_flags |= QVF_VIDEOSYNC;

    r_config.width = width;
    r_config.height = height;
    r_config.flags = vid_flags;
}
