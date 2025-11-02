//
// Copyright(C) 2016-2017 Samuel Villarreal
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
    var_attrib(ATTRIB_POSITION, vec3);
    var_attrib(ATTRIB_TEXCOORD, vec2);
    var_attrib(ATTRIB_COLOR, vec4);
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec2, out_texcoord, TEXCOORD0)
    def_var_out(vec4, out_color,    COLOR0)
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    vec4 vertex                         = vec4(inVarAttrib(ATTRIB_POSITION, input), 1.0);
    outVarPosition(output, position)    = mul(uProjectionMatrix, mul(uModelViewMatrix, vertex));
    outVar(output, out_texcoord)        = inVarAttrib(ATTRIB_TEXCOORD, input);
    outVar(output, out_color)           = inVarAttrib(ATTRIB_COLOR, input);
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
    def_var_in(vec4, out_color,    COLOR0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

def_sampler(2D, tYUVSamp0, 0);
def_sampler(2D, tYUVSamp1, 1);

#if !defined(__ORBIS__) && !defined(__PROSPERO__)
def_sampler(2D, tYUVSamp2, 2);
#endif

static const vec3 offset = vec3(-0.0625, -0.5, -0.5);
static const vec3 Rcoeff = vec3(1.164,  0.000,  1.596);
static const vec3 Gcoeff = vec3(1.164, -0.391, -0.813);
static const vec3 Bcoeff = vec3(1.164,  2.018,  0.000);

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec3 yuv, rgb;
    
    yuv.x = sampleLevelZero(tYUVSamp0, inVar(input, out_texcoord)).r;
    
#if !defined(__ORBIS__) && !defined(__PROSPERO__)
    yuv.y = sampleLevelZero(tYUVSamp1, inVar(input, out_texcoord)).r;
    yuv.z = sampleLevelZero(tYUVSamp2, inVar(input, out_texcoord)).r;
#else
    yuv.yz = sampleLevelZero(tYUVSamp1, inVar(input, out_texcoord)).rg;
#endif

    yuv += offset;
    rgb.r = dot(yuv, Rcoeff);
    rgb.g = dot(yuv, Gcoeff);
    rgb.b = dot(yuv, Bcoeff);
    
    outVarFragment(output, fragment) = vec4(rgb, 1.0);
    outReturn(output)
}

#endif
