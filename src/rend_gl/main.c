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

#include "gl.h"
#include "renderer/ui_scale.h"
#if !defined(RENDERER_DLL)
#include "system/system.h"
#endif

glRefdef_t glr;
glStatic_t gl_static;
glConfig_t gl_config;
statCounters_t c;

entity_t gl_world;

refcfg_t r_config;

static int gl_leaf_count;
static int gl_leaf_maxcount;
static const mleaf_t **gl_leaf_list;
static const vec_t *gl_leaf_mins;
static const vec_t *gl_leaf_maxs;
static const mnode_t *gl_leaf_topnode;

static inline int GL_PointContents(const vec3_t p, const mnode_t *headnode,
                                   bool extended) {
  if (!headnode)
    return 0;
  const mleaf_t *leaf = BSP_PointLeaf(headnode, p);
  if (!leaf)
    return 0;
  return leaf->contents[extended];
}

static void GL_BoxLeafs_r(const mnode_t *node) {
  while (node->plane) {
    box_plane_t side = BoxOnPlaneSideFast(gl_leaf_mins, gl_leaf_maxs, node->plane);
    if (side == BOX_INFRONT) {
      node = node->children[0];
    } else if (side == BOX_BEHIND) {
      node = node->children[1];
    } else {
      if (!gl_leaf_topnode)
        gl_leaf_topnode = node;
      GL_BoxLeafs_r(node->children[0]);
      node = node->children[1];
    }
  }

  if (gl_leaf_count < gl_leaf_maxcount)
    gl_leaf_list[gl_leaf_count++] = (const mleaf_t *)node;
}

static int GL_BoxLeafs_headnode(const vec3_t mins, const vec3_t maxs,
                                const mleaf_t **list, int listsize,
                                const mnode_t *headnode) {
  gl_leaf_list = list;
  gl_leaf_count = 0;
  gl_leaf_maxcount = listsize;
  gl_leaf_mins = mins;
  gl_leaf_maxs = maxs;
  gl_leaf_topnode = NULL;

  GL_BoxLeafs_r(headnode);

  return gl_leaf_count;
}

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
cvar_t *r_overBrightBits;
cvar_t *r_mapOverBrightBits;
cvar_t *r_mapOverBrightCap;
cvar_t *gl_brightness;
cvar_t *gl_dynamic;
cvar_t *gl_dlight_falloff;
cvar_t *gl_modulate_entities;
cvar_t *gl_glowmap_intensity;
cvar_t *gl_flarespeed;
cvar_t *gl_fontshadow;
cvar_t *gl_shaders;
#if USE_MD5
cvar_t *gl_md5_load;
cvar_t *gl_md5_use;
cvar_t *gl_md5_distance;
#endif
cvar_t *gl_damageblend_frac;
cvar_t *gl_waterwarp;
cvar_t *gl_warp_refraction;
cvar_t *gl_fog;
cvar_t *gl_bloom;
cvar_t *gl_bloom_iterations;
cvar_t *gl_bloom_downscale;
cvar_t *gl_bloom_firefly;
cvar_t *gl_bloom_levels;
cvar_t *gl_bloom_threshold;
cvar_t *gl_bloom_knee;
cvar_t *gl_bloom_intensity;
cvar_t *gl_bloom_saturation;
cvar_t *gl_bloom_scene_saturation;
cvar_t *gl_color_correction;
cvar_t *gl_color_brightness;
cvar_t *gl_color_contrast;
cvar_t *gl_color_saturation;
cvar_t *gl_color_tint;
cvar_t *gl_color_split_shadows;
cvar_t *gl_color_split_highlights;
cvar_t *gl_color_split_strength;
cvar_t *gl_color_split_balance;
cvar_t *gl_color_lut;
cvar_t *gl_color_lut_intensity;
cvar_t *gl_hdr;
cvar_t *gl_hdr_exposure;
cvar_t *gl_hdr_white;
cvar_t *gl_hdr_gamma;
cvar_t *gl_hdr_auto;
cvar_t *gl_hdr_auto_min;
cvar_t *gl_hdr_auto_max;
cvar_t *gl_hdr_auto_speed;
cvar_t *gl_swapinterval;
cvar_t *r_dof;
cvar_t *r_dof_allow_stencil;
cvar_t *r_dofBlurRange;
cvar_t *r_dofFocusDistance;
cvar_t *r_crt_brightboost;
cvar_t *r_crt_hardPix;
cvar_t *r_crt_hardScan;
cvar_t *r_crt_maskDark;
cvar_t *r_crt_maskLight;
cvar_t *r_crt_scaleInLinearGamma;
cvar_t *r_crt_shadowMask;
static cvar_t *r_overBrightBits_legacy;
static cvar_t *r_mapOverBrightBits_legacy;
static cvar_t *r_mapOverBrightCap_legacy;
static cvar_t *r_dofBlurRange_legacy;
static cvar_t *r_dofFocusDistance_legacy;
static cvar_t *r_crt_hardPix_legacy;
static cvar_t *r_crt_hardScan_legacy;
static cvar_t *r_crt_maskDark_legacy;
static cvar_t *r_crt_maskLight_legacy;
static cvar_t *r_crt_scaleInLinearGamma_legacy;
static cvar_t *r_crt_shadowMask_legacy;
cvar_t *r_crtmode;
cvar_t *r_resolutionscale;
cvar_t *r_resolutionscale_aggressive;
cvar_t *r_resolutionscale_fixedscale_h;
cvar_t *r_resolutionscale_fixedscale_w;
cvar_t *r_resolutionscale_gooddrawtime;
cvar_t *r_resolutionscale_increasespeed;
cvar_t *r_resolutionscale_lowerspeed;
cvar_t *r_resolutionscale_numframesbeforelowering;
cvar_t *r_resolutionscale_numframesbeforeraising;
cvar_t *r_resolutionscale_targetdrawtime;

// development variables
cvar_t *gl_znear;
cvar_t *gl_drawworld;
cvar_t *gl_drawentities;
cvar_t *gl_drawsky;
cvar_t *r_fastsky;
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
cvar_t *r_lightmap;
cvar_t *gl_fullbright;
cvar_t *gl_vertexlight;
cvar_t *gl_lightgrid;
cvar_t *gl_polyblend;
cvar_t *gl_showerrors;
cvar_t *gl_damageblend_frac;

int32_t gl_shaders_modified;
static bool gl_lightmap_syncing;

typedef struct {
  cvar_t **primary;
  cvar_t **legacy;
} r_cvar_alias_t;

static bool r_cvar_alias_syncing;

static r_cvar_alias_t r_cvar_aliases[] = {
    {&r_overBrightBits, &r_overBrightBits_legacy},
    {&r_mapOverBrightBits, &r_mapOverBrightBits_legacy},
    {&r_mapOverBrightCap, &r_mapOverBrightCap_legacy},
    {&r_dofBlurRange, &r_dofBlurRange_legacy},
    {&r_dofFocusDistance, &r_dofFocusDistance_legacy},
    {&r_crt_hardPix, &r_crt_hardPix_legacy},
    {&r_crt_hardScan, &r_crt_hardScan_legacy},
    {&r_crt_maskDark, &r_crt_maskDark_legacy},
    {&r_crt_maskLight, &r_crt_maskLight_legacy},
    {&r_crt_scaleInLinearGamma, &r_crt_scaleInLinearGamma_legacy},
    {&r_crt_shadowMask, &r_crt_shadowMask_legacy},
};

static void r_cvar_alias_changed(cvar_t *self) {
  if (r_cvar_alias_syncing)
    return;

  r_cvar_alias_syncing = true;

  for (size_t i = 0; i < q_countof(r_cvar_aliases); i++) {
    r_cvar_alias_t *pair = &r_cvar_aliases[i];
    cvar_t *primary = *pair->primary;
    cvar_t *legacy = *pair->legacy;

    if (!primary || !legacy)
      continue;

    if (self == primary) {
      Cvar_SetByVar(legacy, primary->string, FROM_CODE);
      break;
    }

    if (self == legacy) {
      Cvar_SetByVar(primary, legacy->string, FROM_CODE);
      break;
    }
  }

  r_cvar_alias_syncing = false;
}

static void r_cvar_alias_sync_defaults(void) {
  if (r_cvar_alias_syncing)
    return;

  r_cvar_alias_syncing = true;

  for (size_t i = 0; i < q_countof(r_cvar_aliases); i++) {
    r_cvar_alias_t *pair = &r_cvar_aliases[i];
    cvar_t *primary = *pair->primary;
    cvar_t *legacy = *pair->legacy;

    if (!primary || !legacy)
      continue;

    if (!(primary->flags & CVAR_MODIFIED) && (legacy->flags & CVAR_MODIFIED))
      Cvar_SetByVar(primary, legacy->string, FROM_CODE);
    else
      Cvar_SetByVar(legacy, primary->string, FROM_CODE);
  }

  r_cvar_alias_syncing = false;
}

static void r_cvar_alias_register(void) {
  for (size_t i = 0; i < q_countof(r_cvar_aliases); i++) {
    r_cvar_alias_t *pair = &r_cvar_aliases[i];
    if (*pair->primary)
      (*pair->primary)->changed = r_cvar_alias_changed;
    if (*pair->legacy)
      (*pair->legacy)->changed = r_cvar_alias_changed;
  }

  r_cvar_alias_sync_defaults();
}

static float gl_resolutionscale_current_w = 1.0f;
static float gl_resolutionscale_current_h = 1.0f;
static float gl_resolutionscale_session_draw_ms = 0.0f;
static unsigned gl_resolutionscale_draw_samples = 0;
static int gl_resolutionscale_good_frames = 0;
static int gl_resolutionscale_bad_frames = 0;
static int gl_resolutionscale_last_mode = -1;
static int gl_resolutionscale_last_width = 0;
static int gl_resolutionscale_last_height = 0;
static vec3_t gl_color_tint_value = {1.0f, 1.0f, 1.0f};
static vec3_t gl_color_split_shadow_value = {1.0f, 1.0f, 1.0f};
static vec3_t gl_color_split_highlight_value = {1.0f, 1.0f, 1.0f};
static float gl_color_lut_size = 0.0f;
static float gl_color_lut_inv_width = 0.0f;
static float gl_color_lut_inv_height = 0.0f;
static GLuint gl_color_lut_texnum = 0;
static qhandle_t gl_color_lut_handle = 0;
static bool gl_color_lut_valid = false;
static int gl_auto_exposure_index = 0;
static bool gl_auto_exposure_valid = false;
static bool gl_scene_mips_enabled = false;
static bool gl_hdr_auto_warned = false;

#define RESOLUTION_SCALE_MIN 0.1f
#define RESOLUTION_SCALE_MAX 1.0f

// ==============================================================================

static void GL_ResetResolutionScaleHistory(void) {
  gl_resolutionscale_session_draw_ms = 0.0f;
  gl_resolutionscale_draw_samples = 0;
  gl_resolutionscale_good_frames = 0;
  gl_resolutionscale_bad_frames = 0;
}

static int GL_GetResolutionScaleMode(void) {
  if (!r_resolutionscale)
    return 0;

  return Cvar_ClampInteger(r_resolutionscale, 0, 2);
}

static float GL_ClampResolutionScale(float scale) {
  return Q_clipf(scale, RESOLUTION_SCALE_MIN, RESOLUTION_SCALE_MAX);
}

static void GL_UpdateResolutionScale(void) {
  int mode = GL_GetResolutionScaleMode();

  if (mode != gl_resolutionscale_last_mode) {
    gl_resolutionscale_last_mode = mode;
    gl_resolutionscale_current_w = 1.0f;
    gl_resolutionscale_current_h = 1.0f;
    GL_ResetResolutionScaleHistory();
  }

  if (glr.fd.width != gl_resolutionscale_last_width ||
      glr.fd.height != gl_resolutionscale_last_height) {
    gl_resolutionscale_last_width = glr.fd.width;
    gl_resolutionscale_last_height = glr.fd.height;
    GL_ResetResolutionScaleHistory();
  }

  if (mode == 0 || !gl_static.use_shaders) {
    gl_resolutionscale_current_w = 1.0f;
    gl_resolutionscale_current_h = 1.0f;
  } else if (mode == 1) {
    gl_resolutionscale_current_w =
        Cvar_ClampValue(r_resolutionscale_fixedscale_w, RESOLUTION_SCALE_MIN,
                        RESOLUTION_SCALE_MAX);
    gl_resolutionscale_current_h =
        Cvar_ClampValue(r_resolutionscale_fixedscale_h, RESOLUTION_SCALE_MIN,
                        RESOLUTION_SCALE_MAX);
    gl_resolutionscale_good_frames = 0;
    gl_resolutionscale_bad_frames = 0;
  } else {
    float target =
        Cvar_ClampValue(r_resolutionscale_targetdrawtime, 0.0f, 1000.0f);
    float good = Cvar_ClampValue(r_resolutionscale_gooddrawtime, 0.0f, 1000.0f);
    float increase =
        Cvar_ClampValue(r_resolutionscale_increasespeed, 0.0f, 1.0f);
    float lower = Cvar_ClampValue(r_resolutionscale_lowerspeed, 0.0f, 1.0f);
    int raise_frames =
        Cvar_ClampInteger(r_resolutionscale_numframesbeforeraising, 1, 10000);
    int lower_frames =
        Cvar_ClampInteger(r_resolutionscale_numframesbeforelowering, 1, 10000);

    if (good > target)
      good = target;

    if (r_resolutionscale_aggressive->integer) {
      increase *= 2.0f;
      lower *= 2.0f;
      raise_frames = max(1, raise_frames / 2);
      lower_frames = max(1, lower_frames / 2);
    }

    if (gl_resolutionscale_draw_samples > 0) {
      if (gl_resolutionscale_session_draw_ms <= good) {
        gl_resolutionscale_good_frames++;
        gl_resolutionscale_bad_frames = 0;
      } else if (gl_resolutionscale_session_draw_ms >= target) {
        gl_resolutionscale_bad_frames++;
        gl_resolutionscale_good_frames = 0;
      } else {
        gl_resolutionscale_good_frames = 0;
        gl_resolutionscale_bad_frames = 0;
      }

      if (gl_resolutionscale_bad_frames >= lower_frames) {
        float next_scale = gl_resolutionscale_current_w - lower;
        gl_resolutionscale_current_w = GL_ClampResolutionScale(next_scale);
        gl_resolutionscale_current_h = gl_resolutionscale_current_w;
        gl_resolutionscale_bad_frames = 0;
      } else if (gl_resolutionscale_good_frames >= raise_frames) {
        float next_scale = gl_resolutionscale_current_w + increase;
        gl_resolutionscale_current_w = GL_ClampResolutionScale(next_scale);
        gl_resolutionscale_current_h = gl_resolutionscale_current_w;
        gl_resolutionscale_good_frames = 0;
      }
    }
  }

  gl_resolutionscale_current_w =
      GL_ClampResolutionScale(gl_resolutionscale_current_w);
  gl_resolutionscale_current_h =
      GL_ClampResolutionScale(gl_resolutionscale_current_h);

  glr.render_scale_w = gl_resolutionscale_current_w;
  glr.render_scale_h = gl_resolutionscale_current_h;
  glr.render_width = max(1, Q_rint(glr.fd.width * glr.render_scale_w));
  glr.render_height = max(1, Q_rint(glr.fd.height * glr.render_scale_h));
}

static void GL_RecordResolutionScaleTime(unsigned start_ms) {
  unsigned end_ms = Sys_Milliseconds();
  float frame_ms = (float)(end_ms - start_ms);

  if (gl_resolutionscale_draw_samples == 0)
    gl_resolutionscale_session_draw_ms = frame_ms;
  else
    gl_resolutionscale_session_draw_ms =
        gl_resolutionscale_session_draw_ms * 0.9f + frame_ms * 0.1f;

  gl_resolutionscale_draw_samples++;
}

static void GL_SetupFrustum(void) {
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

glCullResult_t GL_CullBox(const vec3_t bounds[2]) {
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

glCullResult_t GL_CullSphere(const vec3_t origin, float radius) {
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

glCullResult_t GL_CullLocalBox(const vec3_t origin, const vec3_t bounds[2]) {
  vec3_t points[8];
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
bool GL_AllocBlock(int width, int height, uint16_t *inuse, int w, int h, int *s,
                   int *t) {
  int i, j, k, x, y, max_inuse, min_inuse;

  x = 0;
  y = height;
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
void GL_MultMatrix(GLfloat *restrict p, const GLfloat *restrict a,
                   const GLfloat *restrict b) {
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      p[i * 4 + j] = a[0 * 4 + j] * b[i * 4 + 0] + a[1 * 4 + j] * b[i * 4 + 1] +
                     a[2 * 4 + j] * b[i * 4 + 2] + a[3 * 4 + j] * b[i * 4 + 3];
    }
  }
}

void GL_SetEntityAxis(void) {
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

  if ((e->scale[0] && e->scale[0] != 1) || (e->scale[1] && e->scale[1] != 1) ||
      (e->scale[2] && e->scale[2] != 1)) {
    VectorScale(glr.entaxis[0], e->scale[0], glr.entaxis[0]);
    VectorScale(glr.entaxis[1], e->scale[1], glr.entaxis[1]);
    VectorScale(glr.entaxis[2], e->scale[2], glr.entaxis[2]);
    glr.entrotated = true;
    glr.entscale = max(e->scale[0], max(e->scale[1], e->scale[2]));
  }
}

void GL_RotationMatrix(GLfloat *matrix) {
  matrix[0] = glr.entaxis[0][0];
  matrix[4] = glr.entaxis[1][0];
  matrix[8] = glr.entaxis[2][0];
  matrix[12] = glr.ent->origin[0];

  matrix[1] = glr.entaxis[0][1];
  matrix[5] = glr.entaxis[1][1];
  matrix[9] = glr.entaxis[2][1];
  matrix[13] = glr.ent->origin[1];

  matrix[2] = glr.entaxis[0][2];
  matrix[6] = glr.entaxis[1][2];
  matrix[10] = glr.entaxis[2][2];
  matrix[14] = glr.ent->origin[2];

  matrix[3] = 0;
  matrix[7] = 0;
  matrix[11] = 0;
  matrix[15] = 1;
}

void GL_RotateForEntity(void) {
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

static void GL_DrawSpriteModel(const model_t *model) {
  const entity_t *e = glr.ent;
  const mspriteframe_t *frame =
      &model->spriteframes[e->frame % model->numframes];
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

  VectorAdd3(e->origin, down, left, tess.vertices);
  VectorAdd3(e->origin, up, left, tess.vertices + 5);
  VectorAdd3(e->origin, down, right, tess.vertices + 10);
  VectorAdd3(e->origin, up, right, tess.vertices + 15);

  tess.vertices[3] = 0;
  tess.vertices[4] = 1;
  tess.vertices[8] = 0;
  tess.vertices[9] = 0;
  tess.vertices[13] = 1;
  tess.vertices[14] = 1;
  tess.vertices[18] = 1;
  tess.vertices[19] = 0;

  GL_LockArrays(4);
  qglDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  GL_UnlockArrays();
}

static void GL_DrawNullModel(void) {
  const entity_t *e = glr.ent;

  if (e->flags & RF_WEAPONMODEL)
    return;

  VectorCopy(e->origin, tess.vertices + 0);
  VectorCopy(e->origin, tess.vertices + 8);
  VectorCopy(e->origin, tess.vertices + 16);

  VectorMA(e->origin, 16, glr.entaxis[0], tess.vertices + 4);
  VectorMA(e->origin, 16, glr.entaxis[1], tess.vertices + 12);
  VectorMA(e->origin, 16, glr.entaxis[2], tess.vertices + 20);

  WN32(tess.vertices + 3, COLOR_RED.u32);
  WN32(tess.vertices + 7, COLOR_RED.u32);

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

static void make_flare_quad(const vec3_t origin, float scale) {
  vec3_t up, down, left, right;

  VectorScale(glr.viewaxis[1], scale, left);
  VectorScale(glr.viewaxis[1], -scale, right);
  VectorScale(glr.viewaxis[2], -scale, down);
  VectorScale(glr.viewaxis[2], scale, up);

  VectorAdd3(origin, down, left, tess.vertices + 0);
  VectorAdd3(origin, up, left, tess.vertices + 3);
  VectorAdd3(origin, down, right, tess.vertices + 6);
  VectorAdd3(origin, up, right, tess.vertices + 9);
}

static void GL_OccludeFlares(void) {
  const bsp_t *bsp = gl_static.world.cache;
  const entity_t *ent;
  glquery_t *q;
  vec3_t dir, org;
  float scale, dist;
  bool set = false;
  int i;

  for (ent = glr.ents.flares; ent; ent = ent->next) {
    q = HashMap_Lookup(glquery_t, gl_static.queries, &ent->skinnum);

    for (i = 0; i < 4; i++)
      if (PlaneDiff(ent->origin, &glr.frustumPlanes[i]) < -2.5f)
        break;
    if (i != 4) {
      if (q)
        q->pending = q->visible = false;
      continue; // not visible
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
      glquery_t new = {0};
      uint32_t map_size = HashMap_Size(gl_static.queries);
      Q_assert(map_size < MAX_EDICTS);
      qglGenQueries(1, &new.query);
      HashMap_Insert(gl_static.queries, &ent->skinnum, &new);
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

    if (bsp &&
        BSP_PointLeaf(bsp->nodes, ent->origin)->contents[0] & CONTENTS_SOLID) {
      VectorNormalize(dir);
      VectorMA(ent->origin, -5.0f, dir, org);
      make_flare_quad(org, scale);
    } else
      make_flare_quad(ent->origin, scale);

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

static void GL_ClassifyEntities(void) {
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

static void GL_DrawEntity(entity_t *ent) {
  model_t *model;

  glr.ent = ent;

  if (ent->flags & RF_VIEWERMODEL)
    return;

  // convert angles to axis
  GL_SetEntityAxis();

  // inline BSP model
  if (ent->model & BIT(31)) {
    const bsp_t *bsp = gl_static.world.cache;
    int index = ~ent->model;

    if (!bsp)
      Com_Error(ERR_DROP, "%s: inline model without world", __func__);

    if (index < 1 || index >= bsp->nummodels)
      Com_Error(ERR_DROP, "%s: inline model %d out of range", __func__, index);

    GL_DrawBspModel(&bsp->models[index]);
    return;
  }

  model = MOD_ForHandle(ent->model);
  if (!model) {
    GL_DrawNullModel();
    return;
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

static void GL_DrawEntities(entity_t *ent) {
  for (; ent; ent = ent->next)
    GL_DrawEntity(ent);
}

static void GL_DrawTearing(void) {
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

static const char *GL_ErrorString(GLenum err) {
  switch (err) {
#define E(x)                                                                   \
  case GL_##x:                                                                 \
    return "GL_" #x;
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

void GL_ClearErrors(void) {
  GLenum err;

  while ((err = qglGetError()) != GL_NO_ERROR)
    ;
}

bool GL_ShowErrors(const char *func) {
  GLenum err = qglGetError();

  if (err == GL_NO_ERROR)
    return false;

  do {
    if (gl_showerrors->integer)
      Com_EPrintf("%s: %s\n", func, GL_ErrorString(err));
  } while ((err = qglGetError()) != GL_NO_ERROR);

  return true;
}

static void GL_PostProcess(glStateBits_t bits, int x, int y, int w, int h) {
  GL_BindArrays(VA_POSTPROCESS);
  GL_StateBits(GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_FALSE | GLS_CULL_DISABLE |
               GLS_TEXTURE_REPLACE | bits);
  GL_ArrayBits(GLA_VERTEX | GLA_TC);
  gl_backend->load_uniforms();

  Vector4Set(tess.vertices, x, y, 0, 1);
  Vector4Set(tess.vertices + 4, x, y + h, 0, 0);
  Vector4Set(tess.vertices + 8, x + w, y, 1, 1);
  Vector4Set(tess.vertices + 12, x + w, y + h, 1, 0);

  GL_LockArrays(4);
  qglDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  GL_UnlockArrays();
}

static inline void gl_pixel_rect_to_virtual(int x, int y, int w, int h,
                                            int *out_x, int *out_y, int *out_w,
                                            int *out_h) {
  R_UIScalePixelRectToVirtual(x, y, w, h, draw.base_scale, out_x, out_y,
                              out_w, out_h);
}

typedef enum {
  PP_NONE = 0,
  PP_WATERWARP = BIT(0),
  PP_BLOOM = BIT(1),
  PP_DOF = BIT(2),
  PP_RESCALE = BIT(3),
  PP_CRT = BIT(4),
  PP_REFRACT = BIT(5),
  PP_POSTFX = BIT(6),
} pp_flags_t;

static void GL_BuildBloom(void) {
  int iterations = Cvar_ClampInteger(gl_bloom_iterations, 1, 8) * 2;
  int downscale = Cvar_ClampInteger(gl_bloom_downscale, 1, 8);
  int w = max(1, glr.render_width / downscale);
  int h = max(1, glr.render_height / downscale);

  qglViewport(0, 0, w, h);
  GL_Ortho(0, w, h, 0, -1, 1);

  // prefilter & downscale
  gls.u_block.fog_color[0] = 1.0f / w;
  gls.u_block.fog_color[1] = 1.0f / h;
  GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
  GL_ForceTexture(TMU_LIGHTMAP,
                  gl_static.postfx_bloom_mrt ? TEXNUM_PP_BLOOM : TEXNUM_BLACK);
  qglBindFramebuffer(GL_FRAMEBUFFER, FBO_BLUR_0);
  GL_PostProcess(GLS_BLOOM_PREFILTER, 0, 0, w, h);

  // blur X/Y
  for (int i = 0; i < iterations; i++) {
    int j = i & 1;

    gls.u_block.fog_color[0] = 1.0f / w;
    gls.u_block.fog_color[1] = 1.0f / h;
    gls.u_block.fog_color[j] = 0;

    GL_ForceTexture(TMU_TEXTURE, j ? TEXNUM_PP_BLUR_1 : TEXNUM_PP_BLUR_0);
    qglBindFramebuffer(GL_FRAMEBUFFER, j ? FBO_BLUR_0 : FBO_BLUR_1);
    GL_PostProcess(GLS_BLUR_GAUSS, 0, 0, w, h);
  }

  if (gls.u_block.postfx_bloom2[3] > 1.0f && qglGenerateMipmap) {
    GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_BLOOM_MIP);
    if (qglReadBuffer)
      qglReadBuffer(GL_COLOR_ATTACHMENT0);
    qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
    qglGenerateMipmap(GL_TEXTURE_2D);
  }

  GL_Setup2D();
}

static void GL_DrawDof(GLuint target_fbo) {
  const int iterations = 4;
  int w = max(1, glr.render_width / 4);
  int h = max(1, glr.render_height / 4);

  qglViewport(0, 0, w, h);
  GL_Ortho(0, w, h, 0, -1, 1);

  // downscale
  gls.u_block.fog_color[0] = 1.0f / w;
  gls.u_block.fog_color[1] = 1.0f / h;
  GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
  qglBindFramebuffer(GL_FRAMEBUFFER, FBO_BLUR_0);
  GL_PostProcess(GLS_BLUR_BOX, 0, 0, w, h);

  // blur X/Y
  for (int i = 0; i < iterations; i++) {
    int j = i & 1;

    gls.u_block.fog_color[0] = 1.0f / w;
    gls.u_block.fog_color[1] = 1.0f / h;
    gls.u_block.fog_color[j] = 0;

    GL_ForceTexture(TMU_TEXTURE, j ? TEXNUM_PP_BLUR_1 : TEXNUM_PP_BLUR_0);
    qglBindFramebuffer(GL_FRAMEBUFFER, j ? FBO_BLUR_0 : FBO_BLUR_1);
    GL_PostProcess(GLS_BLUR_GAUSS, 0, 0, w, h);
  }

  GL_Setup2D();

  glStateBits_t bits = GLS_DOF;

  GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
  GL_ForceTexture(TMU_LIGHTMAP, TEXNUM_PP_BLUR_0);
  GL_ForceTexture(TMU_GLOWMAP, TEXNUM_PP_DEPTH);

  qglBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
  int vx, vy, vw, vh;
  gl_pixel_rect_to_virtual(glr.fd.x, glr.fd.y, glr.fd.width, glr.fd.height, &vx,
                           &vy, &vw, &vh);
  int post_x = vx;
  int post_y = vy;
  int post_w = vw;
  int post_h = vh;

  if (glr.fd.dof_rect_enabled) {
    int left = max(vx, glr.fd.dof_rect.left);
    int top = max(vy, glr.fd.dof_rect.top);
    int right = min(vx + vw, glr.fd.dof_rect.right);
    int bottom = min(vy + vh, glr.fd.dof_rect.bottom);

    if (right <= left || bottom <= top)
      return;

    post_x = left;
    post_y = top;
    post_w = right - left;
    post_h = bottom - top;
  }

  GL_PostProcess(bits, post_x, post_y, post_w, post_h);
}

static void GL_UpdatePostFxUniforms(bool bloom_active) {
  float bloom_threshold = Cvar_ClampValue(gl_bloom_threshold, 0.0f, 10.0f);
  float bloom_knee = Cvar_ClampValue(gl_bloom_knee, 0.0f, 1.0f);
  float bloom_intensity = Cvar_ClampValue(gl_bloom_intensity, 0.0f, 10.0f);
  float scene_saturation =
      bloom_active ? Cvar_ClampValue(gl_bloom_scene_saturation, 0.0f, 4.0f)
                   : 1.0f;
  float bloom_saturation = Cvar_ClampValue(gl_bloom_saturation, 0.0f, 4.0f);
  float bloom_firefly = Cvar_ClampValue(gl_bloom_firefly, 0.0f, 1000.0f);
  int bloom_levels = Cvar_ClampInteger(gl_bloom_levels, 1, 6);
  int bloom_downscale = Cvar_ClampInteger(gl_bloom_downscale, 1, 8);
  int render_w = glr.render_width ? glr.render_width : glr.fd.width;
  int render_h = glr.render_height ? glr.render_height : glr.fd.height;
  int min_dim =
      max(1, min(render_w / bloom_downscale, render_h / bloom_downscale));
  int max_levels = 1;
  while (min_dim > 1 && max_levels < 6) {
    min_dim >>= 1;
    max_levels++;
  }
  if (bloom_levels > max_levels)
    bloom_levels = max_levels;

  gls.u_block.postfx_bloom[0] = bloom_threshold;
  gls.u_block.postfx_bloom[1] = bloom_knee;
  gls.u_block.postfx_bloom[2] = bloom_intensity;
  gls.u_block.postfx_bloom[3] = scene_saturation;

  gls.u_block.postfx_bloom2[0] = bloom_saturation;
  gls.u_block.postfx_bloom2[1] = gl_static.postfx_bloom_mrt ? 1.0f : 0.0f;
  gls.u_block.postfx_bloom2[2] = bloom_firefly;
  gls.u_block.postfx_bloom2[3] = qglGenerateMipmap ? (float)bloom_levels : 1.0f;

  bool hdr_enabled = gl_hdr && gl_hdr->integer && gl_static.hdr_active;
  float hdr_exposure = Cvar_ClampValue(gl_hdr_exposure, 0.0f, 10.0f);
  float hdr_white = Cvar_ClampValue(gl_hdr_white, 0.1f, 20.0f);
  float hdr_gamma = Cvar_ClampValue(gl_hdr_gamma, 1.0f, 3.0f);
  bool hdr_auto = hdr_enabled && gl_hdr_auto && gl_hdr_auto->integer;
  float hdr_auto_min = Cvar_ClampValue(gl_hdr_auto_min, 0.0001f, 10000.0f);
  float hdr_auto_max = Cvar_ClampValue(gl_hdr_auto_max, hdr_auto_min, 10000.0f);
  float hdr_auto_speed = Cvar_ClampValue(gl_hdr_auto_speed, 0.0f, 60.0f);
  float hdr_auto_alpha =
      hdr_auto ? (hdr_auto_speed <= 0.0f
                      ? 1.0f
                      : (1.0f - expf(-hdr_auto_speed * glr.fd.frametime)))
               : 0.0f;

  gls.u_block.postfx_hdr[0] = hdr_enabled ? hdr_exposure : 1.0f;
  gls.u_block.postfx_hdr[1] = hdr_enabled ? hdr_white : 1.0f;
  gls.u_block.postfx_hdr[2] = hdr_enabled ? hdr_gamma : 1.0f;
  gls.u_block.postfx_hdr[3] = hdr_enabled ? 1.0f : 0.0f;

  bool cc_enabled = gl_color_correction && gl_color_correction->integer;
  float cc_brightness =
      cc_enabled ? Cvar_ClampValue(gl_color_brightness, -1.0f, 1.0f) : 0.0f;
  float cc_contrast =
      cc_enabled ? Cvar_ClampValue(gl_color_contrast, 0.0f, 4.0f) : 1.0f;
  float cc_saturation =
      cc_enabled ? Cvar_ClampValue(gl_color_saturation, 0.0f, 4.0f) : 1.0f;
  float split_strength = Cvar_ClampValue(gl_color_split_strength, 0.0f, 1.0f);
  float split_balance = Cvar_ClampValue(gl_color_split_balance, -1.0f, 1.0f);
  float lut_intensity = Cvar_ClampValue(gl_color_lut_intensity, 0.0f, 1.0f);

  if (cc_enabled)
    gls.u_block.postfx_bloom[3] = 1.0f;

  if (hdr_auto && !gl_auto_exposure_valid)
    hdr_auto_alpha = 1.0f;

  if (!gl_color_lut_valid)
    lut_intensity = 0.0f;

  gls.u_block.postfx_color[0] = cc_brightness;
  gls.u_block.postfx_color[1] = cc_contrast;
  gls.u_block.postfx_color[2] = cc_saturation;
  gls.u_block.postfx_color[3] = cc_enabled ? 1.0f : 0.0f;

  gls.u_block.postfx_tint[0] = cc_enabled ? gl_color_tint_value[0] : 1.0f;
  gls.u_block.postfx_tint[1] = cc_enabled ? gl_color_tint_value[1] : 1.0f;
  gls.u_block.postfx_tint[2] = cc_enabled ? gl_color_tint_value[2] : 1.0f;
  gls.u_block.postfx_tint[3] = 0.0f;

  gls.u_block.postfx_auto[0] = hdr_auto ? 1.0f : 0.0f;
  gls.u_block.postfx_auto[1] = hdr_auto_min;
  gls.u_block.postfx_auto[2] = hdr_auto_max;
  gls.u_block.postfx_auto[3] = hdr_auto_alpha;

  gls.u_block.postfx_split_shadow[0] = gl_color_split_shadow_value[0];
  gls.u_block.postfx_split_shadow[1] = gl_color_split_shadow_value[1];
  gls.u_block.postfx_split_shadow[2] = gl_color_split_shadow_value[2];
  gls.u_block.postfx_split_shadow[3] = 0.0f;

  gls.u_block.postfx_split_highlight[0] = gl_color_split_highlight_value[0];
  gls.u_block.postfx_split_highlight[1] = gl_color_split_highlight_value[1];
  gls.u_block.postfx_split_highlight[2] = gl_color_split_highlight_value[2];
  gls.u_block.postfx_split_highlight[3] = 0.0f;

  gls.u_block.postfx_split_params[0] = split_strength;
  gls.u_block.postfx_split_params[1] = split_balance;
  gls.u_block.postfx_split_params[2] = 0.0f;
  gls.u_block.postfx_split_params[3] = 0.0f;

  gls.u_block.postfx_lut[0] = lut_intensity;
  gls.u_block.postfx_lut[1] = gl_color_lut_valid ? gl_color_lut_size : 0.0f;
  gls.u_block.postfx_lut[2] =
      gl_color_lut_valid ? gl_color_lut_inv_width : 0.0f;
  gls.u_block.postfx_lut[3] =
      gl_color_lut_valid ? gl_color_lut_inv_height : 0.0f;
  gls.u_block_dirty = true;
}

static void GL_UpdateAutoExposure(void) {
  bool hdr_auto =
      gls.u_block.postfx_hdr[3] > 0.5f && gls.u_block.postfx_auto[0] > 0.5f;

  if (!hdr_auto) {
    if (gl_scene_mips_enabled) {
      GL_BindTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
      qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      gl_scene_mips_enabled = false;
    }
    gl_auto_exposure_valid = false;
    gl_auto_exposure_index = 0;
    gl_hdr_auto_warned = false;
    return;
  }

  if (!qglGenerateMipmap) {
    if (!gl_hdr_auto_warned) {
      Com_WPrintf("HDR auto exposure requires mipmap generation; disabling.\n");
      gl_hdr_auto_warned = true;
    }
    gl_auto_exposure_valid = false;
    return;
  }

  if (!gl_scene_mips_enabled) {
    GL_BindTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                     GL_LINEAR_MIPMAP_LINEAR);
    gl_scene_mips_enabled = true;
  }

  GL_BindTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
  qglGenerateMipmap(GL_TEXTURE_2D);

  int src = gl_auto_exposure_index;
  int dst = 1 - src;
  GLuint src_tex = src ? TEXNUM_PP_EXPOSURE_1 : TEXNUM_PP_EXPOSURE_0;
  GLuint dst_tex = dst ? TEXNUM_PP_EXPOSURE_1 : TEXNUM_PP_EXPOSURE_0;

  qglBindFramebuffer(GL_FRAMEBUFFER, FBO_EXPOSURE);
  qglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                          dst_tex, 0);

  qglViewport(0, 0, 1, 1);
  GL_Ortho(0, 1, 1, 0, -1, 1);

  GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_SCENE);
  GL_ForceTexture(TMU_EXPOSURE, src_tex);
  GL_PostProcess(GLS_EXPOSURE_UPDATE, 0, 0, 1, 1);

  GL_Setup2D();

  gl_auto_exposure_index = dst;
  gl_auto_exposure_valid = true;
}

static void GL_DrawPostProcess(pp_flags_t flags, GLuint base_tex,
                               GLuint target_fbo) {
  bool waterwarp = (flags & PP_WATERWARP) != 0;
  bool bloom = (flags & PP_BLOOM) != 0;
  bool postfx = (flags & PP_POSTFX) != 0;

  glStateBits_t bits = GLS_DEFAULT;
  if (postfx)
    bits |= GLS_POSTFX;
  if (waterwarp)
    bits |= GLS_WARP_ENABLE;

  if (q_unlikely(gl_showbloom->integer) && bloom) {
    GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_BLUR_0);
    bits = GLS_DEFAULT;
  } else {
    GL_ForceTexture(TMU_TEXTURE, base_tex);
    if (bloom) {
      GLuint bloom_tex = gls.u_block.postfx_bloom2[3] > 1.0f
                             ? TEXNUM_PP_BLOOM_MIP
                             : TEXNUM_PP_BLUR_0;
      GL_ForceTexture(TMU_LIGHTMAP, bloom_tex);
      bits |= GLS_BLOOM_OUTPUT;
    }
    if (postfx) {
      GLuint exposure_tex =
          gl_auto_exposure_valid
              ? (gl_auto_exposure_index ? TEXNUM_PP_EXPOSURE_1
                                        : TEXNUM_PP_EXPOSURE_0)
              : TEXNUM_WHITE;
      GLuint lut_tex = gl_color_lut_valid ? gl_color_lut_texnum : TEXNUM_WHITE;
      GL_ForceTexture(TMU_EXPOSURE, exposure_tex);
      GL_ForceTexture(TMU_LUT, lut_tex);
    }
  }

  qglBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
  int vx, vy, vw, vh;
  gl_pixel_rect_to_virtual(glr.fd.x, glr.fd.y, glr.fd.width, glr.fd.height, &vx,
                           &vy, &vw, &vh);
  GL_PostProcess(bits, vx, vy, vw, vh);
}

static int32_t gl_waterwarp_modified = 0;
static int32_t gl_bloom_modified = 0;
static int32_t gl_bloom_downscale_modified = 0;
static int32_t gl_hdr_modified = 0;
static int32_t r_dof_allow_stencil_modified = 0;
static int32_t gl_crt_modified = 0;
static int32_t gl_warp_refraction_modified = 0;

static void GL_SetCrtUniforms(int width, int height) {
  float hard_pix = r_crt_hardPix ? min(r_crt_hardPix->value, 0.0f) : -8.0f;
  float hard_scan = r_crt_hardScan ? min(r_crt_hardScan->value, 0.0f) : -8.0f;
  float bright_boost =
      r_crt_brightboost ? max(r_crt_brightboost->value, 0.0f) : 1.5f;
  float mask_dark = r_crt_maskDark ? max(r_crt_maskDark->value, 0.0f) : 0.5f;
  float mask_light = r_crt_maskLight ? max(r_crt_maskLight->value, 0.0f) : 1.5f;
  float linear_gamma =
      r_crt_scaleInLinearGamma
          ? Cvar_ClampValue(r_crt_scaleInLinearGamma, 0.0f, 1.0f)
          : 1.0f;
  float shadow_mask =
      r_crt_shadowMask ? Cvar_ClampValue(r_crt_shadowMask, 0.0f, 4.0f) : 0.0f;

  float inv_w = width > 0 ? (1.0f / (float)width) : 0.0f;
  float inv_h = height > 0 ? (1.0f / (float)height) : 0.0f;

  gls.u_block.crt_params[0] = hard_pix;
  gls.u_block.crt_params[1] = hard_scan;
  gls.u_block.crt_params[2] = bright_boost;
  gls.u_block.crt_params[3] = linear_gamma;

  gls.u_block.crt_params2[0] = mask_dark;
  gls.u_block.crt_params2[1] = mask_light;
  gls.u_block.crt_params2[2] = shadow_mask;
  {
    float scan_scale = 1.0f;
    if (glr.render_height > 0 && height > 0) {
      scan_scale = (float)height / (float)glr.render_height;
      if (scan_scale < 1.0f)
        scan_scale = 1.0f;
    }
    gls.u_block.crt_params2[3] = scan_scale;
  }

  gls.u_block.crt_texel[0] = inv_w;
  gls.u_block.crt_texel[1] = inv_h;
  gls.u_block.crt_texel[2] = (float)width;
  gls.u_block.crt_texel[3] = (float)height;

  gls.u_block_dirty = true;
}

static void GL_DrawCrt(void) {
  GL_Setup2D();
  GL_SetCrtUniforms(r_config.width, r_config.height);

  GL_ForceTexture(TMU_TEXTURE, TEXNUM_PP_CRT);
  qglBindFramebuffer(GL_FRAMEBUFFER, 0);

  int vx, vy, vw, vh;
  gl_pixel_rect_to_virtual(glr.fd.x, glr.fd.y, glr.fd.width, glr.fd.height, &vx,
                           &vy, &vw, &vh);
  GL_PostProcess(GLS_CRT, vx, vy, vw, vh);
}

static pp_flags_t GL_BindFramebuffer(void) {
  pp_flags_t flags = PP_NONE;
  bool resized = false;
  bool dof_requested = false;
  bool postfx_enabled = false;
  bool hdr_requested = false;

  if (!gl_static.use_shaders)
    return PP_NONE;

  if ((glr.fd.rdflags & RDF_UNDERWATER) && gl_waterwarp->integer)
    flags |= PP_WATERWARP;

  if (!(glr.fd.rdflags & RDF_NOWORLDMODEL) && gl_bloom->integer)
    flags |= PP_BLOOM;

  dof_requested = (r_dof && r_dof->integer) &&
                  !(glr.fd.rdflags & RDF_NOWORLDMODEL) &&
                  (glr.fd.dof_strength > 0.0f);
  if (dof_requested)
    flags |= PP_DOF;

  if (r_crtmode && r_crtmode->integer)
    flags |= PP_CRT;

  bool refract_active = gl_warp_refraction->value > 0.0f &&
                        gl_static.world.has_trans_warp &&
                        !(glr.fd.rdflags & RDF_NOWORLDMODEL);
  if (refract_active)
    flags |= PP_REFRACT;

  if (glr.render_width != glr.fd.width || glr.render_height != glr.fd.height)
    flags |= PP_RESCALE;

  if (gl_bloom->integer)
    postfx_enabled = true;
  if (gl_color_correction && gl_color_correction->integer)
    postfx_enabled = true;
  if (gl_hdr && gl_hdr->integer) {
    postfx_enabled = true;
    hdr_requested = true;
  }

  if (postfx_enabled)
    flags |= PP_POSTFX;

  if (flags)
    resized = glr.render_width != glr.framebuffer_width ||
              glr.render_height != glr.framebuffer_height;

  static bool gl_dof_active = false;
  bool crt_active = (flags & PP_CRT) != 0;
  if (resized || gl_waterwarp->modified_count != gl_waterwarp_modified ||
      gl_bloom->modified_count != gl_bloom_modified ||
      gl_bloom_downscale->modified_count != gl_bloom_downscale_modified ||
      (gl_hdr && gl_hdr->modified_count != gl_hdr_modified) ||
      (r_dof_allow_stencil &&
       r_dof_allow_stencil->modified_count != r_dof_allow_stencil_modified) ||
      (r_crtmode && r_crtmode->modified_count != gl_crt_modified) ||
      gl_warp_refraction->modified_count != gl_warp_refraction_modified ||
      dof_requested != gl_dof_active) {
    glr.framebuffer_ok =
        GL_InitFramebuffers(dof_requested, crt_active, refract_active,
                            postfx_enabled, hdr_requested);
    glr.framebuffer_width = glr.render_width;
    glr.framebuffer_height = glr.render_height;
    gl_waterwarp_modified = gl_waterwarp->modified_count;
    gl_bloom_modified = gl_bloom->modified_count;
    gl_bloom_downscale_modified = gl_bloom_downscale->modified_count;
    gl_scene_mips_enabled = false;
    gl_auto_exposure_valid = false;
    gl_auto_exposure_index = 0;
    if (gl_hdr)
      gl_hdr_modified = gl_hdr->modified_count;
    if (r_dof_allow_stencil)
      r_dof_allow_stencil_modified = r_dof_allow_stencil->modified_count;
    gl_warp_refraction_modified = gl_warp_refraction->modified_count;
    if (r_crtmode)
      gl_crt_modified = r_crtmode->modified_count;
    gl_dof_active = dof_requested;
    if (gl_bloom->integer || gl_static.postfx_dof)
      gl_backend->update_blur();
  }

  if (!gl_static.postfx_dof)
    flags &= ~PP_DOF;

  if (!flags || !glr.framebuffer_ok)
    return PP_NONE;

  qglBindFramebuffer(GL_FRAMEBUFFER, FBO_SCENE);
  glr.framebuffer_bound = true;

  if (gl_clear->integer || (r_fastsky && r_fastsky->integer)) {
    if ((flags & PP_BLOOM) && gl_static.postfx_bloom_mrt) {
      static const GLenum buffers[2] = {GL_COLOR_ATTACHMENT0,
                                        GL_COLOR_ATTACHMENT1};
      static const vec4_t black = {0, 0, 0, 1};
      qglDrawBuffers(2, buffers);
      qglClearBufferfv(GL_COLOR, 0, gl_static.clearcolor);
      qglClearBufferfv(GL_COLOR, 1, black);
      qglDrawBuffers(1, buffers);
    } else {
      qglClear(GL_COLOR_BUFFER_BIT);
    }
  }

  return flags;
}

void R_RenderFrame(const refdef_t *fd) {
  GL_Flush2D();
  unsigned draw_start_ms = Sys_Milliseconds();

  Q_assert(gl_static.world.cache || (fd->rdflags & RDF_NOWORLDMODEL));

  glr.drawframe++;

  glr.fd = *fd;
  glr.ppl_bits = 0;
  GL_UpdateResolutionScale();

  if (gl_dynamic->integer != 1 || gl_vertexlight->integer)
    glr.fd.num_dlights = 0;

  glr.fog_bits = glr.fog_bits_sky = 0;

  if (gl_static.use_shaders) {
    if (gl_fog->integer > 0) {
      if (glr.fd.fog.density > 0)
        glr.fog_bits |= GLS_FOG_GLOBAL;
      if (glr.fd.heightfog.density > 0 && glr.fd.heightfog.falloff > 0)
        glr.fog_bits |= GLS_FOG_HEIGHT;
      if (glr.fd.fog.sky_factor > 0)
        glr.fog_bits_sky |= GLS_FOG_SKY;
    }

    bool use_ppl = gl_per_pixel_lighting->integer > 0;
    if (use_ppl)
      glr.ppl_bits |= GLS_DYNAMIC_LIGHTS;
  }

  if (lm.dirty) {
    GL_RebuildLighting();
    lm.dirty = false;
  }

  pp_flags_t pp_flags = GL_BindFramebuffer();

  GL_ClassifyEntities();

  gls.u_block.dof_params[0] =
      r_dofFocusDistance ? r_dofFocusDistance->value : 0.0f;
  gls.u_block.dof_params[1] = r_dofBlurRange ? r_dofBlurRange->value : 0.0f;
  gls.u_block.vieworg[3] = (r_dof && r_dof->integer)
                               ? Q_clipf(glr.fd.dof_strength, 0.0f, 1.0f)
                               : 0.0f;
  gls.u_block_dirty = true;

  GL_Setup3D();

  GL_SetupFrustum();

  if (!(glr.fd.rdflags & RDF_NOWORLDMODEL) && gl_drawworld->integer)
    GL_DrawWorld();

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

  tess.dlight_bits = 0;

  // go back into 2D mode
  GL_Setup2D();
  GL_UpdatePostFxUniforms((pp_flags & PP_BLOOM) != 0);

  bool crt_enabled = (pp_flags & PP_CRT) != 0;
  GLuint post_fbo = crt_enabled ? FBO_CRT : 0;
  GLuint base_tex = TEXNUM_PP_SCENE;

  if (pp_flags & PP_DOF) {
    GL_DrawDof(FBO_POST);
    base_tex = TEXNUM_PP_POST;
  }

  if (pp_flags & PP_BLOOM)
    GL_BuildBloom();

  GL_UpdateAutoExposure();

  if (pp_flags & (PP_BLOOM | PP_WATERWARP | PP_DOF | PP_RESCALE | PP_REFRACT |
                  PP_CRT | PP_POSTFX))
    GL_DrawPostProcess(pp_flags, base_tex, post_fbo);

  if (crt_enabled)
    GL_DrawCrt();

  if (gl_polyblend->integer)
    GL_Blend();

#if USE_DEBUG
  if (gl_lightmap->integer > 1)
    Draw_Lightmaps();
#endif

  if (gl_showerrors->integer > 1)
    GL_ShowErrors(__func__);

  GL_RecordResolutionScaleTime(draw_start_ms);
}

bool R_SupportsPerPixelLighting(void) {
  return gl_backend->use_per_pixel_lighting();
}

void R_BeginFrame(void) {
  memset(&c, 0, sizeof(c));

  if (gl_finish->integer)
    qglFinish();

  GL_Setup2D();

  if (gl_clear->integer || (r_fastsky && r_fastsky->integer))
    qglClear(GL_COLOR_BUFFER_BIT);

  if (gl_showerrors->integer > 1)
    GL_ShowErrors(__func__);
}

void R_EndFrame(void) {
#if defined(RENDERER_DLL)
  int async = 0;
  if (Cvar_VariableInteger)
    async = Cvar_VariableInteger("cl_async");
#else
  extern cvar_t *cl_async;
#endif

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

#if defined(RENDERER_DLL)
  if (qglFenceSync && async > 1 && !gl_static.sync)
#else
  if (qglFenceSync && cl_async->integer > 1 && !gl_static.sync)
#endif
    gl_static.sync = qglFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

bool R_VideoSync(void) {
  if (!gl_static.sync)
    return true;

  if (qglClientWaitSync(gl_static.sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0) ==
      GL_TIMEOUT_EXPIRED)
    return false;

  qglDeleteSync(gl_static.sync);
  gl_static.sync = 0;

  return true;
}

// ==============================================================================

static void GL_Strings_f(void) {
  GLint integer = 0;

  Com_Printf("GL_VENDOR: %s\n", qglGetString(GL_VENDOR));
  Com_Printf("GL_RENDERER: %s\n", qglGetString(GL_RENDERER));
  Com_Printf("GL_VERSION: %s\n", qglGetString(GL_VERSION));

  if (gl_config.ver_sl) {
    Com_Printf("GL_SHADING_LANGUAGE_VERSION: %s\n",
               qglGetString(GL_SHADING_LANGUAGE_VERSION));
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
    Com_Printf("GL_MAX_TEXTURE_MAX_ANISOTROPY: %.f\n",
               gl_config.max_anisotropy);
  }

  Com_Printf("GL_PFD: color(%d-bit) Z(%d-bit) stencil(%d-bit)\n",
             gl_config.colorbits, gl_config.depthbits, gl_config.stencilbits);
}

static void GL_GfxInfo_f(void) {
  GLint integer = 0;
  int render_w = glr.render_width ? glr.render_width : glr.fd.width;
  int render_h = glr.render_height ? glr.render_height : glr.fd.height;
  const char *renderer_name = "";
  const cvar_t *renderer_var = Cvar_FindVar ? Cvar_FindVar("r_renderer") : NULL;
  if (renderer_var)
    renderer_name = renderer_var->string;

  Com_Printf("\nGFXINFO\n");
  if (renderer_name && renderer_name[0])
    Com_Printf("Renderer: %s (%s backend)\n", renderer_name, gl_backend->name);
  else
    Com_Printf("Renderer: %s backend\n", gl_backend->name);

  Com_Printf("GL_VENDOR: %s\n", qglGetString(GL_VENDOR));
  Com_Printf("GL_RENDERER: %s\n", qglGetString(GL_RENDERER));
  Com_Printf("GL_VERSION: %s\n", qglGetString(GL_VERSION));
  if (gl_config.ver_sl)
    Com_Printf("GL_SHADING_LANGUAGE_VERSION: %s\n",
               qglGetString(GL_SHADING_LANGUAGE_VERSION));

  Com_Printf("GL_MAX_TEXTURE_SIZE: %d\n", gl_config.max_texture_size);
  if (qglClientActiveTexture) {
    qglGetIntegerv(GL_MAX_TEXTURE_UNITS, &integer);
    Com_Printf("GL_MAX_TEXTURE_UNITS: %d\n", integer);
  }
  if (qglGetIntegerv) {
    qglGetIntegerv(GL_MAX_DRAW_BUFFERS, &integer);
    Com_Printf("GL_MAX_DRAW_BUFFERS: %d\n", integer);
    if (qglGetStringi) {
      qglGetIntegerv(GL_NUM_EXTENSIONS, &integer);
      Com_Printf("GL_NUM_EXTENSIONS: %d\n", integer);
    }
  }

  Com_Printf("GL_PFD: color(%d-bit) Z(%d-bit) stencil(%d-bit)\n",
             gl_config.colorbits, gl_config.depthbits, gl_config.stencilbits);

  if (gl_config.caps & QGL_CAP_TEXTURE_ANISOTROPY)
    Com_Printf("GL_MAX_TEXTURE_MAX_ANISOTROPY: %.f\n",
               gl_config.max_anisotropy);

  Com_Printf(
      "Caps: shaders(%s) npot(%s) ssbo(%s) tex_lod_bias(%s) texture_bits(%s)\n",
      (gl_config.caps & QGL_CAP_SHADER) ? "yes" : "no",
      (gl_config.caps & QGL_CAP_TEXTURE_NON_POWER_OF_TWO) ? "yes" : "no",
      (gl_config.caps & QGL_CAP_SHADER_STORAGE) ? "yes" : "no",
      (gl_config.caps & QGL_CAP_TEXTURE_LOD_BIAS) ? "yes" : "no",
      (gl_config.caps & QGL_CAP_TEXTURE_BITS) ? "yes" : "no");

  Com_Printf("PostFX: shaders(%s) hdr_active(%s) framebuffer_ok(%s) fb(%dx%d) "
             "render(%dx%d) bloom_mrt(%s) dof_depth(%s)\n",
             gl_static.use_shaders ? "on" : "off",
             gl_static.hdr_active ? "yes" : "no",
             glr.framebuffer_ok ? "yes" : "no", glr.framebuffer_width,
             glr.framebuffer_height, render_w, render_h,
             gl_static.postfx_bloom_mrt ? "yes" : "no",
             gl_static.postfx_dof ? "yes" : "no");

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
}

#if USE_DEBUG

static size_t GL_ViewCluster_m(char *buffer, size_t size) {
  return Q_snprintf(buffer, size, "%d", glr.viewcluster1);
}

static size_t GL_ViewLeaf_m(char *buffer, size_t size) {
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

static void gl_lightmap_changed(cvar_t *self) {
  lm.scale = Cvar_ClampValue(gl_coloredlightmaps, 0, 1);
  lm.comp = !(gl_config.caps & QGL_CAP_TEXTURE_BITS) ? GL_RGBA
            : lm.scale                               ? GL_RGB
                                                     : GL_LUMINANCE;
  lm.add = 255 * Cvar_ClampValue(gl_brightness, -1, 1);
  lm.modulate = Cvar_ClampValue(gl_modulate, 0, 1e6f);
  lm.modulate *= Cvar_ClampValue(gl_modulate_world, 0, 1e6f);
  lm.modulate *= gl_static.identity_light;
  if (gl_static.use_shaders &&
      (self == gl_brightness || self == gl_modulate ||
       self == gl_modulate_world) &&
      !gl_vertexlight->integer)
    return;
  lm.dirty = true; // rebuild all lightmaps next frame
}

static void gl_modulate_entities_changed(cvar_t *self) {
  gl_static.entity_modulate = Cvar_ClampValue(gl_modulate, 0, 1e6f);
  gl_static.entity_modulate *= Cvar_ClampValue(gl_modulate_entities, 0, 1e6f);
  gl_static.entity_modulate *= gl_static.identity_light;
}

static void gl_modulate_changed(cvar_t *self) {
  gl_lightmap_changed(self);
  gl_modulate_entities_changed(self);
}

static void gl_lightmap_sync(cvar_t *self) {
  if (gl_lightmap_syncing)
    return;

  gl_lightmap_syncing = true;
  if (self == r_lightmap && gl_lightmap)
    Cvar_SetByVar(gl_lightmap, r_lightmap->string, FROM_CODE);
  else if (self == gl_lightmap && r_lightmap)
    Cvar_SetByVar(r_lightmap, gl_lightmap->string, FROM_CODE);
  gl_lightmap_syncing = false;

  gl_lightmap_changed(self);
}

static void gl_sync_lightmap_defaults(void) {
  if (!r_lightmap || !gl_lightmap)
    return;

  if (!(r_lightmap->flags & CVAR_MODIFIED) &&
      (gl_lightmap->flags & CVAR_MODIFIED))
    Cvar_SetByVar(r_lightmap, gl_lightmap->string, FROM_CODE);
  else
    Cvar_SetByVar(gl_lightmap, r_lightmap->string, FROM_CODE);
}

void GL_UpdateOverbright(void) {
  int requested = r_overBrightBits ? r_overBrightBits->integer : 0;
  int overbright = requested < 0 ? -requested : requested;
  int max_bits = gl_config.colorbits > 16 ? 2 : 1;

  if (!(r_config.flags & QVF_GAMMARAMP))
    overbright = 0;

  if (!(r_config.flags & QVF_FULLSCREEN) && requested >= 0)
    overbright = 0;

  if (overbright > max_bits)
    overbright = max_bits;

  gl_static.overbright_bits = overbright;
  gl_static.identity_light = 1.0f / (float)(1 << gl_static.overbright_bits);

  if (r_mapOverBrightBits)
    gl_static.map_overbright_bits =
        Cvar_ClampInteger(r_mapOverBrightBits, 0, 4);
  else
    gl_static.map_overbright_bits = 0;

  if (r_mapOverBrightCap)
    gl_static.map_overbright_cap =
        Cvar_ClampInteger(r_mapOverBrightCap, 0, 255);
  else
    gl_static.map_overbright_cap = 255;

  gl_static.lightmap_shift =
      gl_static.map_overbright_bits - gl_static.overbright_bits;
}

// ugly hack to reset sky
static void gl_drawsky_changed(cvar_t *self) {
  if (gl_static.world.cache)
    CL_SetSky();
}

static void gl_novis_changed(cvar_t *self) {
  glr.viewcluster1 = glr.viewcluster2 = -2;
}

static void gl_swapinterval_changed(cvar_t *self) {
  if (vid && vid->swap_interval)
    vid->swap_interval(self->integer);
}

static void gl_clearcolor_changed(cvar_t *self) {
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

static void gl_color_tint_changed(cvar_t *self) {
  color_t color;

  if (!SCR_ParseColor(self->string, &color)) {
    Com_WPrintf("Invalid value '%s' for '%s'\n", self->string, self->name);
    Cvar_Reset(self);
    color.u32 = COLOR_U32_WHITE;
  }

  gl_color_tint_value[0] = color.u8[0] / 255.0f;
  gl_color_tint_value[1] = color.u8[1] / 255.0f;
  gl_color_tint_value[2] = color.u8[2] / 255.0f;
}

static void gl_color_split_shadows_changed(cvar_t *self) {
  color_t color;

  if (!SCR_ParseColor(self->string, &color)) {
    Com_WPrintf("Invalid value '%s' for '%s'\n", self->string, self->name);
    Cvar_Reset(self);
    color.u32 = COLOR_U32_WHITE;
  }

  gl_color_split_shadow_value[0] = color.u8[0] / 255.0f;
  gl_color_split_shadow_value[1] = color.u8[1] / 255.0f;
  gl_color_split_shadow_value[2] = color.u8[2] / 255.0f;
}

static void gl_color_split_highlights_changed(cvar_t *self) {
  color_t color;

  if (!SCR_ParseColor(self->string, &color)) {
    Com_WPrintf("Invalid value '%s' for '%s'\n", self->string, self->name);
    Cvar_Reset(self);
    color.u32 = COLOR_U32_WHITE;
  }

  gl_color_split_highlight_value[0] = color.u8[0] / 255.0f;
  gl_color_split_highlight_value[1] = color.u8[1] / 255.0f;
  gl_color_split_highlight_value[2] = color.u8[2] / 255.0f;
}

static void GL_ResetColorLut(void) {
  gl_color_lut_size = 0.0f;
  gl_color_lut_inv_width = 0.0f;
  gl_color_lut_inv_height = 0.0f;
  gl_color_lut_handle = 0;
  gl_color_lut_texnum = 0;
  gl_color_lut_valid = false;
}

static void gl_color_lut_changed(cvar_t *self) {
  GL_ResetColorLut();

  if (!self || !self->string[0])
    return;

  qhandle_t handle = R_RegisterImage(
      self->string, IT_PIC, IF_PERMANENT | IF_EXACT | IF_NO_COLOR_ADJUST);
  if (!handle) {
    Com_WPrintf("Color LUT '%s' could not be loaded\n", self->string);
    return;
  }

  image_t *image = IMG_ForHandle(handle);
  if (!image || !image->upload_width || !image->upload_height) {
    Com_WPrintf("Color LUT '%s' has invalid dimensions\n", self->string);
    return;
  }

  int w = image->upload_width;
  int h = image->upload_height;
  int size = 0;

  if (w == h * h)
    size = h;
  else if (h == w * w)
    size = w;

  if (size <= 0) {
    Com_WPrintf("Color LUT '%s' expects NxN strip (got %dx%d)\n", self->string,
                w, h);
    return;
  }

  gl_color_lut_size = (float)size;
  gl_color_lut_inv_width = 1.0f / (float)w;
  gl_color_lut_inv_height = 1.0f / (float)h;
  gl_color_lut_handle = handle;
  gl_color_lut_texnum = image->texnum;
  gl_color_lut_valid = true;

  GL_BindTexture(TMU_LUT, gl_color_lut_texnum);
  qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void GL_Register(void) {
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
  r_overBrightBits =
      Cvar_Get("r_overbright_bits", "1", CVAR_ARCHIVE | CVAR_FILES);
  r_overBrightBits_legacy =
      Cvar_Get("r_overBrightBits", r_overBrightBits->string,
               CVAR_ARCHIVE | CVAR_FILES | CVAR_NOARCHIVE);
  r_mapOverBrightBits =
      Cvar_Get("r_map_overbright_bits", "0", CVAR_ARCHIVE | CVAR_FILES);
  r_mapOverBrightBits_legacy =
      Cvar_Get("r_mapOverBrightBits", r_mapOverBrightBits->string,
               CVAR_ARCHIVE | CVAR_FILES | CVAR_NOARCHIVE);
  r_mapOverBrightCap =
      Cvar_Get("r_map_overbright_cap", "255", CVAR_ARCHIVE | CVAR_FILES);
  r_mapOverBrightCap_legacy =
      Cvar_Get("r_mapOverBrightCap", r_mapOverBrightCap->string,
               CVAR_ARCHIVE | CVAR_FILES | CVAR_NOARCHIVE);
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
  gl_shaders = Cvar_Get("gl_shaders", "1", CVAR_FILES);
#if USE_MD5
  gl_md5_load = Cvar_Get("gl_md5_load", "1", CVAR_FILES);
  gl_md5_use = Cvar_Get("gl_md5_use", "1", 0);
  gl_md5_distance = Cvar_Get("gl_md5_distance", "2048", 0);
#endif
  gl_damageblend_frac = Cvar_Get("gl_damageblend_frac", "0.2", 0);
  gl_waterwarp = Cvar_Get("gl_waterwarp", "1", 0);
  gl_warp_refraction = Cvar_Get("gl_warp_refraction", "0.1", 0);
  gl_fog = Cvar_Get("gl_fog", "1", 0);
  gl_bloom = Cvar_Get("gl_bloom", "1", 0);
  gl_bloom_iterations = Cvar_Get("gl_bloom_iterations", "1", CVAR_ARCHIVE);
  gl_bloom_downscale = Cvar_Get("gl_bloom_downscale", "4", CVAR_ARCHIVE);
  gl_bloom_firefly = Cvar_Get("gl_bloom_firefly", "10.0", CVAR_ARCHIVE);
  gl_bloom_levels = Cvar_Get("gl_bloom_levels", "1", CVAR_ARCHIVE);
  gl_bloom_threshold = Cvar_Get("gl_bloom_threshold", "1.0", CVAR_ARCHIVE);
  gl_bloom_knee = Cvar_Get("gl_bloom_knee", "0.5", CVAR_ARCHIVE);
  gl_bloom_intensity = Cvar_Get("gl_bloom_intensity", "1.0", CVAR_ARCHIVE);
  gl_bloom_saturation = Cvar_Get("gl_bloom_saturation", "1.0", CVAR_ARCHIVE);
  gl_bloom_scene_saturation =
      Cvar_Get("gl_bloom_scene_saturation", "1.0", CVAR_ARCHIVE);
  gl_color_correction = Cvar_Get("gl_color_correction", "1", CVAR_ARCHIVE);
  gl_color_brightness = Cvar_Get("gl_color_brightness", "0.0", CVAR_ARCHIVE);
  gl_color_contrast = Cvar_Get("gl_color_contrast", "1.0", CVAR_ARCHIVE);
  gl_color_saturation = Cvar_Get("gl_color_saturation", "1.0", CVAR_ARCHIVE);
  gl_color_tint = Cvar_Get("gl_color_tint", "white", CVAR_ARCHIVE);
  gl_color_tint->changed = gl_color_tint_changed;
  gl_color_tint->generator = Com_Color_g;
  gl_color_split_shadows =
      Cvar_Get("gl_color_split_shadows", "white", CVAR_ARCHIVE);
  gl_color_split_shadows->changed = gl_color_split_shadows_changed;
  gl_color_split_shadows->generator = Com_Color_g;
  gl_color_split_highlights =
      Cvar_Get("gl_color_split_highlights", "white", CVAR_ARCHIVE);
  gl_color_split_highlights->changed = gl_color_split_highlights_changed;
  gl_color_split_highlights->generator = Com_Color_g;
  gl_color_split_strength =
      Cvar_Get("gl_color_split_strength", "0.0", CVAR_ARCHIVE);
  gl_color_split_balance =
      Cvar_Get("gl_color_split_balance", "0.0", CVAR_ARCHIVE);
  gl_color_lut = Cvar_Get("gl_color_lut", "", CVAR_ARCHIVE);
  gl_color_lut->changed = gl_color_lut_changed;
  gl_color_lut_intensity =
      Cvar_Get("gl_color_lut_intensity", "1.0", CVAR_ARCHIVE);
  gl_hdr = Cvar_Get("gl_hdr", "0", CVAR_ARCHIVE);
  gl_hdr_exposure = Cvar_Get("gl_hdr_exposure", "1.0", CVAR_ARCHIVE);
  gl_hdr_white = Cvar_Get("gl_hdr_white", "1.0", CVAR_ARCHIVE);
  gl_hdr_gamma = Cvar_Get("gl_hdr_gamma", "2.2", CVAR_ARCHIVE);
  gl_hdr_auto = Cvar_Get("gl_hdr_auto_exposure", "0", CVAR_ARCHIVE);
  gl_hdr_auto_min = Cvar_Get("gl_hdr_auto_min_luma", "0.05", CVAR_ARCHIVE);
  gl_hdr_auto_max = Cvar_Get("gl_hdr_auto_max_luma", "4.0", CVAR_ARCHIVE);
  gl_hdr_auto_speed = Cvar_Get("gl_hdr_auto_speed", "2.0", CVAR_ARCHIVE);

  if (gl_bloom_iterations->modified_count == 0 && gl_bloom->integer > 1) {
    int legacy_iters = Cvar_ClampInteger(gl_bloom, 1, 8);
    Cvar_SetByVar(gl_bloom_iterations, va("%d", legacy_iters), FROM_CODE);
  }
  r_dof = Cvar_Get("r_dof", "1", CVAR_ARCHIVE | CVAR_LATCH);
  r_dof_allow_stencil = Cvar_Get("r_dof_allow_stencil", "0", CVAR_ARCHIVE);
  r_dofBlurRange = Cvar_Get("r_dof_blur_range", "0.0", CVAR_SERVERINFO);
  r_dofBlurRange_legacy = Cvar_Get("r_dofBlurRange", r_dofBlurRange->string,
                                   CVAR_SERVERINFO | CVAR_NOARCHIVE);
  r_dofFocusDistance =
      Cvar_Get("r_dof_focus_distance", "16.0", CVAR_SERVERINFO);
  r_dofFocusDistance_legacy =
      Cvar_Get("r_dofFocusDistance", r_dofFocusDistance->string,
               CVAR_SERVERINFO | CVAR_NOARCHIVE);
  r_crt_brightboost = Cvar_Get("r_crt_brightboost", "1.5", CVAR_SERVERINFO);
  r_crt_hardPix = Cvar_Get("r_crt_hard_pix", "-8.0", CVAR_SERVERINFO);
  r_crt_hardPix_legacy = Cvar_Get("r_crt_hardPix", r_crt_hardPix->string,
                                  CVAR_SERVERINFO | CVAR_NOARCHIVE);
  r_crt_hardScan = Cvar_Get("r_crt_hard_scan", "-8.0", CVAR_SERVERINFO);
  r_crt_hardScan_legacy = Cvar_Get("r_crt_hardScan", r_crt_hardScan->string,
                                   CVAR_SERVERINFO | CVAR_NOARCHIVE);
  r_crt_maskDark = Cvar_Get("r_crt_mask_dark", "0.5", CVAR_SERVERINFO);
  r_crt_maskDark_legacy = Cvar_Get("r_crt_maskDark", r_crt_maskDark->string,
                                   CVAR_SERVERINFO | CVAR_NOARCHIVE);
  r_crt_maskLight = Cvar_Get("r_crt_mask_light", "1.5", CVAR_SERVERINFO);
  r_crt_maskLight_legacy = Cvar_Get("r_crt_maskLight", r_crt_maskLight->string,
                                    CVAR_SERVERINFO | CVAR_NOARCHIVE);
  r_crt_scaleInLinearGamma =
      Cvar_Get("r_crt_scale_in_linear_gamma", "1.0", CVAR_ARCHIVE);
  r_crt_scaleInLinearGamma_legacy =
      Cvar_Get("r_crt_scaleInLinearGamma", r_crt_scaleInLinearGamma->string,
               CVAR_ARCHIVE | CVAR_NOARCHIVE);
  r_crt_shadowMask = Cvar_Get("r_crt_shadow_mask", "0.0", CVAR_ARCHIVE);
  r_crt_shadowMask_legacy =
      Cvar_Get("r_crt_shadowMask", r_crt_shadowMask->string,
               CVAR_ARCHIVE | CVAR_NOARCHIVE);
  r_cvar_alias_register();
  r_crtmode = Cvar_Get("r_crtmode", "0", CVAR_ARCHIVE);
  r_resolutionscale = Cvar_Get("r_resolutionscale", "0", CVAR_USERINFO);
  r_resolutionscale_aggressive =
      Cvar_Get("r_resolutionscale_aggressive", "0", CVAR_ARCHIVE);
  r_resolutionscale_fixedscale_h =
      Cvar_Get("r_resolutionscale_fixedscale_h", "1.0", CVAR_SERVERINFO);
  r_resolutionscale_fixedscale_w =
      Cvar_Get("r_resolutionscale_fixedscale_w", "1.0", CVAR_SERVERINFO);
  r_resolutionscale_gooddrawtime =
      Cvar_Get("r_resolutionscale_gooddrawtime", "0.9", CVAR_SERVERINFO);
  r_resolutionscale_increasespeed =
      Cvar_Get("r_resolutionscale_increasespeed", "0.1", CVAR_SERVERINFO);
  r_resolutionscale_lowerspeed =
      Cvar_Get("r_resolutionscale_lowerspeed", "0.1", CVAR_SERVERINFO);
  r_resolutionscale_numframesbeforelowering = Cvar_Get(
      "r_resolutionscale_numframesbeforelowering", "20", CVAR_USERINFO);
  r_resolutionscale_numframesbeforeraising = Cvar_Get(
      "r_resolutionscale_numframesbeforeraising", "200", CVAR_USERINFO);
  r_resolutionscale_targetdrawtime =
      Cvar_Get("r_resolutionscale_targetdrawtime", "1.125", CVAR_SERVERINFO);
  gl_swapinterval = Cvar_Get("gl_swapinterval", "1", CVAR_ARCHIVE);
  gl_swapinterval->changed = gl_swapinterval_changed;

  // development variables
  gl_znear = Cvar_Get("gl_znear", "2", CVAR_CHEAT);
  gl_drawworld = Cvar_Get("gl_drawworld", "1", CVAR_CHEAT);
  gl_drawentities = Cvar_Get("gl_drawentities", "1", CVAR_CHEAT);
  gl_drawsky = Cvar_Get("gl_drawsky", "1", 0);
  gl_drawsky->changed = gl_drawsky_changed;
  r_fastsky = Cvar_Get("r_fastsky", "0", CVAR_ARCHIVE);
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
  r_lightmap = Cvar_Get("r_lightmap", gl_lightmap->string, CVAR_CHEAT);
  gl_lightmap->changed = gl_lightmap_sync;
  r_lightmap->changed = gl_lightmap_sync;
  gl_fullbright = Cvar_Get("r_fullbright", "0", CVAR_CHEAT);
  gl_fullbright->changed = gl_lightmap_changed;
  gl_vertexlight = Cvar_Get("gl_vertexlight", "0", 0);
  gl_vertexlight->changed = gl_lightmap_changed;
  gl_lightgrid = Cvar_Get("gl_lightgrid", "1", 0);
  gl_polyblend = Cvar_Get("gl_polyblend", "1", 0);
  gl_showerrors = Cvar_Get("gl_showerrors", "1", 0);
  gl_damageblend_frac = Cvar_Get("gl_damageblend_frac", "0.2", 0);

  GL_UpdateOverbright();
  gl_sync_lightmap_defaults();
  gl_lightmap_changed(NULL);
  gl_modulate_entities_changed(NULL);
  gl_swapinterval_changed(gl_swapinterval);
  gl_clearcolor_changed(gl_clearcolor);
  gl_color_tint_changed(gl_color_tint);
  gl_color_split_shadows_changed(gl_color_split_shadows);
  gl_color_split_highlights_changed(gl_color_split_highlights);
  gl_color_lut_changed(gl_color_lut);

  Cmd_AddCommand("strings", GL_Strings_f);
  Cmd_AddCommand("gfxinfo", GL_GfxInfo_f);

#if USE_DEBUG
  Cmd_AddMacro("gl_viewcluster", GL_ViewCluster_m);
  Cmd_AddMacro("gl_viewleaf", GL_ViewLeaf_m);
#endif
}

static void GL_Unregister(void) {
  Cmd_RemoveCommand("strings");
}

static void APIENTRY myDebugProc(GLenum source, GLenum type, GLuint id,
                                 GLenum severity, GLsizei length,
                                 const GLchar *message, const void *userParam) {
  int level = PRINT_DEVELOPER;

  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH:
    level = PRINT_ERROR;
    break;
  case GL_DEBUG_SEVERITY_MEDIUM:
    level = PRINT_WARNING;
    break;
  case GL_DEBUG_SEVERITY_LOW:
    level = PRINT_ALL;
    break;
  }

  Com_LPrintf(level, "%s\n", message);
}

static void GL_SetupConfig(void) {
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

    qglGetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, backbuf, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &integer);
    gl_config.colorbits = integer;
    qglGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, backbuf,
                                           GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
                                           &integer);
    gl_config.colorbits += integer;
    qglGetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, backbuf, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, &integer);
    gl_config.colorbits += integer;

    qglGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH,
                                           GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,
                                           &integer);
    gl_config.depthbits = integer;

    qglGetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, GL_STENCIL, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,
        &integer);
    gl_config.stencilbits = integer;
  }

  if (qglDebugMessageCallback && qglIsEnabled(GL_DEBUG_OUTPUT)) {
    Com_Printf("Enabling GL debug output.\n");
    qglEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    if (Cvar_VariableInteger("gl_debug") < 2)
      qglDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                             GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
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

static void GL_InitTables(void) {
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

static void GL_PostInit(void) {
  r_registration_sequence = 1;

  if (gl_shaders->modified_count != gl_shaders_modified) {
    GL_ShutdownState();
    GL_InitState();
    gl_shaders_modified = gl_shaders->modified_count;
  }
  GL_ClearState();
  if (r_overBrightBits) {
    GL_UpdateOverbright();
    gl_lightmap_changed(NULL);
    gl_modulate_entities_changed(NULL);
  }
  GL_InitImages();
  GL_InitQueries();
  MOD_Init();
}

void GL_InitQueries(void) {
  if (!qglBeginQuery)
    return;

  gl_static.samples_passed = GL_SAMPLES_PASSED;
  if (gl_config.ver_gl >= QGL_VER(3, 3) || gl_config.ver_es >= QGL_VER(3, 0))
    gl_static.samples_passed = GL_ANY_SAMPLES_PASSED;

  Q_assert(!gl_static.queries);
  gl_static.queries =
      HashMap_TagCreate(int, glquery_t, HashInt32, NULL, TAG_RENDERER);
}

void GL_DeleteQueries(void) {
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

static void Draw_Stats_s(void) {
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
  SCR_StatKeyValuef("Faces / batch", c.batchesDrawn
                                         ? (float)c.facesDrawn / c.batchesDrawn
                                         : 0.0f);
  SCR_StatKeyValuef("Tris / batch", c.batchesDrawn
                                        ? (float)c.facesTris / c.batchesDrawn
                                        : 0.0f);
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
bool R_Init(bool total) {
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

  GL_InitArrays();

  GL_InitState();

  GL_InitTables();

  GL_InitDebugDraw();

  GL_PostInit();

  GL_ShowErrors(__func__);

  SCR_RegisterStat("renderer", Draw_Stats_s);

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
void R_Shutdown(bool total) {
  Com_DPrintf("GL_Shutdown( %i )\n", total);

  GL_FreeWorld();
  GL_DeleteQueries();
  GL_ShutdownImages();
  MOD_Shutdown();

  if (!total)
    return;

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

  SCR_UnregisterStat("renderer");

  memset(&gl_static, 0, sizeof(gl_static));
  memset(&gl_config, 0, sizeof(gl_config));
}

/*
===============
R_GetGLConfig
===============
*/
r_opengl_config_t R_GetGLConfig(void) {
#define GET_CVAR(name, def, min, max)                                          \
  Cvar_ClampInteger(Cvar_Get(name, def, CVAR_RENDERER), min, max)

  r_opengl_config_t cfg = {
      .colorbits = GET_CVAR("gl_colorbits", "0", 0, 32),
      .depthbits = GET_CVAR("gl_depthbits", "0", 0, 32),
      .stencilbits = GET_CVAR("gl_stencilbits", "8", 0, 8),
      .multisamples = GET_CVAR("gl_multisamples", "0", 0, 32),
      .debug = GET_CVAR("gl_debug", "0", 0, 2),
  };

  if (cfg.colorbits == 0)
    cfg.colorbits = 24;

  if (cfg.depthbits == 0)
    cfg.depthbits = cfg.colorbits > 16 ? 24 : 16;

  if (cfg.depthbits < 24)
    cfg.stencilbits = 0;

  if (cfg.multisamples < 2)
    cfg.multisamples = 0;

  const char *s = Cvar_Get("gl_profile", DEFGLPROFILE, CVAR_RENDERER)->string;

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
void R_BeginRegistration(const char *name) {
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
void R_EndRegistration(void) {
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
void R_ModeChanged(int width, int height, int flags) {
  if (qglFenceSync)
    flags |= QVF_VIDEOSYNC;

  r_config.width = width;
  r_config.height = height;
  r_config.flags = flags;

  if (r_overBrightBits) {
    GL_UpdateOverbright();
    GL_RebuildGammaTables();
    gl_lightmap_changed(NULL);
    gl_modulate_entities_changed(NULL);
  }
}
