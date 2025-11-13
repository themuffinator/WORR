#include "bloom.hpp"

#include <algorithm>

#include "../qgl.hpp"
#include "crt.hpp"

extern void GL_PostProcess(glStateBits_t bits, int x, int y, int w, int h);

BloomEffect g_bloom_effect;

namespace {

        constexpr GLenum kColorAttachment = GL_COLOR_ATTACHMENT0;

        void CompositeBloom(const BloomRenderContext& ctx, GLuint colorTexture, GLuint bloomTexture, bool applyBloom)
        {
                if (!ctx.viewportWidth || !ctx.viewportHeight || colorTexture == 0)
                        return;

                const bool showDebug = ctx.showDebug;
                const bool combineBloom = applyBloom && !showDebug && bloomTexture != 0;

                GL_Setup2D();

                glStateBits_t bits = showDebug ? GLS_DEFAULT : (combineBloom ? GLS_BLOOM_OUTPUT : GLS_DEFAULT);

                if (showDebug) {
                        const GLuint debugTexture = bloomTexture != 0 ? bloomTexture : colorTexture;
                        GL_ForceTexture(TMU_TEXTURE, debugTexture);
                }
                else {
                        GL_ForceTexture(TMU_TEXTURE, colorTexture);
                        if (combineBloom)
                                GL_ForceTexture(TMU_LIGHTMAP, bloomTexture);
                        if (ctx.waterwarp)
                                bits |= GLS_WARP_ENABLE;
                        if (ctx.updateHdrUniforms)
                                ctx.updateHdrUniforms();
                        if (ctx.tonemap)
                                bits |= GLS_TONEMAP_ENABLE;
                        bits = R_CRTPrepare(bits, ctx.viewportWidth, ctx.viewportHeight);
                        if (ctx.motionBlurReady) {
                                bits |= GLS_MOTION_BLUR;
                                GL_ForceTexture(TMU_GLOWMAP, ctx.depthTexture);
                        }
                }

                qglBindFramebuffer(GL_FRAMEBUFFER, 0);
                GL_PostProcess(bits, ctx.viewportX, ctx.viewportY, ctx.viewportWidth, ctx.viewportHeight);
        }

}

BloomEffect::BloomEffect() noexcept
        : textures_{},
        framebuffers_{},
        sceneWidth_(0),
        sceneHeight_(0),
        downsampleWidth_(0),
        downsampleHeight_(0),
        postprocessInternalFormat_(0),
        postprocessFormat_(0),
        postprocessType_(0),
        initialized_(false),
        resizeErrorLogged_(false)
{
}

BloomEffect::~BloomEffect()
{
        shutdown();
}

void BloomEffect::destroyTextures()
{
	for (GLuint& texture : textures_) {
		if (texture) {
			qglDeleteTextures(1, &texture);
			texture = 0;
		}
	}
}

void BloomEffect::destroyFramebuffers()
{
	for (GLuint& fbo : framebuffers_) {
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
        postprocessInternalFormat_ = 0;
        postprocessFormat_ = 0;
        postprocessType_ = 0;
        initialized_ = false;
        resizeErrorLogged_ = false;
}

/*
=============
BloomEffect::allocateTexture

Initializes a bloom texture while preserving the caller texture state.
=============
*/
void BloomEffect::allocateTexture(GLuint tex, int width, int height, GLenum internalFormat, GLenum format, GLenum type) const
{
	const glTmu_t prevActiveTmu = gls.server_tmu;
	const GLuint prevTexture = gls.texnums[TMU_TEXTURE];

	GL_ActiveTexture(TMU_TEXTURE);

	qglBindTexture(GL_TEXTURE_2D, tex);
	gls.texnums[TMU_TEXTURE] = tex;
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	qglTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr);

	qglBindTexture(GL_TEXTURE_2D, prevTexture);
	gls.texnums[TMU_TEXTURE] = prevTexture;
	GL_ActiveTexture(prevActiveTmu);
}

bool BloomEffect::attachFramebuffer(GLuint fbo, GLuint texture, int width, int height, const char* name) const
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

bool BloomEffect::resize(int sceneWidth, int sceneHeight)
{
        if (sceneWidth <= 0 || sceneHeight <= 0) {
                shutdown();
                return false;
        }

        ensureInitialized();
        if (!initialized_)
                return false;

        const float downscale = (std::max)(Cvar_ClampValue(r_bloomScale, 1.0f, 16.0f), 1.0f);
        const int downW = (std::max)(static_cast<int>(sceneWidth / downscale), 1);
        const int downH = (std::max)(static_cast<int>(sceneHeight / downscale), 1);
        const GLenum internalFormat = gl_static.postprocess_internal_format ? gl_static.postprocess_internal_format : GL_RGBA;
        const GLenum format = gl_static.postprocess_format ? gl_static.postprocess_format : GL_RGBA;
        const GLenum type = gl_static.postprocess_type ? gl_static.postprocess_type : GL_UNSIGNED_BYTE;

        if (sceneWidth_ == sceneWidth && sceneHeight_ == sceneHeight &&
                downsampleWidth_ == downW && downsampleHeight_ == downH &&
                postprocessInternalFormat_ == internalFormat && postprocessFormat_ == format &&
                postprocessType_ == type) {
                resizeErrorLogged_ = false;
                return true;
        }

        sceneWidth_ = sceneWidth;
        sceneHeight_ = sceneHeight;
        downsampleWidth_ = downW;
        downsampleHeight_ = downH;
        postprocessInternalFormat_ = internalFormat;
        postprocessFormat_ = format;
        postprocessType_ = type;

        allocateTexture(textures_[Downsample], downsampleWidth_, downsampleHeight_, internalFormat, format, type);
        allocateTexture(textures_[BrightPass], downsampleWidth_, downsampleHeight_, internalFormat, format, type);
        allocateTexture(textures_[Blur0], downsampleWidth_, downsampleHeight_, internalFormat, format, type);
        allocateTexture(textures_[Blur1], downsampleWidth_, downsampleHeight_, internalFormat, format, type);

        bool ok = true;
        ok &= attachFramebuffer(framebuffers_[DownsampleFbo], textures_[Downsample], downsampleWidth_, downsampleHeight_, "BLOOM_DOWNSAMPLE");
        ok &= attachFramebuffer(framebuffers_[BrightPassFbo], textures_[BrightPass], downsampleWidth_, downsampleHeight_, "BLOOM_BRIGHTPASS");
        ok &= attachFramebuffer(framebuffers_[BlurFbo0], textures_[Blur0], downsampleWidth_, downsampleHeight_, "BLOOM_BLUR0");
        ok &= attachFramebuffer(framebuffers_[BlurFbo1], textures_[Blur1], downsampleWidth_, downsampleHeight_, "BLOOM_BLUR1");

        if (!ok) {
                if (!resizeErrorLogged_ && gl_showerrors->integer)
                        Com_EPrintf("BloomEffect: failed to build framebuffers for %dx%d target\n", downsampleWidth_, downsampleHeight_);
                resizeErrorLogged_ = true;
                shutdown();
                return false;
        }

        resizeErrorLogged_ = false;
        return true;
}

/*
=============
BloomEffect::render

Renders the bloom post-processing passes while preserving the caller state.
=============
*/
void BloomEffect::render(const BloomRenderContext& ctx)
{
	GLint prevFbo = 0;
	GLint prevViewport[4] = { 0, 0, 0, 0 };
	qglGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
	qglGetIntegerv(GL_VIEWPORT, prevViewport);

	const GLuint prevTexture = gls.texnums[TMU_TEXTURE];
	const GLuint prevLightmap = gls.texnums[TMU_LIGHTMAP];
	const GLuint prevGlowmap = gls.texnums[TMU_GLOWMAP];
	const glTmu_t prevActiveTmu = gls.server_tmu;

	const auto restoreState = [&]() {
		if (prevFbo >= 0)
			qglBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
		qglViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
		GL_ForceTexture(TMU_TEXTURE, prevTexture);
		GL_ForceTexture(TMU_LIGHTMAP, prevLightmap);
		GL_ForceTexture(TMU_GLOWMAP, prevGlowmap);
		GL_ActiveTexture(prevActiveTmu);
	};

	if (ctx.viewportWidth <= 0 || ctx.viewportHeight <= 0) {
		restoreState();
		return;
	}

	const bool canRunDepthOfField = ctx.depthOfField && !ctx.showDebug && ctx.runDepthOfField;
	if (ctx.sceneTexture == 0) {
		if (canRunDepthOfField)
			ctx.runDepthOfField();
		CompositeBloom(ctx, ctx.sceneTexture, ctx.showDebug ? ctx.bloomTexture : 0, false);
		GL_ShowErrors("Bloom pass");
		restoreState();
		return;
	}

	const bool bloomReady = initialized_ && downsampleWidth_ > 0 && downsampleHeight_ > 0 && ctx.bloomTexture != 0;
	const auto runDepthOfField = [&]() {
		if (canRunDepthOfField)
			ctx.runDepthOfField();
	};

	if (!bloomReady) {
		runDepthOfField();
		const GLuint colorTexture = (canRunDepthOfField && ctx.dofTexture) ? ctx.dofTexture : ctx.sceneTexture;
		const GLuint debugTexture = ctx.showDebug ? ctx.bloomTexture : 0;
		CompositeBloom(ctx, colorTexture, debugTexture, false);
		GL_ShowErrors("Bloom pass");
		restoreState();
		return;
	}

	bool bloomFailed = false;

	qglViewport(0, 0, downsampleWidth_, downsampleHeight_);
	GL_Ortho(0, downsampleWidth_, downsampleHeight_, 0, -1, 1);

	const float invW = 1.0f / downsampleWidth_;
	const float invH = 1.0f / downsampleHeight_;
	const float blurScale = (std::max)(r_bloomBlurScale->value, 0.0f);
	const float bloomKnee = Cvar_ClampValue(r_bloomKnee, 0.0f, 5.0f);
	const bool useBlur = blurScale > 0.0f;
	const int passes = (std::max)(static_cast<int>(Cvar_ClampValue(r_bloomPasses, 1.0f, 8.0f)), 1);
	const int kernelMode = static_cast<int>(Cvar_ClampValue(r_bloomKernel, 0.0f, 1.0f));
	const glStateBits_t blurMode = kernelMode == 0 ? GLS_BLUR_GAUSS : GLS_BLUR_BOX;

	gls.u_block.bbr_params[0] = invW;
	gls.u_block.bbr_params[1] = invH;
	gls.u_block.bbr_params[2] = 0.0f;
	gls.u_block.bbr_params[3] = 0.0f;
	gls.u_block_dirty = true;
	GL_ForceTexture(TMU_TEXTURE, ctx.bloomTexture);
	qglBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[DownsampleFbo]);
	GL_PostProcess(GLS_BLUR_BOX, 0, 0, downsampleWidth_, downsampleHeight_);
	if (GL_ShowErrors("Bloom pass"))
		bloomFailed = true;

	if (!bloomFailed) {
		gls.u_block.bbr_params[0] = invW;
		gls.u_block.bbr_params[1] = invH;
		gls.u_block.bbr_params[2] = (std::max)(r_bloomBrightThreshold->value, 0.0f);
		gls.u_block.bbr_params[3] = bloomKnee;
		gls.u_block_dirty = true;
		GL_ForceTexture(TMU_TEXTURE, textures_[Downsample]);
		GL_ForceTexture(TMU_LIGHTMAP, ctx.sceneTexture);
		qglBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[BrightPassFbo]);
		GL_PostProcess(GLS_BLOOM_BRIGHTPASS, 0, 0, downsampleWidth_, downsampleHeight_);
		if (GL_ShowErrors("Bloom pass"))
			bloomFailed = true;
	}

	GLuint currentTexture = textures_[BrightPass];
	if (!bloomFailed && useBlur) {
		const float texelScale = blurScale;
		for (int pass = 0; pass < passes && !bloomFailed; ++pass) {
			for (int axis = 0; axis < 2; ++axis) {
				const bool horizontal = axis == 0;
				gls.u_block.bbr_params[0] = horizontal ? invW * texelScale : 0.0f;
				gls.u_block.bbr_params[1] = horizontal ? 0.0f : invH * texelScale;
				gls.u_block_dirty = true;
				GL_ForceTexture(TMU_TEXTURE, currentTexture);
				qglBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[BlurFbo0 + axis]);
				GL_PostProcess(blurMode, 0, 0, downsampleWidth_, downsampleHeight_);
				if (GL_ShowErrors("Bloom pass")) {
					bloomFailed = true;
					break;
				}
				currentTexture = textures_[Blur0 + axis];
			}
		}
	}

	if (bloomFailed) {
		runDepthOfField();
		const GLuint colorTexture = (canRunDepthOfField && ctx.dofTexture) ? ctx.dofTexture : ctx.sceneTexture;
		const GLuint debugTexture = ctx.showDebug ? ctx.bloomTexture : 0;
		CompositeBloom(ctx, colorTexture, debugTexture, false);
		GL_ShowErrors("Bloom pass");
		restoreState();
		return;
	}

	const GLuint bloomTexture = currentTexture;

	runDepthOfField();

	const GLuint colorTexture = (canRunDepthOfField && ctx.dofTexture) ? ctx.dofTexture : ctx.sceneTexture;
	CompositeBloom(ctx, colorTexture, bloomTexture, true);
	GL_ShowErrors("Bloom pass");
	restoreState();
}



