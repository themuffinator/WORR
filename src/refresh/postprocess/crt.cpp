#include "crt.hpp"

#include <algorithm>
#include <cmath>

extern cvar_t* r_crtmode;
extern cvar_t* r_crt_hardScan;
extern cvar_t* r_crt_hardPix;
extern cvar_t* r_crt_maskDark;
extern cvar_t* r_crt_maskLight;
extern cvar_t* r_crt_scaleInLinearGamma;
extern cvar_t* r_crt_shadowMask;
extern cvar_t* r_crt_brightBoost;
extern cvar_t* r_crt_warpX;
extern cvar_t* r_crt_warpY;

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

        struct CrtStateCache {
                CrtConfig config{};
                int width = 0;
                int height = 0;
                bool initialized = false;
        };

        [[nodiscard]] bool nearly_equal(float lhs, float rhs) noexcept
        {
                const float diff = std::fabs(lhs - rhs);
                const float scale = (std::max)((std::max)(std::fabs(lhs), std::fabs(rhs)), 1.0f);
                return diff <= scale * 1.0e-6f;
        }

        [[nodiscard]] bool config_changed(const CrtConfig& lhs, const CrtConfig& rhs) noexcept
        {
                return !nearly_equal(lhs.hardScan, rhs.hardScan) ||
                        !nearly_equal(lhs.hardPix, rhs.hardPix) ||
                        !nearly_equal(lhs.maskDark, rhs.maskDark) ||
                        !nearly_equal(lhs.maskLight, rhs.maskLight) ||
                        !nearly_equal(lhs.scaleInLinearGamma, rhs.scaleInLinearGamma) ||
                        !nearly_equal(lhs.shadowMask, rhs.shadowMask) ||
                        !nearly_equal(lhs.brightBoost, rhs.brightBoost) ||
                        !nearly_equal(lhs.warpX, rhs.warpX) ||
                        !nearly_equal(lhs.warpY, rhs.warpY);
        }

        [[nodiscard]] CrtConfig gather_config(int mode) noexcept
        {
                const bool warp_enabled = mode != 0;
                const float warpX = warp_enabled ? (r_crt_warpX ? r_crt_warpX->value : 0.031f) : 0.0f;
                const float warpY = warp_enabled ? (r_crt_warpY ? r_crt_warpY->value : 0.041f) : 0.0f;

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

/*
=============
R_CRTEnabled

Determines whether CRT post-processing is active.
=============
*/
[[nodiscard]] bool R_CRTEnabled() noexcept
{
	return gl_static.use_shaders && r_postProcessing && r_postProcessing->integer &&
		r_fbo && r_fbo->integer && r_crtmode && r_crtmode->integer == 1;
}

glStateBits_t R_CRTPrepare(glStateBits_t bits, int viewportWidth, int viewportHeight)
{
        if (!R_CRTEnabled())
                return bits;

        static CrtStateCache cache;

        const auto mode = r_crtmode->integer;
        const CrtConfig config = gather_config(mode);

        const int width = (std::max)(viewportWidth, 1);
        const int height = (std::max)(viewportHeight, 1);

        const bool changed = !cache.initialized || config_changed(config, cache.config);
        const bool viewport_changed = !cache.initialized || cache.width != width || cache.height != height;

        if (changed || viewport_changed) {
                const float width_f = static_cast<float>(width);
                const float height_f = static_cast<float>(height);
                const float invWidth = 1.0f / width_f;
                const float invHeight = 1.0f / height_f;

                Vector4Set(gls.u_block.crt_params0, config.hardScan, config.hardPix, config.maskDark, config.maskLight);
                Vector4Set(gls.u_block.crt_params1, config.warpX, config.warpY, config.scaleInLinearGamma, config.shadowMask);
                Vector4Set(gls.u_block.crt_params2, config.brightBoost, 0.0f, 0.0f, 0.0f);
                Vector4Set(gls.u_block.crt_screen, width_f, height_f, invWidth, invHeight);
                gls.u_block_dirty = true;

                cache.config = config;
                cache.width = width;
                cache.height = height;
                cache.initialized = true;
        }

        return bits | GLS_CRT_ENABLE;
}
