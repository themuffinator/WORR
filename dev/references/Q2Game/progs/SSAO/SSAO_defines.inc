///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of 
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File changes (yyyy-mm-dd)
// 2016-09-07: filip.strugar@intel.com: first commit
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ====================================================================
// Main defines

#define SSAO_ENABLE_NORMAL_WORLD_TO_VIEW_CONVERSION 1
#define INTELSSAO_MAIN_DISK_SAMPLE_COUNT (32)

struct ASSAOConstants
{
	vec2                    ViewportPixelSize;                      // .zw == 1.0 / ViewportSize.xy
    vec2                    HalfViewportPixelSize;                  // .zw == 1.0 / ViewportHalfSize.xy

    vec2                    DepthUnpackConsts;
    vec2                    CameraTanHalfFOV;

    vec2                    NDCToViewMul;
    vec2                    NDCToViewAdd;

    ivec2                   PerPassFullResCoordOffset;
    vec2                    PerPassFullResUVOffset;

    vec2                    Viewport2xPixelSize;
    vec2                    Viewport2xPixelSize_x_025;              // Viewport2xPixelSize * 0.25 (for fusing add+mul into mad)

    float                   EffectRadius;                           // world (viewspace) maximum size of the shadow
    float                   EffectShadowStrength;                   // global strength of the effect (0 - 5)
    float                   EffectShadowPow;
    float                   EffectShadowClamp;

    float                   EffectFadeOutMul;                       // effect fade out from distance (ex. 25)
    float                   EffectFadeOutAdd;                       // effect fade out to distance   (ex. 100)
    float                   EffectHorizonAngleThreshold;            // limit errors on slopes and caused by insufficient geometry tessellation (0.05 to 0.5)
    float                   EffectSamplingRadiusNearLimitRec;          // if viewspace pixel closer than this, don't enlarge shadow sampling radius anymore (makes no sense to grow beyond some distance, not enough samples to cover everything, so just limit the shadow growth; could be SSAOSettingsFadeOutFrom * 0.1 or less)

    float                   DepthPrecisionOffsetMod;
    float                   NegRecEffectRadius;                     // -1.0 / EffectRadius
    float                   LoadCounterAvgDiv;                      // 1.0 / ( halfDepthMip[SSAO_DEPTH_MIP_LEVELS-1].sizeX * halfDepthMip[SSAO_DEPTH_MIP_LEVELS-1].sizeY )
    float                   AdaptiveSampleCountLimit;

    float                   InvSharpness;
    int                     PassIndex;
    vec2                    QuarterResPixelSize;                    // used for importance map only

    vec4                    PatternRotScaleMatrices[5];

    float                   NormalsUnpackMul;
    float                   NormalsUnpackAdd;
    float                   DetailAOStrength;
    float                   Dummy0;

#if SSAO_ENABLE_NORMAL_WORLD_TO_VIEW_CONVERSION
    mat4                    NormalsWorldToViewspaceMatrix;
#endif
};

begin_cbuffer(ASSAO_Params, 0)
    cbuffer_member(ASSAOConstants, g_ASSAOConsts);
end_cbuffer()

//----------------------------------------------------
float ScreenSpaceToViewSpaceDepth(float screenDepth)
{
    float depthLinearizeMul = g_ASSAOConsts.DepthUnpackConsts.x;
    float depthLinearizeAdd = g_ASSAOConsts.DepthUnpackConsts.y;

    return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

//----------------------------------------------------
vec4 ScreenSpaceToViewSpaceDepth(vec4 screenDepth)
{
    float depthLinearizeMul = g_ASSAOConsts.DepthUnpackConsts.x;
    float depthLinearizeAdd = g_ASSAOConsts.DepthUnpackConsts.y;

    return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

//----------------------------------------------------
// from [0, width], [0, height] to [-1, 1], [-1, 1]
vec2 ScreenSpaceToClipSpacePositionXY(vec2 screenPos )
{
    return screenPos * g_ASSAOConsts.Viewport2xPixelSize.xy - vec2(1.0f, 1.0f);
}

//----------------------------------------------------
vec3 ScreenSpaceToViewSpacePosition( vec2 screenPos, float viewspaceDepth )
{
    return vec3(g_ASSAOConsts.CameraTanHalfFOV.xy * viewspaceDepth * ScreenSpaceToClipSpacePositionXY(screenPos), viewspaceDepth);
}

//----------------------------------------------------
vec3 ClipSpaceToViewSpacePosition( vec2 clipPos, float viewspaceDepth )
{
    return vec3(g_ASSAOConsts.CameraTanHalfFOV.xy * viewspaceDepth * clipPos, viewspaceDepth);
}

//----------------------------------------------------
vec3 NDCToViewspace(vec2 pos, float viewspaceDepth)
{
    vec3 ret;

    // [Kex]: Support for resolution scaling
    ret.xy = (g_ASSAOConsts.NDCToViewMul * (pos.xy * uResolutionScale.zw) + g_ASSAOConsts.NDCToViewAdd) * viewspaceDepth;
    ret.z = viewspaceDepth;
    return ret;
}

//----------------------------------------------------
// calculate effect radius and fit our screen sampling pattern inside it
void CalculateRadiusParameters(const float pixCenterLength, const vec2 pixelDirRBViewspaceSizeAtCenterZ,
                               out float pixLookupRadiusMod, out float effectRadius, out float falloffCalcMulSq)
{
    effectRadius = g_ASSAOConsts.EffectRadius;

    // leaving this out for performance reasons: use something similar if radius needs to scale based on distance
    //effectRadius *= pow( pixCenterLength, g_ASSAOConsts.RadiusDistanceScalingFunctionPow);

    // when too close, on-screen sampling disk will grow beyond screen size; limit this to avoid closeup temporal artifacts
    const float tooCloseLimitMod = saturate(pixCenterLength * g_ASSAOConsts.EffectSamplingRadiusNearLimitRec) * 0.8 + 0.2;
    
    effectRadius *= tooCloseLimitMod;

    // 0.85 is to reduce the radius to allow for more samples on a slope to still stay within influence
    pixLookupRadiusMod = (0.85 * effectRadius) / pixelDirRBViewspaceSizeAtCenterZ.x;

    // used to calculate falloff (both for AO samples and per-sample weights)
    falloffCalcMulSq = -1.0f / (effectRadius*effectRadius);
}

//----------------------------------------------------
vec4 CalculateEdges(const float centerZ, const float leftZ, const float rightZ, const float topZ, const float bottomZ)
{
    // slope-sensitive depth-based edge detection
    vec4 edgesLRTB = vec4(leftZ, rightZ, topZ, bottomZ) - centerZ;
    vec4 edgesLRTBSlopeAdjusted = edgesLRTB + edgesLRTB.yxwz;
    edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));
    return saturate((1.3 - edgesLRTB / (centerZ * 0.040)));
}

//----------------------------------------------------
// packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
float PackEdges(vec4 edgesLRTB)
{
    edgesLRTB = round(saturate(edgesLRTB) * 3.05);
    return dot(edgesLRTB, vec4(64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0));
}

//----------------------------------------------------
vec4 UnpackEdges(float _packedVal)
{
    uint packedVal = uint(_packedVal * 255.5);
    vec4 edgesLRTB;
    
    // there's really no need for mask (as it's an 8 bit input) but I'll leave it in so it doesn't cause any trouble in the future
    edgesLRTB.x = float((packedVal >> 6) & 0x03) / 3.0;
    edgesLRTB.y = float((packedVal >> 4) & 0x03) / 3.0;
    edgesLRTB.z = float((packedVal >> 2) & 0x03) / 3.0;
    edgesLRTB.w = float((packedVal >> 0) & 0x03) / 3.0;

    return saturate(edgesLRTB + g_ASSAOConsts.InvSharpness);
}

#if defined(GLSL_VERSION)
//----------------------------------------------------
uint f32tof16(float val)
{
    uint f32 = floatBitsToUint(val);
    uint f16 = 0u;
    uint sign = (f32 >> 16) & 0x8000u;
    int exponent = int((f32 >> 23) & 0xFFu) - 127;
    uint mantissa = f32 & 0x007FFFFFu;
    if(exponent == 128)
    {
        // Infinity or NaN
        // NaN bits that are masked out by 0x3FF get discarded.
        // This can turn some NaNs to infinity, but this is allowed by the spec.
        f16 = sign | (0x1Fu << 10);
        f16 |= (mantissa & 0x3FFu);
    }
    else if(exponent > 15)
    {
        // Overflow - flush to Infinity
        f16 = sign | (0x1Fu << 10);
    }
    else if(exponent > -15)
    {
        // Representable value
        exponent += 15;
        mantissa >>= 13;
        f16 = sign | uint(exponent << 10) | mantissa;
    }
    else
    {
        f16 = sign;
    }
    return f16;
}

//----------------------------------------------------
float f16tof32(uint val)
{
    uint sign = (val & 0x8000u) << 16;
    int exponent = int((val & 0x7C00u) >> 10);
    uint mantissa = val & 0x03FFu;
    float f32 = 0.0;
    if(exponent == 0)
    {
        if(mantissa != 0u)
        {
            const float scale = 1.0 / (1 << 24);
            f32 = scale * mantissa;
        }
    }
    else if(exponent == 31)
    {
        return uintBitsToFloat(sign | 0x7F800000u | mantissa);
    }
    else
    {
        exponent -= 15;
        float scale;
        if(exponent < 0)
        {
            // The negative unary operator is buggy on OSX.
            // Work around this by using abs instead.
            scale = 1.0 / (1 << abs(exponent));
        }
        else
        {
            scale = 1 << exponent;
        }
        float decimal = 1.0 + float(mantissa) / float(1 << 10);
        f32 = scale * decimal;
    }

    if(sign != 0u)
    {
        f32 = -f32;
    }

    return f32;
}

#endif

//----------------------------------------------------
uint PackFloat16(vec2 v)
{
    uvec2 p;
    p.x = f32tof16(v.x);
    p.y = f32tof16(v.y);
    return p.x | (p.y << 16);
}

//----------------------------------------------------
vec2 UnpackFloat16(uint a)
{
    vec2 tmp;
    tmp.x = f16tof32(a & 0xFFFF);
    tmp.y = f16tof32(a >> 16);
    return vec2(tmp);
}
