#pragma once

#include "../gl.hpp"

struct BloomRenderContext {
	GLuint sceneTexture;
	GLuint bloomTexture;
	GLuint dofTexture;
	GLuint depthTexture;
	int viewportX;
	int viewportY;
	int viewportWidth;
	int viewportHeight;
	bool waterwarp;
	bool depthOfField;
	bool showDebug;
	bool tonemap;
	bool motionBlurReady;
	void (*updateHdrUniforms)();
	void (*runDepthOfField)();
};

class BloomEffect {
public:
	BloomEffect() noexcept;
	~BloomEffect();

	void initialize();
	void shutdown();

	void resize(int sceneWidth, int sceneHeight);
	void render(const BloomRenderContext& ctx);

	[[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

private:
	enum TextureSlot : size_t {
		Downsample,
		BrightPass,
		Blur0,
		Blur1,
		TextureCount
	};

	enum FramebufferSlot : size_t {
		DownsampleFbo,
		BrightPassFbo,
		BlurFbo0,
		BlurFbo1,
		FramebufferCount
	};

	void destroyTextures();
	void destroyFramebuffers();
	void ensureInitialized();
	void allocateTexture(GLuint tex, int width, int height) const;
	bool attachFramebuffer(GLuint fbo, GLuint texture, int width, int height, const char* name) const;

	GLuint textures_[TextureCount];
	GLuint framebuffers_[FramebufferCount];
	int sceneWidth_;
	int sceneHeight_;
	int downsampleWidth_;
	int downsampleHeight_;
	bool initialized_;
};

extern BloomEffect g_bloom_effect;

