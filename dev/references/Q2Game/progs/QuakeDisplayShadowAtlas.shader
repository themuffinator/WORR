//
// Copyright(C) 2019-2020 Nightdive Studios, LLC
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
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    vec4 vertex                         = vec4(inVarAttrib(0, input), 1.0);
    outVarPosition(output, position)    = mul(uProjectionMatrix, mul(uModelViewMatrix, vertex));
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

def_sampler(2D, tShadowMap, 0);

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    float d = LinearizeDepth(uZNear, uZFar, sampleLevelZero(tShadowMap, inVar(input, out_texcoord)).r);

    outVarFragment(output, fragment) = vec4(d, d, d, 1.0);
    outReturn(output)
}

#endif
