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

static void write_dynamic_lights(sizebuf_t *buf) {
  GLSL(
      float shadow_hash(vec3 p) {
        return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
      }

      mat2 shadow_rotate(float angle) {
        float s = sin(angle);
        float c = cos(angle);
        return mat2(c, -s, s, c);
      }

      float shadow_compare(vec2 uv, float layer, float depth, float bias) {
        vec2 clamped_uv = clamp(uv, 0.0, 1.0);
        float stored = texture(u_shadowmap, vec3(clamped_uv, layer)).r;
        return depth - bias <= stored ? 1.0 : 0.0;
      }

      float shadow_vsm(vec2 uv, float layer, float depth, float bias,
                       float bleed, float min_var) {
        vec2 clamped_uv = clamp(uv, 0.0, 1.0);
        vec2 moments = texture(u_shadowmap, vec3(clamped_uv, layer)).rg;
        float mean = moments.x;
        float mean2 = moments.y;
        float depth_bias = depth - bias;
        float d = depth_bias - mean;
        float variance = max(mean2 - mean * mean, min_var);
        float p = variance / (variance + d * d);
        float result = depth_bias <= mean ? 1.0 : p;
        if (bleed > 0.0)
          result = clamp((result - bleed) / (1.0 - bleed), 0.0, 1.0);
        return result;
      }

      float shadow_evsm(vec2 uv, float layer, float depth, float bias,
                        float exponent, float bleed, float min_var) {
        vec2 clamped_uv = clamp(uv, 0.0, 1.0);
        vec2 moments = texture(u_shadowmap, vec3(clamped_uv, layer)).rg;
        float depth_bias = depth - bias;
        float warped = exp(min(exponent * depth_bias, 80.0));
        float mean = moments.x;
        float mean2 = moments.y;
        float d = warped - mean;
        float variance = max(mean2 - mean * mean, min_var);
        float p = variance / (variance + d * d);
        float result = warped <= mean ? 1.0 : p;
        if (bleed > 0.0)
          result = clamp((result - bleed) / (1.0 - bleed), 0.0, 1.0);
        return result;
      }

      float shadow_pcf(vec2 uv, float layer, float depth, float bias,
                       float texel, int quality) {
        if (texel <= 0.0 || quality <= 0)
          return shadow_compare(uv, layer, depth, bias);

        if (quality == 1) {
          vec2 o = vec2(texel);
          float shadow = 0.0;
          shadow += shadow_compare(uv + vec2(-o.x, -o.y), layer, depth, bias);
          shadow += shadow_compare(uv + vec2(o.x, -o.y), layer, depth, bias);
          shadow += shadow_compare(uv + vec2(-o.x, o.y), layer, depth, bias);
          shadow += shadow_compare(uv + vec2(o.x, o.y), layer, depth, bias);
          return shadow * 0.25;
        }

        if (quality == 2) {
          float shadow = 0.0;
          for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
              shadow += shadow_compare(uv + vec2(float(x), float(y)) * texel,
                                       layer, depth, bias);
            }
          }
          return shadow / 9.0;
        }

        float shadow = 0.0;
        for (int y = 0; y < 4; y++) {
          float oy = float(y) - 1.5;
          for (int x = 0; x < 4; x++) {
            float ox = float(x) - 1.5;
            shadow +=
                shadow_compare(uv + vec2(ox, oy) * texel, layer, depth, bias);
          }
        }
        return shadow * 0.0625;
      }

      const vec2 pcss_offsets[16] = vec2[16](
          vec2(-0.613, 0.354), vec2(0.170, -0.676), vec2(0.476, 0.168),
          vec2(-0.415, -0.354), vec2(-0.070, 0.726), vec2(0.656, -0.140),
          vec2(-0.243, -0.761), vec2(0.780, 0.524), vec2(-0.907, -0.090),
          vec2(0.267, 0.918), vec2(-0.674, 0.605), vec2(0.097, -0.222),
          vec2(-0.003, 0.102), vec2(0.540, 0.839), vec2(-0.824, 0.273),
          vec2(0.919, -0.451));

      float shadow_pcss(vec2 uv, float layer, float depth, float bias,
                        float texel, float softness, int blocker_samples,
                        int filter_samples) {
        if (texel <= 0.0 || softness <= 0.0)
          return shadow_compare(uv, layer, depth, bias);

        int blockers = 0;
        float avg_blocker = 0.0;
        float angle = shadow_hash(vec3(uv, depth)) * 6.2831853;
        mat2 rot = shadow_rotate(angle);
        float search_radius = texel * softness * 2.0;

        for (int i = 0; i < 16; i++) {
          if (i >= blocker_samples)
            break;
          vec2 offset = rot * pcss_offsets[i] * search_radius;
          float sample_depth =
              texture(u_shadowmap, vec3(clamp(uv + offset, 0.0, 1.0), layer)).r;
          if (sample_depth < depth - bias) {
            avg_blocker += sample_depth;
            blockers++;
          }
        }

        if (blockers == 0)
          return 1.0;

        avg_blocker /= float(blockers);
        float penumbra = (depth - avg_blocker) / max(avg_blocker, 0.0001);
        float filter_radius = max(texel, penumbra * softness * texel);

        float shadow = 0.0;
        int samples = max(filter_samples, 1);
        for (int i = 0; i < 16; i++) {
          if (i >= samples)
            break;
          vec2 offset = rot * pcss_offsets[i] * filter_radius;
          shadow += shadow_compare(uv + offset, layer, depth, bias);
        }

        return shadow / float(min(samples, 16));
      }

      float shadow_compare_csm(vec2 uv, float layer, float depth, float bias) {
        float stored = texture(u_shadowmap_csm, vec3(uv, layer)).r;
        return depth - bias <= stored ? 1.0 : 0.0;
      }

      float shadow_vsm_csm(vec2 uv, float layer, float depth, float bias,
                           float bleed, float min_var) {
        vec2 moments = texture(u_shadowmap_csm, vec3(uv, layer)).rg;
        float mean = moments.x;
        float mean2 = moments.y;
        float depth_bias = depth - bias;
        float d = depth_bias - mean;
        float variance = max(mean2 - mean * mean, min_var);
        float p = variance / (variance + d * d);
        float result = depth_bias <= mean ? 1.0 : p;
        if (bleed > 0.0)
          result = clamp((result - bleed) / (1.0 - bleed), 0.0, 1.0);
        return result;
      }

      float shadow_evsm_csm(vec2 uv, float layer, float depth, float bias,
                            float exponent, float bleed, float min_var) {
        vec2 moments = texture(u_shadowmap_csm, vec3(uv, layer)).rg;
        float depth_bias = depth - bias;
        float warped = exp(min(exponent * depth_bias, 80.0));
        float mean = moments.x;
        float mean2 = moments.y;
        float d = warped - mean;
        float variance = max(mean2 - mean * mean, min_var);
        float p = variance / (variance + d * d);
        float result = warped <= mean ? 1.0 : p;
        if (bleed > 0.0)
          result = clamp((result - bleed) / (1.0 - bleed), 0.0, 1.0);
        return result;
      }

      float shadow_pcf_csm(vec2 uv, float layer, float depth, float bias,
                           float texel, int quality) {
        if (texel <= 0.0 || quality <= 0)
          return shadow_compare_csm(uv, layer, depth, bias);

        if (quality == 1) {
          vec2 o = vec2(texel);
          float shadow = 0.0;
          shadow +=
              shadow_compare_csm(uv + vec2(-o.x, -o.y), layer, depth, bias);
          shadow +=
              shadow_compare_csm(uv + vec2(o.x, -o.y), layer, depth, bias);
          shadow +=
              shadow_compare_csm(uv + vec2(-o.x, o.y), layer, depth, bias);
          shadow += shadow_compare_csm(uv + vec2(o.x, o.y), layer, depth, bias);
          return shadow * 0.25;
        }

        if (quality == 2) {
          float shadow = 0.0;
          for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
              shadow += shadow_compare_csm(
                  uv + vec2(float(x), float(y)) * texel, layer, depth, bias);
            }
          }
          return shadow / 9.0;
        }

        float shadow = 0.0;
        for (int y = 0; y < 4; y++) {
          float oy = float(y) - 1.5;
          for (int x = 0; x < 4; x++) {
            float ox = float(x) - 1.5;
            shadow += shadow_compare_csm(uv + vec2(ox, oy) * texel, layer,
                                         depth, bias);
          }
        }
        return shadow * 0.0625;
      }

      float shadow_pcss_csm(vec2 uv, float layer, float depth, float bias,
                            float texel, float softness, int blocker_samples,
                            int filter_samples) {
        if (texel <= 0.0 || softness <= 0.0)
          return shadow_compare_csm(uv, layer, depth, bias);

        int blockers = 0;
        float avg_blocker = 0.0;
        float angle = shadow_hash(vec3(uv, depth)) * 6.2831853;
        mat2 rot = shadow_rotate(angle);
        float search_radius = texel * softness * 2.0;

        for (int i = 0; i < 16; i++) {
          if (i >= blocker_samples)
            break;
          vec2 offset = rot * pcss_offsets[i] * search_radius;
          float sample_depth =
              texture(u_shadowmap_csm, vec3(uv + offset, layer)).r;
          if (sample_depth < depth - bias) {
            avg_blocker += sample_depth;
            blockers++;
          }
        }

        if (blockers == 0)
          return 1.0;

        avg_blocker /= float(blockers);
        float penumbra = (depth - avg_blocker) / max(avg_blocker, 0.0001);
        float filter_radius = max(texel, penumbra * softness * texel);

        float shadow = 0.0;
        int samples = max(filter_samples, 1);
        for (int i = 0; i < 16; i++) {
          if (i >= samples)
            break;
          vec2 offset = rot * pcss_offsets[i] * filter_radius;
          shadow += shadow_compare_csm(uv + offset, layer, depth, bias);
        }

        return shadow / float(min(samples, 16));
      }

      vec2 shadow_cube_uv(vec3 dir, out float face) {
        vec3 adir = abs(dir);
        vec2 uv;
        if (adir.x >= adir.y && adir.x >= adir.z) {
          if (dir.x > 0.0) {
            face = 0.0;
            uv = vec2(-dir.z, dir.y) / adir.x;
          } else {
            face = 1.0;
            uv = vec2(dir.z, dir.y) / adir.x;
          }
        } else if (adir.y >= adir.x && adir.y >= adir.z) {
          if (dir.y > 0.0) {
            face = 2.0;
            uv = vec2(dir.x, -dir.z) / adir.y;
          } else {
            face = 3.0;
            uv = vec2(dir.x, dir.z) / adir.y;
          }
        } else {
          if (dir.z > 0.0) {
            face = 4.0;
            uv = vec2(dir.x, dir.y) / adir.z;
          } else {
            face = 5.0;
            uv = vec2(-dir.x, dir.y) / adir.z;
          }
        }
        return uv * 0.5 + 0.5;
      }

      float shadow_point(vec3 light_dir, float dist, float radius,
                         float shadow_index, float bias, int method) {
        if (shadow_index < 0.0 || u_shadow_params.x <= 0.0)
          return 1.0;

        float face;
        vec2 uv = shadow_cube_uv(light_dir / max(dist, 1.0), face);
        float layer = shadow_index * 6.0 + face;
        float depth = dist / radius;
        float base_texel = u_shadow_params.x;
        float softness = max(u_shadow_params.z, 0.0);
        float texel = base_texel * softness;
        int quality = int(u_shadow_params2.y + 0.5);
        int blocker_samples = int(clamp(u_shadow_params4.x, 1.0, 16.0));
        int filter_samples = int(clamp(u_shadow_params4.y, 1.0, 16.0));

        if (method == 4)
          return shadow_pcss(uv, layer, depth, bias, base_texel, softness,
                             blocker_samples, filter_samples);

        if (method == 3)
          return shadow_evsm(uv, layer, depth, bias, u_shadow_params3.z,
                             u_shadow_params2.z, u_shadow_params2.w);

        if (method == 2)
          return shadow_vsm(uv, layer, depth, bias, u_shadow_params2.z,
                            u_shadow_params2.w);

        if (method == 1)
          return shadow_pcf(uv, layer, depth, bias, texel, quality);

        return shadow_compare(uv, layer, depth, bias);
      }

      float shadow_spot(vec3 light_vec, float dist, float radius,
                        float shadow_index, vec3 spot_dir, float spot_cos,
                        float bias, int method) {
        if (shadow_index < 0.0 || u_shadow_params.x <= 0.0)
          return 1.0;

        vec3 dir = light_vec / max(dist, 1.0);
        vec3 forward = normalize(spot_dir);
        float proj = dot(dir, forward);
        if (proj <= 0.0)
          return 1.0;

        vec3 basis_up =
            abs(forward.z) > 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
        vec3 right = normalize(cross(basis_up, forward));
        vec3 up = cross(forward, right);

        float spot_sin = sqrt(max(1.0 - spot_cos * spot_cos, 0.0));
        float inv_tan = spot_cos / max(spot_sin, 0.0001);
        vec2 proj_uv =
            vec2(dot(dir, right), dot(dir, up)) * (inv_tan / max(proj, 0.001));
        vec2 uv = proj_uv * 0.5 + 0.5;

        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
          return 1.0;

        float layer = shadow_index * 6.0;
        float depth = dist / radius;
        float base_texel = u_shadow_params.x;
        float softness = max(u_shadow_params.z, 0.0);
        float texel = base_texel * softness;
        int quality = int(u_shadow_params2.y + 0.5);
        int blocker_samples = int(clamp(u_shadow_params4.x, 1.0, 16.0));
        int filter_samples = int(clamp(u_shadow_params4.y, 1.0, 16.0));

        if (method == 4)
          return shadow_pcss(uv, layer, depth, bias, base_texel, softness,
                             blocker_samples, filter_samples);

        if (method == 3)
          return shadow_evsm(uv, layer, depth, bias, u_shadow_params3.z,
                             u_shadow_params2.z, u_shadow_params2.w);

        if (method == 2)
          return shadow_vsm(uv, layer, depth, bias, u_shadow_params2.z,
                            u_shadow_params2.w);

        if (method == 1)
          return shadow_pcf(uv, layer, depth, bias, texel, quality);

        return shadow_compare(uv, layer, depth, bias);
      }

      float shadow_csm(vec3 world_pos, float bias) {
        int cascades = int(u_csm_params.y + 0.5);
        if (cascades <= 0 || u_csm_params.x <= 0.0)
          return 1.0;

        float view_depth = dot(world_pos - u_vieworg.xyz, u_viewdir.xyz);
        if (view_depth < 0.0)
          return 1.0;

        int cascade = 0;
        int next_cascade = -1;
        float blend = 0.0;
        float split_next = u_csm_splits.x;

        if (view_depth < u_csm_splits.x) {
          cascade = 0;
          split_next = u_csm_splits.x;
        } else if (view_depth < u_csm_splits.y) {
          cascade = 1;
          split_next = u_csm_splits.y;
        } else if (view_depth < u_csm_splits.z) {
          cascade = 2;
          split_next = u_csm_splits.z;
        } else {
          cascade = 3;
        }

        if (cascade >= cascades)
          cascade = cascades - 1;

        if (cascade < cascades - 1) {
          float fade_range = split_next * u_csm_params.z;
          if (view_depth > split_next - fade_range) {
            next_cascade = cascade + 1;
            blend = (view_depth - (split_next - fade_range)) /
                    max(fade_range, 0.001);
            blend = clamp(blend, 0.0, 1.0);
          }
        }

        vec4 shadow_pos = u_csm_matrix[cascade] * vec4(world_pos, 1.0);
        vec3 proj = shadow_pos.xyz / shadow_pos.w;

        float shadow = 1.0;
        if (proj.x >= 0.0 && proj.x <= 1.0 && proj.y >= 0.0 && proj.y <= 1.0 &&
            proj.z >= 0.0 && proj.z <= 1.0) {
          float layer = float(cascade);
          float shadow_depth = proj.z;
          float base_texel = u_csm_params.x;
          float softness = max(u_shadow_params.z, 0.0);
          float texel = base_texel * softness;
          int method = int(u_shadow_params2.x + 0.5);
          int quality = int(u_shadow_params2.y + 0.5);
          int blocker_samples = int(clamp(u_shadow_params4.x, 1.0, 16.0));
          int filter_samples = int(clamp(u_shadow_params4.y, 1.0, 16.0));

          if (method == 4)
            shadow =
                shadow_pcss_csm(proj.xy, layer, shadow_depth, bias, base_texel,
                                softness, blocker_samples, filter_samples);
          else if (method == 3)
            shadow = shadow_evsm_csm(proj.xy, layer, shadow_depth, bias,
                                     u_shadow_params3.z, u_shadow_params2.z,
                                     u_shadow_params2.w);
          else if (method == 2)
            shadow = shadow_vsm_csm(proj.xy, layer, shadow_depth, bias,
                                    u_shadow_params2.z, u_shadow_params2.w);
          else if (method == 1)
            shadow = shadow_pcf_csm(proj.xy, layer, shadow_depth, bias, texel,
                                    quality);
          else
            shadow = shadow_compare_csm(proj.xy, layer, shadow_depth, bias);
        }

        if (next_cascade != -1 && blend > 0.0) {
          vec4 shadow_pos2 = u_csm_matrix[next_cascade] * vec4(world_pos, 1.0);
          vec3 proj2 = shadow_pos2.xyz / shadow_pos2.w;
          float shadow2 = 1.0;
          if (proj2.x >= 0.0 && proj2.x <= 1.0 && proj2.y >= 0.0 &&
              proj2.y <= 1.0 && proj2.z >= 0.0 && proj2.z <= 1.0) {
            float layer = float(next_cascade);
            float shadow_depth = proj2.z;
            float base_texel = u_csm_params.x;
            float softness = max(u_shadow_params.z, 0.0);
            float texel = base_texel * softness;
            int method = int(u_shadow_params2.x + 0.5);
            int quality = int(u_shadow_params2.y + 0.5);
            int blocker_samples = int(clamp(u_shadow_params4.x, 1.0, 16.0));
            int filter_samples = int(clamp(u_shadow_params4.y, 1.0, 16.0));

            if (method == 4)
              shadow2 = shadow_pcss_csm(proj2.xy, layer, shadow_depth, bias,
                                        base_texel, softness, blocker_samples,
                                        filter_samples);
            else if (method == 3)
              shadow2 = shadow_evsm_csm(proj2.xy, layer, shadow_depth, bias,
                                        u_shadow_params3.z, u_shadow_params2.z,
                                        u_shadow_params2.w);
            else if (method == 2)
              shadow2 = shadow_vsm_csm(proj2.xy, layer, shadow_depth, bias,
                                       u_shadow_params2.z, u_shadow_params2.w);
            else if (method == 1)
              shadow2 = shadow_pcf_csm(proj2.xy, layer, shadow_depth, bias,
                                       texel, quality);
            else
              shadow2 = shadow_compare_csm(proj2.xy, layer, shadow_depth, bias);
          }
          shadow = mix(shadow, shadow2, blend);
        }

        if (u_shadow_params4.w > 1.5) {
          float f = float(cascade) * 0.25;
          if (next_cascade != -1)
            f = mix(f, float(next_cascade) * 0.25, blend);
          if (fract(gl_FragCoord.x * 0.5 + gl_FragCoord.y * 0.5) > 0.5)
            shadow *= 0.5;
          shadow = clamp(shadow + f * 0.2, 0.0, 1.0);
        }

        return shadow;
      }

      vec3 calc_dynamic_lights() {
        vec3 shade = vec3(0);
        vec3 receiver_pos = v_world_pos + v_norm * u_shadow_params3.y;

        for (int i = 0; i < num_dlights; i++) {
          vec3 base_light_pos = dlights[i].position;
          float light_cone = dlights[i].cone.w;

          vec3 shadow_vec = receiver_pos - base_light_pos;
          float shadow_dist = length(shadow_vec);

          vec3 light_pos = base_light_pos;
          if (light_cone == 0.0 && dlights[i].shadow.x < 0.0)
            light_pos += v_norm * 16.0;

          vec3 light_dir = light_pos - v_world_pos;
          float dist = length(light_dir);
          float radius = dlights[i].radius + DLIGHT_CUTOFF;
          float len = max(radius - dist - DLIGHT_CUTOFF, 0.0) / radius;
          vec3 dir = light_dir / max(dist, 1.0);
          float lambert;

          if (dlights[i].color.r < 0.0f)
            lambert = 1.0f;
          else
            lambert = max(dot(v_norm, dir), 0.0);
          vec3 result =
              ((dlights[i].color.rgb * dlights[i].color.a) * len) * lambert;
          float bias =
              (u_shadow_params.y + u_shadow_params3.x * (1.0 - lambert)) *
              u_shadow_params4.z;
          int method = int(u_shadow_params2.x + 0.5);
          if (method == 4 && dlights[i].shadow.z < 0.5)
            method = 1;

          if (light_cone != 0.0) {
            float mag = -dot(dir, dlights[i].cone.xyz);
            result *= max(1.0 - (1.0 - mag) * (1.0 / (1.0 - light_cone)), 0.0);
          }

          if (dlights[i].shadow.y > 0.5) {
            result *= shadow_spot(shadow_vec, shadow_dist, radius,
                                  dlights[i].shadow.x, dlights[i].cone.xyz,
                                  light_cone, bias, method);
          } else {
            result *= shadow_point(shadow_vec, shadow_dist, radius,
                                   dlights[i].shadow.x, bias, method);
          }

          shade += result;
        }

        if (u_sun_dir.w > 0.5 && u_sun_color.a > 0.0) {
          vec3 sun_dir = normalize(u_sun_dir.xyz);
          float lambert = max(dot(v_norm, sun_dir), 0.0);
          if (lambert > 0.0) {
            float bias =
                (u_shadow_params.y + u_shadow_params3.x * (1.0 - lambert)) *
                u_shadow_params4.z;
            float shadow = shadow_csm(receiver_pos, bias);
            shade += u_sun_color.rgb * u_sun_color.a * lambert * shadow;
          }
        }

        return shade;
      })
}
static void write_shadedot(sizebuf_t *buf) {
  GLSL(float shadedot(vec3 normal) {
    float d = dot(normal, u_shadedir);
    if (d < 0.0)
      d *= 0.3;
    return d + 1.0;
  })
}

#if USE_MD5
static void write_skel_shader(sizebuf_t *buf, glStateBits_t bits) {
  const bool shadow = bits & GLS_SHADOWMAP;

  GLSL(
      struct Joint {
        vec4 pos;
        mat3x3 axis;
      };
      layout(std140) uniform Skeleton { Joint u_joints[256]; };)

  if (gl_config.caps & QGL_CAP_SHADER_STORAGE) {
    GLSL(
        layout(std430, binding = 0)
            readonly buffer Weights { vec4 b_weights[]; };

        layout(std430, binding = 1)
            readonly buffer JointNums { uint b_jointnums[]; };)
  } else {
    GLSL(uniform samplerBuffer u_weights; uniform usamplerBuffer u_jointnums;)
  }

  GLSL(in vec3 a_norm; in uvec2 a_vert;)

  if (!shadow) {
    GLSL(in vec2 a_tc; out vec2 v_tc; out vec4 v_color;)
  }

  if (bits &
      (GLS_FOG_HEIGHT | GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT | GLS_SHADOWMAP))
    GLSL(out vec3 v_world_pos;)
  if (bits & (GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT))
    GLSL(out vec3 v_norm;)

  if (bits & GLS_MESH_SHADE)
    write_shadedot(buf);

  GLSF("void main() {\n");
  GLSL(vec3 out_pos = vec3(0.0); vec3 out_norm = vec3(0.0);

       uint start = a_vert[0]; uint count = a_vert[1];)

  GLSF("for (uint i = start; i < start + count; i++) {\n");
  if (gl_config.caps & QGL_CAP_SHADER_STORAGE) {
    GLSL(uint jointnum = b_jointnums[i / 4U]; jointnum >>= (i & 3U) * 8U;
         jointnum &= 255U;

         vec4 weight = b_weights[i];)
  } else {
    GLSL(uint jointnum = texelFetch(u_jointnums, int(u_jointnum_ofs + i)).r;
         vec4 weight = texelFetch(u_weights, int(u_weight_ofs + i));)
  }
  GLSL(Joint joint = u_joints[jointnum];

       vec3 wv = joint.pos.xyz + (weight.xyz * joint.axis) * joint.pos.w;
       out_pos += wv * weight.w;

       out_norm += a_norm * joint.axis * weight.w;)
  GLSF("}\n");

  if (!shadow)
    GLSL(v_tc = a_tc;)

  if (!shadow) {
    if (bits & GLS_MESH_SHADE)
      GLSL(v_color = vec4(u_color.rgb * shadedot(out_norm), u_color.a);)
    else
      GLSL(v_color = u_color;)
  }

  if (bits & GLS_MESH_SHELL)
    GLSL(out_pos += out_norm * u_shellscale;)

  if (bits &
      (GLS_FOG_HEIGHT | GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT | GLS_SHADOWMAP))
    GLSL(v_world_pos = (m_model * vec4(out_pos, 1.0)).xyz;)
  if (bits & (GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT))
    GLSL(v_norm = normalize((mat3(m_model) * out_norm).xyz);)
  GLSL(gl_Position = m_proj * m_view * m_model * vec4(out_pos, 1.0);)
  GLSF("}\n");
}
#endif

static void write_getnormal(sizebuf_t *buf) {
  GLSL(vec3 get_normal(int norm) {
    const float pi = 3.14159265358979323846;
    const float scale = pi * (2.0 / 255.0);
    float lat = float(uint(norm) & 255U) * scale;
    float lng = float((uint(norm) >> 8) & 255U) * scale;
    return vec3(sin(lat) * cos(lng), sin(lat) * sin(lng), cos(lat));
  })
}

static void write_mesh_shader(sizebuf_t *buf, glStateBits_t bits) {
  const bool shadow = bits & GLS_SHADOWMAP;

  GLSL(in ivec4 a_new_pos;)

  if (bits & GLS_MESH_LERP)
    GLSL(in ivec4 a_old_pos;)

  if (!shadow) {
    GLSL(in vec2 a_tc; out vec2 v_tc; out vec4 v_color;)
  }

  if (bits &
      (GLS_FOG_HEIGHT | GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT | GLS_SHADOWMAP))
    GLSL(out vec3 v_world_pos;)
  if (bits & (GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT))
    GLSL(out vec3 v_norm;)

  if (bits &
      (GLS_MESH_SHELL | GLS_MESH_SHADE | GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT))
    write_getnormal(buf);

  if (bits & GLS_MESH_SHADE)
    write_shadedot(buf);

  GLSF("void main() {\n");
  if (!shadow)
    GLSL(v_tc = a_tc;)

  if (bits & GLS_MESH_LERP) {
    if (bits &
        (GLS_MESH_SHELL | GLS_MESH_SHADE | GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT))
      GLSL(vec3 old_norm = get_normal(a_old_pos.w);
           vec3 new_norm = get_normal(a_new_pos.w);
           vec3 norm =
               normalize(old_norm * u_backlerp + new_norm * u_frontlerp);)

    GLSL(vec3 pos = vec3(a_old_pos.xyz) * u_old_scale +
                    vec3(a_new_pos.xyz) * u_new_scale + u_translate;)

    if (bits & GLS_MESH_SHELL)
      GLSL(pos += norm * u_shellscale;)

    if (!shadow) {
      if (bits & GLS_MESH_SHADE)
        GLSL(v_color = vec4(u_color.rgb * (shadedot(old_norm) * u_backlerp +
                                           shadedot(new_norm) * u_frontlerp),
                            u_color.a);)
      else
        GLSL(v_color = u_color;)
    }

    if (bits & (GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT))
      GLSL(v_norm = normalize((mat3(m_model) * norm).xyz);)
  } else {
    if (bits &
        (GLS_MESH_SHELL | GLS_MESH_SHADE | GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT))
      GLSL(vec3 norm = get_normal(a_new_pos.w);)

    GLSL(vec3 pos = vec3(a_new_pos.xyz) * u_new_scale + u_translate;)

    if (bits & GLS_MESH_SHELL)
      GLSL(pos += norm * u_shellscale;)

    if (!shadow) {
      if (bits & GLS_MESH_SHADE)
        GLSL(v_color = vec4(u_color.rgb * shadedot(norm), u_color.a);)
      else
        GLSL(v_color = u_color;)
    }

    if (bits & (GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT))
      GLSL(v_norm = normalize((mat3(m_model) * norm).xyz);)
  }

  if (bits &
      (GLS_FOG_HEIGHT | GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT | GLS_SHADOWMAP))
    GLSL(v_world_pos = (m_model * vec4(pos, 1.0)).xyz;)

  GLSL(gl_Position = m_proj * m_view * m_model * vec4(pos, 1.0);)
  GLSF("}\n");
}

static void write_vertex_shader(sizebuf_t *buf, glStateBits_t bits) {
  write_header(buf, bits);
  write_block(buf, bits);

#if USE_MD5
  if (bits & GLS_MESH_MD5) {
    write_skel_shader(buf, bits);
    return;
  }
#endif

  if (bits & GLS_MESH_MD2) {
    write_mesh_shader(buf, bits);
    return;
  }

  if (bits & GLS_SHADOWMAP) {
    GLSL(in vec4 a_pos;)
    GLSL(out vec3 v_world_pos;)
    GLSF("void main() {\n");
    GLSL(v_world_pos = (m_model * a_pos).xyz;)
    GLSL(gl_Position = m_proj * m_view * m_model * a_pos;)
    GLSF("}\n");
    return;
  }

  GLSL(in vec4 a_pos;)
  if (bits & GLS_SKY_MASK) {
    GLSL(out vec3 v_dir;)
  } else {
    GLSL(in vec2 a_tc;)
    GLSL(out vec2 v_tc;)
  }

  if (bits & GLS_LIGHTMAP_ENABLE) {
    GLSL(in vec2 a_lmtc;)
    GLSL(out vec2 v_lmtc;)
  }

  if (!(bits & GLS_TEXTURE_REPLACE)) {
    GLSL(in vec4 a_color;)
    GLSL(out vec4 v_color;)
  }

  if (bits & (GLS_FOG_HEIGHT | GLS_DYNAMIC_LIGHTS))
    GLSL(out vec3 v_world_pos;)
  if (bits & GLS_DYNAMIC_LIGHTS) {
    GLSL(in vec3 a_norm;)
    GLSL(out vec3 v_norm;)
  }

  GLSF("void main() {\n");
  if (bits & GLS_CLASSIC_SKY) {
    GLSL(v_dir = (m_sky[1] * a_pos).xyz;)
  } else if (bits & GLS_DEFAULT_SKY) {
    GLSL(v_dir = (m_sky[0] * a_pos).xyz;)
  } else if (bits & GLS_SCROLL_ENABLE) {
    GLSL(v_tc = a_tc + u_scroll;)
  } else {
    GLSL(v_tc = a_tc;)
  }

  if (bits & GLS_LIGHTMAP_ENABLE)
    GLSL(v_lmtc = a_lmtc;)

  if (!(bits & GLS_TEXTURE_REPLACE))
    GLSL(v_color = a_color;)

  if (bits & (GLS_FOG_HEIGHT | GLS_DYNAMIC_LIGHTS))
    GLSL(v_world_pos = (m_model * a_pos).xyz;)
  if (bits & GLS_DYNAMIC_LIGHTS)
    GLSL(v_norm = normalize((mat3(m_model) * a_norm).xyz);)
  GLSL(gl_Position = m_proj * m_view * m_model * a_pos;)
  GLSF("}\n");
}

#define MAX_SIGMA 25
#define MAX_RADIUS 50

// https://lisyarus.github.io/blog/posts/blur-coefficients-generator.html
static void write_gaussian_blur(sizebuf_t *buf) {
  float sigma = gl_static.bloom_sigma;
  int radius = min(sigma * 2 + 0.5f, MAX_RADIUS);
  int samples = radius + 1;
  int raw_samples = (radius * 2) + 1;
  float offsets[MAX_RADIUS + 1];
  float weights[(MAX_RADIUS * 2) + 1];

  // should not really happen
  if (radius < 1) {
    GLSL(vec4 blur(sampler2D src, vec2 tc, vec2 dir) {
      return texture(src, tc);
    })
    return;
  }

  float sum = 0;
  for (int i = -radius, j = 0; i <= radius; i++, j++) {
    float w = expf(-(i * i) / (sigma * sigma));
    weights[j] = w;
    sum += w;
  }

  for (int i = 0; i < raw_samples; i++)
    weights[i] /= sum;

  for (int i = -radius, j = 0; i <= radius; i += 2, j++) {
    if (i == radius) {
      offsets[j] = i;
      weights[j] = weights[i + radius];
    } else {
      float w0 = weights[i + radius + 0];
      float w1 = weights[i + radius + 1];
      float w = w0 + w1;

      if (w > 0)
        offsets[j] = i + w1 / w;
      else
        offsets[j] = i;

      weights[j] = w;
    }
  }

  GLSP("#define BLUR_SAMPLES %d\n", samples);

  GLSF("const float blur_offsets[BLUR_SAMPLES] = float[BLUR_SAMPLES](\n");
  for (int i = 0; i < samples - 1; i++)
    GLSP("%f, ", offsets[i]);
  GLSP("%f);\n", offsets[samples - 1]);

  GLSF("const float blur_weights[BLUR_SAMPLES] = float[BLUR_SAMPLES](\n");
  for (int i = 0; i < samples - 1; i++)
    GLSP("%f, ", weights[i]);
  GLSP("%f);\n", weights[samples - 1]);

  GLSL(vec4 blur(sampler2D src, vec2 tc, vec2 dir) {
    vec4 result = vec4(0.0);
    for (int i = 0; i < BLUR_SAMPLES; i++)
      result += texture(src, tc + dir * blur_offsets[i]) * blur_weights[i];
    return result;
  })
}

static void write_box_blur(sizebuf_t *buf) {
  GLSL(vec4 blur(sampler2D src, vec2 tc, vec2 dir) {
    vec4 result = vec4(0.0);
    const float o = 0.25;
    result += texture(src, tc + vec2(-o, -o) * dir) * 0.25;
    result += texture(src, tc + vec2(-o, o) * dir) * 0.25;
    result += texture(src, tc + vec2(o, -o) * dir) * 0.25;
    result += texture(src, tc + vec2(o, o) * dir) * 0.25;
    return result;
  })
}

// XXX: this is very broken. but that's how it is in re-release.
static void write_height_fog(sizebuf_t *buf, glStateBits_t bits) {
  GLSL({
    float dir_z = normalize(v_world_pos - u_vieworg.xyz).z;
    float s = sign(dir_z);
    dir_z += 0.00001 * (1.0 - s * s);
    float eye = u_vieworg.z - u_heightfog_start.w;
    float pos = v_world_pos.z - u_heightfog_start.w;
    float density =
        (exp(-u_heightfog_falloff * eye) - exp(-u_heightfog_falloff * pos)) /
        (u_heightfog_falloff * dir_z);
    float extinction = 1.0 - clamp(exp(-density), 0.0, 1.0);
    float fraction = clamp((pos - u_heightfog_start.w) /
                               (u_heightfog_end.w - u_heightfog_start.w),
                           0.0, 1.0);
    vec3 fog_color =
        mix(u_heightfog_start.rgb, u_heightfog_end.rgb, fraction) * extinction;
    float fog = (1.0 - exp(-(u_heightfog_density * frag_depth))) * extinction;
    diffuse.rgb = mix(diffuse.rgb, fog_color.rgb, fog);
    )

    if (bits & GLS_BLOOM_GENERATE)
        GLSL(bloom.rgb *= 1.0 - fog;)

    GLSL(
  })
}

static void write_fragment_shader(sizebuf_t *buf, glStateBits_t bits) {
  write_header(buf, bits);

  if (bits & GLS_UNIFORM_MASK)
    write_block(buf, bits);

  if (bits & GLS_SHADOWMAP) {
    GLSL(in vec3 v_world_pos;)
    if (gl_config.ver_es)
      GLSL(layout(location = 0))
    GLSL(out vec4 o_color;)

    GLSF("void main() {\n");
    GLSL(float dist = length(v_world_pos - u_vieworg.xyz);)
    GLSL(float depth = dist * u_shadow_params.w;)
    GLSL(if (u_shadow_params3.w > 0.5) depth = gl_FragCoord.z;)
    GLSL(int method = int(u_shadow_params2.x + 0.5);)
    GLSL(
        if (method == 3) {)
        GLSL(    float e = max(u_shadow_params3.z, 1.0);)
        GLSL(    float warp = exp(min(e * depth, 80.0));)
        GLSL(    float warp2 = exp(min(2.0 * e * depth, 80.0));)
        GLSL(    o_color = vec4(warp, warp2, 0.0, 1.0);)
        GLSL(
        } else {)
        GLSL(    o_color = vec4(depth, depth * depth, 0.0, 1.0);)
        GLSL(
        })
    GLSF("}\n");
    return;
  }

  if (bits & GLS_DYNAMIC_LIGHTS)
    write_dynamic_light_block(buf);

  if (bits & GLS_CLASSIC_SKY) {
    GLSL(uniform sampler2D u_texture1; uniform sampler2D u_texture2;)
  } else if (bits & GLS_DEFAULT_SKY) {
    GLSL(uniform samplerCube u_texture;)
  } else {
    GLSL(uniform sampler2D u_texture;)
    if (bits & (GLS_BLOOM_OUTPUT | GLS_BLOOM_PREFILTER))
      GLSL(uniform sampler2D u_bloom;)
    if (bits & GLS_REFRACT_ENABLE)
      GLSL(uniform sampler2D u_refract;)
  }

  if (bits & GLS_DOF) {
    GLSL(uniform sampler2D u_blur;)
    GLSL(uniform sampler2D u_depth;)
  }

  if (bits & (GLS_POSTFX | GLS_EXPOSURE_UPDATE))
    GLSL(uniform sampler2D u_exposure;)

  if (bits & GLS_POSTFX)
    GLSL(uniform sampler2D u_lut;)

  if (bits & GLS_SKY_MASK)
    GLSL(in vec3 v_dir;)
  else
    GLSL(in vec2 v_tc;)

  if (bits & GLS_LIGHTMAP_ENABLE) {
    GLSL(uniform sampler2D u_lightmap;)
    GLSL(in vec2 v_lmtc;)
  }

  if (bits & GLS_GLOWMAP_ENABLE)
    GLSL(uniform sampler2D u_glowmap;)

  if (bits & GLS_DYNAMIC_LIGHTS) {
    GLSL(uniform sampler2DArray u_shadowmap;)
    GLSL(uniform sampler2DArray u_shadowmap_csm;)
  }

  if (!(bits & GLS_TEXTURE_REPLACE))
    GLSL(in vec4 v_color;)

  if (gl_config.ver_es)
    GLSL(layout(location = 0))
  GLSL(out vec4 o_color;)

  if (bits & GLS_BLOOM_GENERATE) {
    if (gl_config.ver_es)
      GLSL(layout(location = 1))
    GLSL(out vec4 o_bloom;)
  }

  if (bits & (GLS_FOG_HEIGHT | GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT))
    GLSL(in vec3 v_world_pos;)

  if (bits & (GLS_DYNAMIC_LIGHTS | GLS_RIMLIGHT))
    GLSL(in vec3 v_norm;)

  if (bits & GLS_DYNAMIC_LIGHTS)
    write_dynamic_lights(buf);

  if (bits & GLS_BLUR_GAUSS)
    write_gaussian_blur(buf);
  else if (bits & GLS_BLUR_BOX)
    write_box_blur(buf);

  if (bits & GLS_DOF) {
    GLSL(float linearize_depth(float depth) {
      float z = depth * 2.0 - 1.0;
      return abs(u_dof_params.w / (z + u_dof_params.z));
    })
  }

  if (bits & GLS_CRT) {
    GLSL(
        float crt_to_linear_1(float c) {
          if (u_crt_params.w <= 0.0)
            return c;
          return (c <= 0.04045) ? (c / 12.92) : pow((c + 0.055) / 1.055, 2.4);
        } vec3 crt_to_linear(vec3 c) {
          return vec3(crt_to_linear_1(c.r), crt_to_linear_1(c.g),
                      crt_to_linear_1(c.b));
        } float crt_to_srgb_1(float c) {
          if (u_crt_params.w <= 0.0)
            return c;
          return (c < 0.0031308) ? (c * 12.92)
                                 : (1.055 * pow(c, 0.41666) - 0.055);
        } vec3 crt_to_srgb(vec3 c) {
          return vec3(crt_to_srgb_1(c.r), crt_to_srgb_1(c.g),
                      crt_to_srgb_1(c.b));
        } float crt_gauss(float x, float scale) {
          return exp2(scale * pow(abs(x), 2.0));
        } vec2 crt_dist(vec2 pos) {
          pos = pos * u_crt_texel.zw;
          return -((pos - floor(pos)) - vec2(0.5));
        } vec3 crt_fetch(vec2 pos, vec2 off) {
          vec2 p =
              (floor(pos * u_crt_texel.zw + off) + vec2(0.5)) * u_crt_texel.xy;
          vec3 c = texture(u_texture, p).rgb * u_crt_params.z;
          return crt_to_linear(c);
        } vec3 crt_horz3(vec2 pos, float off, float hard_pix) {
          vec3 b = crt_fetch(pos, vec2(-1.0, off));
          vec3 c = crt_fetch(pos, vec2(0.0, off));
          vec3 d = crt_fetch(pos, vec2(1.0, off));
          float dst = crt_dist(pos).x;
          float wb = crt_gauss(dst - 1.0, hard_pix);
          float wc = crt_gauss(dst + 0.0, hard_pix);
          float wd = crt_gauss(dst + 1.0, hard_pix);
          return (b * wb + c * wc + d * wd) / (wb + wc + wd);
        } vec3 crt_horz5(vec2 pos, float off, float hard_pix) {
          vec3 a = crt_fetch(pos, vec2(-2.0, off));
          vec3 b = crt_fetch(pos, vec2(-1.0, off));
          vec3 c = crt_fetch(pos, vec2(0.0, off));
          vec3 d = crt_fetch(pos, vec2(1.0, off));
          vec3 e = crt_fetch(pos, vec2(2.0, off));
          float dst = crt_dist(pos).x;
          float wa = crt_gauss(dst - 2.0, hard_pix);
          float wb = crt_gauss(dst - 1.0, hard_pix);
          float wc = crt_gauss(dst + 0.0, hard_pix);
          float wd = crt_gauss(dst + 1.0, hard_pix);
          float we = crt_gauss(dst + 2.0, hard_pix);
          return (a * wa + b * wb + c * wc + d * wd + e * we) /
                 (wa + wb + wc + wd + we);
        } float crt_scan(vec2 pos, float off, float hard_scan) {
          float dst = crt_dist(pos).y;
          return crt_gauss(dst + off, hard_scan);
        } vec3 crt_tri(vec2 pos, float hard_pix, float hard_scan) {
          vec3 a = crt_horz3(pos, -1.0, hard_pix);
          vec3 b = crt_horz5(pos, 0.0, hard_pix);
          vec3 c = crt_horz3(pos, 1.0, hard_pix);
          float wa = crt_scan(pos, -1.0, hard_scan);
          float wb = crt_scan(pos, 0.0, hard_scan);
          float wc = crt_scan(pos, 1.0, hard_scan);
          return a * wa + b * wb + c * wc;
        } vec3 crt_mask(vec2 pos) {
          float mask = u_crt_params2.z;
          if (mask < 0.5)
            return vec3(1.0);

          float mask_dark = u_crt_params2.x;
          float mask_light = u_crt_params2.y;
          vec3 out_mask = vec3(mask_dark);

          if (mask < 1.5) {
            float line = mask_light;
            float odd = 0.0;
            if (fract(pos.x * 0.16666666) < 0.5)
              odd = 1.0;
            if (fract((pos.y + odd) * 0.5) < 0.5)
              line = mask_dark;

            pos.x = fract(pos.x * 0.33333333);
            if (pos.x < 0.333)
              out_mask.r = mask_light;
            else if (pos.x < 0.666)
              out_mask.g = mask_light;
            else
              out_mask.b = mask_light;
            out_mask *= line;
          } else if (mask < 2.5) {
            pos.x = fract(pos.x * 0.33333333);
            if (pos.x < 0.333)
              out_mask.r = mask_light;
            else if (pos.x < 0.666)
              out_mask.g = mask_light;
            else
              out_mask.b = mask_light;
          } else if (mask < 3.5) {
            pos.x += pos.y * 3.0;
            pos.x = fract(pos.x * 0.16666666);
            if (pos.x < 0.333)
              out_mask.r = mask_light;
            else if (pos.x < 0.666)
              out_mask.g = mask_light;
            else
              out_mask.b = mask_light;
          } else {
            pos = floor(pos * vec2(1.0, 0.5));
            pos.x += pos.y * 3.0;
            pos.x = fract(pos.x * 0.16666666);
            if (pos.x < 0.333)
              out_mask.r = mask_light;
            else if (pos.x < 0.666)
              out_mask.g = mask_light;
            else
              out_mask.b = mask_light;
          }

          return out_mask;
        } float crt_scanline_mod(float hard_scan) {
          float scan_dark = exp2(hard_scan * 0.25);
          float scale = max(u_crt_params2.w, 1.0);
          float line = mod(floor(gl_FragCoord.y / scale), 2.0);
          return mix(scan_dark, 1.0, line);
        })
  }

  if (bits & (GLS_POSTFX | GLS_BLOOM_PREFILTER | GLS_EXPOSURE_UPDATE)) {
    GLSL(
        float postfx_luma(vec3 color) {
          return dot(color, vec3(0.2126, 0.7152, 0.0722));
        } vec3 postfx_saturate(vec3 color, float saturation) {
          float luma = postfx_luma(color);
          return mix(vec3(luma), color, saturation);
        })
  }

  if (bits & GLS_POSTFX) {
    GLSL(vec3 postfx_tonemap_aces(vec3 x) {
      const float a = 2.51;
      const float b = 0.03;
      const float c = 2.43;
      const float d = 0.59;
      const float e = 0.14;
      return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
    })
  }

  if (bits & GLS_EXPOSURE_UPDATE) {
    GLSF("void main() {\n");
    GLSL(ivec2 tex_size = textureSize(u_texture, 0);
         float max_dim = float(max(tex_size.x, tex_size.y));
         float max_mip = max(floor(log2(max_dim)), 0.0);
         vec3 scene = textureLod(u_texture, vec2(0.5), max_mip).rgb;
         float luma = postfx_luma(scene);
         luma = clamp(luma, u_postfx_auto.y, u_postfx_auto.z);
         float target = u_postfx_hdr.x / max(luma, 1e-4);
         float prev = texture(u_exposure, vec2(0.5)).r;
         float alpha = clamp(u_postfx_auto.w, 0.0, 1.0);
         float exposure = mix(prev, target, alpha);
         o_color = vec4(exposure, exposure, exposure, 1.0);)
    GLSF("}\n");
    return;
  }

  GLSF("void main() {\n");
  if (bits & GLS_REFRACT_ENABLE)
    GLSL(vec2 warp_ofs = vec2(0.0);)
  if (bits & GLS_CLASSIC_SKY) {
    GLSL(float len = length(v_dir); vec2 dir = v_dir.xy * (3.0 / len);
         vec2 tc1 = dir + vec2(u_time * 0.0625);
         vec2 tc2 = dir + vec2(u_time * 0.1250);
         vec4 solid = texture(u_texture1, tc1);
         vec4 alpha = texture(u_texture2, tc2);
         vec4 diffuse = vec4((solid.rgb - alpha.rgb * 0.25) * 0.65, 1.0);)
  } else if (bits & GLS_DEFAULT_SKY) {
    GLSL(vec4 diffuse = texture(u_texture, v_dir);)
  } else {
    GLSL(vec2 tc = v_tc;)
    if (bits & GLS_REFRACT_ENABLE)
      GLSL(vec2 tc_base = tc;)

    if (bits & GLS_WARP_ENABLE) {
      if (bits & GLS_REFRACT_ENABLE) {
        GLSL(warp_ofs = w_amp * sin(tc.ts * w_phase + u_time);)
        GLSL(tc += warp_ofs;)
      } else {
        GLSL(tc += w_amp * sin(tc.ts * w_phase + u_time);)
      }
    }

    if (bits & GLS_CRT) {
      GLSL(float hard_pix = u_crt_params.x; float hard_scan = u_crt_params.y;
           vec3 color = crt_tri(tc, hard_pix, hard_scan);
           color *= crt_scanline_mod(hard_scan);

           if (u_crt_params2.z > 0.0) color *=
           crt_mask(gl_FragCoord.xy * 1.000001);

           color = min(color, vec3(1.0)); color = crt_to_srgb(color);
           vec4 diffuse = vec4(color, 1.0);)
    } else if (bits & GLS_DOF) {
      GLSL(vec4 scene = texture(u_texture, tc);
           vec4 blurred = texture(u_blur, tc);
           float depth_sample = texture(u_depth, tc).r;
           float focus_dist = u_dof_params.x; if (focus_dist <= 0.0) {
             float focus_depth = texture(u_depth, vec2(0.5, 0.5)).r;
             focus_dist = linearize_depth(focus_depth);
           } float blur_range = u_dof_params.y;
           if (blur_range <= 0.0) blur_range = max(64.0, focus_dist * 0.25);
           float dist = linearize_depth(depth_sample);
           float blur_factor =
               clamp(abs(dist - focus_dist) / blur_range, 0.0, 1.0);
           blur_factor = smoothstep(0.0, 1.0, blur_factor);
           blur_factor *= clamp(u_vieworg.w, 0.0, 1.0);
           vec4 diffuse = mix(scene, blurred, blur_factor);)
    } else if (bits & GLS_BLOOM_PREFILTER) {
      GLSL(
          vec2 texel = u_fog_color.xy; vec2 o = vec2(0.25) * texel;
          vec3 scene = vec3(0.0);
          scene += texture(u_texture, tc + vec2(-o.x, -o.y)).rgb;
          scene += texture(u_texture, tc + vec2(-o.x, o.y)).rgb;
          scene += texture(u_texture, tc + vec2(o.x, -o.y)).rgb;
          scene += texture(u_texture, tc + vec2(o.x, o.y)).rgb; scene *= 0.25;

          vec3 glow = vec3(0.0); if (u_postfx_bloom2.y > 0.5) {
            glow += texture(u_bloom, tc + vec2(-o.x, -o.y)).rgb;
            glow += texture(u_bloom, tc + vec2(-o.x, o.y)).rgb;
            glow += texture(u_bloom, tc + vec2(o.x, -o.y)).rgb;
            glow += texture(u_bloom, tc + vec2(o.x, o.y)).rgb;
            glow *= 0.25;
          }

                                 float luma = postfx_luma(scene);
          float firefly = u_postfx_bloom2.z;
          if (firefly > 0.0 && luma > firefly) {
            scene *= firefly / max(luma, 1e-5);
            luma = firefly;
          } float threshold = max(u_postfx_bloom.x, 0.0);
          float knee = threshold * u_postfx_bloom.y + 1e-5;
          float soft = clamp(luma - threshold + knee, 0.0, 2.0 * knee);
          soft = (soft * soft) / (4.0 * knee + 1e-5);
          float contribution = max(luma - threshold, 0.0) + soft;
          vec3 bright = scene * (contribution / max(luma, 1e-5));
          vec3 prefilter = bright + glow; vec4 diffuse = vec4(prefilter, 1.0);)
    } else if (bits & GLS_BLUR_MASK)
      GLSL(vec4 diffuse = blur(u_texture, tc, u_fog_color.xy);)
    else
      GLSL(vec4 diffuse = texture(u_texture, tc);)
  }

  if (bits & GLS_ALPHATEST_ENABLE)
    GLSL(if (diffuse.a <= 0.666) discard;)

  if (!(bits & GLS_TEXTURE_REPLACE))
    GLSL(vec4 color = v_color;)

  if (bits & GLS_BLOOM_GENERATE)
    GLSL(vec4 bloom = vec4(0.0);)

  if (bits & GLS_LIGHTMAP_ENABLE) {
    GLSL(vec4 lightmap = texture(u_lightmap, v_lmtc);)

    if (bits & GLS_GLOWMAP_ENABLE) {
      GLSL(vec4 glowmap = texture(u_glowmap, tc);)

      if (bits & GLS_INTENSITY_ENABLE)
        GLSL(glowmap.a *= u_intensity2;)

      GLSL(lightmap.rgb = mix(lightmap.rgb, vec3(1.0), glowmap.a);)

      if (bits & GLS_BLOOM_GENERATE) {
        GLSL(bloom.rgb = diffuse.rgb * glowmap.a;)
      }
    }

    if (bits & GLS_DYNAMIC_LIGHTS) {
      GLSL(lightmap.rgb += calc_dynamic_lights();)
    }

    GLSL(diffuse.rgb *= (lightmap.rgb + u_add) * u_modulate;)
  } else if ((bits & GLS_DYNAMIC_LIGHTS) && !(bits & GLS_TEXTURE_REPLACE)) {
    GLSL(color.rgb += calc_dynamic_lights() * u_modulate;)
  }

  if (bits & GLS_INTENSITY_ENABLE)
    GLSL(diffuse.rgb *= u_intensity;)

  if (bits & GLS_DEFAULT_FLARE)
    GLSL(diffuse.rgb *= (diffuse.r + diffuse.g + diffuse.b) / 3.0;
         diffuse.rgb *= v_color.a;)

  if (!(bits & GLS_TEXTURE_REPLACE))
    GLSL(diffuse *= color;)

  if (!(bits & GLS_LIGHTMAP_ENABLE) && (bits & GLS_GLOWMAP_ENABLE)) {
    GLSL(vec4 glowmap = texture(u_glowmap, tc);)
    if (bits & GLS_INTENSITY_ENABLE)
      GLSL(diffuse.rgb += glowmap.rgb * u_intensity2;)
    else
      GLSL(diffuse.rgb += glowmap.rgb;)

    if (bits & GLS_BLOOM_GENERATE) {
      GLSL(bloom.rgb = glowmap.rgb;)
      if (bits & GLS_INTENSITY_ENABLE)
        GLSL(bloom.rgb *= u_intensity2;)
    }
  }

  if (bits & GLS_RIMLIGHT) {
    GLSL(vec3 view_dir = normalize(u_vieworg.xyz - v_world_pos);
         float rim = 1.0 - max(dot(normalize(v_norm), view_dir), 0.0);
         rim = pow(rim, 2.0);
         diffuse = vec4(v_color.rgb * rim, v_color.a * rim);)
  }

  if (bits & GLS_BLOOM_GENERATE) {
    if (bits & GLS_BLOOM_SHELL)
      GLSL(bloom = diffuse;)
    else
      GLSL(bloom.a = diffuse.a;)
  }

  if (bits & (GLS_FOG_GLOBAL | GLS_FOG_HEIGHT))
    GLSL(float frag_depth = gl_FragCoord.z / gl_FragCoord.w;)

  if (bits & GLS_FOG_GLOBAL) {
    GLSL({
      float d = u_fog_color.a * frag_depth;
      float fog = 1.0 - exp(-(d * d));
      diffuse.rgb = mix(diffuse.rgb, u_fog_color.rgb, fog);
        )

        if (bits & GLS_BLOOM_GENERATE)
            GLSL(bloom.rgb *= 1.0 - fog;)

        GLSL(
    })
  }

  if (bits & GLS_FOG_HEIGHT)
    write_height_fog(buf, bits);

  if (bits & GLS_FOG_SKY)
    GLSL(diffuse.rgb = mix(diffuse.rgb, u_fog_color.rgb, u_fog_sky_factor);)
if (bits & GLS_REFRACT_ENABLE) {
  GLSL(if (u_refract_scale > 0.0) {
    vec2 base_tc = gl_FragCoord.xy * u_crt_texel.xy;
    vec2 refr_tc = base_tc;
    vec2 warp_tc = warp_ofs * u_refract_scale;

    vec2 dxtc = dFdx(tc_base);
    vec2 dytc = dFdy(tc_base);
    float det = dxtc.x * dytc.y - dxtc.y * dytc.x;
    if (abs(det) > 1e-6) {
      vec2 pix_ofs = vec2((dytc.y * warp_tc.x - dxtc.y * warp_tc.y),
                          (-dytc.x * warp_tc.x + dxtc.x * warp_tc.y)) /
                     det;
      refr_tc = base_tc + pix_ofs * u_crt_texel.xy;
    } else {
      refr_tc = base_tc + warp_tc;
    }

    vec3 base = texture(u_refract, base_tc).rgb;
    vec3 refr = texture(u_refract, refr_tc).rgb;
    float alpha = clamp(diffuse.a, 0.0, 1.0);
    vec3 desired = mix(refr, diffuse.rgb, alpha);
    if (alpha > 0.0001) {
      vec3 adjusted = (desired - base * (1.0 - alpha)) / alpha;
      diffuse.rgb = clamp(adjusted, 0.0, 1.0);
    } else {
      diffuse.rgb = desired;
    }
  })
}

if (bits & GLS_BLOOM_OUTPUT) {
  GLSL(vec3 bloom_sample = texture(u_bloom, tc).rgb;)
  GLSL(if (u_postfx_bloom2.w > 1.0) {
    vec3 accum = vec3(0.0);
    float weight = 1.0;
    float weight_sum = 0.0;
    for (int i = 0; i < 6; i++) {
      if (float(i) >= u_postfx_bloom2.w)
        break;
      accum += textureLod(u_bloom, tc, float(i)).rgb * weight;
      weight_sum += weight;
      weight *= 0.5;
    }
    bloom_sample = accum / max(weight_sum, 1e-5);
  })
  if (bits & GLS_POSTFX) {
    GLSL(diffuse.rgb = postfx_saturate(diffuse.rgb, u_postfx_bloom.w);
         bloom_sample = postfx_saturate(bloom_sample, u_postfx_bloom2.x);
         diffuse.rgb += bloom_sample * u_postfx_bloom.z;)
  } else {
    GLSL(diffuse.rgb += bloom_sample;)
  }
}

if (bits & GLS_POSTFX) {
  GLSL(vec3 postfx_color = diffuse.rgb;)
  if (!(bits & GLS_BLOOM_OUTPUT)) {
    GLSL(postfx_color = postfx_saturate(postfx_color, u_postfx_bloom.w);)
  }
  GLSL(
      if (u_postfx_hdr.w > 0.5) {
        if (u_postfx_hdr.z > 1.0)
          postfx_color =
              pow(max(postfx_color, vec3(0.0)), vec3(u_postfx_hdr.z));
        float exposure = u_postfx_hdr.x;
        if (u_postfx_auto.x > 0.5)
          exposure = texture(u_exposure, vec2(0.5)).r;
        postfx_color *= exposure;
        vec3 mapped = postfx_tonemap_aces(postfx_color);
        vec3 white = postfx_tonemap_aces(vec3(max(u_postfx_hdr.y, 1e-4)));
        postfx_color = mapped / max(white, vec3(1e-4));
        if (u_postfx_hdr.z > 1.0)
          postfx_color =
              pow(max(postfx_color, vec3(0.0)), vec3(1.0 / u_postfx_hdr.z));
      } if (u_postfx_color.w > 0.5) {
        postfx_color =
            (postfx_color - vec3(0.5)) * u_postfx_color.y + vec3(0.5);
        postfx_color += u_postfx_color.x;
        postfx_color = postfx_saturate(postfx_color, u_postfx_color.z);
        postfx_color *= u_postfx_tint.rgb;
      } if (u_postfx_split_params.x > 0.0) {
        float luma = postfx_luma(postfx_color);
        float balance = clamp(u_postfx_split_params.y, -1.0, 1.0);
        float pivot = 0.5 + balance * 0.5;
        float weight = smoothstep(pivot - 0.25, pivot + 0.25, luma);
        vec3 toned = mix(postfx_color * u_postfx_split_shadow.rgb,
                         postfx_color * u_postfx_split_highlight.rgb, weight);
        postfx_color = mix(postfx_color, toned, u_postfx_split_params.x);
      } if (u_postfx_lut.x > 0.0 && u_postfx_lut.y > 1.0) {
        vec3 lut_color = clamp(postfx_color, 0.0, 1.0);
        float size = u_postfx_lut.y;
        float slice = lut_color.b * (size - 1.0);
        float slice0 = floor(slice);
        float slice1 = min(slice0 + 1.0, size - 1.0);
        float t = slice - slice0;
        float u = lut_color.r * (size - 1.0) + 0.5;
        float v = lut_color.g * (size - 1.0) + 0.5;
        vec2 uv0;
        vec2 uv1;
        if (u_postfx_lut.z < u_postfx_lut.w) {
          uv0 = vec2((slice0 * size + u) * u_postfx_lut.z, v * u_postfx_lut.w);
          uv1 = vec2((slice1 * size + u) * u_postfx_lut.z, v * u_postfx_lut.w);
        } else {
          uv0 = vec2(u * u_postfx_lut.z, (slice0 * size + v) * u_postfx_lut.w);
          uv1 = vec2(u * u_postfx_lut.z, (slice1 * size + v) * u_postfx_lut.w);
        }
        vec3 graded = mix(texture(u_lut, uv0).rgb, texture(u_lut, uv1).rgb, t);
        postfx_color = mix(postfx_color, graded, u_postfx_lut.x);
      } diffuse.rgb = clamp(postfx_color, 0.0, 1.0);)
}

if (bits & GLS_BLOOM_GENERATE)
  GLSL(o_bloom = bloom;)

GLSL(o_color = diffuse;)
GLSF("}\n");
}

static GLuint create_shader(GLenum type, const sizebuf_t *buf) {
  const GLchar *data = (const GLchar *)buf->data;
  GLint size = buf->cursize;

  GLuint shader = qglCreateShader(type);
  if (!shader) {
    Com_EPrintf("Couldn't create shader\n");
    return 0;
  }

  Com_DDDPrintf("Compiling %s shader (%d bytes):\n%.*s\n",
                type == GL_VERTEX_SHADER ? "vertex" : "fragment", size, size,
                data);

  qglShaderSource(shader, 1, &data, &size);
  qglCompileShader(shader);
  GLint status = 0;
  qglGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (!status) {
    char buffer[MAX_STRING_CHARS];

    buffer[0] = 0;
    qglGetShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
    qglDeleteShader(shader);

    if (buffer[0])
      Com_Printf("%s", buffer);

    Com_EPrintf("Error compiling %s shader\n",
                type == GL_VERTEX_SHADER ? "vertex" : "fragment");
    return 0;
  }

  return shader;
}

static bool bind_uniform_block(GLuint program, const char *name,
                               size_t cpu_size, GLuint binding) {
  GLuint index = qglGetUniformBlockIndex(program, name);
  if (index == GL_INVALID_INDEX) {
    Com_EPrintf("%s block not found\n", name);
    return false;
  }

  GLint gpu_size = 0;
  qglGetActiveUniformBlockiv(program, index, GL_UNIFORM_BLOCK_DATA_SIZE,
                             &gpu_size);
  if (gpu_size != cpu_size) {
    Com_EPrintf("%s block size mismatch: %d != %zu\n", name, gpu_size,
                cpu_size);
    return false;
  }

  qglUniformBlockBinding(program, index, binding);
  return true;
}

static void bind_texture_unit(GLuint program, const char *name, GLuint tmu) {
  GLint loc = qglGetUniformLocation(program, name);
  if (loc == -1) {
    Com_EPrintf("Texture %s not found\n", name);
    return;
  }
  qglUniform1i(loc, tmu);
}

static GLuint create_and_use_program(glStateBits_t bits) {
  char buffer[MAX_SHADER_CHARS];
  sizebuf_t sb;

  GLuint program = qglCreateProgram();
  if (!program) {
    Com_EPrintf("Couldn't create program\n");
    return 0;
  }

  SZ_Init(&sb, buffer, sizeof(buffer), "GLSL");
  write_vertex_shader(&sb, bits);
  GLuint shader_v = create_shader(GL_VERTEX_SHADER, &sb);
  if (!shader_v)
    goto fail;

  SZ_Clear(&sb);
  write_fragment_shader(&sb, bits);
  GLuint shader_f = create_shader(GL_FRAGMENT_SHADER, &sb);
  if (!shader_f) {
    qglDeleteShader(shader_v);
    goto fail;
  }

  qglAttachShader(program, shader_v);
  qglAttachShader(program, shader_f);

#if USE_MD5
  if (bits & GLS_MESH_MD5) {
    qglBindAttribLocation(program, VERT_ATTR_MESH_TC, "a_tc");
    qglBindAttribLocation(program, VERT_ATTR_MESH_NORM, "a_norm");
    qglBindAttribLocation(program, VERT_ATTR_MESH_VERT, "a_vert");
  } else
#endif
      if (bits & GLS_MESH_MD2) {
    qglBindAttribLocation(program, VERT_ATTR_MESH_TC, "a_tc");
    if (bits & GLS_MESH_LERP)
      qglBindAttribLocation(program, VERT_ATTR_MESH_OLD_POS, "a_old_pos");
    qglBindAttribLocation(program, VERT_ATTR_MESH_NEW_POS, "a_new_pos");
  } else {
    qglBindAttribLocation(program, VERT_ATTR_POS, "a_pos");
    if (!(bits & GLS_SKY_MASK))
      qglBindAttribLocation(program, VERT_ATTR_TC, "a_tc");
    if (bits & GLS_LIGHTMAP_ENABLE)
      qglBindAttribLocation(program, VERT_ATTR_LMTC, "a_lmtc");
    if (!(bits & GLS_TEXTURE_REPLACE))
      qglBindAttribLocation(program, VERT_ATTR_COLOR, "a_color");
    if (bits & GLS_DYNAMIC_LIGHTS)
      qglBindAttribLocation(program, VERT_ATTR_NORMAL, "a_norm");
  }

  if (bits & GLS_BLOOM_GENERATE && !gl_config.ver_es) {
    qglBindFragDataLocation(program, 0, "o_color");
    qglBindFragDataLocation(program, 1, "o_bloom");
  }

  qglLinkProgram(program);

  qglDeleteShader(shader_v);
  qglDeleteShader(shader_f);

  GLint status = 0;
  qglGetProgramiv(program, GL_LINK_STATUS, &status);
  if (!status) {
    char buffer[MAX_STRING_CHARS];

    buffer[0] = 0;
    qglGetProgramInfoLog(program, sizeof(buffer), NULL, buffer);

    if (buffer[0])
      Com_Printf("%s", buffer);

    Com_EPrintf("Error linking program\n");
    goto fail;
  }

  if (!bind_uniform_block(program, "Uniforms", sizeof(gls.u_block),
                          UBO_UNIFORMS))
    goto fail;

#if USE_MD5
  if (bits & GLS_MESH_MD5)
    if (!bind_uniform_block(program, "Skeleton",
                            sizeof(glJoint_t) * MD5_MAX_JOINTS, UBO_SKELETON))
      goto fail;
#endif

  if (bits & GLS_DYNAMIC_LIGHTS) {
    if (!bind_uniform_block(program, "DynamicLights", sizeof(gls.u_dlights),
                            UBO_DLIGHTS))
      goto fail;
  }

  qglUseProgram(program);

#if USE_MD5
  if (bits & GLS_MESH_MD5 && !(gl_config.caps & QGL_CAP_SHADER_STORAGE)) {
    bind_texture_unit(program, "u_weights", TMU_SKEL_WEIGHTS);
    bind_texture_unit(program, "u_jointnums", TMU_SKEL_JOINTNUMS);
  }
#endif

  if (!(bits & GLS_SHADOWMAP)) {
    if (bits & GLS_CLASSIC_SKY) {
      bind_texture_unit(program, "u_texture1", TMU_TEXTURE);
      bind_texture_unit(program, "u_texture2", TMU_LIGHTMAP);
    } else {
      bind_texture_unit(program, "u_texture", TMU_TEXTURE);
      if (bits & (GLS_BLOOM_OUTPUT | GLS_BLOOM_PREFILTER))
        bind_texture_unit(program, "u_bloom", TMU_LIGHTMAP);
    }

    if (bits & GLS_DOF) {
      bind_texture_unit(program, "u_blur", TMU_LIGHTMAP);
      bind_texture_unit(program, "u_depth", TMU_GLOWMAP);
    }

    if (bits & GLS_LIGHTMAP_ENABLE)
      bind_texture_unit(program, "u_lightmap", TMU_LIGHTMAP);

    if (bits & GLS_GLOWMAP_ENABLE)
      bind_texture_unit(program, "u_glowmap", TMU_GLOWMAP);

    if (bits & GLS_REFRACT_ENABLE)
      bind_texture_unit(program, "u_refract", TMU_REFRACT);

    if (bits & (GLS_POSTFX | GLS_EXPOSURE_UPDATE))
      bind_texture_unit(program, "u_exposure", TMU_EXPOSURE);

    if (bits & GLS_POSTFX)
      bind_texture_unit(program, "u_lut", TMU_LUT);

    if (bits & GLS_DYNAMIC_LIGHTS) {
      bind_texture_unit(program, "u_shadowmap", TMU_SHADOWMAP);
      bind_texture_unit(program, "u_shadowmap_csm", TMU_SHADOWMAP_CSM);
    }
  }

  return program;

fail:
  qglDeleteProgram(program);
  return 0;
}

static void shader_use_program(glStateBits_t key) {
  GLuint *prog = HashMap_Lookup(GLuint, gl_static.programs, &key);

  if (prog && *prog) {
    qglUseProgram(*prog);
  } else {
    GLuint val = create_and_use_program(key);
    if (prog)
      *prog = val;
    else
      HashMap_Insert(gl_static.programs, &key, &val);
  }
}

static void shader_state_bits(glStateBits_t bits) {
  glStateBits_t diff = bits ^ gls.state_bits;

  if (diff & GLS_COMMON_MASK)
    GL_CommonStateBits(bits);

  if (diff & GLS_SHADER_MASK)
    shader_use_program(bits & GLS_SHADER_MASK);

  if (diff & GLS_SCROLL_MASK && bits & GLS_SCROLL_ENABLE) {
    GL_ScrollPos(gls.u_block.scroll, bits);
    gls.u_block_dirty = true;
  }

  if (diff & GLS_BLOOM_GENERATE && glr.framebuffer_bound) {
    int n = (bits & GLS_BLOOM_GENERATE) ? 2 : 1;
    qglDrawBuffers(
        n, (const GLenum[]){GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1});
  }
}

static void shader_array_bits(glArrayBits_t bits) {
  glArrayBits_t diff = bits ^ gls.array_bits;

  for (int i = 0; i < VERT_ATTR_COUNT; i++) {
    if (!(diff & BIT(i)))
      continue;
    if (bits & BIT(i))
      qglEnableVertexAttribArray(i);
    else
      qglDisableVertexAttribArray(i);
  }
}

static void shader_array_pointers(const glVaDesc_t *desc, const GLfloat *ptr) {
  uintptr_t base = (uintptr_t)ptr;

  for (int i = 0; i < VERT_ATTR_COUNT; i++) {
    const glVaDesc_t *d = &desc[i];
    if (d->size) {
      const GLenum type = d->type ? GL_UNSIGNED_BYTE : GL_FLOAT;
      qglVertexAttribPointer(i, d->size, type, d->type, d->stride,
                             (void *)(base + d->offset));
    }
  }
}

static void shader_tex_coord_pointer(const GLfloat *ptr) {
  qglVertexAttribPointer(VERT_ATTR_TC, 2, GL_FLOAT, GL_FALSE, 0, ptr);
}

static void shader_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
  qglVertexAttrib4f(VERT_ATTR_COLOR, r, g, b, a);
}

static void shader_load_uniforms(void) {
  GL_BindBuffer(GL_UNIFORM_BUFFER, gl_static.uniform_buffer);
  qglBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(gls.u_block), &gls.u_block);
  c.uniformUploads++;
}

static void shader_load_lights(void) {
  // dlight bits changed, set up the buffer.
  // if you didn't modify dlights just leave the bits
  // # alone or set to 0.
  if (!glr.ppl_dlight_bits)
    return;

  int i = 0;
  int nl = min(q_countof(gls.u_dlights.lights), glr.fd.num_dlights);

  c.dlightsTotal += nl;

  for (int n = 0; n < nl; n++) {
    if (!(glr.ppl_dlight_bits & 1 << n)) {
      c.dlightsNotUsed++;
      continue;
    }

    const dlight_t *dl = &glr.fd.dlights[n];

    float cull_radius = GL_DlightInfluenceRadius(dl);
    if (cull_radius <= 0.0f ||
        GL_CullSphere(dl->origin, cull_radius) == CULL_OUT) {
      c.dlightsCulled++;
      continue;
    }

    VectorCopy(dl->origin, gls.u_dlights.lights[i].position);
    gls.u_dlights.lights[i].radius = dl->radius;
    VectorCopy(dl->color, gls.u_dlights.lights[i].color);
    gls.u_dlights.lights[i].color[3] = dl->intensity;
    if (dl->conecos) {
      VectorCopy(dl->cone, gls.u_dlights.lights[i].cone);
    }
    gls.u_dlights.lights[i].cone[3] = dl->conecos;
    gls.u_dlights.lights[i].shadow[0] = (float)glr.shadowmap_index[n];
    gls.u_dlights.lights[i].shadow[1] = glr.shadowmap_is_spot[n] ? 1.0f : 0.0f;
    gls.u_dlights.lights[i].shadow[2] =
        glr.shadowmap_pcss[n] ? 1.0f : 0.0f;
    gls.u_dlights.lights[i].shadow[3] = 0.0f;

    i++;
  }

  gls.u_dlights.num_dlights = i;
  c.dlightsUsed += i;

  GL_BindBuffer(GL_UNIFORM_BUFFER, gl_static.dlight_buffer);
  qglBufferSubData(GL_UNIFORM_BUFFER, 0,
                   sizeof(GLint[4]) + (sizeof(gls.u_dlights.lights[0]) *
                                       gls.u_dlights.num_dlights),
                   &gls.u_dlights);
  c.uniformUploads++;
  c.dlightUploads++;
}

static void shader_load_matrix(GLenum mode, const GLfloat *matrix,
                               const GLfloat *view) {
  switch (mode) {
  case GL_MODELVIEW:
    memcpy(gls.u_block.m_model, matrix, sizeof(gls.u_block.m_model));
    memcpy(gls.u_block.m_view, view, sizeof(gls.u_block.m_view));
    break;
  case GL_PROJECTION:
    memcpy(gls.u_block.m_proj, matrix, sizeof(gls.u_block.m_proj));
    break;
  default:
    Q_assert(!"bad mode");
  }

  gls.u_block_dirty = true;
}

static void shader_setup_2d(void) {
  gls.u_block.time = glr.fd.time;
  gls.u_block.modulate = 1.0f;
  gls.u_block.add = 0.0f;
  gls.u_block.intensity = 1.0f;
  gls.u_block.intensity2 = 1.0f;
  gls.u_block.refract_scale = 0.0f;
  gls.u_block.refract_pad = 0.0f;

  gls.u_block.w_amp[0] = 0.0025f;
  gls.u_block.w_amp[1] = 0.0025f;
  gls.u_block.w_phase[0] = M_PIf * 10;
  gls.u_block.w_phase[1] = M_PIf * 10;
}

static void shader_setup_fog(void) {
  if (!(glr.fog_bits | glr.fog_bits_sky))
    return;

  VectorCopy(glr.fd.fog.color, gls.u_block.fog_color);
  gls.u_block.fog_color[3] = glr.fd.fog.density / 64;
  gls.u_block.fog_sky_factor = glr.fd.fog.sky_factor;

  VectorCopy(glr.fd.heightfog.start.color, gls.u_block.heightfog_start);
  gls.u_block.heightfog_start[3] = glr.fd.heightfog.start.dist;

  VectorCopy(glr.fd.heightfog.end.color, gls.u_block.heightfog_end);
  gls.u_block.heightfog_end[3] = glr.fd.heightfog.end.dist;

  gls.u_block.heightfog_density = glr.fd.heightfog.density;
  gls.u_block.heightfog_falloff = glr.fd.heightfog.falloff;
}

static void shader_setup_3d(void) {
  gls.u_block.time = glr.fd.time;
  gls.u_block.modulate =
      gl_modulate->value * gl_modulate_world->value * gl_static.identity_light;
  gls.u_block.add = gl_brightness->value;
  gls.u_block.intensity = gl_intensity->value;
  gls.u_block.intensity2 = gl_intensity->value * gl_glowmap_intensity->value;
  gls.u_block.refract_scale = Cvar_ClampValue(gl_warp_refraction, 0.0f, 2.0f);
  gls.u_block.refract_pad = 0.0f;

  gls.u_block.w_amp[0] = 0.0625f;
  gls.u_block.w_amp[1] = 0.0625f;
  gls.u_block.w_phase[0] = 4;
  gls.u_block.w_phase[1] = 4;

  gls.dlight_bits = 0;

  int refract_w = glr.framebuffer_bound ? glr.render_width : r_config.width;
  int refract_h = glr.framebuffer_bound ? glr.render_height : r_config.height;
  gls.u_block.crt_texel[0] = refract_w > 0 ? (1.0f / (float)refract_w) : 0.0f;
  gls.u_block.crt_texel[1] = refract_h > 0 ? (1.0f / (float)refract_h) : 0.0f;
  gls.u_block.crt_texel[2] = (float)refract_w;
  gls.u_block.crt_texel[3] = (float)refract_h;

  shader_setup_fog();

  R_RotateForSky();

  // setup default matrices for world
  memcpy(gls.u_block.m_sky, glr.skymatrix, sizeof(gls.u_block.m_sky));
  memcpy(gls.u_block.m_model, gl_identity, sizeof(gls.u_block.m_model));

  VectorCopy(glr.fd.vieworg, gls.u_block.vieworg);
  {
    vec3_t viewaxis[3];
    AnglesToAxis(glr.fd.viewangles, viewaxis);
    VectorCopy(viewaxis[0], gls.u_block.viewdir);
    gls.u_block.viewdir[3] = 0.0f;
  }
  if (gl_shadowmaps->integer && gl_static.shadowmap_ok &&
      gl_static.shadowmap_size > 0) {
    gls.u_block.shadow_params[0] = 1.0f / (float)gl_static.shadowmap_size;
    gls.u_block.shadow_params[1] = gl_shadowmap_bias->value;
    gls.u_block.shadow_params[2] = gl_shadowmap_softness->value;
  } else {
    gls.u_block.shadow_params[0] = 0.0f;
    gls.u_block.shadow_params[1] = 0.0f;
    gls.u_block.shadow_params[2] = 0.0f;
  }
  gls.u_block.shadow_params[3] = 0.0f;

  if (gl_shadowmaps->integer && gl_static.shadowmap_ok &&
      gl_static.shadowmap_size > 0) {
    int method = (int)Cvar_ClampValue(gl_shadowmap_filter, 0, 4);
    int quality = (int)Cvar_ClampValue(gl_shadowmap_quality, 0, 3);
    float vsm_bleed = 0.2f;
    float vsm_min_var = 0.001f;

    switch (quality) {
    case 0:
      vsm_bleed = 0.3f;
      vsm_min_var = 0.005f;
      break;
    case 1:
      vsm_bleed = 0.2f;
      vsm_min_var = 0.001f;
      break;
    case 2:
      vsm_bleed = 0.1f;
      vsm_min_var = 0.0005f;
      break;
    default:
      vsm_bleed = 0.05f;
      vsm_min_var = 0.0002f;
      break;
    }

    if (gl_shadow_vsm_bleed && gl_shadow_vsm_bleed->value >= 0.0f)
      vsm_bleed = gl_shadow_vsm_bleed->value;
    if (gl_shadow_vsm_min_variance && gl_shadow_vsm_min_variance->value >= 0.0f)
      vsm_min_var = gl_shadow_vsm_min_variance->value;

    gls.u_block.shadow_params2[0] = (float)method;
    gls.u_block.shadow_params2[1] = (float)quality;
    gls.u_block.shadow_params2[2] = vsm_bleed;
    gls.u_block.shadow_params2[3] = vsm_min_var;

    gls.u_block.shadow_params3[0] =
        gl_shadow_bias_slope ? gl_shadow_bias_slope->value : 0.0f;
    gls.u_block.shadow_params3[1] =
        gl_shadow_normal_offset ? gl_shadow_normal_offset->value : 0.0f;
    gls.u_block.shadow_params3[2] =
        gl_shadow_evsm_exponent ? gl_shadow_evsm_exponent->value : 0.0f;
    gls.u_block.shadow_params3[3] = 0.0f;

    gls.u_block.shadow_params4[0] = gl_shadow_pcss_blocker_samples
                                        ? gl_shadow_pcss_blocker_samples->value
                                        : 8.0f;
    gls.u_block.shadow_params4[1] = gl_shadow_pcss_filter_samples
                                        ? gl_shadow_pcss_filter_samples->value
                                        : 16.0f;
    gls.u_block.shadow_params4[2] =
        gl_shadow_bias_scale ? gl_shadow_bias_scale->value : 1.0f;
    gls.u_block.shadow_params4[3] =
        gl_shadow_debug ? gl_shadow_debug->value : 0.0f;
  } else {
    gls.u_block.shadow_params2[0] = 0.0f;
    gls.u_block.shadow_params2[1] = 0.0f;
    gls.u_block.shadow_params2[2] = 0.0f;
    gls.u_block.shadow_params2[3] = 0.0f;

    gls.u_block.shadow_params3[0] = 0.0f;
    gls.u_block.shadow_params3[1] = 0.0f;
    gls.u_block.shadow_params3[2] = 0.0f;
    gls.u_block.shadow_params3[3] = 0.0f;

    gls.u_block.shadow_params4[0] = 0.0f;
    gls.u_block.shadow_params4[1] = 0.0f;
    gls.u_block.shadow_params4[2] = 0.0f;
    gls.u_block.shadow_params4[3] = 0.0f;
  }

  if (gl_shadowmaps->integer && gl_static.sun_shadowmap_ok &&
      glr.csm_cascades > 0) {
    VectorCopy(glr.sun_dir, gls.u_block.sun_dir);
    gls.u_block.sun_dir[3] = 1.0f;
    VectorCopy(glr.sun_color, gls.u_block.sun_color);
    gls.u_block.sun_color[3] = glr.sun_intensity;
    Vector4Copy(glr.csm_splits, gls.u_block.csm_splits);
    gls.u_block.csm_params[0] = gl_static.sun_shadowmap_size > 0
                                    ? 1.0f / (float)gl_static.sun_shadowmap_size
                                    : 0.0f;
    gls.u_block.csm_params[1] = (float)glr.csm_cascades;
    gls.u_block.csm_params[2] = gl_csm_blend ? gl_csm_blend->value : 0.0f;
    gls.u_block.csm_params[3] = 0.0f;
    memcpy(gls.u_block.csm_matrix, glr.csm_matrix,
           sizeof(gls.u_block.csm_matrix));
  } else {
    Vector4Clear(gls.u_block.sun_dir);
    Vector4Clear(gls.u_block.sun_color);
    Vector4Clear(gls.u_block.csm_splits);
    Vector4Clear(gls.u_block.csm_params);
    memset(gls.u_block.csm_matrix, 0, sizeof(gls.u_block.csm_matrix));
  }

  if (gl_shadowmaps->integer && gl_static.shadowmap_ok &&
      gl_static.shadowmap_tex)
    GL_BindTexture(TMU_SHADOWMAP, gl_static.shadowmap_tex);
  else if (gls.texnums[TMU_SHADOWMAP])
    GL_BindTexture(TMU_SHADOWMAP, 0);

  if (gl_shadowmaps->integer && gl_static.sun_shadowmap_ok &&
      gl_static.sun_shadowmap_tex && glr.csm_cascades > 0)
    GL_BindTexture(TMU_SHADOWMAP_CSM, gl_static.sun_shadowmap_tex);
  else if (gls.texnums[TMU_SHADOWMAP_CSM])
    GL_BindTexture(TMU_SHADOWMAP_CSM, 0);

  gls.u_block_dirty = true;
}

static void shader_disable_state(void) {
  qglActiveTexture(GL_TEXTURE0 + TMU_LUT);
  qglBindTexture(GL_TEXTURE_2D, 0);

  qglActiveTexture(GL_TEXTURE0 + TMU_EXPOSURE);
  qglBindTexture(GL_TEXTURE_2D, 0);

  qglActiveTexture(GL_TEXTURE0 + TMU_SHADOWMAP);
  qglBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  qglActiveTexture(GL_TEXTURE0 + TMU_SHADOWMAP_CSM);
  qglBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  qglActiveTexture(GL_TEXTURE2);
  qglBindTexture(GL_TEXTURE_2D, 0);

  qglActiveTexture(GL_TEXTURE1);
  qglBindTexture(GL_TEXTURE_2D, 0);

  qglActiveTexture(GL_TEXTURE0);
  qglBindTexture(GL_TEXTURE_2D, 0);

  qglBindTexture(GL_TEXTURE_CUBE_MAP, 0);

  for (int i = 0; i < VERT_ATTR_COUNT; i++)
    qglDisableVertexAttribArray(i);
}

static void shader_clear_state(void) {
  shader_disable_state();
  shader_use_program(GLS_DEFAULT);
}

static void shader_update_blur(void) {
  float base_height =
      glr.render_height ? (float)glr.render_height : (float)glr.fd.height;
  int downscale =
      gl_bloom_downscale ? Cvar_ClampInteger(gl_bloom_downscale, 1, 8) : 4;
  float sigma =
      Cvar_ClampValue(gl_bloom_sigma, 1, MAX_SIGMA) * base_height / 2160;
  sigma *= 4.0f / (float)downscale;
  if (gl_static.bloom_sigma == sigma)
    return;

  gl_static.bloom_sigma = sigma;

  bool changed = false;
  uint32_t map_size = HashMap_Size(gl_static.programs);
  for (int i = 0; i < map_size; i++) {
    glStateBits_t *bits = HashMap_GetKey(glStateBits_t, gl_static.programs, i);
    if (*bits & GLS_BLUR_GAUSS) {
      GLuint *prog = HashMap_GetValue(GLuint, gl_static.programs, i);
      qglDeleteProgram(*prog);
      *prog = create_and_use_program(*bits);
      changed = true;
    }
  }

  if (changed)
    shader_use_program(gls.state_bits & GLS_SHADER_MASK);
}

static void gl_bloom_sigma_changed(cvar_t *self) { shader_update_blur(); }

static void shader_init(void) {
  gl_bloom_sigma = Cvar_Get("gl_bloom_sigma", "8", 0);
  gl_bloom_sigma->changed = gl_bloom_sigma_changed;

  gl_static.programs =
      HashMap_TagCreate(glStateBits_t, GLuint, HashInt64, NULL, TAG_RENDERER);

  qglGenBuffers(1, &gl_static.uniform_buffer);
  GL_BindBufferBase(GL_UNIFORM_BUFFER, UBO_UNIFORMS, gl_static.uniform_buffer);
  qglBufferData(GL_UNIFORM_BUFFER, sizeof(gls.u_block), NULL, GL_DYNAMIC_DRAW);

#if USE_MD5
  if (gl_config.caps & QGL_CAP_SKELETON_MASK) {
    qglGenBuffers(1, &gl_static.skeleton_buffer);
    GL_BindBufferBase(GL_UNIFORM_BUFFER, UBO_SKELETON,
                      gl_static.skeleton_buffer);

    if ((gl_config.caps & QGL_CAP_SKELETON_MASK) == QGL_CAP_BUFFER_TEXTURE)
      qglGenTextures(2, gl_static.skeleton_tex);
  }
#endif

  if (gl_config.ver_gl >= QGL_VER(3, 2))
    qglEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

  qglGenBuffers(1, &gl_static.dlight_buffer);
  GL_BindBuffer(GL_UNIFORM_BUFFER, gl_static.dlight_buffer);
  GL_BindBufferBase(GL_UNIFORM_BUFFER, UBO_DLIGHTS, gl_static.dlight_buffer);
  qglBufferData(GL_UNIFORM_BUFFER, sizeof(gls.u_dlights), NULL,
                GL_DYNAMIC_DRAW);

  // precache common shader
  shader_use_program(GLS_DEFAULT);

  gl_per_pixel_lighting = Cvar_Get("gl_per_pixel_lighting", "1", 0);
}

static void shader_shutdown(void) {
  shader_disable_state();
  qglUseProgram(0);

  gl_bloom_sigma->changed = NULL;

  if (gl_static.programs) {
    uint32_t map_size = HashMap_Size(gl_static.programs);
    for (int i = 0; i < map_size; i++) {
      GLuint *prog = HashMap_GetValue(GLuint, gl_static.programs, i);
      qglDeleteProgram(*prog);
    }
    HashMap_Destroy(gl_static.programs);
    gl_static.programs = NULL;
  }

  if (gl_static.uniform_buffer) {
    qglDeleteBuffers(1, &gl_static.uniform_buffer);
    gl_static.uniform_buffer = 0;
  }
  if (gl_static.dlight_buffer) {
    qglDeleteBuffers(1, &gl_static.dlight_buffer);
    gl_static.dlight_buffer = 0;
  }

#if USE_MD5
  if (gl_static.skeleton_buffer) {
    qglDeleteBuffers(1, &gl_static.skeleton_buffer);
    gl_static.skeleton_buffer = 0;
  }
  if (gl_static.skeleton_tex[0] || gl_static.skeleton_tex[1]) {
    qglDeleteTextures(2, gl_static.skeleton_tex);
    gl_static.skeleton_tex[0] = gl_static.skeleton_tex[1] = 0;
  }
#endif

  if (gl_config.ver_gl >= QGL_VER(3, 2))
    qglDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

static bool shader_use_per_pixel_lighting(void) {
  if (gl_per_pixel_lighting->integer)
    return true;

  return gl_shadowmaps && gl_shadowmaps->integer;
}

const glbackend_t backend_shader = {
    .name = "GLSL",

    .init = shader_init,
    .shutdown = shader_shutdown,
    .clear_state = shader_clear_state,
    .setup_2d = shader_setup_2d,
    .setup_3d = shader_setup_3d,

    .load_matrix = shader_load_matrix,
    .load_uniforms = shader_load_uniforms,
    .update_blur = shader_update_blur,

    .state_bits = shader_state_bits,
    .array_bits = shader_array_bits,

    .array_pointers = shader_array_pointers,
    .tex_coord_pointer = shader_tex_coord_pointer,

    .color = shader_color,
    .use_per_pixel_lighting = shader_use_per_pixel_lighting,
    .load_lights = shader_load_lights};
