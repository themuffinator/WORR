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

#include "gl.hpp"
#include "common/sizebuf.hpp"

#define MAX_SHADER_CHARS    4096

#define GLSL(x)     SZ_Write(buf, CONST_STR_LEN(#x "\n"));
#define GLSF(x)     SZ_Write(buf, CONST_STR_LEN(x))
#define GLSP(...)   shader_printf(buf, __VA_ARGS__)

static cvar_t *gl_bloom_sigma;
cvar_t *gl_per_pixel_lighting;

q_printf(2, 3)
static void shader_printf(sizebuf_t *buf, const char *fmt, ...)
{
    va_list ap;
    size_t len;

    Q_assert(buf->cursize <= buf->maxsize);

    va_start(ap, fmt);
    len = Q_vsnprintf((char *)buf->data + buf->cursize, buf->maxsize - buf->cursize, fmt, ap);
    va_end(ap);

    Q_assert(len <= buf->maxsize - buf->cursize);
    buf->cursize += len;
}

static void write_header(sizebuf_t *buf, glStateBits_t bits)
{
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

static void write_block(sizebuf_t *buf, glStateBits_t bits)
{
    GLSF("layout(std140) uniform Uniforms {\n");
    GLSL(
        mat4 m_model;
        mat4 m_view;
        mat4 m_proj;
    );

    if (bits & GLS_MESH_ANY) {
        GLSL(
            vec3 u_old_scale;
            vec3 u_new_scale;
            vec3 u_translate;
            vec3 u_shadedir;
            vec4 u_color;
            vec4 pad_0;
            float pad_1;
            float pad_2;
            float pad_3;
            uint u_weight_ofs;
            uint u_jointnum_ofs;
            float u_shellscale;
            float u_backlerp;
            float u_frontlerp;
        )
    } else {
        GLSL(mat4 m_sky[2];)
    }

    GLSL(
        float u_time;
        float u_modulate;
        float u_add;
        float u_intensity;
        float u_intensity2;
        float u_fog_sky_factor;
        vec2 w_amp;
        vec2 w_phase;
        vec2 u_scroll;
        vec4 u_fog_color;
        vec4 u_heightfog_start;
        vec4 u_heightfog_end;
        float u_heightfog_density;
        float u_heightfog_falloff;
        float pad_5;
        float pad_4;
        vec4 u_dof_params;
        vec4 u_dof_screen;
        vec4 u_dof_depth;
        vec4 u_vieworg;
    )
    GLSF("};\n");
}

static void write_dynamic_light_block(sizebuf_t *buf)
{
    GLSL(
        struct dlight_t
        {
            vec3    position;
            float   radius;
            vec4    color;
            vec4    cone;
        };
    )
    GLSF("#define DLIGHT_CUTOFF 64\n");
    GLSF("layout(std140) uniform DynamicLights {\n");
    GLSF("#define MAX_DLIGHTS " STRINGIFY(MAX_DLIGHTS) "\n");
    GLSL(
        int             num_dlights;
        int             dpad_1;
        int             dpad_2;
        int             dpad_3;
        dlight_t        dlights[MAX_DLIGHTS];
    )
    GLSF("};\n");
}

static void write_dynamic_lights(sizebuf_t *buf)
{
    GLSL(vec3 calc_dynamic_lights() {
        vec3 shade = vec3(0);

        for (int i = 0; i < num_dlights; i++) {
            vec3 light_pos = dlights[i].position;
            float light_cone = dlights[i].cone.w;

            if (light_cone == 0.0)
                light_pos += v_norm * 16.0;

            vec3 light_dir   = light_pos - v_world_pos;
            float dist       = length(light_dir);
            float radius     = dlights[i].radius + DLIGHT_CUTOFF;
            float len        = max(radius - dist - DLIGHT_CUTOFF, 0.0) / radius;
            vec3 dir         = light_dir / max(dist, 1.0);
            float lambert;
            
            if (dlights[i].color.r < 0.0f)
                lambert = 1.0f;
            else
                lambert = max(dot(v_norm, dir), 0.0);
            vec3 result      = ((dlights[i].color.rgb * dlights[i].color.a) * len) * lambert;

            if (light_cone != 0.0) {
                float mag = -dot(dir, dlights[i].cone.xyz);
                result *= max(1.0 - (1.0 - mag) * (1.0 / (1.0 - light_cone)), 0.0);
            }

            shade += result;
        }

        return shade;
    })
}

static void write_shadedot(sizebuf_t *buf)
{
    GLSL(
        float shadedot(vec3 normal) {
            float d = dot(normal, u_shadedir);
            if (d < 0.0)
                d *= 0.3;
            return d + 1.0;
        }
    )
}

#if USE_MD5
static void write_skel_shader(sizebuf_t *buf, glStateBits_t bits)
{
    GLSL(
        struct Joint {
            vec4 pos;
            mat3x3 axis;
        };
        layout(std140) uniform Skeleton {
            Joint u_joints[256];
        };
    )

    if (gl_config.caps & QGL_CAP_SHADER_STORAGE) {
        GLSL(
            layout(std430, binding = 0) readonly buffer Weights {
                vec4 b_weights[];
            };

            layout(std430, binding = 1) readonly buffer JointNums {
                uint b_jointnums[];
            };
        )
    } else {
        GLSL(
            uniform samplerBuffer u_weights;
            uniform usamplerBuffer u_jointnums;
        )
    }

    GLSL(
        in vec2 a_tc;
        in vec3 a_norm;
        in uvec2 a_vert;

        out vec2 v_tc;
        out vec4 v_color;
    )

    if (bits & (GLS_FOG_HEIGHT | GLS_DYNAMIC_LIGHTS))
        GLSL(out vec3 v_world_pos;)
    if (bits & GLS_DYNAMIC_LIGHTS)
        GLSL(out vec3 v_norm;)

    if (bits & GLS_MESH_SHADE)
        write_shadedot(buf);

    GLSF("void main() {\n");
    GLSL(
        vec3 out_pos = vec3(0.0);
        vec3 out_norm = vec3(0.0);

        uint start = a_vert[0];
        uint count = a_vert[1];
    )

    GLSF("for (uint i = start; i < start + count; i++) {\n");
        if (gl_config.caps & QGL_CAP_SHADER_STORAGE) {
            GLSL(
                uint jointnum = b_jointnums[i / 4U];
                jointnum >>= (i & 3U) * 8U;
                jointnum &= 255U;

                vec4 weight = b_weights[i];
            )
        } else {
            GLSL(
                uint jointnum = texelFetch(u_jointnums, int(u_jointnum_ofs + i)).r;
                vec4 weight   = texelFetch(u_weights,   int(u_weight_ofs   + i));
            )
        }
        GLSL(
            Joint joint = u_joints[jointnum];

            vec3 wv = joint.pos.xyz + (weight.xyz * joint.axis) * joint.pos.w;
            out_pos += wv * weight.w;

            out_norm += a_norm * joint.axis * weight.w;
        )
    GLSF("}\n");

    GLSL(v_tc = a_tc;)

    if (bits & GLS_MESH_SHADE)
        GLSL(v_color = vec4(u_color.rgb * shadedot(out_norm), u_color.a);)
    else
        GLSL(v_color = u_color;)

    if (bits & GLS_MESH_SHELL)
        GLSL(out_pos += out_norm * u_shellscale;)

    if (bits & (GLS_FOG_HEIGHT | GLS_DYNAMIC_LIGHTS))
        GLSL(v_world_pos = (m_model * vec4(out_pos, 1.0)).xyz;)
    if (bits & GLS_DYNAMIC_LIGHTS)
        GLSL(v_norm = normalize((mat3(m_model) * out_norm).xyz);)
    GLSL(gl_Position = m_proj * m_view * m_model * vec4(out_pos, 1.0);)
    GLSF("}\n");
}
#endif

static void write_getnormal(sizebuf_t *buf)
{
    GLSL(
        vec3 get_normal(int norm) {
            const float pi = 3.14159265358979323846;
            const float scale = pi * (2.0 / 255.0);
            float lat = float( uint(norm)       & 255U) * scale;
            float lng = float((uint(norm) >> 8) & 255U) * scale;
            return vec3(
                sin(lat) * cos(lng),
                sin(lat) * sin(lng),
                cos(lat)
            );
        }
    )
}

static void write_mesh_shader(sizebuf_t *buf, glStateBits_t bits)
{
    GLSL(
        in vec2 a_tc;
        in ivec4 a_new_pos;
    )

    if (bits & GLS_MESH_LERP)
        GLSL(in ivec4 a_old_pos;)

    GLSL(
        out vec2 v_tc;
        out vec4 v_color;
    )

    if (bits & (GLS_FOG_HEIGHT | GLS_DYNAMIC_LIGHTS))
        GLSL(out vec3 v_world_pos;)
    if (bits & GLS_DYNAMIC_LIGHTS)
        GLSL(out vec3 v_norm;)

    if (bits & (GLS_MESH_SHELL | GLS_MESH_SHADE | GLS_DYNAMIC_LIGHTS))
        write_getnormal(buf);

    if (bits & GLS_MESH_SHADE)
        write_shadedot(buf);

    GLSF("void main() {\n");
    GLSL(v_tc = a_tc;)

    if (bits & GLS_MESH_LERP) {
        if (bits & (GLS_MESH_SHELL | GLS_MESH_SHADE | GLS_DYNAMIC_LIGHTS))
            GLSL(
                vec3 old_norm = get_normal(a_old_pos.w);
                vec3 new_norm = get_normal(a_new_pos.w);
                vec3 norm = normalize(old_norm * u_backlerp + new_norm * u_frontlerp);
            )

        GLSL(vec3 pos = vec3(a_old_pos.xyz) * u_old_scale + vec3(a_new_pos.xyz) * u_new_scale + u_translate;)

        if (bits & GLS_MESH_SHELL)
            GLSL(pos += norm * u_shellscale;)

        if (bits & GLS_MESH_SHADE)
            GLSL(v_color = vec4(u_color.rgb * (shadedot(old_norm) * u_backlerp + shadedot(new_norm) * u_frontlerp), u_color.a);)
        else
            GLSL(v_color = u_color;)

        if (bits & GLS_DYNAMIC_LIGHTS)
            GLSL(v_norm = normalize((mat3(m_model) * norm).xyz);)
    } else {
        if (bits & (GLS_MESH_SHELL | GLS_MESH_SHADE | GLS_DYNAMIC_LIGHTS))
            GLSL(vec3 norm = get_normal(a_new_pos.w);)

        GLSL(vec3 pos = vec3(a_new_pos.xyz) * u_new_scale + u_translate;)

        if (bits & GLS_MESH_SHELL)
            GLSL(pos += norm * u_shellscale;)

        if (bits & GLS_MESH_SHADE)
            GLSL(v_color = vec4(u_color.rgb * shadedot(norm), u_color.a);)
        else
            GLSL(v_color = u_color;)

        if (bits & GLS_DYNAMIC_LIGHTS)
            GLSL(v_norm = normalize((mat3(m_model) * norm).xyz);)
    }

    if (bits & (GLS_FOG_HEIGHT | GLS_DYNAMIC_LIGHTS))
        GLSL(v_world_pos = (m_model * vec4(pos, 1.0)).xyz;)

    GLSL(gl_Position = m_proj * m_view * m_model * vec4(pos, 1.0);)
    GLSF("}\n");
}

static void write_vertex_shader(sizebuf_t *buf, glStateBits_t bits)
{
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

#define MAX_SIGMA   25
#define MAX_RADIUS  50

// https://lisyarus.github.io/blog/posts/blur-coefficients-generator.html
static void write_gaussian_blur(sizebuf_t *buf)
{
    float sigma = gl_static.bloom_sigma;
    int radius = min(sigma * 2 + 0.5f, MAX_RADIUS);
    int samples = radius + 1;
    int raw_samples = (radius * 2) + 1;
    float offsets[MAX_RADIUS + 1];
    float weights[(MAX_RADIUS * 2) + 1];

    // should not really happen
    if (radius < 1) {
        GLSL(vec4 blur(sampler2D src, vec2 tc, vec2 dir) { return texture(src, tc); })
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

    GLSL(
        vec4 blur(sampler2D src, vec2 tc, vec2 dir) {
            vec4 result = vec4(0.0);
            for (int i = 0; i < BLUR_SAMPLES; i++)
                result += texture(src, tc + dir * blur_offsets[i]) * blur_weights[i];
            return result;
        }
    )
}

static void write_box_blur(sizebuf_t *buf)
{
    GLSL(
        vec4 blur(sampler2D src, vec2 tc, vec2 dir) {
            vec4 result = vec4(0.0);
            const float o = 0.25;
            result += texture(src, tc + vec2(-o, -o) * dir) * 0.25;
            result += texture(src, tc + vec2(-o,  o) * dir) * 0.25;
            result += texture(src, tc + vec2( o, -o) * dir) * 0.25;
            result += texture(src, tc + vec2( o,  o) * dir) * 0.25;
            return result;
        }
    )
}

static void write_bokeh_fragment(sizebuf_t *buf, glStateBits_t bits)
{
    write_header(buf, bits);
    write_block(buf, bits);

    GLSL(uniform sampler2D u_bokeh_source;);
    if (bits & (GLS_BOKEH_INITIAL | GLS_BOKEH_DOWNSAMPLE | GLS_BOKEH_COMBINE))
        GLSL(uniform sampler2D u_bokeh_coc;);
    if (bits & GLS_BOKEH_COMBINE)
        GLSL(uniform sampler2D u_bokeh_gather;);

    GLSL(in vec2 v_tc;);

    if (gl_config.ver_es)
        GLSL(layout(location = 0));
    GLSL(out vec4 o_color;);

    if (bits & GLS_BOKEH_INITIAL) {
        GLSL(const int kernelSampleCount = 22;);
        GLSL(const vec2 kernel[kernelSampleCount] = vec2[](
            vec2(0.00000000, 0.00000000),
            vec2(0.53333336, 0.00000000),
            vec2(0.33252790, 0.41697681),
            vec2(-0.11867785, 0.51996160),
            vec2(-0.48051673, 0.23140470),
            vec2(-0.48051673, -0.23140468),
            vec2(-0.11867763, -0.51996166),
            vec2(0.33252785, -0.41697690),
            vec2(1.00000000, 0.00000000),
            vec2(0.90096885, 0.43388376),
            vec2(0.62348980, 0.78183150),
            vec2(0.22252098, 0.97492790),
            vec2(-0.22252095, 0.97492790),
            vec2(-0.62349000, 0.78183140),
            vec2(-0.90096885, 0.43388382),
            vec2(-1.00000000, 0.00000000),
            vec2(-0.90096885, -0.43388376),
            vec2(-0.62348960, -0.78183160),
            vec2(-0.22252055, -0.97492800),
            vec2(0.22252150, -0.97492780),
            vec2(0.62348970, -0.78183160),
            vec2(0.90096885, -0.43388376)
        ););

        GLSL(float bokeh_sample_weight(float coc, float radius) {
            return clamp((coc - radius + 2.0) / 2.0, 0.0, 1.0);
        });

        GLSL(vec3 luma_boost(vec3 color) {
            float lum = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722)) * u_dof_params.w;
            vec3 vOut = color * (1.0 + 0.2 * lum * lum * lum);
            return vOut * vOut;
        });
    }

    GLSF("void main() {\n");
    GLSL(vec2 tc = v_tc;);

    if (bits & GLS_BOKEH_COC) {
        GLSL(float depth = texture(u_bokeh_source, tc).r;);
        GLSL(float znear = u_dof_depth.x;);
        GLSL(float zfar = u_dof_depth.y;);
        GLSL(float ndc = depth * 2.0 - 1.0;);
        GLSL(float linear = (2.0 * znear * zfar) / max(zfar + znear - ndc * (zfar - znear), 0.00001););
        GLSL(float focus_distance = u_dof_params.y;);
        GLSL(float focus_range = max(u_dof_params.z, 0.0001););
        GLSL(float coc = clamp((linear - focus_distance) / focus_range, -1.0, 1.0););
        GLSL(float sign_val = coc < 0.0 ? -1.0 : 1.0;);
        GLSL(coc = smoothstep(0.1, 1.0, abs(coc)) * sign_val;);
        GLSL(o_color = vec4(coc * u_dof_params.x, 0.0, 0.0, 1.0););
        GLSL(return;);
    }

    if (bits & GLS_BOKEH_INITIAL) {
        GLSL(vec2 inv_resolution = vec2(u_dof_screen.z, u_dof_screen.w););
        GLSL(vec3 bgColor = vec3(0.0););
        GLSL(vec3 fgColor = vec3(0.0););
        GLSL(float bgWeight = 0.0;);
        GLSL(float fgWeight = 0.0;);
        GLSL(float center_coc = texture(u_bokeh_coc, tc).r;);

        GLSL(for (int k = 0; k < kernelSampleCount; ++k) {
            vec2 sample_dir = kernel[k] * u_dof_params.x;
            float radius = length(sample_dir);
            vec2 offset = sample_dir * inv_resolution;
            vec4 sample_color = texture(u_bokeh_source, tc + offset);
            float sample_coc = texture(u_bokeh_coc, tc + offset).r;
            vec3 lum = sample_color.rgb + luma_boost(sample_color.rgb);

            float bgw = bokeh_sample_weight(max(0.0, min(sample_coc, center_coc)), radius);
            bgColor += lum * bgw;
            bgWeight += bgw;

            float fgw = bokeh_sample_weight(-sample_coc, radius);
            fgColor += lum * fgw;
            fgWeight += fgw;
        });

        GLSL(if (bgWeight > 0.0) bgColor /= bgWeight;);
        GLSL(if (fgWeight > 0.0) fgColor /= fgWeight;);
        GLSL(float total = min(1.0, fgWeight * 3.14159265359 / float(kernelSampleCount)););
        GLSL(vec3 color = mix(bgColor, fgColor, total););
        GLSL(o_color = vec4(color, total););
        GLSL(return;);
    }

    if (bits & GLS_BOKEH_DOWNSAMPLE) {
        GLSL(vec2 inv_resolution = vec2(u_dof_screen.z, u_dof_screen.w););
        GLSL(vec4 offset = inv_resolution.xyxy * vec2(-0.5, 0.5).xxyy;);
        GLSL(float coc1 = texture(u_bokeh_coc, tc + offset.xy).r;);
        GLSL(float coc2 = texture(u_bokeh_coc, tc + offset.zy).r;);
        GLSL(float coc3 = texture(u_bokeh_coc, tc + offset.xw).r;);
        GLSL(float coc4 = texture(u_bokeh_coc, tc + offset.zw).r;);
        GLSL(float cocMin = min(min(coc1, coc2), min(coc3, coc4)););
        GLSL(float cocMax = max(max(coc1, coc2), max(coc3, coc4)););
        GLSL(float cocAvg = cocMax >= -cocMin ? cocMax : cocMin;);
        GLSL(vec3 color = texture(u_bokeh_source, tc).rgb;);
        GLSL(o_color = vec4(color, cocAvg););
        GLSL(return;);
    }

    if (bits & GLS_BOKEH_GATHER) {
        GLSL(vec2 inv_resolution = vec2(u_dof_screen.z, u_dof_screen.w););
        GLSL(vec4 offset = inv_resolution.xyxy * vec2(-0.5, 0.5).xxyy;);
        GLSL(vec4 color = texture(u_bokeh_source, tc + offset.xy););
        GLSL(color += texture(u_bokeh_source, tc + offset.zy););
        GLSL(color += texture(u_bokeh_source, tc + offset.xw););
        GLSL(color += texture(u_bokeh_source, tc + offset.zw););
        GLSL(o_color = color * 0.25;);
        GLSL(return;);
    }

    if (bits & GLS_BOKEH_COMBINE) {
        GLSL(float coc = texture(u_bokeh_coc, tc).a;);
        GLSL(vec4 dof = texture(u_bokeh_gather, tc););
        GLSL(vec4 src = texture(u_bokeh_source, tc););
        GLSL(float s = smoothstep(0.1, 1.0, abs(coc)););
        GLSL(vec3 color = mix(src.rgb, dof.rgb, s + dof.a - s * dof.a););
        GLSL(o_color = vec4(color, src.a););
        GLSL(return;);
    }

    GLSL(o_color = vec4(0.0););
    GLSF("}\n");
}

// XXX: this is very broken. but that's how it is in re-release.
static void write_height_fog(sizebuf_t *buf, glStateBits_t bits)
{
    GLSL({
        float dir_z = normalize(v_world_pos - u_vieworg.xyz).z;
        float s = sign(dir_z);
        dir_z += 0.00001 * (1.0 - s * s);
        float eye = u_vieworg.z - u_heightfog_start.w;
        float pos = v_world_pos.z - u_heightfog_start.w;
        float density = (exp(-u_heightfog_falloff * eye) -
                         exp(-u_heightfog_falloff * pos)) / (u_heightfog_falloff * dir_z);
        float extinction = 1.0 - clamp(exp(-density), 0.0, 1.0);
        float height_range = clamp(u_heightfog_end.w - u_heightfog_start.w, 0.00001, 1e30);
        float fraction = clamp((v_world_pos.z - u_heightfog_start.w) / height_range, 0.0, 1.0);
        vec3 fog_color = mix(u_heightfog_start.rgb, u_heightfog_end.rgb, fraction) * extinction;
        float fog = (1.0 - exp(-(u_heightfog_density * frag_depth))) * extinction;
        diffuse.rgb = mix(diffuse.rgb, fog_color.rgb, fog);
    )

    if (bits & GLS_BLOOM_GENERATE)
        GLSL(bloom.rgb *= 1.0 - fog;)

    GLSL(})
}

static void write_fragment_shader(sizebuf_t *buf, glStateBits_t bits)
{
    if (bits & GLS_BOKEH_MASK) {
        write_bokeh_fragment(buf, bits);
        return;
    }

    write_header(buf, bits);

    if (bits & GLS_UNIFORM_MASK)
        write_block(buf, bits);

    if (bits & GLS_DYNAMIC_LIGHTS)
        write_dynamic_light_block(buf);

    if (bits & GLS_CLASSIC_SKY) {
        GLSL(
            uniform sampler2D u_texture1;
            uniform sampler2D u_texture2;
        )
    } else if (bits & GLS_DEFAULT_SKY) {
        GLSL(uniform samplerCube u_texture;)
    } else {
        GLSL(uniform sampler2D u_texture;)
        if (bits & GLS_BLOOM_OUTPUT)
            GLSL(uniform sampler2D u_bloom;)
    }

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

    if (bits & (GLS_FOG_HEIGHT | GLS_DYNAMIC_LIGHTS))
        GLSL(in vec3 v_world_pos;)

    if (bits & GLS_DYNAMIC_LIGHTS) {
        GLSL(in vec3 v_norm;)
        write_dynamic_lights(buf);
    }

    if (bits & GLS_BLUR_GAUSS)
        write_gaussian_blur(buf);
    else if (bits & GLS_BLUR_BOX)
        write_box_blur(buf);

    GLSF("void main() {\n");
    if (bits & GLS_CLASSIC_SKY) {
        GLSL(
            float len = length(v_dir);
            vec2 dir = v_dir.xy * (3.0 / len);
            vec2 tc1 = dir + vec2(u_time * 0.0625);
            vec2 tc2 = dir + vec2(u_time * 0.1250);
            vec4 solid = texture(u_texture1, tc1);
            vec4 alpha = texture(u_texture2, tc2);
            vec4 diffuse = vec4((solid.rgb - alpha.rgb * 0.25) * 0.65, 1.0);
        )
    } else if (bits & GLS_DEFAULT_SKY) {
        GLSL(vec4 diffuse = texture(u_texture, v_dir);)
    } else {
        GLSL(vec2 tc = v_tc;)

        if (bits & GLS_WARP_ENABLE)
            GLSL(tc += w_amp * sin(tc.ts * w_phase + u_time);)

        if (bits & GLS_BLUR_MASK)
            GLSL(vec4 diffuse = blur(u_texture, tc, u_fog_color.xy);)
        else
            GLSL(vec4 diffuse = texture(u_texture, tc);)
    }

    if (bits & GLS_ALPHATEST_ENABLE)
        GLSL(if (diffuse.a <= 0.666) discard;)

    if (!(bits & GLS_TEXTURE_REPLACE))
        GLSL(vec4 color = v_color;)

    if (bits & GLS_BLOOM_GENERATE)        GLSL(vec4 bloom = vec4(0.0);)

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
            GLSL(
                lightmap.rgb += calc_dynamic_lights();
            )
        }

        GLSL(diffuse.rgb *= (lightmap.rgb + u_add) * u_modulate;)
    } else if ((bits & GLS_DYNAMIC_LIGHTS) && !(bits & GLS_TEXTURE_REPLACE)) {
        GLSL(color.rgb += calc_dynamic_lights() * u_modulate;)
    }

    if (bits & GLS_INTENSITY_ENABLE)
        GLSL(diffuse.rgb *= u_intensity;)

    if (bits & GLS_DEFAULT_FLARE)
        GLSL(
                diffuse.rgb *= (diffuse.r + diffuse.g + diffuse.b) / 3.0;
                diffuse.rgb *= v_color.a;
        )

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

        GLSL(})
    }

    if (bits & GLS_FOG_HEIGHT)
        write_height_fog(buf, bits);

    if (bits & GLS_FOG_SKY)
        GLSL(diffuse.rgb = mix(diffuse.rgb, u_fog_color.rgb, u_fog_sky_factor);)

    if (bits & GLS_BLOOM_OUTPUT)
        GLSL(diffuse.rgb += texture(u_bloom, tc).rgb;)

    if (bits & GLS_BLOOM_GENERATE)
        GLSL(o_bloom = bloom;)

    GLSL(o_color = diffuse;)
    GLSF("}\n");
}

static GLuint create_shader(GLenum type, const sizebuf_t *buf)
{
    const GLchar *data = (const GLchar *)buf->data;
    GLint size = buf->cursize;

    GLuint shader = qglCreateShader(type);
    if (!shader) {
        Com_EPrintf("Couldn't create shader\n");
        return 0;
    }

    Com_DDDPrintf("Compiling %s shader (%d bytes):\n%.*s\n",
                  type == GL_VERTEX_SHADER ? "vertex" : "fragment", size, size, data);

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

static bool bind_uniform_block(GLuint program, const char *name, size_t cpu_size, GLuint binding)
{
    GLuint index = qglGetUniformBlockIndex(program, name);
    if (index == GL_INVALID_INDEX) {
        Com_EPrintf("%s block not found\n", name);
        return false;
    }

    GLint gpu_size = 0;
    qglGetActiveUniformBlockiv(program, index, GL_UNIFORM_BLOCK_DATA_SIZE, &gpu_size);
    if (gpu_size != cpu_size) {
        Com_EPrintf("%s block size mismatch: %d != %zu\n", name, gpu_size, cpu_size);
        return false;
    }

    qglUniformBlockBinding(program, index, binding);
    return true;
}

static void bind_texture_unit(GLuint program, const char *name, GLuint tmu)
{
    GLint loc = qglGetUniformLocation(program, name);
    if (loc == -1) {
        Com_EPrintf("Texture %s not found\n", name);
        return;
    }
    qglUniform1i(loc, tmu);
}

static GLuint create_and_use_program(glStateBits_t bits)
{
    char buffer[MAX_SHADER_CHARS];
    sizebuf_t sb;

    GLuint program = qglCreateProgram();
    if (!program) {
        Com_EPrintf("Couldn't create program\n");
        return 0;
    }

    GLuint shader_v = 0;
    GLuint shader_f = 0;
    GLint status = 0;

    SZ_Init(&sb, buffer, sizeof(buffer), "GLSL");
    write_vertex_shader(&sb, bits);
    shader_v = create_shader(GL_VERTEX_SHADER, &sb);
    if (!shader_v)
        goto fail;

    SZ_Clear(&sb);
    write_fragment_shader(&sb, bits);
    shader_f = create_shader(GL_FRAGMENT_SHADER, &sb);
    if (!shader_f)
        goto fail;

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
    shader_v = 0;
    qglDeleteShader(shader_f);
    shader_f = 0;

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

    if (!bind_uniform_block(program, "Uniforms", sizeof(gls.u_block), UBO_UNIFORMS))
        goto fail;

#if USE_MD5
    if (bits & GLS_MESH_MD5)
        if (!bind_uniform_block(program, "Skeleton", sizeof(glJoint_t) * MD5_MAX_JOINTS, UBO_SKELETON))
            goto fail;
#endif

    if (bits & GLS_DYNAMIC_LIGHTS) {
        if (!bind_uniform_block(program, "DynamicLights", sizeof(gls.u_dlights), UBO_DLIGHTS))
            goto fail;
    }

    qglUseProgram(program);

#if USE_MD5
    if (bits & GLS_MESH_MD5 && !(gl_config.caps & QGL_CAP_SHADER_STORAGE)) {
        bind_texture_unit(program, "u_weights", TMU_SKEL_WEIGHTS);
        bind_texture_unit(program, "u_jointnums", TMU_SKEL_JOINTNUMS);
    }
#endif

    if (bits & GLS_BOKEH_MASK) {
        bind_texture_unit(program, "u_bokeh_source", TMU_TEXTURE);
        if (bits & (GLS_BOKEH_INITIAL | GLS_BOKEH_DOWNSAMPLE | GLS_BOKEH_COMBINE))
            bind_texture_unit(program, "u_bokeh_coc", TMU_LIGHTMAP);
        if (bits & GLS_BOKEH_COMBINE)
            bind_texture_unit(program, "u_bokeh_gather", TMU_GLOWMAP);
    } else if (bits & GLS_CLASSIC_SKY) {
        bind_texture_unit(program, "u_texture1", TMU_TEXTURE);
        bind_texture_unit(program, "u_texture2", TMU_LIGHTMAP);
    } else {
        bind_texture_unit(program, "u_texture", TMU_TEXTURE);
        if (bits & GLS_BLOOM_OUTPUT)
            bind_texture_unit(program, "u_bloom", TMU_LIGHTMAP);
    }

    if (bits & GLS_LIGHTMAP_ENABLE)
        bind_texture_unit(program, "u_lightmap", TMU_LIGHTMAP);

    if (bits & GLS_GLOWMAP_ENABLE)
        bind_texture_unit(program, "u_glowmap", TMU_GLOWMAP);
    
    return program;

fail:
    if (shader_v)
        qglDeleteShader(shader_v);
    if (shader_f)
        qglDeleteShader(shader_f);
    qglDeleteProgram(program);
    return 0;
}

static void shader_use_program(glStateBits_t key)
{
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

static void shader_state_bits(glStateBits_t bits)
{
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
        static constexpr GLenum bloom_targets[] = {
            GL_COLOR_ATTACHMENT0,
            GL_COLOR_ATTACHMENT1,
        };
        int n = (bits & GLS_BLOOM_GENERATE) ? 2 : 1;
        qglDrawBuffers(n, bloom_targets);
    }
}

static void shader_array_bits(glArrayBits_t bits)
{
    glArrayBits_t diff = bits ^ gls.array_bits;

    for (int i = 0; i < VERT_ATTR_COUNT; i++) {
        const auto attr_bit = static_cast<glArrayBits_t>(BIT(i));

        if (!(diff & attr_bit))
            continue;
        if (bits & attr_bit)
            qglEnableVertexAttribArray(i);
        else
            qglDisableVertexAttribArray(i);
    }
}

static void shader_array_pointers(const glVaDesc_t *desc, const GLfloat *ptr)
{
    uintptr_t base = (uintptr_t)ptr;

    for (int i = 0; i < VERT_ATTR_COUNT; i++) {
        const glVaDesc_t *d = &desc[i];
        if (d->size) {
            const GLenum type = d->type ? GL_UNSIGNED_BYTE : GL_FLOAT;
            qglVertexAttribPointer(i, d->size, type, d->type, d->stride, (void *)(base + d->offset));
        }
    }
}

static void shader_tex_coord_pointer(const GLfloat *ptr)
{
    qglVertexAttribPointer(VERT_ATTR_TC, 2, GL_FLOAT, GL_FALSE, 0, ptr);
}

static void shader_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    qglVertexAttrib4f(VERT_ATTR_COLOR, r, g, b, a);
}

static void shader_load_uniforms(void)
{
    GL_BindBuffer(GL_UNIFORM_BUFFER, gl_static.uniform_buffer);
    qglBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(gls.u_block), &gls.u_block);
    c.uniformUploads++;
}

static void shader_load_lights(void)
{
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

        if (GL_CullSphere(dl->sphere, dl->sphere[3]) == CULL_OUT) {
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

        i++;
    }

    gls.u_dlights.num_dlights = i;
    c.dlightsUsed += i;

    GL_BindBuffer(GL_UNIFORM_BUFFER, gl_static.dlight_buffer);
    qglBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GLint[4]) + (sizeof(gls.u_dlights.lights[0]) * gls.u_dlights.num_dlights), &gls.u_dlights);
    c.uniformUploads++;
    c.dlightUploads++;
}

static void shader_load_matrix(GLenum mode, const GLfloat *matrix, const GLfloat *view)
{
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

static void shader_setup_2d(void)
{
    gls.u_block.time = glr.fd.time;
    gls.u_block.modulate = 1.0f;
    gls.u_block.add = 0.0f;
    gls.u_block.intensity = 1.0f;
    gls.u_block.intensity2 = 1.0f;

    gls.u_block.w_amp[0] = 0.0025f;
    gls.u_block.w_amp[1] = 0.0025f;
    gls.u_block.w_phase[0] = M_PIf * 10;
    gls.u_block.w_phase[1] = M_PIf * 10;
}

static void shader_setup_fog(void)
{
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

static void shader_setup_3d(void)
{
    gls.u_block.time = glr.fd.time;
    gls.u_block.modulate = gl_modulate->value * gl_modulate_world->value;
    gls.u_block.add = gl_brightness->value;
    gls.u_block.intensity = gl_intensity->value;
    gls.u_block.intensity2 = gl_intensity->value * gl_glowmap_intensity->value;

    gls.u_block.w_amp[0] = 0.0625f;
    gls.u_block.w_amp[1] = 0.0625f;
    gls.u_block.w_phase[0] = 4;
    gls.u_block.w_phase[1] = 4;

    gls.dlight_bits = 0;

    shader_setup_fog();

    R_RotateForSky();

    // setup default matrices for world
    memcpy(gls.u_block.m_sky, glr.skymatrix, sizeof(gls.u_block.m_sky));
    memcpy(gls.u_block.m_model, gl_identity, sizeof(gls.u_block.m_model));
    
    VectorCopy(glr.fd.vieworg, gls.u_block.vieworg);
}

static void shader_disable_state(void)
{
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

static void shader_clear_state(void)
{
    shader_disable_state();
    shader_use_program(GLS_DEFAULT);
}

static void shader_update_blur(void)
{
    float sigma = 0.0f;

    if (gl_bloom->integer) {
        float bloom_sigma = Cvar_ClampValue(gl_bloom_sigma, 1, MAX_SIGMA) * glr.fd.height / 2160;
        if (sigma < bloom_sigma)
            sigma = bloom_sigma;
    }

    if (sigma <= 0.0f)
        sigma = 1.0f;

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

static void gl_bloom_sigma_changed(cvar_t *self)
{
    if (gl_bloom->integer && glr.fd.height > 0)
        shader_update_blur();
}

static void shader_init(void)
{
    gl_bloom_sigma = Cvar_Get("gl_bloom_sigma", "8", 0);
    gl_bloom_sigma->changed = gl_bloom_sigma_changed;

    gl_static.programs = HashMap_TagCreate(glStateBits_t, GLuint, HashInt64, NULL, TAG_RENDERER);

    qglGenBuffers(1, &gl_static.uniform_buffer);
    GL_BindBufferBase(GL_UNIFORM_BUFFER, UBO_UNIFORMS, gl_static.uniform_buffer);
    qglBufferData(GL_UNIFORM_BUFFER, sizeof(gls.u_block), NULL, GL_DYNAMIC_DRAW);

#if USE_MD5
    if (gl_config.caps & QGL_CAP_SKELETON_MASK) {
        qglGenBuffers(1, &gl_static.skeleton_buffer);
        GL_BindBufferBase(GL_UNIFORM_BUFFER, UBO_SKELETON, gl_static.skeleton_buffer);

        if ((gl_config.caps & QGL_CAP_SKELETON_MASK) == QGL_CAP_BUFFER_TEXTURE)
            qglGenTextures(2, gl_static.skeleton_tex);
    }
#endif

    if (gl_config.ver_gl >= QGL_VER(3, 2))
        qglEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    
    qglGenBuffers(1, &gl_static.dlight_buffer);
    GL_BindBuffer(GL_UNIFORM_BUFFER, gl_static.dlight_buffer);
    GL_BindBufferBase(GL_UNIFORM_BUFFER, UBO_DLIGHTS, gl_static.dlight_buffer);
    qglBufferData(GL_UNIFORM_BUFFER, sizeof(gls.u_dlights), NULL, GL_DYNAMIC_DRAW);

    // precache common shader
    shader_use_program(GLS_DEFAULT);

    gl_per_pixel_lighting = Cvar_Get("gl_per_pixel_lighting", "1", 0);
}

static void shader_shutdown(void)
{
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

static bool shader_use_per_pixel_lighting(void)
{
    return !!gl_per_pixel_lighting->integer;
}

extern const glbackend_t backend_shader = {
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
    .load_lights = shader_load_lights
};
