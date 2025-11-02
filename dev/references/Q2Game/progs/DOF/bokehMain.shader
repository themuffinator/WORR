//
// Copyright(C) 2020-2021 Samuel Villarreal
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
#if defined(BOKEH_COC)
    def_var_pixelTarget(float, 0)
#else
    def_var_pixelTarget(vec4, 0)
#endif
end_output

def_sampler(2D, tDOFDisplay, 0);

#if defined(BOKEH_DOWNSAMPLE_COC) || defined(BOKEH_COMBINE)
def_sampler(2D, tCoC, 1);
#endif

#if defined(BOKEH_COMBINE)
def_sampler(2D, tDOFCombine, 2);
#endif

#if defined(BOKEH_COC)
    #if defined(__ORBIS__) ||  defined(__PROSPERO__)
        #pragma PSSL_target_output_format (target 0 FMT_32_R)
    #endif
#endif

//----------------------------------------------------
begin_cbuffer(BokehParams, 0)
    cbuffer_member(float, uBlurRange);
    cbuffer_member(float, uFocusDistance);
    cbuffer_member(float, uFocusRange);
    cbuffer_member(float, uLumaStrength);
    cbuffer_member(float, uScreenWidth);
    cbuffer_member(float, uScreenHeight);
end_cbuffer()

#if defined(BOKEH_INITIAL_BLUR)

//----------------------------------------------------
#define kernelSampleCount   22
static const vec2 kernel[kernelSampleCount] =
{
    vec2(0, 0),
    vec2(0.53333336, 0),
    vec2(0.3325279, 0.4169768),
    vec2(-0.11867785, 0.5199616),
    vec2(-0.48051673, 0.2314047),
    vec2(-0.48051673, -0.23140468),
    vec2(-0.11867763, -0.51996166),
    vec2(0.33252785, -0.4169769),
    vec2(1, 0),
    vec2(0.90096885, 0.43388376),
    vec2(0.6234898, 0.7818315),
    vec2(0.22252098, 0.9749279),
    vec2(-0.22252095, 0.9749279),
    vec2(-0.62349, 0.7818314),
    vec2(-0.90096885, 0.43388382),
    vec2(-1, 0),
    vec2(-0.90096885, -0.43388376),
    vec2(-0.6234896, -0.7818316),
    vec2(-0.22252055, -0.974928),
    vec2(0.2225215, -0.9749278),
    vec2(0.6234897, -0.7818316),
    vec2(0.90096885, -0.43388376)
};

//----------------------------------------------------
float GetBokehSampleWeight(float coc, float radius)
{
    return saturate((coc - radius + 2.0) / 2.0);
}

//----------------------------------------------------
vec3 LumaBoost(const in vec3 color)
{
    float lum = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722)) * uLumaStrength;
    vec3 vOut = color * (1.0 + 0.2 * lum * lum * lum);
    return vOut * vOut;
}

#endif // BOKEH_INITIAL_BLUR

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec2 vCoord = inVar(input, out_texcoord);
    
//----------------------------------------------------
#if defined(BOKEH_COC)
    ivec2 iCoord = ivec2((vCoord + (uScreenBounds.xy * uResolutionScale.zw)) * ScreenSize());
    float depth = LinearizeDepth(uZNear, uZFar, load(tDOFDisplay, iCoord, 0).r) * uZFar;
    float coc = clamp((depth - uFocusDistance) / uFocusRange, -1.0, 1.0);
    float fSign = (coc < 0.0) ? -1.0 : 1.0;
    
    coc = smoothstep(0.1, 1.0, abs(coc)) * fSign;
    
    outVarPixelTarget(output, 0).r = coc * uBlurRange;

//----------------------------------------------------
#elif defined(BOKEH_INITIAL_BLUR)
    vec2 rcpScreenSize = rcp(vec2(uScreenWidth, uScreenHeight));
    vec3 color = vec3(0.0, 0.0, 0.0);
    float coc = sampleLevelZero(tDOFDisplay, vCoord).a;
    vec3 bgColor = vec3(0.0, 0.0, 0.0);
    vec3 fgColor = vec3(0.0, 0.0, 0.0);
    float bgWeight = 0.0;
    float fgWeight = 0.0;
    
#if !defined(__ORBIS__) && !defined (__PROSPERO__) 
    unroll
#endif
    for(int k = 0; k < kernelSampleCount; ++k)
    {
        vec2 o = kernel[k] * uBlurRange;
        float r = length(o);
        o *= rcpScreenSize;
        vec4 vTmp = sampleLevelZero(tDOFDisplay, vCoord + o * uResolutionScale.xy);
        vec3 vLum = vTmp.rgb + LumaBoost(vTmp.rgb);
        
        float bgw = GetBokehSampleWeight(max(0.0, min(vTmp.a, coc)), r);
        bgColor += vLum * bgw;
        bgWeight += bgw;
        
        float fgw = GetBokehSampleWeight(-vTmp.a, r);
        fgColor += vLum * fgw;
        fgWeight += fgw;
    }
    
    bgColor *= (1.0 / max(bgWeight, 1.0));
    fgColor *= (1.0 / max(fgWeight, 1.0));
    
    float total = min(1.0, fgWeight * 3.14159265359 / kernelSampleCount);
    color = mix(bgColor, fgColor, total);
    
    outVarPixelTarget(output, 0) = vec4(color, total);
    
//----------------------------------------------------
#elif defined(BOKEH_DOWNSAMPLE_COC)
    vec2 rcpScreenSize = rcp(vec2(uScreenWidth, uScreenHeight));
    vec4 o = rcpScreenSize.xyxy * vec2(-0.5, 0.5).xxyy;
    
    float coc1 = sampleLevelZero(tCoC, vCoord + o.xy).r;
    float coc2 = sampleLevelZero(tCoC, vCoord + o.zy).r;
    float coc3 = sampleLevelZero(tCoC, vCoord + o.xw).r;
    float coc4 = sampleLevelZero(tCoC, vCoord + o.zw).r;
    
    float cocMin = min(min(min(coc1, coc2), coc3), coc4);
    float cocMax = max(max(max(coc1, coc2), coc3), coc4);
    float cocAvg = cocMax >= -cocMin ? cocMax : cocMin;
    
    outVarPixelTarget(output, 0) = vec4(sampleLevelZero(tDOFDisplay, vCoord).rgb, cocAvg);

//----------------------------------------------------
#elif defined(BOKEH_TENT_FILTER)
    vec2 rcpScreenSize = rcp(vec2(uScreenWidth, uScreenHeight));
    vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 o = rcpScreenSize.xyxy * vec2(-0.5, 0.5).xxyy;
    
    color += sampleLevelZero(tDOFDisplay, vCoord + o.xy);
    color += sampleLevelZero(tDOFDisplay, vCoord + o.zy);
    color += sampleLevelZero(tDOFDisplay, vCoord + o.xw);
    color += sampleLevelZero(tDOFDisplay, vCoord + o.zw);
    
    color *= 0.25;
    
    outVarPixelTarget(output, 0) = color;
    
//----------------------------------------------------
#elif defined(BOKEH_COMBINE)
    float coc = sampleLevelZero(tCoC, vCoord).r;
    vec4 dof = sampleLevelZero(tDOFCombine, vCoord);
    vec4 src = sampleLevelZero(tDOFDisplay, vCoord);
    
    float s = smoothstep(0.1, 1.0, abs(coc));
    vec3 color = mix(src.rgb, dof.rgb, s + dof.a - s * dof.a);
    outVarPixelTarget(output, 0) = vec4(color, src.a);
#endif

    outReturn(output)
}

#endif
