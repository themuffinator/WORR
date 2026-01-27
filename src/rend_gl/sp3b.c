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
