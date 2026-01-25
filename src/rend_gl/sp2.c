
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
                         float shadow_index, float bias) {
        if (shadow_index < 0.0 || u_shadow_params.x <= 0.0)
          return 1.0;

        float face;
        vec2 uv = shadow_cube_uv(light_dir / max(dist, 1.0), face);
        float layer = shadow_index * 6.0 + face;
        float depth = dist / radius;
        float base_texel = u_shadow_params.x;
        float softness = max(u_shadow_params.z, 0.0);
        float texel = base_texel * softness;
        int method = int(u_shadow_params2.x + 0.5);
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
                        float bias) {
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
        int method = int(u_shadow_params2.x + 0.5);
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
          if (light_cone == 0.0)
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

          if (light_cone != 0.0) {
            float mag = -dot(dir, dlights[i].cone.xyz);
            result *= max(1.0 - (1.0 - mag) * (1.0 / (1.0 - light_cone)), 0.0);
          }

          if (dlights[i].shadow.y > 0.5) {
            result *= shadow_spot(shadow_vec, shadow_dist, dlights[i].radius,
                                  dlights[i].shadow.x, dlights[i].cone.xyz,
                                  light_cone, bias);
          } else {
            result *= shadow_point(shadow_vec, shadow_dist, dlights[i].radius,
                                   dlights[i].shadow.x, bias);
          }

          shade += result;
        }

        if (u_sun_dir.w > 0.5 && u_sun_color.a > 0.0) {
          vec3 sun_dir = normalize(u_sun_dir.xyz);
          float lambert = max(dot(v_norm, sun_dir), 0.0);
          if (lambert > 0.0) {
            float bias =
                u_shadow_params.y + u_shadow_params3.x * (1.0 - lambert);
            float shadow = shadow_csm(receiver_pos, bias);
            shade += u_sun_color.rgb * u_sun_color.a * lambert * shadow;
          }
        }

        return shade;
      })
}
