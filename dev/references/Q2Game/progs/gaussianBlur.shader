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

begin_cbuffer(PostProcess_GaussianBlur, 0)
    cbuffer_member(float, uBlurRadius);
    cbuffer_member(float, uSize);
    cbuffer_member(int, uDirection);
    cbuffer_member(int, uBlurType);
end_cbuffer()

//----------------------------------------------------
vec2 GetBlurStep(const in vec2 vCoord,
                 const in float stepCount,
                 const in float blur,
                 const in float hstep,
                 const in float vstep)
{
    vec2 outVec = vec2(vCoord.x + stepCount * blur * hstep,
                       vCoord.y + stepCount * blur * vstep);
                       
    return clamp(outVec, 0.0, 1.0);
}

def_sampler(2D, tGaussTex, 0);

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec4 sum = vec4(0.0, 0.0, 0.0, 0.0);
    float blur = uBlurRadius / uSize;
    float hstep;
    float vstep;
    
    branch
    if(uDirection == 0)
    {
        hstep = 0.0;
        vstep = 1.0;
    }
    else
    {
        hstep = 1.0;
        vstep = 0.0;
    }
    
    vec2 vTC = inVar(input, out_texcoord);
    
    sum += sample(tGaussTex, GetBlurStep(vTC, -4.0, blur, hstep, vstep)) * 0.0162162162;
    sum += sample(tGaussTex, GetBlurStep(vTC, -3.0, blur, hstep, vstep)) * 0.0540540541;
    sum += sample(tGaussTex, GetBlurStep(vTC, -2.0, blur, hstep, vstep)) * 0.1216216216;
    sum += sample(tGaussTex, GetBlurStep(vTC, -1.0, blur, hstep, vstep)) * 0.1945945946;
    
    sum += sample(tGaussTex, vTC) * 0.2270270270;
    
    sum += sample(tGaussTex, GetBlurStep(vTC, 1.0, blur, hstep, vstep)) * 0.1945945946;
    sum += sample(tGaussTex, GetBlurStep(vTC, 2.0, blur, hstep, vstep)) * 0.1216216216;
    sum += sample(tGaussTex, GetBlurStep(vTC, 3.0, blur, hstep, vstep)) * 0.0540540541;
    sum += sample(tGaussTex, GetBlurStep(vTC, 4.0, blur, hstep, vstep)) * 0.0162162162;
    
    outVarFragment(output, fragment) = vec4(sum.rgb, 1.0);
    outReturn(output)
}

#endif
