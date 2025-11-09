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

begin_cbuffer(PostProcess_SimpleBlur, 0)
    cbuffer_member(vec4, uParams);
end_cbuffer()

def_sampler(2D, tSimpleBlurTex, 0);

//----------------------------------------------------
vec4 ComputeTexel(const vec2 vTCoord, const float w)
{
    return sampleLevelZero(tSimpleBlurTex, vTCoord / uParams.xy) * w;
}

//----------------------------------------------------
void ProcessBlur(const vec2 vTCoord, const float x, const float y, inout vec4 outColor, inout float fWeight, inout float fLum)
{
    fLum = pow(uParams.w, 0.5) - pow((x * x) + (y * y), 0.5);
    outColor += ComputeTexel(vTCoord + vec2(x, y), fLum);
    fWeight += fLum;
}

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec4 outColor = vec4(0.0, 0.0, 0.0, 0.0);
    vec2 uv = inVar(input, out_texcoord);
    
    float x = uv.x * uParams.x - 0.5;
    float y = uv.y * uParams.y - 0.5;
    
    vec2 vTCoord = vec2(x, y);
    
    float w = 0.0;
    float l = 0.0;
    
    const int count = cast(int, uParams.z);
    
    for(int i = -count; i <= count; i += 2)
    {
        for(int j = -count; j <= count; j += 2)
        {
            ProcessBlur(vTCoord, i, j, outColor, w, l);
        }
    }
    
    if(w < 1.0)
    {
        w = 1.0;
    }
    
    outColor /= w;
    outVarFragment(output, fragment) = vec4(outColor.rgb, 1.0);
    outReturn(output)
}

#endif
