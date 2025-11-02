#include "refresh/postprocess/bloom.hpp"

#include <algorithm>
#include <cmath>

#include "refresh/qgl.hpp"

BloomEffect g_bloom_effect;

namespace {

constexpr GLenum kColorAttachment = GL_COLOR_ATTACHMENT0;
constexpr int kMaxBloomPasses = 8;
constexpr float kSceneSaturationEpsilon = 1e-4f;

inline void SetBlurDirection(float x, float y)
{
    gls.u_block.bloom_params[0] = x;
    gls.u_block.bloom_params[1] = y;
    gls.u_block_dirty = true;
}

inline int GetBloomPassCount()
{
    if (!r_bloomPasses)
        return 1;

    return std::clamp(r_bloomPasses->integer, 1, kMaxBloomPasses);
}

}

void R_SetPostProcessUniforms(float dirX, float dirY)
{
    const float threshold = std::max(r_bloomBrightThreshold ? r_bloomBrightThreshold->value : 0.0f, 0.0f);
    const float intensity = std::max(r_bloomIntensity ? r_bloomIntensity->value : 0.0f, 0.0f);
    const float bloomSaturation = std::max(r_bloomSaturation ? r_bloomSaturation->value : 0.0f, 0.0f);
    const float sceneSaturation = std::max(r_bloomSceneSaturation ? r_bloomSceneSaturation->value : 0.0f, 0.0f);
    const float colorStrength = std::max(r_colorCorrection ? r_colorCorrection->value : 0.0f, 0.0f);

    Vector4Set(gls.u_block.bloom_params, dirX, dirY, threshold, intensity);
    Vector4Set(gls.u_block.bloom_color, bloomSaturation, sceneSaturation, colorStrength, 0.0f);
    gls.u_block_dirty = true;
}

bool R_ColorCorrectionActive(void)
{
    const float sceneSaturation = r_bloomSceneSaturation ? r_bloomSceneSaturation->value : 1.0f;
    const float colorStrength = r_colorCorrection ? r_colorCorrection->value : 0.0f;

    return std::fabs(sceneSaturation - 1.0f) > kSceneSaturationEpsilon || colorStrength > 0.0f;
}

BloomEffect::BloomEffect() noexcept
    : textures_{},
      framebuffers_{},
      sceneWidth_(0),
      sceneHeight_(0),
      downsampleWidth_(0),
      downsampleHeight_(0),
      initialized_(false)
{
}

BloomEffect::~BloomEffect()
{
    shutdown();
}

void BloomEffect::destroyTextures()
{
    for (GLuint &texture : textures_) {
        if (texture) {
            qglDeleteTextures(1, &texture);
            texture = 0;
        }
    }
}

void BloomEffect::destroyFramebuffers()
{
    for (GLuint &fbo : framebuffers_) {
        if (fbo) {
            qglDeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
    }
}

void BloomEffect::initialize()
{
    if (initialized_)
        return;

    qglGenTextures(TextureCount, textures_);
    qglGenFramebuffers(FramebufferCount, framebuffers_);

    initialized_ = true;
}

void BloomEffect::ensureInitialized()
{
    if (!initialized_)
        initialize();
}

void BloomEffect::shutdown()
{
    if (!initialized_)
        return;

    destroyTextures();
    destroyFramebuffers();

    sceneWidth_ = 0;
    sceneHeight_ = 0;
    downsampleWidth_ = 0;
    downsampleHeight_ = 0;
    initialized_ = false;
}

void BloomEffect::allocateTexture(GLuint tex, int width, int height) const
{
    qglBindTexture(GL_TEXTURE_2D, tex);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

bool BloomEffect::attachFramebuffer(GLuint fbo, GLuint texture, int width, int height, const char *name) const
{
    qglBindFramebuffer(GL_FRAMEBUFFER, fbo);

    if (width > 0 && height > 0)
        qglFramebufferTexture2D(GL_FRAMEBUFFER, kColorAttachment, GL_TEXTURE_2D, texture, 0);
    else
        qglFramebufferTexture2D(GL_FRAMEBUFFER, kColorAttachment, GL_TEXTURE_2D, 0, 0);

    if (width <= 0 || height <= 0) {
        qglBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    GLenum status = qglCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        if (gl_showerrors->integer)
            Com_EPrintf("%s framebuffer status %#x\n", name, status);
        qglBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    qglBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void BloomEffect::resize(int sceneWidth, int sceneHeight)
{
    if (sceneWidth <= 0 || sceneHeight <= 0) {
        shutdown();
        return;
    }

    ensureInitialized();
    if (!initialized_)
        return;

    int downW = std::max(sceneWidth / 4, 1);
    int downH = std::max(sceneHeight / 4, 1);

    if (sceneWidth_ == sceneWidth && sceneHeight_ == sceneHeight &&
        downsampleWidth_ == downW && downsampleHeight_ == downH)
        return;

    sceneWidth_ = sceneWidth;
    sceneHeight_ = sceneHeight;
    downsampleWidth_ = downW;
    downsampleHeight_ = downH;

    allocateTexture(textures_[Downsample], downsampleWidth_, downsampleHeight_);
    allocateTexture(textures_[BrightPass], downsampleWidth_, downsampleHeight_);
    allocateTexture(textures_[Blur0], downsampleWidth_, downsampleHeight_);
    allocateTexture(textures_[Blur1], downsampleWidth_, downsampleHeight_);

    bool ok = true;
    ok &= attachFramebuffer(framebuffers_[DownsampleFbo], textures_[Downsample], downsampleWidth_, downsampleHeight_, "BLOOM_DOWNSAMPLE");
    ok &= attachFramebuffer(framebuffers_[BrightPassFbo], textures_[BrightPass], downsampleWidth_, downsampleHeight_, "BLOOM_BRIGHTPASS");
    ok &= attachFramebuffer(framebuffers_[BlurFbo0], textures_[Blur0], downsampleWidth_, downsampleHeight_, "BLOOM_BLUR0");
    ok &= attachFramebuffer(framebuffers_[BlurFbo1], textures_[Blur1], downsampleWidth_, downsampleHeight_, "BLOOM_BLUR1");

    if (!ok)
        shutdown();
}

extern void GL_PostProcess(glStateBits_t bits, int x, int y, int w, int h);

void BloomEffect::render(const BloomRenderContext &ctx)
{
    const bool bloomRequested = r_bloom && r_bloom->integer;
    const bool dofActive = ctx.depthOfField && !ctx.showDebug && ctx.runDepthOfField;
    const bool applyColorCorrection = !ctx.showDebug && R_ColorCorrectionActive();

    if (!bloomRequested || !initialized_ || downsampleWidth_ <= 0 || downsampleHeight_ <= 0) {
        if (dofActive)
            ctx.runDepthOfField();

        GL_Setup2D();

        glStateBits_t bits = GLS_DEFAULT;
        if (!ctx.showDebug) {
            if (ctx.waterwarp)
                bits |= GLS_WARP_ENABLE;
            if (applyColorCorrection)
                bits |= GLS_COLOR_CORRECTION;
            R_SetPostProcessUniforms(0.0f, 0.0f);
            GL_ForceTexture(TMU_TEXTURE, ctx.depthOfField ? ctx.dofTexture : ctx.sceneTexture);
        } else {
            GL_ForceTexture(TMU_TEXTURE, ctx.bloomTexture);
        }

        qglBindFramebuffer(GL_FRAMEBUFFER, 0);
        GL_PostProcess(bits, ctx.viewportX, ctx.viewportY, ctx.viewportWidth, ctx.viewportHeight);
        return;
    }

    qglViewport(0, 0, downsampleWidth_, downsampleHeight_);
    GL_Ortho(0, downsampleWidth_, downsampleHeight_, 0, -1, 1);

    const float invW = 1.0f / downsampleWidth_;
    const float invH = 1.0f / downsampleHeight_;

    R_SetPostProcessUniforms(invW, invH);

    GL_ForceTexture(TMU_TEXTURE, ctx.bloomTexture);
    qglBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[DownsampleFbo]);
    SetBlurDirection(invW, invH);
    GL_PostProcess(GLS_BLUR_BOX, 0, 0, downsampleWidth_, downsampleHeight_);

    GL_ForceTexture(TMU_TEXTURE, textures_[Downsample]);
    GL_ForceTexture(TMU_LIGHTMAP, ctx.sceneTexture);
    qglBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[BrightPassFbo]);
    SetBlurDirection(invW, invH);
    GL_PostProcess(GLS_BLOOM_BRIGHTPASS, 0, 0, downsampleWidth_, downsampleHeight_);

    GLuint currentTexture = textures_[BrightPass];
    const int passes = GetBloomPassCount();
    for (int pass = 0; pass < passes; ++pass) {
        for (int axis = 0; axis < 2; ++axis) {
            const bool horizontal = axis == 0;
            SetBlurDirection(horizontal ? invW : 0.0f, horizontal ? 0.0f : invH);
            GL_ForceTexture(TMU_TEXTURE, currentTexture);
            qglBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[BlurFbo0 + axis]);
            GL_PostProcess(GLS_BLUR_GAUSS, 0, 0, downsampleWidth_, downsampleHeight_);
            currentTexture = textures_[Blur0 + axis];
        }
    }

    const GLuint bloomTexture = currentTexture;

    if (dofActive)
        ctx.runDepthOfField();

    GL_Setup2D();

    glStateBits_t bits = GLS_DEFAULT;
    if (ctx.showDebug) {
        GL_ForceTexture(TMU_TEXTURE, bloomTexture);
    } else {
        R_SetPostProcessUniforms(0.0f, 0.0f);
        GL_ForceTexture(TMU_TEXTURE, ctx.depthOfField ? ctx.dofTexture : ctx.sceneTexture);
        GL_ForceTexture(TMU_LIGHTMAP, bloomTexture);
        bits |= GLS_BLOOM_OUTPUT;
        if (ctx.waterwarp)
            bits |= GLS_WARP_ENABLE;
        if (applyColorCorrection)
            bits |= GLS_COLOR_CORRECTION;
    }

    qglBindFramebuffer(GL_FRAMEBUFFER, 0);
    GL_PostProcess(bits, ctx.viewportX, ctx.viewportY, ctx.viewportWidth, ctx.viewportHeight);
}

