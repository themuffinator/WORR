#pragma once

#include "../gl.hpp"

#include <vector>

class HdrLuminanceReducer {
public:
	HdrLuminanceReducer() noexcept = default;

	void shutdown() noexcept;
	bool resize(int width, int height) noexcept;

	bool available() const noexcept { return !levels_.empty(); }
	bool reduce(GLuint sceneTexture, int width, int height) noexcept;

	bool readbackAverage(float* rgba) const noexcept;
	bool readbackHistogram(int maxSamples, std::vector<float>& scratch, int& outWidth, int& outHeight) const noexcept;

	GLuint resultTexture() const noexcept;

private:
	struct Level {
		GLuint texture = 0;
		GLuint fbo = 0;
		int width = 0;
		int height = 0;
	};

	void destroyLevels() noexcept;
	bool ensureSize(int width, int height) noexcept;

	std::vector<Level> levels_;
	int source_width_ = 0;
	int source_height_ = 0;
	mutable bool has_result_ = false;
};

extern HdrLuminanceReducer g_hdr_luminance;

