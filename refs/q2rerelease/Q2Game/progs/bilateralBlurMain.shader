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

#define R   5

begin_cbuffer(PostProcess_BilateralBlur, 0)
    cbuffer_member(vec2,    uTextureSize);
    cbuffer_member(float,   uStrides);
    cbuffer_member(float,   uDepthFactor);
end_cbuffer()

//----------------------------------------------------
static const float g_fGaussWeights[6] =
#if defined(GLSL_VERSION) && !defined(VULKAN)
float[]
(
#else
{
#endif
    0.153170, 0.144893, 0.122649, 0.092902, 0.062970, 0
#if defined(GLSL_VERSION) && !defined(VULKAN)
);
#else
};
#endif

def_sampler(2D, tBilateralBlurSrc, 0);

static const vec2 axis_x = vec2(1.0, 0.0);
static const vec2 axis_y = vec2(0.0, 1.0);

//----------------------------------------------------
void ProcessBlur(const vec2 vTCoord, const vec2 vStep, const int r, const float compareDepth, inout float result, inout float weightSum)
{
    vec2 samplePos = vTCoord + vStep * r;
    vec2 samples = sampleLevelZero(tBilateralBlurSrc, samplePos).rg;
    float weight = 0.3 + g_fGaussWeights[int(abs(r))];
    weight *= max(0.0, 1.0 - uDepthFactor * abs(samples.g - compareDepth));
    result += samples.r * weight;
    weightSum += weight;
}

//----------------------------------------------------
vec4 BilateralBlur(vec2 axis, float size, const in vec2 vTCoord)
{  
    float result = 0.0;
    float weightSum = 0.0;
    float texelSize = uStrides / size;
    vec2 vStep = (axis * texelSize);
    float compareDepth = sampleLevelZero(tBilateralBlurSrc, vTCoord).g;
    
    ProcessBlur(vTCoord, vStep, -R+ 0, compareDepth, result, weightSum);
    ProcessBlur(vTCoord, vStep, -R+ 1, compareDepth, result, weightSum);
    ProcessBlur(vTCoord, vStep, -R+ 2, compareDepth, result, weightSum);
    ProcessBlur(vTCoord, vStep, -R+ 3, compareDepth, result, weightSum);
    ProcessBlur(vTCoord, vStep, -R+ 4, compareDepth, result, weightSum);
    ProcessBlur(vTCoord, vStep, -R+ 5, compareDepth, result, weightSum);
    ProcessBlur(vTCoord, vStep, -R+ 6, compareDepth, result, weightSum);
    ProcessBlur(vTCoord, vStep, -R+ 7, compareDepth, result, weightSum);
    ProcessBlur(vTCoord, vStep, -R+ 8, compareDepth, result, weightSum);
    ProcessBlur(vTCoord, vStep, -R+ 9, compareDepth, result, weightSum);
    ProcessBlur(vTCoord, vStep, -R+10, compareDepth, result, weightSum);
    
    result /= (weightSum + 0.0001);
#ifdef BLUR_H
    return vec4(result, compareDepth, 0.0, 0.0); // outputting to RG16F
#else
    return vec4(result, result, result, 1.0); // outputting to RGBA8
#endif
}

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
#ifdef BLUR_H
    outVarFragment(output, fragment) = BilateralBlur(axis_x, uTextureSize.x, inVar(input, out_texcoord));
#else
    outVarFragment(output, fragment) = BilateralBlur(axis_y, uTextureSize.y, inVar(input, out_texcoord));
#endif
    outReturn(output)
}

#endif
