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
#include "progs/SSAO/SSAO_defines.inc"

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_pixelTarget(vec4, 0)
    def_var_pixelTarget(vec4, 1)
    def_var_pixelTarget(vec4, 2)
    def_var_pixelTarget(vec4, 3)
end_output

def_sampler(2D, tDepthSource, 0);

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    ivec2 baseCoord = ivec2(inFragCoord.xy) * 2;
    float a = loadOffset(tDepthSource, baseCoord, 0, ivec2(0, 0)).x;
    float b = loadOffset(tDepthSource, baseCoord, 0, ivec2(1, 0)).x;
    float c = loadOffset(tDepthSource, baseCoord, 0, ivec2(0, 1)).x;
    float d = loadOffset(tDepthSource, baseCoord, 0, ivec2(1, 1)).x;
    
    vec4 out0 = ScreenSpaceToViewSpaceDepth(a).xxxx;
    vec4 out1 = ScreenSpaceToViewSpaceDepth(b).xxxx;
    vec4 out2 = ScreenSpaceToViewSpaceDepth(c).xxxx;
    vec4 out3 = ScreenSpaceToViewSpaceDepth(d).xxxx;
    
    outVarPixelTarget(output, 0) = out0;
    outVarPixelTarget(output, 1) = out1;
    outVarPixelTarget(output, 2) = out2;
    outVarPixelTarget(output, 3) = out3;
    
    outReturn(output)
}

#endif
