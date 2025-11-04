#include "hdr_luminance.hpp"

extern void GL_PostProcess(glStateBits_t bits, int x, int y, int w, int h);

#include <algorithm>

namespace {

	constexpr GLenum kColorAttachment = GL_COLOR_ATTACHMENT0;

	static void setupTexture(GLuint texture, int width, int height)
	{
		qglBindTexture(GL_TEXTURE_2D, texture);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		const GLenum internal = gl_static.postprocess_internal_format ? gl_static.postprocess_internal_format : GL_RGBA16F;
		const GLenum format = gl_static.postprocess_format ? gl_static.postprocess_format : GL_RGBA;
		const GLenum type = gl_static.postprocess_type ? gl_static.postprocess_type : GL_HALF_FLOAT;

		qglTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, format, type, nullptr);
	}

	static bool attachFramebuffer(GLuint fbo, GLuint texture, int width, int height)
	{
		qglBindFramebuffer(GL_FRAMEBUFFER, fbo);
		qglFramebufferTexture2D(GL_FRAMEBUFFER, kColorAttachment, GL_TEXTURE_2D, texture, 0);

		GLenum status = qglCheckFramebufferStatus(GL_FRAMEBUFFER);
		qglBindFramebuffer(GL_FRAMEBUFFER, 0);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			if (gl_showerrors->integer)
				Com_EPrintf("HDR luminance framebuffer status %#x\n", status);
			return false;
		}

		return true;
	}

	static void restoreFramebuffer(GLint previous)
	{
		if (previous >= 0)
			qglBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
	}

} // namespace

HdrLuminanceReducer g_hdr_luminance;

void HdrLuminanceReducer::destroyLevels() noexcept
{
	if (!levels_.empty()) {
		std::vector<GLuint> textures;
		std::vector<GLuint> framebuffers;
		textures.reserve(levels_.size());
		framebuffers.reserve(levels_.size());

		for (const Level& level : levels_) {
			if (level.texture)
				textures.push_back(level.texture);
			if (level.fbo)
				framebuffers.push_back(level.fbo);
		}

		if (!textures.empty())
			qglDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
		if (!framebuffers.empty())
			qglDeleteFramebuffers(static_cast<GLsizei>(framebuffers.size()), framebuffers.data());
	}

	levels_.clear();
	source_width_ = 0;
	source_height_ = 0;
	has_result_ = false;
}

void HdrLuminanceReducer::shutdown() noexcept
{
	destroyLevels();
}

bool HdrLuminanceReducer::resize(int width, int height) noexcept
{
	if (width == source_width_ && height == source_height_)
		return !levels_.empty();

	destroyLevels();

	if (width <= 0 || height <= 0)
		return false;

	source_width_ = width;
	source_height_ = height;

	int current_w = width;
	int current_h = height;

	while (current_w > 1 || current_h > 1) {
		Level level;
		level.width = (std::max)(1, current_w / 2);
		level.height = (std::max)(1, current_h / 2);
		levels_.push_back(level);
		current_w = level.width;
		current_h = level.height;
	}

	if (levels_.empty()) {
		Level level;
		level.width = 1;
		level.height = 1;
		levels_.push_back(level);
	}

	std::vector<GLuint> textures(levels_.size(), 0);
	std::vector<GLuint> framebuffers(levels_.size(), 0);
	qglGenTextures(static_cast<GLsizei>(textures.size()), textures.data());
	qglGenFramebuffers(static_cast<GLsizei>(framebuffers.size()), framebuffers.data());

	for (size_t i = 0; i < levels_.size(); ++i) {
		Level& level = levels_[i];
		level.texture = textures[i];
		level.fbo = framebuffers[i];
		setupTexture(level.texture, level.width, level.height);
		if (!attachFramebuffer(level.fbo, level.texture, level.width, level.height)) {
			destroyLevels();
			return false;
		}
	}

	has_result_ = false;
	return true;
}

bool HdrLuminanceReducer::ensureSize(int width, int height) noexcept
{
	if (width <= 0 || height <= 0)
		return false;

	if (width == source_width_ && height == source_height_ && !levels_.empty())
		return true;

	return resize(width, height);
}

bool HdrLuminanceReducer::reduce(GLuint sceneTexture, int width, int height) noexcept
{
	if (!ensureSize(width, height)) {
		has_result_ = false;
		return false;
	}

	if (levels_.empty()) {
		has_result_ = false;
		return false;
	}

	GLint prev_fbo = 0;
	GLint viewport[4] = { 0, 0, 0, 0 };
	qglGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
	qglGetIntegerv(GL_VIEWPORT, viewport);

	GLuint current_texture = sceneTexture;
	int current_w = width;
	int current_h = height;

	for (Level& level : levels_) {
		const float inv_w = current_w > 0 ? 0.5f / static_cast<float>(current_w) : 0.0f;
		const float inv_h = current_h > 0 ? 0.5f / static_cast<float>(current_h) : 0.0f;

		qglViewport(0, 0, level.width, level.height);
		GL_Ortho(0, level.width, level.height, 0, -1, 1);

		gls.u_block.hdr_reduce_params[0] = inv_w;
		gls.u_block.hdr_reduce_params[1] = inv_h;
		gls.u_block.hdr_reduce_params[2] = static_cast<float>(current_w);
		gls.u_block.hdr_reduce_params[3] = static_cast<float>(current_h);
		gls.u_block_dirty = true;

		GL_ForceTexture(TMU_TEXTURE, current_texture);
		qglBindFramebuffer(GL_FRAMEBUFFER, level.fbo);
		GL_PostProcess(GLS_HDR_REDUCE, 0, 0, level.width, level.height);

		current_texture = level.texture;
		current_w = level.width;
		current_h = level.height;
	}

	restoreFramebuffer(prev_fbo);
	GL_ForceTexture(TMU_TEXTURE, sceneTexture);
	qglViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

	has_result_ = true;
	return true;
}

GLuint HdrLuminanceReducer::resultTexture() const noexcept
{
	if (levels_.empty())
		return 0;
	return levels_.back().texture;
}

bool HdrLuminanceReducer::readbackAverage(float* rgba) const noexcept
{
	if (!rgba || !has_result_ || levels_.empty())
		return false;

	const Level& level = levels_.back();
        GLint prev_fbo = 0;
        GLint prev_read_buffer = 0;
        qglGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        if (qglReadBuffer)
                qglGetIntegerv(GL_READ_BUFFER, &prev_read_buffer);

        qglBindFramebuffer(GL_FRAMEBUFFER, level.fbo);
        if (qglReadBuffer)
                qglReadBuffer(kColorAttachment);
        qglReadPixels(0, 0, level.width, level.height, GL_RGBA, GL_FLOAT, rgba);
        restoreFramebuffer(prev_fbo);
        if (qglReadBuffer)
                qglReadBuffer(prev_read_buffer);
        return true;
}

bool HdrLuminanceReducer::readbackHistogram(int maxSamples, std::vector<float>& scratch, int& outWidth, int& outHeight) const noexcept
{
	if (!has_result_ || levels_.empty())
		return false;

	const Level* target = nullptr;
	for (const Level& level : levels_) {
		if (level.width <= maxSamples && level.height <= maxSamples) {
			target = &level;
			break;
		}
	}

	if (!target)
		target = &levels_[levels_.size() > 1 ? levels_.size() - 2 : 0];

	if (!target)
		return false;

	const size_t total_pixels = static_cast<size_t>(target->width) * target->height;
	scratch.resize(total_pixels * 4);

        GLint prev_fbo = 0;
        GLint prev_read_buffer = 0;
        qglGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        if (qglReadBuffer)
                qglGetIntegerv(GL_READ_BUFFER, &prev_read_buffer);

        qglBindFramebuffer(GL_FRAMEBUFFER, target->fbo);
        if (qglReadBuffer)
                qglReadBuffer(kColorAttachment);
        qglReadPixels(0, 0, target->width, target->height, GL_RGBA, GL_FLOAT, scratch.data());
        restoreFramebuffer(prev_fbo);
        if (qglReadBuffer)
                qglReadBuffer(prev_read_buffer);

        outWidth = target->width;
        outHeight = target->height;
	return true;
}

