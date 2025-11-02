#include "postprocess/crt.hpp"

#include "refresh/gl.hpp"

#include <algorithm>

static cvar_t *r_crt;
static cvar_t *r_crt_hardScan;
static cvar_t *r_crt_hardPix;
static cvar_t *r_crt_maskDark;
static cvar_t *r_crt_maskLight;
static cvar_t *r_crt_brightBoost;
static cvar_t *r_crt_linearGamma;
static cvar_t *r_crt_shadowMask;
static cvar_t *r_crt_warpX;
static cvar_t *r_crt_warpY;

void CRT_RegisterCvars()
{
    r_crt = Cvar_Get("r_crt", "0", 0);
    r_crt_hardScan = Cvar_Get("r_crt_hardScan", "-8.0", 0);
    r_crt_hardPix = Cvar_Get("r_crt_hardPix", "-3.0", 0);
    r_crt_maskDark = Cvar_Get("r_crt_maskDark", "0.5", 0);
    r_crt_maskLight = Cvar_Get("r_crt_maskLight", "1.5", 0);
    r_crt_brightBoost = Cvar_Get("r_crt_brightBoost", "1.2", 0);
    r_crt_linearGamma = Cvar_Get("r_crt_linearGamma", "1", 0);
    r_crt_shadowMask = Cvar_Get("r_crt_shadowMask", "1", 0);
    r_crt_warpX = Cvar_Get("r_crt_warpX", "0.031", 0);
    r_crt_warpY = Cvar_Get("r_crt_warpY", "0.041", 0);
}

bool CRT_IsEnabled()
{
    return r_crt && r_crt->integer != 0;
}

int32_t CRT_ModifiedCount()
{
    return r_crt ? r_crt->modified_count : 0;
}

void CRT_UpdateUniforms(int viewportWidth, int viewportHeight)
{
    if (!CRT_IsEnabled())
        return;

    const float width = static_cast<float>(std::max(viewportWidth, 1));
    const float height = static_cast<float>(std::max(viewportHeight, 1));

    gls.u_block.crt_params1[0] = r_crt_hardScan ? r_crt_hardScan->value : -8.0f;
    gls.u_block.crt_params1[1] = r_crt_hardPix ? r_crt_hardPix->value : -3.0f;
    gls.u_block.crt_params1[2] = r_crt_maskDark ? r_crt_maskDark->value : 0.5f;
    gls.u_block.crt_params1[3] = r_crt_maskLight ? r_crt_maskLight->value : 1.5f;

    gls.u_block.crt_params2[0] = r_crt_linearGamma && r_crt_linearGamma->integer ? 1.0f : 0.0f;
    const int shadowMask = r_crt_shadowMask ? std::clamp(r_crt_shadowMask->integer, 0, 4) : 1;
    gls.u_block.crt_params2[1] = static_cast<float>(shadowMask);
    gls.u_block.crt_params2[2] = r_crt_brightBoost ? r_crt_brightBoost->value : 1.2f;
    gls.u_block.crt_params2[3] = 0.0f;

    gls.u_block.crt_params3[0] = r_crt_warpX ? r_crt_warpX->value : 0.031f;
    gls.u_block.crt_params3[1] = r_crt_warpY ? r_crt_warpY->value : 0.041f;
    gls.u_block.crt_params3[2] = width;
    gls.u_block.crt_params3[3] = height;

    gls.u_block_dirty = true;
}
