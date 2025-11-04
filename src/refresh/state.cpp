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

#include "gl.hpp"

#include <algorithm>
#include <cmath>

glState_t gls;

const glbackend_t *gl_backend;

const mat4_t gl_identity = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
};

// for uploading
void GL_ForceTexture(glTmu_t tmu, GLuint texnum)
{
    GL_ActiveTexture(tmu);

    if (gls.texnums[tmu] == texnum)
        return;

    qglBindTexture(GL_TEXTURE_2D, texnum);
    gls.texnums[tmu] = texnum;

    c.texSwitches++;
}

// for drawing
void GL_BindTexture(glTmu_t tmu, GLuint texnum)
{
#if USE_DEBUG
    if (gl_nobind->integer && tmu == TMU_TEXTURE)
        texnum = TEXNUM_DEFAULT;
#endif

    if (gls.texnums[tmu] == texnum)
        return;

    if (qglBindTextureUnit) {
        qglBindTextureUnit(tmu, texnum);
    } else {
        GL_ActiveTexture(tmu);
        qglBindTexture(GL_TEXTURE_2D, texnum);
    }
    gls.texnums[tmu] = texnum;

    c.texSwitches++;
}

void GL_ForceCubemap(GLuint texnum)
{
    GL_ActiveTexture(TMU_TEXTURE);

    if (gls.texnumcube == texnum)
        return;

    qglBindTexture(GL_TEXTURE_CUBE_MAP, texnum);
    gls.texnumcube = texnum;

    c.texSwitches++;
}

void GL_BindCubemap(GLuint texnum)
{
    if (!gl_drawsky->integer)
        texnum = TEXNUM_CUBEMAP_BLACK;

    if (gls.texnumcube == texnum)
        return;

    if (qglBindTextureUnit) {
        qglBindTextureUnit(TMU_TEXTURE, texnum);
    } else {
        GL_ActiveTexture(TMU_TEXTURE);
        qglBindTexture(GL_TEXTURE_CUBE_MAP, texnum);
    }
    gls.texnumcube = texnum;

    c.texSwitches++;
}

void GL_DeleteBuffers(GLsizei n, const GLuint *buffers)
{
    int i, j;

    for (i = 0; i < n; i++)
        if (buffers[i])
            break;
    if (i == n)
        return;

    Q_assert(qglDeleteBuffers);
    qglDeleteBuffers(n, buffers);

    // invalidate bindings
    for (i = 0; i < n; i++)
        for (j = 0; j < GLB_COUNT; j++)
            if (gls.currentbuffer[j] == buffers[i])
                gls.currentbuffer[j] = 0;
}

void GL_CommonStateBits(glStateBits_t bits)
{
    glStateBits_t diff = bits ^ gls.state_bits;

    if (diff & GLS_BLEND_MASK) {
        if (bits & GLS_BLEND_MASK) {
            qglEnable(GL_BLEND);
            if (bits & GLS_BLEND_BLEND)
                qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            else if (bits & GLS_BLEND_ADD)
                qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
            else if (bits & GLS_BLEND_MODULATE)
                qglBlendFunc(GL_DST_COLOR, GL_ONE);
        } else {
            qglDisable(GL_BLEND);
        }
    }

    if (diff & GLS_DEPTHMASK_FALSE) {
        if (bits & GLS_DEPTHMASK_FALSE)
            qglDepthMask(GL_FALSE);
        else
            qglDepthMask(GL_TRUE);
    }

    if (diff & GLS_DEPTHTEST_DISABLE) {
        if (bits & GLS_DEPTHTEST_DISABLE)
            qglDisable(GL_DEPTH_TEST);
        else
            qglEnable(GL_DEPTH_TEST);
    }

    if (diff & GLS_CULL_DISABLE) {
        if (bits & GLS_CULL_DISABLE)
            qglDisable(GL_CULL_FACE);
        else
            qglEnable(GL_CULL_FACE);
    }
}

void GL_ScrollPos(vec2_t scroll, glStateBits_t bits)
{
    float speed = 1.6f;

    if (bits & (GLS_SCROLL_X | GLS_SCROLL_Y))
        speed = 0.78125f;
    else if (bits & GLS_SCROLL_SLOW)
        speed = 0.5f;

    if (bits & GLS_SCROLL_FLIP)
        speed = -speed;

    speed *= glr.fd.time;

    if (bits & GLS_SCROLL_Y) {
        scroll[0] = 0;
        scroll[1] = speed;
    } else {
        scroll[0] = -speed;
        scroll[1] = 0;
    }
}

void GL_Ortho(GLfloat xmin, GLfloat xmax, GLfloat ymin, GLfloat ymax, GLfloat znear, GLfloat zfar)
{
    GLfloat width, height, depth;
    mat4_t matrix;

    width  = xmax - xmin;
    height = ymax - ymin;
    depth  = zfar - znear;

    matrix[ 0] = 2 / width;
    matrix[ 4] = 0;
    matrix[ 8] = 0;
    matrix[12] = -(xmax + xmin) / width;

    matrix[ 1] = 0;
    matrix[ 5] = 2 / height;
    matrix[ 9] = 0;
    matrix[13] = -(ymax + ymin) / height;

    matrix[ 2] = 0;
    matrix[ 6] = 0;
    matrix[10] = -2 / depth;
    matrix[14] = -(zfar + znear) / depth;

    matrix[ 3] = 0;
    matrix[ 7] = 0;
    matrix[11] = 0;
    matrix[15] = 1;

    gl_backend->load_matrix(GL_PROJECTION, matrix, gl_identity);
}

void GL_Setup2D(void)
{
    qglViewport(0, 0, r_config.width, r_config.height);

    GL_Ortho(0, r_config.width, r_config.height, 0, -1, 1);
    draw.scale = 1;

    if (draw.scissor) {
        qglDisable(GL_SCISSOR_TEST);
        draw.scissor = false;
    }

    if (gl_backend->setup_2d)
        gl_backend->setup_2d();

    gl_backend->load_matrix(GL_MODELVIEW, gl_identity, gl_identity);
}

void GL_Frustum(GLfloat fov_x, GLfloat fov_y, GLfloat reflect_x)
{
    mat4_t matrix;

    float znear = gl_znear->value, zfar;

    if (glr.fd.rdflags & RDF_NOWORLDMODEL)
        zfar = 2048;
    else
        zfar = gl_static.world.size * 2;

    glr.view_znear = znear;
    glr.view_zfar = zfar;

    Matrix_Frustum(fov_x, fov_y, reflect_x, znear, zfar, matrix);
    for (int i = 0; i < 16; ++i)
        glr.projmatrix[i] = matrix[i];
    gl_backend->load_matrix(GL_PROJECTION, matrix, gl_identity);
}

static void GL_RotateForViewer(void)
{
    GLfloat *matrix = glr.viewmatrix;

    AnglesToAxis(glr.fd.viewangles, glr.viewaxis);

    Matrix_FromOriginAxis(glr.fd.vieworg, glr.viewaxis, matrix);

    GL_ForceMatrix(glr.entmatrix, matrix);
}

void GL_Setup3D(void)
{
    if (glr.framebuffer_bound)
        qglViewport(0, 0, glr.fd.width, glr.fd.height);
    else
        qglViewport(glr.fd.x, r_config.height - (glr.fd.y + glr.fd.height),
                    glr.fd.width, glr.fd.height);

    if (gl_backend->setup_3d)
        gl_backend->setup_3d();

    GL_Frustum(glr.fd.fov_x, glr.fd.fov_y, 1.0f);

    GL_RotateForViewer();

    if (gl_static.use_shaders) {
        Matrix_Multiply(glr.projmatrix, glr.viewmatrix, glr.view_proj_matrix);
        glr.view_proj_valid = Matrix_Invert(glr.view_proj_matrix, glr.inv_view_proj_matrix);

        const float viewport_width = glr.fd.width > 0 ? static_cast<float>(glr.fd.width) : 1.0f;
        const float viewport_height = glr.fd.height > 0 ? static_cast<float>(glr.fd.height) : 1.0f;
        const float min_ndc_threshold = (std::max)(glr.motion_blur_min_velocity, 0.0f);
        const float min_pixel_threshold = (std::max)(glr.motion_blur_min_velocity_pixels, 0.0f);

        bool motion_ready = false;
        if (glr.motion_blur_enabled && glr.prev_view_proj_valid && glr.view_proj_valid && glr.motion_blur_scale > 0.0f) {
            mat4_t current_to_prev;
            Matrix_Multiply(glr.prev_view_proj_matrix, glr.inv_view_proj_matrix, current_to_prev);

            float max_ndc_velocity = 0.0f;
            float max_pixel_velocity = 0.0f;
            const int grid = R_MOTION_BLUR_GRID_SIZE;
            const float inv_grid = 1.0f / static_cast<float>(grid);

            // Sample a coarse 20x20 grid of clip-space points to estimate motion strength
            // in a way that mirrors the rerelease's tiled velocity analysis.
            for (int gy = 0; gy < grid; ++gy) {
                const float v = (static_cast<float>(gy) + 0.5f) * inv_grid;
                const float ndc_y = v * 2.0f - 1.0f;
                for (int gx = 0; gx < grid; ++gx) {
                    const float u = (static_cast<float>(gx) + 0.5f) * inv_grid;
                    const float ndc_x = u * 2.0f - 1.0f;

                    vec4_t current_clip = { ndc_x, ndc_y, 0.0f, 1.0f };
                    vec4_t previous_clip;
                    Matrix_TransformVec4(current_clip, current_to_prev, previous_clip);

                    const float curr_w = current_clip[3];
                    const float prev_w = previous_clip[3];
                    if (std::fabs(curr_w) <= 1.0e-6f || std::fabs(prev_w) <= 1.0e-6f)
                        continue;

                    const float inv_curr_w = 1.0f / curr_w;
                    const float inv_prev_w = 1.0f / prev_w;
                    const float current_ndc_x = current_clip[0] * inv_curr_w;
                    const float current_ndc_y = current_clip[1] * inv_curr_w;
                    const float prev_ndc_x = previous_clip[0] * inv_prev_w;
                    const float prev_ndc_y = previous_clip[1] * inv_prev_w;
                    const float velocity_x = (current_ndc_x - prev_ndc_x) * 0.5f;
                    const float velocity_y = (current_ndc_y - prev_ndc_y) * 0.5f;

                    const float base_ndc_speed = std::sqrt(velocity_x * velocity_x + velocity_y * velocity_y);
                    const float scaled_ndc_speed = base_ndc_speed * glr.motion_blur_scale;
                    if (scaled_ndc_speed > max_ndc_velocity)
                        max_ndc_velocity = scaled_ndc_speed;

                    const float pixel_velocity_x = velocity_x * viewport_width;
                    const float pixel_velocity_y = velocity_y * viewport_height;
                    const float base_pixel_speed = std::sqrt(pixel_velocity_x * pixel_velocity_x + pixel_velocity_y * pixel_velocity_y);
                    const float scaled_pixel_speed = base_pixel_speed * glr.motion_blur_scale;
                    if (scaled_pixel_speed > max_pixel_velocity)
                        max_pixel_velocity = scaled_pixel_speed;
                }
            }

            motion_ready = (max_ndc_velocity >= min_ndc_threshold) || (max_pixel_velocity >= min_pixel_threshold);
        }
        glr.motion_blur_ready = motion_ready;

        if (glr.view_proj_valid) {
            for (int i = 0; i < 16; ++i)
                gls.u_block.motion_inv_view_proj[i] = glr.inv_view_proj_matrix[i];
        } else {
            for (int i = 0; i < 16; ++i)
                gls.u_block.motion_inv_view_proj[i] = gl_identity[i];
            glr.motion_blur_ready = false;
        }

        if (glr.prev_view_proj_valid) {
            for (int i = 0; i < 16; ++i)
                gls.u_block.motion_prev_view_proj[i] = glr.prev_view_proj_matrix[i];
        } else {
            for (int i = 0; i < 16; ++i)
                gls.u_block.motion_prev_view_proj[i] = gl_identity[i];
        }

        const float blur_strength = glr.motion_blur_ready ? Q_bound(0.0f, glr.motion_blur_scale, 1.0f) : 0.0f;
        gls.u_block.motion_params[0] = blur_strength;
        gls.u_block.motion_params[1] = 0.0f;
        gls.u_block.motion_params[2] = 0.0f;
        gls.u_block.motion_params[3] = 0.0f;

        float search_radius_pixels = 0.0f;
        if (glr.fd.width > 0 && glr.fd.height > 0) {
            const float viewport_max = (std::max)(viewport_width, viewport_height);
            search_radius_pixels = Q_bound(1.0f, viewport_max * 0.0025f, 8.0f);
        }
        const float jitter = std::fmod(static_cast<float>(glr.drawframe) * 0.61803398875f, 1.0f);

        gls.u_block.motion_thresholds[0] = min_ndc_threshold;
        gls.u_block.motion_thresholds[1] = min_pixel_threshold;
        gls.u_block.motion_thresholds[2] = search_radius_pixels;
        gls.u_block.motion_thresholds[3] = jitter;

        float history_weights[R_MOTION_BLUR_HISTORY_FRAMES] = { 0.0f, 0.0f, 0.0f };
        int history_indices[R_MOTION_BLUR_HISTORY_FRAMES] = { 0, 0, 0 };
        int valid_history = 0;

        if (glr.motion_blur_enabled && glr.view_proj_valid && glr.motion_blur_history_count > 0) {
            const int max_frames = (std::min)(glr.motion_blur_history_count, R_MOTION_BLUR_HISTORY_FRAMES);
            for (int i = 0; i < max_frames; ++i) {
                int slot = (glr.motion_blur_history_index + R_MOTION_BLUR_HISTORY_FRAMES - glr.motion_blur_history_count + i) % R_MOTION_BLUR_HISTORY_FRAMES;
                if (!glr.motion_history_valid[slot])
                    continue;
                history_indices[valid_history++] = slot;
            }
        }

        const float current_weight = (std::max)(1.0f - blur_strength, 0.0f);
        const float history_total = blur_strength;
        const float per_history_weight = (valid_history > 0) ? history_total / static_cast<float>(valid_history) : 0.0f;
        for (int i = 0; i < valid_history; ++i)
            history_weights[i] = per_history_weight;

        gls.u_block.motion_history_params[0] = static_cast<float>(valid_history);
        gls.u_block.motion_history_params[1] = current_weight;
        gls.u_block.motion_history_params[2] = blur_strength;
        gls.u_block.motion_history_params[3] = 0.0f;

        gls.u_block.motion_history_weights[0] = history_weights[0];
        gls.u_block.motion_history_weights[1] = history_weights[1];
        gls.u_block.motion_history_weights[2] = history_weights[2];
        gls.u_block.motion_history_weights[3] = 0.0f;

        for (int i = 0; i < R_MOTION_BLUR_HISTORY_FRAMES; ++i) {
            const float *source = gl_identity;
            if (i < valid_history) {
                const int slot = history_indices[i];
                source = glr.motion_history_view_proj[slot];
            }
            for (int j = 0; j < 16; ++j)
                gls.u_block.motion_history_view_proj[i][j] = source[j];
        }
        gls.u_block_dirty = true;
    } else {
        glr.view_proj_valid = false;
        glr.motion_blur_ready = false;
        gls.u_block.motion_thresholds[0] = 0.0f;
        gls.u_block.motion_thresholds[1] = 0.0f;
        gls.u_block.motion_thresholds[2] = 0.0f;
        gls.u_block.motion_thresholds[3] = 0.0f;
        gls.u_block.motion_params[0] = 0.0f;
        gls.u_block.motion_params[1] = 0.0f;
        gls.u_block.motion_params[2] = 0.0f;
        gls.u_block.motion_params[3] = 0.0f;
        gls.u_block.motion_history_params[0] = 0.0f;
        gls.u_block.motion_history_params[1] = 0.0f;
        gls.u_block.motion_history_params[2] = 0.0f;
        gls.u_block.motion_history_params[3] = 0.0f;
        gls.u_block.motion_history_weights[0] = 0.0f;
        gls.u_block.motion_history_weights[1] = 0.0f;
        gls.u_block.motion_history_weights[2] = 0.0f;
        gls.u_block.motion_history_weights[3] = 0.0f;
        for (int i = 0; i < R_MOTION_BLUR_HISTORY_FRAMES; ++i) {
            for (int j = 0; j < 16; ++j)
                gls.u_block.motion_history_view_proj[i][j] = gl_identity[j];
        }
    }

    // enable depth writes before clearing
    GL_StateBits(GLS_DEFAULT);

    // clear both wanted & active dlight bits
    gls.dlight_bits = glr.ppl_dlight_bits = 0;

    qglClear(GL_DEPTH_BUFFER_BIT | gl_static.stencil_buffer_bit);
}

void GL_DrawOutlines(GLsizei count, GLenum type, const void *indices)
{
    GL_BindTexture(TMU_TEXTURE, TEXNUM_WHITE);
    GL_StateBits(GLS_DEPTHMASK_FALSE | GLS_TEXTURE_REPLACE | (gls.state_bits & GLS_MESH_MASK));
    if (gls.currentva)
        GL_ArrayBits(GLA_VERTEX);
    GL_DepthRange(0, 0);

    if (qglPolygonMode) {
        qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        if (type)
            qglDrawElements(GL_TRIANGLES, count, type, indices);
        else
            qglDrawArrays(GL_TRIANGLES, 0, count);

        qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    } else if (type) {
        uintptr_t base = (uintptr_t)indices;
        uintptr_t size = 0;

        switch (type) {
        case GL_UNSIGNED_INT:
            size = 4 * 3;
            break;
        case GL_UNSIGNED_SHORT:
            size = 2 * 3;
            break;
        default:
            Q_assert(!"bad type");
        }

        for (int i = 0; i < count / 3; i++, base += size)
            qglDrawElements(GL_LINE_LOOP, 3, type, VBO_OFS(base));
    } else {
        for (int i = 0; i < count / 3; i++)
            qglDrawArrays(GL_LINE_LOOP, i * 3, 3);
    }

    GL_DepthRange(0, 1);
}

void GL_ClearState(void)
{
    qglClearColor(Vector4Unpack(gl_static.clearcolor));
    GL_ClearDepth(1);
    qglClearStencil(0);

    qglEnable(GL_DEPTH_TEST);
    qglDepthFunc(GL_LEQUAL);
    GL_DepthRange(0, 1);
    qglDepthMask(GL_TRUE);
    qglDisable(GL_BLEND);
    qglFrontFace(GL_CW);
    qglCullFace(GL_BACK);
    qglEnable(GL_CULL_FACE);

    // unbind buffers
    if (qglBindBuffer) {
        qglBindBuffer(GL_ARRAY_BUFFER, 0);
        qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    gl_backend->clear_state();

    qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | gl_static.stencil_buffer_bit);

    memset(&gls, 0, sizeof(gls));
    GL_ShowErrors(__func__);
}

extern const glbackend_t backend_legacy;
extern const glbackend_t backend_shader;

void GL_InitState(void)
{
    gl_static.use_shaders = gl_shaders->integer > 0;

    if (gl_static.use_shaders) {
        if (!(gl_config.caps & QGL_CAP_SHADER)) {
            Com_WPrintf("GLSL rendering backend not available.\n");
            gl_static.use_shaders = false;
            Cvar_Set("gl_shaders", "0");
        }
    } else {
        if (!(gl_config.caps & QGL_CAP_LEGACY)) {
            Com_WPrintf("Legacy rendering backend not available.\n");
            gl_static.use_shaders = true;
            Cvar_Set("gl_shaders", "1");
        }
    }

    gl_shaders_modified = gl_shaders->modified_count;

    gl_backend = gl_static.use_shaders ? &backend_shader : &backend_legacy;
    gl_backend->init();

    if (!R_ShadowAtlasInit()) {
        Com_DPrintf("Shadow atlas initialization failed; disabling shadow atlas support.\n");
    }

    Com_Printf("Using %s rendering backend.\n", gl_backend->name);
}

void GL_ShutdownState(void)
{
    R_ShadowAtlasShutdown();

    if (gl_backend) {
        gl_backend->shutdown();
        gl_backend = NULL;
    }
}
