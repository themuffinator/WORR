//
// Copyright(C) 2016-2017 Samuel Villarreal
// Copyright(C) 2016 Night Dive Studios, Inc.
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

#include "progs/common.inc"

#ifdef SHADER_VERTEX

//----------------------------------------------------
// input
begin_input(inVertex)
    var_attrib(0, vec4);
    var_attrib(1, vec2);
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec2, out_texcoord, TEXCOORD0)
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    outVarPosition(output, position)    = inVarAttrib(0, input);
    outVar(output, out_texcoord)        = inVarAttrib(1, input);
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

begin_cbuffer(PostProcess_MotionBlur, 0)
    cbuffer_member(vec4, uMotionBlurParams);
end_cbuffer()

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

#ifdef MOTION_BLUR_APPLY
def_sampler(2D, tMBGeneral, 0);
def_sampler(2D, tMBEncode, 1);
def_sampler(2D, tMBVelocity, 2);

#define PIXEL_BIAS  0.0001

#endif

#if MOTION_BLUR_PACK || MOTION_BLUR_TILE_GEN || MOTION_BLUR_NEIGHBORHOOD_MAX || MOTION_BLUR_VISUALIZE
def_sampler(2D, tMBVelocity, 0);
#endif

#ifdef MOTION_BLUR_PACK
def_sampler(2D, tMBDepth, 1);
#endif

//----------------------------------------------------
vec3 NRand3(vec2 seed)
{
    return frac(sin(dot(seed.xy, vec2(34.483, 89.637))) * vec3(29156.4765, 38273.5639, 47843.7546));
}

//----------------------------------------------------
vec2 UnpackLengthAndDepth(vec2 packedLenDepth)
{
    packedLenDepth.x = (packedLenDepth.x * packedLenDepth.x) / 32.0f;
    packedLenDepth.y = packedLenDepth.y;
    return packedLenDepth;
}

//----------------------------------------------------
float MBSampleWeight(float centerDepth, float sampleDepth, float centerVelLen,
                     float sampleVelLen, float sampleIndex, float lenToSampleIndex)
{
    const vec2 depthCompare = saturate(0.5f + vec2(1, -1) * (sampleDepth - centerDepth));
    const vec2 spreadCompare = saturate(1 + lenToSampleIndex * vec2(centerVelLen, sampleVelLen) - sampleIndex);
    return dot(depthCompare.xy, spreadCompare.xy);
}

//----------------------------------------------------
// Packs velocity into 8-bit RG color channels
vec2 EncodeMotionVector(vec2 vMotion)
{
    const float fMagX = sqrt(abs(vMotion.x));
    const float fMagY = sqrt(abs(vMotion.y));
    vMotion.x = fMagX * ((vMotion.x > 0.0) ? 1.0 : -1.0);
    vMotion.y = fMagY * ((vMotion.y > 0.0) ? 1.0 : -1.0);

    vMotion.x = vMotion.x * 0.5 + 127.0 / 255.0;
    vMotion.y = vMotion.y * 0.5 + 127.0 / 255.0;
    
    return vMotion;
}

//----------------------------------------------------
// Unpacks velocity from a 8-bit RG color channel
vec2 DecodeMotionVector(vec2 vMotionEncoded)
{
    vMotionEncoded.x = (vMotionEncoded.x - 127.0/255.0) * 2.0;
    vMotionEncoded.y = (vMotionEncoded.y - 127.0/255.0) * 2.0;
    const float fMagX = (vMotionEncoded.x * vMotionEncoded.x);
    const float fMagY = (vMotionEncoded.y * vMotionEncoded.y);
    vMotionEncoded.x = fMagX * ((vMotionEncoded.x > 0.0) ? 1.0 : -1.0);
    vMotionEncoded.y = fMagY * ((vMotionEncoded.y > 0.0) ? 1.0 : -1.0);
    
    return vMotionEncoded;
}

#ifdef MOTION_BLUR_VISUALIZE
//----------------------------------------------------
float DrawLine(vec2 p1, vec2 p2, vec2 uv, float fThickness)
{
    float a = abs(distance(p1, uv));
    float b = abs(distance(p2, uv));
    float c = abs(distance(p1, p2));

    if(a >= c || b >= c)
    {
        return 0.0;
    }

    float p = (a + b + c) * 0.5;

    // median to (p1, p2) vector
    float h = 2 / c * sqrt(p * (p - a) * (p - b) * (p - c));

    return mix(1.0, 0.0, smoothstep(0.5 * fThickness, 1.5 * fThickness, h));
}
#endif

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    //---------------------------------------------------------------------------------------------------------
    // Blur Pre-process pass
    //---------------------------------------------------------------------------------------------------------
#ifdef MOTION_BLUR_PACK
    // kex stores initial velocity into RG16F. For the post process we want to pack them into a RGBA target
    // after we finish the pre-process step
    vec2 vel = UnpackVelocity(sampleLevelZero(tMBVelocity, inVar(input, out_texcoord)).rg);
    vec2 delta = vel * (uMotionBlurParams.xy*1000.0);
    
    // apply the time scale to the velocity vector
    float depth = LinearizeDepth(uZNear, uZFar, sampleLevelZero(tMBDepth, inVar(input, out_texcoord)).r);
    float fDeltaMag = dot(delta, delta);
    float maxLen = vel.x == 0.0 ? uMotionBlurParams.w : uMotionBlurParams.z;
    float invLen = inversesqrt(fDeltaMag + 1e-6);
    
    delta *= clamp(maxLen * invLen, 0.0, 1.0);
    
    // pack depth and velocity length for later use
    float z = sqrt(length(delta)*32.0);
    float w = depth;
    
    outVarFragment(output, fragment) = vec4(EncodeMotionVector(delta), z, w);
#endif // MOTION_BLUR_PACK

    //---------------------------------------------------------------------------------------------------------
    // Max tile velocity pass
    //---------------------------------------------------------------------------------------------------------
#ifdef MOTION_BLUR_TILE_GEN
    vec2 coords = inVar(input, out_texcoord) * uMotionBlurParams.xy;
    vec4 maxVel = load(tMBVelocity, ivec2(coords), 0);
    vec2 dir = uMotionBlurParams.w == 0.0 ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    
    for(float i = 0.0; i < uMotionBlurParams.z; i += 1.0)
    {
        vec4 vel = load(tMBVelocity, ivec2(coords + i * dir), 0);
        if(vel.z > maxVel.z)
        {
            maxVel = vel;
        }
    }
    
    outVarFragment(output, fragment) = vec4(maxVel.rgb, 0.0);
#endif // MOTION_BLUR_TILE_GEN

    //---------------------------------------------------------------------------------------------------------
    // Max tile neighborhood pass
    //---------------------------------------------------------------------------------------------------------
#ifdef MOTION_BLUR_NEIGHBORHOOD_MAX
    vec3 maxVel = sampleLevelZero(tMBVelocity, inVar(input, out_texcoord)).rgb;
    vec3 vel;
    
#define SampleVelocity(coord)   \
    vel = sampleLevelZero(tMBVelocity, inVar(input, out_texcoord) + coord * uMotionBlurParams.xy).rgb;   \
    if(vel.b > maxVel.b) maxVel = vel
    
    SampleVelocity(vec2( 0.0, -1.0));
    SampleVelocity(vec2(-1.0,  0.0));
    SampleVelocity(vec2( 1.0,  0.0));
    SampleVelocity(vec2( 0.0,  1.0));
    
#define SampleDiagonalVelocity(coord)   \
    vel = sampleLevelZero(tMBVelocity, inVar(input, out_texcoord) + coord * uMotionBlurParams.xy).rgb;   \
    if(vel.b > maxVel.b && dot(DecodeMotionVector(vel.xy), coord) > 0) maxVel = vel
    
    // for diagonal tiles, check if the maximum velocity affects the center tile
    SampleDiagonalVelocity(vec2(-1.0, -1.0));
    SampleDiagonalVelocity(vec2( 1.0, -1.0));
    SampleDiagonalVelocity(vec2(-1.0,  1.0));
    SampleDiagonalVelocity(vec2( 1.0,  1.0));
    
    outVarFragment(output, fragment) = vec4(maxVel, 0.0);
#endif // MOTION_BLUR_NEIGHBORHOOD_MAX
    
    //---------------------------------------------------------------------------------------------------------
    // Blur pass
    //---------------------------------------------------------------------------------------------------------
#ifdef MOTION_BLUR_APPLY
    vec2 WPos = inFragCoord.xy;
    const vec2 vTC = inVar(input, out_texcoord);
    const vec2 pixQuadIdx = fmod(WPos, 2);
    float samplingDither = (-0.25 + 2.0 * 0.25 * pixQuadIdx.x) * (-1.0 + 2.0 * pixQuadIdx.y);
    
    vec2 tileBorderDist = abs(frac(WPos * uMotionBlurParams.xy) - 0.5) * 2.0;
    tileBorderDist *= (samplingDither < 0) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    
    // randomize lookup into max velocity to reduce visibility of tiles with opposing directions
    float rndValue = NRand3(vTC).x - 0.5;
    vec2 tileOffset = (tileBorderDist * rndValue) * uMotionBlurParams.xy;
    
    vec3 maxVel = sampleLevelZero(tMBVelocity, vTC + tileOffset).rgb;
    maxVel.xy = DecodeMotionVector(maxVel.xy);
    
    const float fScrW = uScreenBounds.z * uViewWidth;
    const float fScrH = uScreenBounds.w * uViewHeight;
    const float fScrX = uScreenBounds.x * uViewWidth;
    const float fScrY = uScreenBounds.y * uViewHeight;
    
    const float fBX1 = fScrX + ((fScrW - fScrX) * uResolutionScale.x);
    const float fBY1 = fScrY + ((fScrH - fScrY) * uResolutionScale.y);
    
    const vec4 vScreenBounds = vec4(fBX1, fBY1, fScrX, fScrY) / uMotionBlurParams.zwzw;  
    const vec4 samplerCenter = sampleLevelZero(tMBGeneral, clamp(vTC, vScreenBounds.zw, vScreenBounds.xy));
    
    if(length(maxVel.xy) < PIXEL_BIAS)
    {
        outVarFragment(output, fragment) = samplerCenter;
        outReturn(output)
    }
   
#if MOTION_BLUR_QUALITY == 3 || (defined(__ORBIS__) || defined (__PROSPERO__))
    const int nSamples = 24;
#elif MOTION_BLUR_QUALITY == 2
    const int nSamples = 14;
#else
    const int nSamples = 6;
#endif
    
    const float weightStep = rcp(float(nSamples));
    const vec2 blurStep = maxVel.xy * weightStep;
    const vec2 centerLenDepth = UnpackLengthAndDepth(sampleLevelZero(tMBEncode, vTC).zw);
    const float lengthToSampleIndex = rcp(length(blurStep));
    
    vec4 acc = samplerCenter;
    float fTotalWeights = 1.0;
    
#if !defined(__ORBIS__) && !defined (__PROSPERO__) 
    unroll
#endif
    for(int s = 1; s < nSamples/2; ++s)
    {
        const float curStep = (s + samplingDither);
        vec2 tc0 = clamp(vTC + blurStep * curStep, vScreenBounds.zw, vScreenBounds.xy);
        vec2 tc1 = clamp(vTC - blurStep * curStep, vScreenBounds.zw, vScreenBounds.xy);
        
        vec2 lenDepth0 = UnpackLengthAndDepth(sampleLevelZero(tMBEncode, tc0).zw);
        vec2 lenDepth1 = UnpackLengthAndDepth(sampleLevelZero(tMBEncode, tc1).zw);
        
        float weight0 = MBSampleWeight(centerLenDepth.y, lenDepth0.y, centerLenDepth.x, lenDepth0.x,
                                       s, lengthToSampleIndex);
        float weight1 = MBSampleWeight(centerLenDepth.y, lenDepth1.y, centerLenDepth.x, lenDepth1.x,
                                       s, lengthToSampleIndex);
        
        bvec2 mirror = bvec2(lenDepth1.x > lenDepth0.x, lenDepth0.y > lenDepth1.y);
        
        weight0 = all(mirror) ? weight1 : weight0;
        weight1 = any(mirror) ? weight1 : weight0;
        
        weight0 = step(1.0, weight0);
        weight1 = step(1.0, weight1);
        
        acc += sampleLevelZero(tMBGeneral, clamp(tc0, vScreenBounds.zw, vScreenBounds.xy)) * weight0;
        acc += sampleLevelZero(tMBGeneral, clamp(tc1, vScreenBounds.zw, vScreenBounds.xy)) * weight1;
        
        fTotalWeights += weight0;
        fTotalWeights += weight1;
    }
    
    acc /= fTotalWeights;
    
    outVarFragment(output, fragment) = acc;
#endif // MOTION_BLUR_APPLY

    //---------------------------------------------------------------------------------------------------------
    // Motion blur visualization
    //---------------------------------------------------------------------------------------------------------
#ifdef MOTION_BLUR_VISUALIZE
    const vec2 vTC = inVar(input, out_texcoord);
    vec3 maxVel = sampleLevelZero(tMBVelocity, vTC).rgb;
    maxVel.xy = DecodeMotionVector(maxVel.xy);
    
    const vec2 tilePos = vTC - (frac(vTC / uMotionBlurParams.xy) - 0.5f) * uMotionBlurParams.xy;
    const vec2 tileBorderPos = tilePos - uMotionBlurParams.xy*0.5;
    
    float d1 = DrawLine(tileBorderPos, tileBorderPos + vec2(uMotionBlurParams.x, 0.0), vTC, 0.00075 / pow(uResolutionScale.x, 0.5));
    float d2 = DrawLine(tileBorderPos, tileBorderPos + vec2(0.0, uMotionBlurParams.y), vTC, 0.00075 / pow(uResolutionScale.y, 0.5));
    float d3 = DrawLine(tilePos, tilePos + maxVel.xy, vTC, 0.0008);
    
    float d = max(max(d1, d2), d3);
    
    outVarFragment(output, fragment) = d == d3 ? vec4(d, d, d, d) : vec4(d*0.5, 0.0, d, d);
#endif // MOTION_BLUR_VISUALIZE

    outReturn(output)
}

#endif
