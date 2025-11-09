//
// Copyright(C) 2022-2023 Nightdive Studios, LLC
//
// ORIGINAL AUTHOR
//      Samuel Villarreal
//

#include "progs/common.inc"

#ifdef SHADER_VERTEX

//----------------------------------------------------
// input
begin_input(inVertex)
    var_attrib(0, vec3);
    var_attrib(1, vec2);
    var_attrib(2, vec4);
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec2, out_texcoord, TEXCOORD0)
    def_var_out(vec4, out_color, COLOR0)
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    vec4 vertex                         = vec4(inVarAttrib(0, input), 1.0);
    
    vec4 vPosition                      = mul(uModelViewMatrix, vertex);
    outVarPosition(output, position)    = mul(uProjectionMatrix, vPosition);
    outVar(output, out_texcoord)        = inVarAttrib(1, input);
    outVar(output, out_color)           = inVarAttrib(2, input);
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
    def_var_in(vec4, out_color, COLOR0)
end_input

#include "progs/includes/QuakeCommonDefines.inc"

//----------------------------------------------------
// output
begin_output(outPixel)
    RT_DIFFUSE_TARGET
#ifndef GLSLC_NX
    RT_MOTIONBLUR_TARGET
#endif
    RT_NORMALS_TARGET
    RT_LIGHTBUFFER_TARGET
    RT_GLOWBUFFER_TARGET
end_output

def_sampler(2D, tQuakeParticleImage, 0);

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec4 color = sample(tQuakeParticleImage, inVar(input, out_texcoord));
    
    RT_DIFFUSE = color * inVar(input, out_color);
    outReturn(output)
}

#endif
