//
// Copyright(C) 2022-2023 Nightdive Studios, LLC
//
// ORIGINAL AUTHOR
//      Samuel Villarreal
//

//----------------------------------------------------
begin_cbuffer(WorldFog, 101) 
    cbuffer_member(vec4, uFogColor);                // w includes density
    cbuffer_member(vec4, uHeightFogColorStart);     // w includes start distance
    cbuffer_member(vec4, uHeightFogColorEnd);       // w includes end distance
    cbuffer_member(float, uSkyFogFactor);
    cbuffer_member(float, uHeightFogFallOff);
    cbuffer_member(float, uHeightFogDensity);
end_cbuffer()

//----------------------------------------------------
vec3 ComputeFogColor(const in vec4 vColor, const in float fDepth, const in vec2 vCoordinates)
{
    float f = exp(-pow(uFogColor.w * fDepth, 2.0));
    vec3 vOutputColor = mix(uFogColor.rgb, vColor.rgb, f);
    
    if(uHeightFogFallOff > 0.0)
    {
        if(vCoordinates.x > uResolutionScale.x)
        {
            return vec3(0.0, 0.0, 0.0);
        }
        if(vCoordinates.y > uResolutionScale.y)
        {
            return vec3(0.0, 0.0, 0.0);
        }
        
        vec3 C = UVToEyePos(uInvFocalCoords, vCoordinates, fDepth);
        C.z = -C.z;
        
        vec3 vWorldPos = mul(uInverseModelViewMatrix, vec4(C, 1.0)).xyz;
        
        vec3 vWorldToCamera = normalize(uViewOrigin - vWorldPos);
        vWorldToCamera = -vWorldToCamera;
        
        const float verticalStartDistance = uHeightFogColorStart.w;
        const float verticalEndDistance = uHeightFogColorEnd.w;
        
        float altitude = uViewOrigin.z - verticalStartDistance;
        float signView = step(0.1, sign(vWorldToCamera.z)) * 2.0 - 1.0;
        float dy = vWorldToCamera.z + (0.00001 * signView);
        float hitDistance = fDepth;
        float falloff = uHeightFogFallOff;
        
        // Reference: http://www.iquilezles.org/www/articles/fog/fog.htm
        float hitAltitude = altitude + hitDistance * dy;
        float totalDensity = (exp(-falloff * altitude) - exp(-falloff * hitAltitude)) / (falloff * dy);
        float extinction = 1.0 - saturate(exp(-totalDensity));
        
        float planeAltitudeStart = verticalStartDistance;
        float planeAltitudeEnd = verticalEndDistance;
        float normalizedHeight = saturate((hitAltitude - planeAltitudeStart) / (planeAltitudeEnd - planeAltitudeStart));
        
        vec4 verticalFogColor;
        verticalFogColor.rgb = (extinction * lerp(uHeightFogColorStart.rgb, uHeightFogColorEnd.rgb, normalizedHeight));
        verticalFogColor.a = extinction * (1.0 - exp(-(uHeightFogDensity * fDepth)));
        
        vOutputColor = mix(vOutputColor, verticalFogColor.rgb, verticalFogColor.a);
    }
    
    return vOutputColor;
}
