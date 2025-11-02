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
// This software contains source code provided by NVIDIA Corporation.
// Copyright (c) 2011 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, NONINFRINGEMENT,IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA 
// OR ITS SUPPLIERS BE  LIABLE  FOR  ANY  DIRECT, SPECIAL,  INCIDENTAL,  INDIRECT,  OR  
// CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS 
// OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY 
// OTHER PECUNIARY LOSS) ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, 
// EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

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

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

def_sampler(2D, tFxaaTex, 0);

#ifdef FXAA_FAST
//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec2 texcoord = inVar(input, out_texcoord);
    vec2 frameBufSize = vec2(float(uViewWidth), float(uViewHeight));
    
    vec3 rgbNW = sampleLevelZero(tFxaaTex, texcoord + (vec2(-1.0, -1.0) / frameBufSize)).xyz;
    vec3 rgbNE = sampleLevelZero(tFxaaTex, texcoord + (vec2( 1.0, -1.0) / frameBufSize)).xyz;
    vec3 rgbSW = sampleLevelZero(tFxaaTex, texcoord + (vec2(-1.0,  1.0) / frameBufSize)).xyz;
    vec3 rgbSE = sampleLevelZero(tFxaaTex, texcoord + (vec2( 1.0,  1.0) / frameBufSize)).xyz;
    vec3 rgbM  = sampleLevelZero(tFxaaTex, texcoord).xyz;
    
    vec3 luma = vec3(0.299, 0.587, 0.114);
    
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);
    
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * rcp(8.0)), rcp(128.0));
    
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    
    dir = min(vec2(8.0, 8.0),
              max(vec2(-8.0, -8.0),
                  dir * rcpDirMin)) / frameBufSize;
    
    vec3 rgbA = rcp(2.0) * (sampleLevelZero(tFxaaTex, texcoord + dir * (1.0/3.0 - 0.5)).xyz +
                             sampleLevelZero(tFxaaTex, texcoord + dir * (2.0/3.0 - 0.5)).xyz);
    
    vec3 rgbB = rgbA * rcp(2.0) + rcp(4.0) * (sampleLevelZero(tFxaaTex, texcoord + dir * (0.0/3.0 - 0.5)).xyz +
                                                sampleLevelZero(tFxaaTex, texcoord + dir * (3.0/3.0 - 0.5)).xyz);
    float lumaB = dot(rgbB, luma);
    
    if((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        outVarFragment(output, fragment) = vec4(rgbA, 1.0);
        outReturn(output)
    }
    
    outVarFragment(output, fragment) = vec4(rgbB, 1.0);
    outReturn(output)
}

#else

/*============================================================================
                              COMPILE-IN KNOBS
------------------------------------------------------------------------------
FXAA_EDGE_THRESHOLD - The minimum amount of local contrast required 
                      to apply algorithm.
                      1.0/3.0  - too little
                      1.0/4.0  - good start
                      1.0/8.0  - applies to more edges
                      1.0/16.0 - overkill
------------------------------------------------------------------------------
FXAA_EDGE_THRESHOLD_MIN - Trims the algorithm from processing darks.
                          Perf optimization.
                          1.0/32.0 - visible limit (smaller isn't visible)
                          1.0/16.0 - good compromise
                          1.0/12.0 - upper limit (seeing artifacts)
------------------------------------------------------------------------------
FXAA_SEARCH_STEPS - Maximum number of search steps for end of span.
------------------------------------------------------------------------------
FXAA_SEARCH_THRESHOLD - Controls when to stop searching.
                        1.0/4.0 - seems to be the best quality wise
------------------------------------------------------------------------------
FXAA_SUBPIX_TRIM - Controls sub-pixel aliasing removal.
                   1.0/2.0 - low removal
                   1.0/3.0 - medium removal
                   1.0/4.0 - default removal
                   1.0/8.0 - high removal
                   0.0 - complete removal
------------------------------------------------------------------------------
FXAA_SUBPIX_CAP - Insures fine detail is not completely removed.
                  This is important for the transition of sub-pixel detail,
                  like fences and wires.
                  3.0/4.0 - default (medium amount of filtering)
                  7.0/8.0 - high amount of filtering
                  1.0 - no capping of sub-pixel aliasing removal
============================================================================*/
#define FXAA_EDGE_THRESHOLD      (1.0 / 8.0)
#define FXAA_EDGE_THRESHOLD_MIN  (1.0 / 24.0)
#define FXAA_SEARCH_STEPS        32
#define FXAA_SEARCH_THRESHOLD    (1.0 / 4.0)
#define FXAA_SUBPIX_CAP          (3.0 / 4.0)
#define FXAA_SUBPIX_TRIM         (1.0 / 4.0)

#define FXAA_SUBPIX_TRIM_SCALE (1.0 / (1.0 - FXAA_SUBPIX_TRIM))

// Return the luma, the estimation of luminance from rgb inputs.
// This approximates luma using one FMA instruction,
// skipping normalization and tossing out blue.
// FxaaLuma() will range 0.0 to 2.963210702.
float FxaaLuma(vec3 rgb) 
{
    return rgb.y * (0.587/0.299) + rgb.x; 
} 

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec2 pos = inVar(input, out_texcoord);
    vec2 rcpFrame = vec2(rcp(float(uViewWidth)), rcp(float(uViewHeight)));
    
    // Early exit if local contrast below edge detect limit
    vec3 rgbN = sampleLevelZero(tFxaaTex, pos + vec2( 0.0, -1.0) * rcpFrame).xyz;
    vec3 rgbW = sampleLevelZero(tFxaaTex, pos + vec2(-1.0,  0.0) * rcpFrame).xyz;
    vec3 rgbM = sampleLevelZero(tFxaaTex, pos + vec2( 0.0,  0.0) * rcpFrame).xyz;
    vec3 rgbE = sampleLevelZero(tFxaaTex, pos + vec2( 1.0,  0.0) * rcpFrame).xyz;
    vec3 rgbS = sampleLevelZero(tFxaaTex, pos + vec2( 0.0,  1.0) * rcpFrame).xyz;
    float lumaN = FxaaLuma(rgbN);
    float lumaW = FxaaLuma(rgbW);
    float lumaM = FxaaLuma(rgbM);
    float lumaE = FxaaLuma(rgbE);
    float lumaS = FxaaLuma(rgbS); 
    float rangeMin = min(lumaM, min(min(lumaN, lumaW), min(lumaS, lumaE)));
    float rangeMax = max(lumaM, max(max(lumaN, lumaW), max(lumaS, lumaE)));
    float range = rangeMax - rangeMin;
    vec3 rgbL = rgbN + rgbW + rgbM + rgbE + rgbS;
    
    // Compute lowpass
    float lumaL = (lumaN + lumaW + lumaE + lumaS) * 0.25;
    float rangeL = abs(lumaL - lumaM);
    float blendL = 0.0;
    if(range != 0)
    {
        blendL = max(0.0, (rangeL / range) - FXAA_SUBPIX_TRIM) * FXAA_SUBPIX_TRIM_SCALE;
    }
    
    blendL = min(FXAA_SUBPIX_CAP, blendL);
    
    // Choose vertical or horizontal search
    vec3 rgbNW = sampleLevelZero(tFxaaTex, pos + vec2(-1.0, -1.0) * rcpFrame).xyz;
    vec3 rgbNE = sampleLevelZero(tFxaaTex, pos + vec2( 1.0, -1.0) * rcpFrame).xyz;
    vec3 rgbSW = sampleLevelZero(tFxaaTex, pos + vec2(-1.0,  1.0) * rcpFrame).xyz;
    vec3 rgbSE = sampleLevelZero(tFxaaTex, pos + vec2( 1.0,  1.0) * rcpFrame).xyz;
    rgbL += (rgbNW + rgbNE + rgbSW + rgbSE);
    rgbL *= 1.0 / 9.0;
    float lumaNW = FxaaLuma(rgbNW);
    float lumaNE = FxaaLuma(rgbNE);
    float lumaSW = FxaaLuma(rgbSW);
    float lumaSE = FxaaLuma(rgbSE);
    
    float edgeVert = abs((0.25 * lumaNW) + (-0.5 * lumaN) + (0.25 * lumaNE)) +
                     abs((0.50 * lumaW ) + (-1.0 * lumaM) + (0.50 * lumaE )) +
                     abs((0.25 * lumaSW) + (-0.5 * lumaS) + (0.25 * lumaSE));
    float edgeHorz = abs((0.25 * lumaNW) + (-0.5 * lumaW) + (0.25 * lumaSW)) +
                     abs((0.50 * lumaN ) + (-1.0 * lumaM) + (0.50 * lumaS )) +
                     abs((0.25 * lumaNE) + (-0.5 * lumaE) + (0.25 * lumaSE));
                     
    bool horzSpan = edgeHorz >= edgeVert;
    float lengthSign = horzSpan ? -rcpFrame.y : -rcpFrame.x;
    if(!horzSpan)
    {
        lumaN = lumaW;
        lumaS = lumaE;
    }
    
    float gradientN = abs(lumaN - lumaM);
    float gradientS = abs(lumaS - lumaM);
    lumaN = (lumaN + lumaM) * 0.5;
    lumaS = (lumaS + lumaM) * 0.5;
    
    // Choose side of pixel where gradient is highest
    bool pairN = gradientN >= gradientS;
    if(!pairN)
    {
        lumaN = lumaS;
        gradientN = gradientS;
        lengthSign *= -1.0;
    }
    
    vec2 posN;
    posN.x = pos.x + (horzSpan ? 0.0 : lengthSign * 0.5);
    posN.y = pos.y + (horzSpan ? lengthSign * 0.5 : 0.0);
    
    gradientN *= FXAA_SEARCH_THRESHOLD;
    
    vec2 posP = posN;
    vec2 offNP = horzSpan ? vec2(rcpFrame.x, 0.0) : vec2(0.0f, rcpFrame.y); 
    float lumaEndN = lumaN;
    float lumaEndP = lumaN;
    bool doneN = false;
    bool doneP = false;
    posN += offNP * vec2(-1.0, -1.0);
    posP += offNP * vec2( 1.0,  1.0);

    for(int i = 0; i < FXAA_SEARCH_STEPS; i++) 
    {
        if(!doneN) lumaEndN = FxaaLuma(sampleLevelZero(tFxaaTex, posN.xy).xyz);
        if(!doneP) lumaEndP = FxaaLuma(sampleLevelZero(tFxaaTex, posP.xy).xyz);
       
        doneN = doneN || (abs(lumaEndN - lumaN) >= gradientN);
        doneP = doneP || (abs(lumaEndP - lumaN) >= gradientN);
        if(doneN && doneP) 
        {
            break;
        }
        
        if(!doneN) posN -= offNP;
        if(!doneP) posP += offNP; 
    }

    // Handle if center is on positive or negative side
    float dstN = horzSpan ? pos.x - posN.x : pos.y - posN.y;
    float dstP = horzSpan ? posP.x - pos.x : posP.y - pos.y;
    bool directionN = dstN < dstP;
    lumaEndN = directionN ? lumaEndN : lumaEndP;
    
    // Check if pixel is in section of span which gets no filtering
    if(((lumaM - lumaN) < 0.0) == ((lumaEndN - lumaN) < 0.0))
    {
        lengthSign = 0.0;
    }
 
    // Compute sub-pixel offset and filter span
    float spanLength = (dstP + dstN);
    dstN = directionN ? dstN : dstP;
    float subPixelOffset = (0.5 + (dstN * -rcp(spanLength))) * lengthSign;
    
    vec2 posF = vec2(pos.x + (horzSpan ? 0.0 : subPixelOffset), pos.y + (horzSpan ? subPixelOffset : 0.0));
    vec3 rgbF = sampleLevelZero(tFxaaTex, posF).xyz;
    
    outVarFragment(output, fragment) = vec4(mix(rgbF, rgbL, blendL), 1.0);
    outReturn(output)
}

#endif

#endif
