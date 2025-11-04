#include "crt.hpp"

#include <algorithm>

extern cvar_t* r_crtmode;
extern cvar_t* r_crt_hardScan;
extern cvar_t* r_crt_hardPix;
extern cvar_t* r_crt_maskDark;
extern cvar_t* r_crt_maskLight;
extern cvar_t* r_crt_scaleInLinearGamma;
extern cvar_t* r_crt_shadowMask;
extern cvar_t* r_crt_brightBoost;

namespace {
	struct CrtConfig {
		float hardScan;
		float hardPix;
		float maskDark;
		float maskLight;
		float scaleInLinearGamma;
		float shadowMask;
		float brightBoost;
		float warpX;
		float warpY;
	};

	[[nodiscard]] CrtConfig gather_config(int mode) noexcept
	{
		const float warpX = (mode == 1) ? 0.031f : 0.0f;
		const float warpY = (mode == 1) ? 0.041f : 0.0f;

		return CrtConfig{
			.hardScan = r_crt_hardScan ? r_crt_hardScan->value : -8.0f,
			.hardPix = r_crt_hardPix ? r_crt_hardPix->value : -3.0f,
			.maskDark = r_crt_maskDark ? r_crt_maskDark->value : 0.5f,
			.maskLight = r_crt_maskLight ? r_crt_maskLight->value : 1.5f,
			.scaleInLinearGamma = (r_crt_scaleInLinearGamma && r_crt_scaleInLinearGamma->integer) ? 1.0f : 0.0f,
			.shadowMask = r_crt_shadowMask ? std::clamp(r_crt_shadowMask->value, 0.0f, 4.0f) : 3.0f,
			.brightBoost = r_crt_brightBoost ? (std::max)(r_crt_brightBoost->value, 0.0f) : 1.0f,
			.warpX = warpX,
			.warpY = warpY,
		};
	}
} // namespace

[[nodiscard]] bool R_CRTEnabled() noexcept
{
	return gl_static.use_shaders && r_crtmode && r_crtmode->integer == 1;
}

glStateBits_t R_CRTPrepare(glStateBits_t bits, int viewportWidth, int viewportHeight)
{
	if (!R_CRTEnabled())
		return bits;

	const auto mode = r_crtmode->integer;
	const CrtConfig config = gather_config(mode);

	const float width = static_cast<float>((std::max)(viewportWidth, 1));
	const float height = static_cast<float>((std::max)(viewportHeight, 1));
	const float invWidth = 1.0f / width;
	const float invHeight = 1.0f / height;

	Vector4Set(gls.u_block.crt_params0, config.hardScan, config.hardPix, config.maskDark, config.maskLight);
	Vector4Set(gls.u_block.crt_params1, config.warpX, config.warpY, config.scaleInLinearGamma, config.shadowMask);
	Vector4Set(gls.u_block.crt_params2, config.brightBoost, 0.0f, 0.0f, 0.0f);
	Vector4Set(gls.u_block.crt_screen, width, height, invWidth, invHeight);
	gls.u_block_dirty = true;

	return bits | GLS_CRT_ENABLE;
}
