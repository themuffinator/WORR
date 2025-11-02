//
// Copyright(C) 2022-2023 Nightdive Studios, LLC
//
// ORIGINAL AUTHOR
//      Samuel Villarreal
//

#define MAX_SHADOW_VIEWS    256

struct quakeUBShadowStruct_s // 104 bytes
{
    mat4  uVolumeMatrix;
    vec4  uViewportRect;
    vec4  uSourcePos;    // w includes light source range
    float uBias;
    float uShadeAmount;
};

def_samplerCompare(2D, tShadowMap, 4);

//----------------------------------------------------
begin_cbuffer(ShadowItems, 0)
    cbuffer_member(quakeUBShadowStruct_s, sbShadowItems[MAX_SHADOW_VIEWS]);
end_cbuffer()

#ifdef GLSLC_NX
    static const vec2 SHADOW_ATLAS_DIMS = vec2(8192.0, 4096.0);
#else
    #define SHADOW_ATLAS_DIMS   8192.0
#endif

//----------------------------------------------------
vec3 GetProjectedCoordinates(const in vec3 vFragPos, const in mat4 clipMatrix, const in vec4 viewportRect)
{
    vec4 volumeSpace = mul(clipMatrix, vec4(vFragPos, 1.0));
    vec3 projCoords = volumeSpace.xyz / volumeSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.xy *= ((viewportRect.zw - viewportRect.xy) / SHADOW_ATLAS_DIMS);
    
    return projCoords;
}

//----------------------------------------------------
vec2 GetClampedUV(const in vec2 uv, in vec4 rect)
{
    vec2 clampedUV = uv;
    
#if REVERSE_NDC
    // image is upside down. thank OpenGL for this whole reverse NDC bullshit
    clampedUV.y = rect.y + (rect.w - clampedUV.y);
#endif

    clampedUV.x = clamp(clampedUV.x, rect.x+0.5, rect.z-0.5);
    clampedUV.y = clamp(clampedUV.y, rect.y+0.5, rect.w-0.5);

    return clampedUV / SHADOW_ATLAS_DIMS;
}

//----------------------------------------------------
float GetShadowMapDepth(const in vec2 uv, in vec4 rect, float compareDepth, float bias)
{
    vec2 clampedUV = GetClampedUV(uv, rect);
    
#if !CLIP_SPACE_ZERO_TO_ONE
    float fTmp = -compareDepth * 0.5 + 0.5;
    float closestDepth = sampleCmpLevelZero(tShadowMap, clampedUV, fTmp - bias);
#else
    float closestDepth = sampleCmpLevelZero(tShadowMap, clampedUV, (-compareDepth - bias));
#endif

    return closestDepth;
}

//----------------------------------------------------
float AccumShadow(const in vec2 vProjCoords, const float fX, const float fY,
                  const float currentDepth, const in quakeUBShadowStruct_s shadowItem)
{

    vec2 vOffsets = vec2(fX, fY);
    float result = GetShadowMapDepth(
        vProjCoords + vOffsets,
        shadowItem.uViewportRect,
        currentDepth,
        shadowItem.uBias);
    
    return result;
}

//----------------------------------------------------
float ProjectionInVolume(const in vec3 projCoords, const in vec4 viewportRect)
{
    vec3 pxy = vec3(0.0, 0.0, 0.0);
    vec2 vShadowMapDimensions = (viewportRect.zw - viewportRect.xy) / SHADOW_ATLAS_DIMS;
    vec3 left;
    vec3 right;
    
    right.x = vShadowMapDimensions.x;
    right.y = vShadowMapDimensions.y;
    right.z = 1.0;
    left.x = 0.0;
    left.y = 0.0;
    left.z = 0.0;
    
    pxy += (1.0 - step(projCoords, right));
    pxy += (1.0 - step(left, projCoords));
    
    return pxy.x + pxy.y + pxy.z;
}

//----------------------------------------------------
float ComputeShadow(in vec3 projCoords, const in quakeUBShadowStruct_s shadowItem)
{
#ifdef GLSLC_NX
    const vec2 vSMSize = SHADOW_ATLAS_DIMS;
#else
    const vec2 vSMSize = vec2(SHADOW_ATLAS_DIMS, SHADOW_ATLAS_DIMS);
#endif
    const vec2 vInvSMSize = rcp(vSMSize);
    
    projCoords.xy += (vInvSMSize * shadowItem.uViewportRect.xy);
    
    float currentDepth = -projCoords.z;
    
    float fShadowDepth = 1.0;

    vec2 uv = projCoords.xy * SHADOW_ATLAS_DIMS;
    
    vec2 baseUV = uv;
    
    baseUV.x = floor(uv.x + 0.5);
    baseUV.y = floor(uv.y + 0.5);

    float s = (uv.x + 0.5 - baseUV.x);
    float t = (uv.y + 0.5 - baseUV.y);

    baseUV -= vec2(0.5, 0.5);

    float sum = 0.0;
    
#if 0
    fShadowDepth = AccumShadow(baseUV, 0.0, 0.0, currentDepth, shadowItem);
#else
    float uw0 = (3.0 - 2.0 * s);
    float uw1 = (1.0 + 2.0 * s);
    
    float u0 = (2.0 - s) / uw0 - 1.0;
    float u1 = s / uw1 + 1.0;
    
    float vw0 = (3.0 - 2.0 * t);
    float vw1 = (1.0 + 2.0 * t);
    
    float v0 = (2.0 - t) / vw0 - 1.0;
    float v1 = t / vw1 + 1.0;

    sum += uw0 * vw0 * AccumShadow(baseUV, u0, v0, currentDepth, shadowItem);
    sum += uw1 * vw0 * AccumShadow(baseUV, u1, v0, currentDepth, shadowItem);
    sum += uw0 * vw1 * AccumShadow(baseUV, u0, v1, currentDepth, shadowItem);
    sum += uw1 * vw1 * AccumShadow(baseUV, u1, v1, currentDepth, shadowItem);
    
    fShadowDepth = sum * (1.0 / 16.0);
#endif
    fShadowDepth = min(fShadowDepth + shadowItem.uShadeAmount, 1.0);
    
    return fShadowDepth;
}
