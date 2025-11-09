//
// Copyright(C) 2007-2014 Samuel Villarreal
// Copyright(C) 2015 Night Dive Studios, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#pragma once

//-----------------------------------------------------------------------------
//
// Common functions
//
//-----------------------------------------------------------------------------

// ----------------------------------------------------
float LinearizeDepth(float near, float far, float depth)
{
    return 2.0 * near / (far + near - depth * (far - near));
}

static const vec4 BIT_SHIFT_PACK   = vec4(16581375.0, 65025.0, 255.0, 1.0);
static const vec4 BIT_SHIFT_UNPACK = vec4(1.0 / 16581375.0, 1.0 / 65025.0, 1.0 / 255.0, 1.0);
static const vec4 BIT_MASK         = vec4(0.0, 1.0 / 255.0, 1.0 / 255.0, 1.0 / 255.0);
static const float PACK_SCALAR     = 1.7777;

// ----------------------------------------------------
vec4 DepthToRGBA(const in float depth)
{
#if 0
    vec4 res = fract(depth * BIT_SHIFT_PACK);
    res -= res.xxyz * BIT_MASK;
    return res;
#else
    // switching to R32F actually helped the bandwidth a little bit
    return vec4(depth, 0.0, 0.0, 0.0);
#endif
}

// ----------------------------------------------------
float RGBAToDepth(const in vec4 depth)
{
#if 0
    return dot(depth, BIT_SHIFT_UNPACK);
#else
    return depth.r;
#endif
}

// ----------------------------------------------------
vec2 PackNormal(const in vec3 normal)
{
    float p = sqrt(normal.z * 8.0 + 8.0);
    return vec2(normal.xy / p + 0.5);
}

// ----------------------------------------------------
vec3 UnpackNormal(const in vec2 pack)
{
    vec2 fenc = pack * 4.0 - 2.0;
    float f = dot(fenc, fenc);
    float g = sqrt(1.0 - f / 4.0);
    
    vec3 n;
    n.xy = fenc * g;
    n.z = 1.0 - f / 2.0;
    
    return n;
}

// ----------------------------------------------------
vec2 PackVelocity(const in vec2 velocity)
{
    vec2 result = velocity * 0.5 + 0.5;
    result = vec2(pow(result.x, 3.0), pow(result.y, 3.0));
    
    return result;
}

// ----------------------------------------------------
vec2 UnpackVelocity(const in vec2 velocity)
{
    float t = 1.0 / 3.0;
    vec2 result = vec2(pow(abs(velocity.x), t), pow(abs(velocity.y), t));
    
    return result * 2.0 - 1.0;
}

// ----------------------------------------------------
vec3 PixelNormalFromPosition(const in vec3 fragPos)
{
#if defined(CLIP_SPACE_ZERO_TO_ONE)
    return -normalize(cross(dFdx(fragPos), dFdy(fragPos)));
#else
    return normalize(cross(dFdx(fragPos), dFdy(fragPos)));
#endif
}

// ----------------------------------------------------
vec3 UVToEyePos(const in vec2 focalCoords, vec2 uv, float eye_z)
{
    uv = (uv * uResolutionScale.zw) - (uScreenBounds.xy * uResolutionScale.zw);
    
    // UGHHHHHHHHH....
#if defined(REVERSE_NDC)
    uv.y = 1.0 - uv.y;
#endif

    uv = (uv * vec2(2.0, 2.0) - vec2(1.0, 1.0));
    return vec3(uv * focalCoords * eye_z, eye_z);
}

// ----------------------------------------------------
vec3 PositionFromDepth(const in vec2 invFocalCoords, const in vec2 texCoord,
                       const float depth, const float zFar, const in mat4 mtxInverseView)
{
    vec3 eyePos = UVToEyePos(invFocalCoords, texCoord, depth * zFar);
    eyePos.z *= -1.0;
    return mul(mtxInverseView, vec4(eyePos, 1.0)).xyz;
}

// ----------------------------------------------------
vec3 FastPositionFromDepth(const in vec4 frustumCorner, const in vec3 origin,
                           const float depth, const float zFar)
{
    float clipSpaceW = depth * zFar;
    return origin + (frustumCorner.xyz * clipSpaceW);
}

// ----------------------------------------------------
vec2 ScreenSize(void)
{
    return vec2(float(uViewWidth), float(uViewHeight));
}

#define ScreenCoords() (inFragCoord.xy / ScreenSize())

// ----------------------------------------------------
vec4 ComputeFog(const in float depth, const in vec4 color,
                const float fFar, const float fNear, const in vec3 fogColor)
{
    float z;
    float fogFactor;
    vec4 finalColor;

    z = depth;
    
    if(z <= fNear)
    {
        return color;
    }
    
    z -= fNear;
    
    fogFactor = clamp(1.0 - (z / (fFar - fNear)), 0.0, 1.0);
    finalColor = mix(vec4(fogColor, 1.0), color, fogFactor);

    return finalColor;
}

// ----------------------------------------------------
vec3 SaturateColor(const in vec3 base, const in vec3 color, const float power)
{
    float p = sqrt(base.r * base.r * color.r +
                   base.g * base.g * color.g +
                   base.b * base.b * color.b);
                   
    p = clamp(p, 0.0, 1.0);
    
    vec3 rgb;
    
    rgb.r = p + (base.r - p) * power;
    rgb.g = p + (base.g - p) * power;
    rgb.b = p + (base.b - p) * power;
    
    return clamp(rgb + (color * 0.5), 0.0, 1.0);
}
