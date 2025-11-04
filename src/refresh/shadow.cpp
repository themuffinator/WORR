#include "gl.hpp"

#include <algorithm>

namespace {

constexpr int kShadowAtlasWidth = 8192;
constexpr int kShadowAtlasHeight = 4096;
constexpr int kShadowTilesPerRow = 16;
constexpr int kShadowTilesPerColumn = 16;
constexpr size_t kMaxShadowViews = MAX_SHADOW_VIEWS;

bool has_required_gl_capabilities()
{
    return qglGenTextures && qglDeleteTextures && qglBindTexture &&
           qglTexParameteri && qglTexImage2D && qglGenFramebuffers &&
           qglDeleteFramebuffers && qglBindFramebuffer &&
           qglFramebufferTexture2D && qglCheckFramebufferStatus;
}

void destroy_resources()
{
    auto &shadow = gl_static.shadow;

    if (shadow.framebuffer && qglDeleteFramebuffers) {
        qglDeleteFramebuffers(1, &shadow.framebuffer);
        shadow.framebuffer = 0;
    }

    if (shadow.texture && qglDeleteTextures) {
        qglDeleteTextures(1, &shadow.texture);
        shadow.texture = 0;
    }

    shadow.supported = false;
    shadow.view_count = 0;
}

void reset_frame_state()
{
    auto &shadow = gl_static.shadow;
    shadow.view_count = 0;

    for (auto &assignment : shadow.assignments) {
        assignment.valid = false;
        assignment.atlas_index = -1;
        assignment.face = -1;
        assignment.resolution = 0;
    }
}

void configure_texture_parameters(GLuint texture)
{
    qglBindTexture(GL_TEXTURE_2D, texture);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef GL_TEXTURE_COMPARE_MODE
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
#endif
    qglBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace

bool R_ShadowAtlasInit(void)
{
    auto &shadow = gl_static.shadow;

    shadow.width = kShadowAtlasWidth;
    shadow.height = kShadowAtlasHeight;

    if (!has_required_gl_capabilities()) {
        destroy_resources();
        return false;
    }

    destroy_resources();

    qglGenTextures(1, &shadow.texture);
    if (!shadow.texture) {
        destroy_resources();
        return false;
    }

    qglBindTexture(GL_TEXTURE_2D, shadow.texture);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                  shadow.width, shadow.height, 0, GL_DEPTH_COMPONENT,
                  GL_UNSIGNED_INT, nullptr);
    configure_texture_parameters(shadow.texture);

    qglGenFramebuffers(1, &shadow.framebuffer);
    if (!shadow.framebuffer) {
        destroy_resources();
        return false;
    }

    qglBindFramebuffer(GL_FRAMEBUFFER, shadow.framebuffer);
    qglFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_TEXTURE_2D, shadow.texture, 0);

    GLenum status = qglCheckFramebufferStatus(GL_FRAMEBUFFER);
    qglBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        destroy_resources();
        return false;
    }

    shadow.supported = true;
    reset_frame_state();

    return true;
}

void R_ShadowAtlasShutdown(void)
{
    destroy_resources();
}

void R_ShadowAtlasBeginFrame(void)
{
    if (!gl_static.shadow.supported)
        return;

    reset_frame_state();
}

bool R_ShadowAtlasAllocateView(const shadow_view_parameters_t &params,
                               int face, int resolution,
                               shadow_view_assignment_t *out_assignment)
{
    auto &shadow = gl_static.shadow;

    if (!shadow.supported)
        return false;

    if (shadow.view_count >= kMaxShadowViews)
        return false;

    const size_t index = shadow.view_count++;
    const int tile_width = shadow.width / kShadowTilesPerRow;
    const int tile_height = shadow.height / kShadowTilesPerColumn;
    const int column = static_cast<int>(index % kShadowTilesPerRow);
    const int row = static_cast<int>(index / kShadowTilesPerRow);
    const float x0 = static_cast<float>(column * tile_width);
    const float y0 = static_cast<float>(row * tile_height);
    const float x1 = x0 + static_cast<float>(tile_width);
    const float y1 = y0 + static_cast<float>(tile_height);

    shadow_view_assignment_t assignment{};
    assignment.parameters = params;
    Vector4Set(assignment.parameters.viewport_rect, x0, y0, x1, y1);
    Vector4Set(assignment.cube_face_offset, x0, y0,
               static_cast<float>(tile_width), static_cast<float>(tile_height));
    assignment.atlas_index = static_cast<int>(index);
    assignment.face = face;
    assignment.resolution = resolution;
    assignment.valid = true;

    shadow.assignments[index] = assignment;

    if (out_assignment)
        *out_assignment = assignment;

    return true;
}

size_t R_ShadowAtlasViewCount(void)
{
    return gl_static.shadow.view_count;
}

const shadow_view_assignment_t *R_ShadowAtlasViews(void)
{
    return gl_static.shadow.assignments.data();
}
