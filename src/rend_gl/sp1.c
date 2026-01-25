/*
Copyright (C) 2018 Andrey Nazarov

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

#include "common/sizebuf.h"
#include "gl.h"

#define MAX_SHADER_CHARS 65536

#define GLSL(x) SZ_Write(buf, CONST_STR_LEN(#x "\n"));
#define GLSF(x) SZ_Write(buf, CONST_STR_LEN(x))
#define GLSP(...) shader_printf(buf, __VA_ARGS__)

static cvar_t *gl_bloom_sigma;
cvar_t *gl_per_pixel_lighting;

q_printf(2, 3) static void shader_printf(sizebuf_t *buf, const char *fmt, ...) {
  va_list ap;
  size_t len;

  Q_assert(buf->cursize <= buf->maxsize);

  va_start(ap, fmt);
  len = Q_vsnprintf((char *)buf->data + buf->cursize,
                    buf->maxsize - buf->cursize, fmt, ap);
  va_end(ap);

  Q_assert(len <= buf->maxsize - buf->cursize);
  buf->cursize += len;
}

static void write_header(sizebuf_t *buf, glStateBits_t bits) {
#if USE_MD5
  if (bits & GLS_MESH_MD5 && gl_config.caps & QGL_CAP_SHADER_STORAGE) {
    if (gl_config.ver_es)
      GLSF("#version 310 es\n");
    else
      GLSF("#version 430\n");
  } else
#endif
      if (gl_config.ver_es) {
    GLSF("#version 300 es\n");
  } else if (gl_config.ver_sl >= QGL_VER(1, 40)) {
    GLSF("#version 140\n");
  } else {
    GLSF("#version 130\n");
    GLSF("#extension GL_ARB_uniform_buffer_object : require\n");
  }

  if (gl_config.ver_es) {
    GLSL(precision mediump float;)
    if (bits & GLS_MESH_ANY)
      GLSL(precision mediump int;)
  }
}

static void write_block(sizebuf_t *buf, glStateBits_t bits) {
  GLSF("layout(std140) uniform Uniforms {\n");
  GLSL(mat4 m_model; mat4 m_view; mat4 m_proj;);

  if (bits & GLS_MESH_ANY) {
    GLSL(vec3 u_old_scale; vec3 u_new_scale; vec3 u_translate; vec3 u_shadedir;
         vec4 u_color; vec4 pad_0; float pad_1; float pad_2; float pad_3;
         uint u_weight_ofs; uint u_jointnum_ofs; float u_shellscale;
         float u_backlerp; float u_frontlerp;)
  } else {
    GLSL(mat4 m_sky[2];)
  }

  GLSL(float u_time; float u_modulate; float u_add; float u_intensity;
       float u_intensity2; float u_fog_sky_factor; vec2 w_amp; vec2 w_phase;
       vec2 u_scroll; vec4 u_fog_color; vec4 u_heightfog_start;
       vec4 u_heightfog_end; float u_heightfog_density;
       float u_heightfog_falloff; float u_refract_scale; float u_refract_pad;
       vec4 u_dof_params; vec4 u_vieworg; vec4 u_shadow_params;
       vec4 u_shadow_params2; vec4 u_shadow_params3; vec4 u_shadow_params4;
       vec4 u_crt_params; vec4 u_crt_params2; vec4 u_crt_texel;
       vec4 u_postfx_bloom; vec4 u_postfx_bloom2; vec4 u_postfx_hdr;
       vec4 u_postfx_color; vec4 u_postfx_tint; vec4 u_postfx_auto;
       vec4 u_postfx_split_shadow; vec4 u_postfx_split_highlight;
       vec4 u_postfx_split_params; vec4 u_postfx_lut; vec4 u_viewdir;
       vec4 u_sun_dir; vec4 u_sun_color; vec4 u_csm_splits; vec4 u_csm_params;)
  GLSF("mat4 u_csm_matrix[" STRINGIFY(MAX_CSM_CASCADES) "];\n");
  GLSF("};\n");
}

static void write_dynamic_light_block(sizebuf_t *buf) {
  GLSL(struct dlight_t {
    vec3 position;
    float radius;
    vec4 color;
    vec4 cone;
    vec4 shadow;
  };)
  GLSF("#define DLIGHT_CUTOFF 64\n");
  GLSF("layout(std140) uniform DynamicLights {\n");
  GLSF("#define MAX_DLIGHTS " STRINGIFY(MAX_DLIGHTS) "\n");
  GLSL(int num_dlights; int dpad_1; int dpad_2; int dpad_3;
       dlight_t dlights[MAX_DLIGHTS];)
  GLSF("};\n");
}
