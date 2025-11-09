//
// Copyright(C) 2022-2023 Nightdive Studios, LLC
//
// ORIGINAL AUTHOR
//      Samuel Villarreal
//

#define CLUSTER_MAX_X       10
#define CLUSTER_MAX_Y       9
#define CLUSTER_MAX_Z       12
#define MAX_CLUSTER_ITEMS   32
#define CLUSTER_TOTAL_CELLS (CLUSTER_MAX_X*CLUSTER_MAX_Y*CLUSTER_MAX_Z)

def_structuredBuffer(uint, sbTable, 2);
def_structuredBuffer(uint, sbIndexes, 3);

#include "progs/includes/QuakeShadowsCommon.inc"

//----------------------------------------------------
struct quakeLightClusterInfo_s // 64 bytes
{
    vec4 vPosAndRange;  // camera pos + range
    vec4 vRGBAndIntensity;  // rgb + intensity
    vec4 vConeDirAndAngle;  // cone direction + cone angle
    vec3 vWorldOrigin;
    uint dwParams; // light type, cast shadow, shadow view offset
};

struct quakeLightClusterLookup_s
{
    uint offset;
    uint count;
};

//----------------------------------------------------
begin_cbuffer(ShadowViewIndexes, 0)
    cbuffer_member(vec4, uShadowViewIndex[MAX_SHADOW_VIEWS]);
end_cbuffer()

//----------------------------------------------------
begin_cbuffer(LightCluster, 0)
    cbuffer_member(quakeLightClusterInfo_s, uClusterLights[992]);
    cbuffer_member(float, uClusterMinZ);
    cbuffer_member(float, uClusterZSliceFactor);
    cbuffer_member(int, uShowOverdraw);
    cbuffer_member(int, uShowNormals);
end_cbuffer()

static const vec3 g_vZeroTwoOne = vec3(0.0, 2.0, 1.0);
static const vec3 g_vZero       = vec3(0.0, 0.0, 0.0);

//----------------------------------------------------
void ComputeLightShading(inout vec4 modulatedColor, const in vec3 vPosition, const in vec3 vLightPos, const float fRange,
                         const in vec4 vLightColor, const float fLightIntensity, const in vec3 vNormals)
{
    const vec3 vLightToPos  = vLightPos - vPosition;
    const float fDist       = length(vLightToPos);
    const float fLen        = max(fRange - fDist, 0.0) / fRange;
    const vec3 vDir         = vLightToPos / max(fDist, 1.0);
    const float fLambert    = max(dot(vNormals, vDir), 0.0);
    
    modulatedColor.rgb += ((vLightColor.rgb * fLightIntensity) * fLen) * fLambert;
}

//----------------------------------------------------
void ComputeSpotLight(inout vec4 modulatedColor, const in vec3 vPosition, const in vec3 vLightPos, const float fRange,
                      const in vec4 vLightColor, const float fLightIntensity, const in vec3 vNormals,
                      const float fCone, const in vec3 vSpotDirection)
{
    const vec3 vLightToPos  = vLightPos - vPosition;
    const float fDist       = length(vLightToPos);
    const float fLen        = max(fRange - fDist, 0.0) / fRange;
    const vec3 vDir         = vLightToPos / max(fDist, 1.0);
    const float fMag        = -dot(vDir, vSpotDirection);
    const float fLambert    = max(dot(vNormals, vDir), 0.0);
    const vec3 vResult      = ((vLightColor.rgb * fLightIntensity) * fLen) * fLambert;
    
    modulatedColor.rgb += (vResult * max(1.0 - (1.0 - fMag) * rcp(1.0 - fCone), 0.0));
}

//----------------------------------------------------
int GetZSliceFromZ(const float fLinearZ)
{
    const float fSlice = log2(max(1.0, fLinearZ / uClusterMinZ)) * uClusterZSliceFactor;
    int sliceResult = min(CLUSTER_MAX_Z-1, int(floor(fSlice)));
    return sliceResult;
}

//----------------------------------------------------
ivec3 GetClusteredCell(const in vec2 vFragScreenCoordinates, const float fFragZ)
{
    vec2 vTC = (vFragScreenCoordinates * uResolutionScale.zw) - (uScreenBounds.xy * uResolutionScale.zw);
    
#ifndef REVERSE_NDC
    vTC.y = 1.0 - vTC.y;
#endif

    float fDeltaX = 1.0 - vTC.x;
    float fDeltaY = 1.0 - vTC.y;
    
    int cellX = cast(int, fDeltaX * cast(float, CLUSTER_MAX_X));
    int cellY = cast(int, fDeltaY * cast(float, CLUSTER_MAX_Y));
    int cellZ = GetZSliceFromZ(fFragZ);
    
    return ivec3(cellX, cellY, cellZ);
}

static const uint TABLE_MASK = 0xffff;

//----------------------------------------------------
quakeLightClusterLookup_s GetLookupData(const in ivec3 vCell)
{
    uint table = sbTable[(vCell.y * CLUSTER_MAX_X) + vCell.x + (vCell.z * CLUSTER_MAX_Y * CLUSTER_MAX_X)];
    
    uint offset = (table & TABLE_MASK);
    uint count = ((table >> 16) & TABLE_MASK);
    
    quakeLightClusterLookup_s lookup;
    lookup.offset = offset;
    lookup.count = count;
    
    return lookup;
}

//----------------------------------------------------
float ComputeClusteredShading(inout vec4 modulatedColor, const in quakeLightClusterLookup_s lookup,
                              const in vec2 vFragScreenCoordinates, const float fFragZ, const in vec3 vNormals)
{
    float fOverdrawAccum = 0.0;
    
    if(lookup.count != 0)
    {
        uint i, index;
        quakeLightClusterInfo_s clusterData;
        vec4 vLightingColor;
        float fIntensity;
        uint lightType;
        int maxShadowViews;
        
        vec3 C = UVToEyePos(uInvFocalCoords, vFragScreenCoordinates, fFragZ);
        C.z = -C.z;
        
        const vec3 vWorldPos = mul(uInverseModelViewMatrix, vec4(C, 1.0)).xyz;
        
        for(i = 0; i < lookup.count; ++i)
        {
            vLightingColor = vec4(0.0, 0.0, 0.0, 0.0);
            
            index = sbIndexes[(lookup.offset * MAX_CLUSTER_ITEMS) + i];
            clusterData = uClusterLights[index];
            
            fIntensity = clusterData.vRGBAndIntensity.w;
            fOverdrawAccum += 1.0;
            
            lightType = clusterData.dwParams & 0x7;

            if(lightType == 1)
            {
                maxShadowViews = 1;
                ComputeSpotLight(
                    vLightingColor, C, clusterData.vPosAndRange.xyz, clusterData.vPosAndRange.w, clusterData.vRGBAndIntensity,
                    fIntensity, vNormals, clusterData.vConeDirAndAngle.w, clusterData.vConeDirAndAngle.xyz);
            }
            else
            {
                maxShadowViews = 6;
                ComputeLightShading(
                    vLightingColor, C, clusterData.vPosAndRange.xyz + (vNormals * 16.0), clusterData.vPosAndRange.w,
                    clusterData.vRGBAndIntensity, fIntensity, vNormals);
            }
            
            // if fragment is not even lit then there's no point in shadowing it
            if(((clusterData.dwParams >> 3) & 0x1) != 0 && dot(vLightingColor, vLightingColor) > 0.0)
            {
                // there's no cubemap to work with due to everything being a part of a shadowmap atlas, so
                // determine the face based on fragment position and its direction from the light source
                const vec3 vWorldPosN = normalize(vWorldPos - clusterData.vWorldOrigin);
                const vec3 vWorldAxis = abs(vWorldPosN);
                
                // order of render target axis per shadowmap face (max 6) is ordered as follows:
                // x, z, y, -x, -z, -y
                
                // branchless method of acquiring the best plane axis and determining which face of the shadowmap it belongs to
                const vec3 vSA = step(vWorldAxis.yxx, vWorldAxis.xyz) * step(vWorldAxis.zzy, vWorldAxis.xyz);
                const vec3 vSB = g_vZeroTwoOne + (step(sign(vWorldPosN), g_vZero) * 3.0);
                
                const int shadowRT = min(int(dot(vSA, vSB) + 0.5), maxShadowViews - 1);
                const uint dwShadowOffset = shadowRT + ((clusterData.dwParams >> 4) & 0xFFFFFFF);
                const uint rtIndex = uint(uShadowViewIndex[dwShadowOffset].x + 0.5);
                const quakeUBShadowStruct_s shadowItem = sbShadowItems[rtIndex];
                
                vec3 projCoords = GetProjectedCoordinates(
                    vWorldPos,
                    shadowItem.uVolumeMatrix,
                    shadowItem.uViewportRect);
                    
                const float fShadow = ComputeShadow(projCoords, shadowItem);
                vLightingColor *= fShadow;
                fOverdrawAccum += 1.0;
            }
                
            modulatedColor.rgb += vLightingColor.rgb;
        }
    }
    
    return fOverdrawAccum;
}

//----------------------------------------------------
#define NUM_OVERDRAW_COLORS     11
static const vec3 g_vOverdrawColors[NUM_OVERDRAW_COLORS] =
#if defined(GLSL_VERSION) && !defined(VULKAN)
vec3[]
(
#else
{
#endif
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.25, 1.0),
    vec3(0.0, 0.5, 0.7),
    vec3(0.0, 0.7, 0.5),
    vec3(0.0, 1.0, 0.0),
    vec3(0.25, 1.0, 0.0),
    vec3(0.5, 1.0, 0.0),
    vec3(0.7, 0.7, 0.0),
    vec3(0.8, 0.4, 0.0),
    vec3(1.0, 0.2, 0.0),
    vec3(1.0, 0.0, 0.0)
#if defined(GLSL_VERSION) && !defined(VULKAN)
);
#else
};
#endif

//----------------------------------------------------
vec3 GetOverDrawColor(const uint dwOverdrawCount)
{
    return g_vOverdrawColors[min(dwOverdrawCount, uint(NUM_OVERDRAW_COLORS-1))];
}
