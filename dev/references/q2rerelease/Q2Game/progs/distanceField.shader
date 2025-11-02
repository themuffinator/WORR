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

def_sampler(2D, tDistFieldTex, 0);

static const vec3   outline_color   = vec3(0.0,0.0,0.0);
static const float  glyph_center    = 0.50;
static const float  outline_center  = 0.55;
static const float  glow_center     = 1.25;

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)

    vec4  color = sampleLevelZero(tDistFieldTex, inVar(input, out_texcoord));
    float dist  = color.r;
    float width = fwidth(dist);
    float alpha = smoothstep(glyph_center - width, glyph_center + width, dist);

    // Glow + outline
    vec3 rgb = mix(inVar(input, out_color).rgb, inVar(input, out_color).rgb, alpha);
    float mu = smoothstep(glyph_center, glow_center, sqrt(dist));
    
    color = vec4(rgb, max(alpha,mu));
    
    float beta = smoothstep(outline_center - width, outline_center + width, dist);
    
    rgb = mix(outline_color, color.rgb, beta);
    outVarFragment(output, fragment) = vec4(rgb, max(color.a, beta) * inVar(input, out_color).a);
    
    outReturn(output)
}

#endif
