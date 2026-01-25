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
