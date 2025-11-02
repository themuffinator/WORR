//
// Copyright(C) 2022-2023 Nightdive Studios, LLC
//
// ORIGINAL AUTHOR
//      Samuel Villarreal
//

#if QUAKE_DEPTH_ONLY == 0
#if QUAKE_UNIFORM_BUFFER_USE_SKY_PARAMS
begin_cbuffer(QuakeSkyParams, 0)
    cbuffer_member(float, uSpeedScale);
    cbuffer_member(float, uSkyAlpha);
end_cbuffer()
#endif

#ifdef QUAKE_HAS_CLUSTERED_SHADING
    #include "progs/includes/QuakeClusteredShading.inc"
#endif

//----------------------------------------------------
float ScreenSpaceToViewSpaceDepth(float screenDepth)
{
    const float fM22 = -(uZFar) / (uZFar - uZNear);
    const float fM32 = -(uZFar * uZNear) / (uZFar - uZNear);
    
    float fDepthLinearizeMul = -fM32;
    float fDepthLinearizeAdd = fM22;
    if(fDepthLinearizeMul * fDepthLinearizeAdd < 0)
    {
        fDepthLinearizeAdd = -fDepthLinearizeAdd;
    }
    
    return fDepthLinearizeMul / (fDepthLinearizeAdd - screenDepth);
}

/*
//----------------------------------------------------
float ComputeAlphaWeight(const float fFragZ, const float fPremultipliedAlpha)
{
    const float a = pow(fPremultipliedAlpha + 0.01, 4.0);
    const float b = 0.03 / (0.00001 + pow(fFragZ / (0.1 + uAlphaWeight), 4.0));
    
    return (a + max(0.01, min(uAlphaClamp, b)));
}
*/

//----------------------------------------------------
bool ComputeShadedColor(inout vec4 vColor, const in vec2 vTexCoords, const in vec4 vShadeColor,
                        const bool bDiscardAlphaPixel, const float fFragZ,
                        const in vec3 vPlane, const in vec4 fragCoord
    )
{
    if(bDiscardAlphaPixel && vColor.a <= 0.999)
    {
        return false;
    }
    
#if QUAKE_WARP_EFFECT == 1
    vec4 vNewShadeColor = vec4(1.0, 1.0, 1.0, 1.0);
#else
    vec4 vNewShadeColor = vec4(vShadeColor.rgb * 2.0, 1.0);
#endif
    
#ifdef QUAKE_HAS_CLUSTERED_SHADING
    const vec2 vTC = fragCoord.xy / ScreenSize();
    
    const ivec3 cluster = GetClusteredCell(vTC, fFragZ);
    quakeLightClusterLookup_s lookup = GetLookupData(cluster);
    const float fOverdraw = ComputeClusteredShading(vNewShadeColor, lookup, vTC, fFragZ, vPlane);
    if(uShowOverdraw == 1)
    {
        const uint dwOverDrawID = uint(fOverdraw);
        vColor.rgb = GetOverDrawColor(dwOverDrawID);
        return true;
    }
    if(uShowNormals == 1)
    {
        vColor.rgb = vPlane.rgb;
        return true;
    }
#endif
    
    vColor.rgb *= vNewShadeColor.rgb;
    return true;
}

//----------------------------------------------------
vec3 GetWorldPositionFromDepth(const float fZDepth, const in vec2 vFragCoords)
{
    vec3 eyePos = UVToEyePos(uInvFocalCoords, vFragCoords, fZDepth);
    eyePos.z *= -1.0;
    return mul(uInverseModelViewMatrix, vec4(eyePos, 1.0)).xyz;
}

//----------------------------------------------------
vec2 ComputeSkyLayerCoords(const float fZDepth, const in vec2 vFragCoords, const float fScrollSpeed)
{
    vec3 vPos = GetWorldPositionFromDepth(fZDepth, vFragCoords);
    vec3 vDir = (vPos - uViewOrigin);
    vDir.z *= 3.0; // flatten sphere

    // use the exact same formula as seen in the source code
    float dist = 6.0*63.0 / sqrt(dot(vDir, vDir));
    
    vec2 outCoord;
    outCoord.x = ((fScrollSpeed + vDir.x * dist) * (1.0/128.0));
    outCoord.y = ((fScrollSpeed + vDir.y * dist) * (1.0/128.0));
    
    return outCoord;
}

#endif // QUAKE_DEPTH_ONLY

//----------------------------------------------------
vec2 ComputeMotionVector(const in vec4 pos1, const in vec4 pos2)
{
    vec2 a = pos1.xy / pos1.w;
    vec2 b = pos2.xy / pos2.w;
    
#if defined(REVERSE_NDC) || defined(__ORBIS__) || defined(__PROSPERO__)
    vec2 v = vec2(a.x - b.x, b.y - a.y);
#else
    vec2 v = a - b;
#endif

    return PackVelocity(v);
}

#define AMP     (1.0 / 16.0)
#define SPEED   (1.0 / 3.0)
#define PI      3.1415926535897932384626433832795

//----------------------------------------------------
vec2 ComputeWarpCoords(const in vec2 vTexCoords, const float fTime)
{
    float fT = (fTime * SPEED);
    
    float fx = vTexCoords.x;
    float fy = vTexCoords.y;
    
    float fs = fy + (AMP + sin((fT + fx) * (PI * 2.0)) * AMP);
    float ft = fx + (AMP + sin((fT + fy) * (PI * 2.0)) * AMP);
    
    return vec2(fs, ft);
}
