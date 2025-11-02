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
#include "progs/SSAO/SSAO_GenerateAO.inc"

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_pixelTarget(vec4, 0)
end_output

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    vec2 inPos = inFragCoord.xy;
    float outShadowTerm;
    float outWeight;
    vec4 outEdges;
    GenerateSSAOShadowsInternal(outShadowTerm, outEdges, outWeight, inPos.xy, 1, false);
    vec2 out0;
    out0.x = outShadowTerm;
    out0.y = PackEdges(outEdges);
    outVarPixelTarget(output, 0) = vec4(out0, 0.0, 0.0);
    outReturn(output)
}

#endif
