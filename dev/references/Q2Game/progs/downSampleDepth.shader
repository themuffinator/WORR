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
    def_var_in(vec2, out_texcoord, TEXCOORD0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

def_sampler(2D, tDepthDS, 0);

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    /*vec2 vSize = vec2(0.5) / textureSize(uTex0, 0);
    vec4 vOffset = vec4(vSize, -vSize);
    
    float depth = RGBAToDepth(texture(uTex0, out_texcoord));
    
    float fD00 = RGBAToDepth(texture(uTex0, out_texcoord+vOffset.xy));
    float fD01 = RGBAToDepth(texture(uTex0, out_texcoord+vOffset.xw));
    float fD11 = RGBAToDepth(texture(uTex0, out_texcoord+vOffset.zw));
    float fD10 = RGBAToDepth(texture(uTex0, out_texcoord+vOffset.zy));
    
    float fDepth = min(min(min(fD00, fD01), min(fD11, fD10)), depth);*/
    
    ivec4 vOffset = ivec4(1, 1, -1, -1);
    ivec2 vCoord = ivec2(inFragCoord.xy*2.0);
    
    float depth = LinearizeDepth(uZNear, uZFar, RGBAToDepth(load(tDepthDS, vCoord, 0)));
    
    float fD00 = LinearizeDepth(uZNear, uZFar, RGBAToDepth(load(tDepthDS, vCoord+vOffset.xy, 0)));
    float fD01 = LinearizeDepth(uZNear, uZFar, RGBAToDepth(load(tDepthDS, vCoord+vOffset.xw, 0)));
    float fD11 = LinearizeDepth(uZNear, uZFar, RGBAToDepth(load(tDepthDS, vCoord+vOffset.zw, 0)));
    float fD10 = LinearizeDepth(uZNear, uZFar, RGBAToDepth(load(tDepthDS, vCoord+vOffset.zy, 0)));
    
    float fDepth = min(min(min(fD00, fD01), min(fD11, fD10)), depth);
    
    if(fDepth >= 0.9999)
    {
        outVarFragment(output, fragment) = vec4(1.0, 1.0, 1.0, 1.0);
        outReturn(output)
    }
    else
    {
        outVarFragment(output, fragment) = DepthToRGBA(fDepth);
    }
    
    outReturn(output)
}

#endif
