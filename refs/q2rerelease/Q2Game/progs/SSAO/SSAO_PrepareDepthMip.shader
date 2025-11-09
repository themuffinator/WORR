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

def_sampler(2D, tDepthSource1, 0);
#if 0
def_sampler(2D, tDepthSource2, 1);
def_sampler(2D, tDepthSource3, 2);
def_sampler(2D, tDepthSource4, 3);
#endif

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
#if 0
    ivec2 baseCoord = ivec2(inFragCoord.xy) * 2;
    
    vec4 depthsArr[4];
    float depthsOutArr[4];
    
    depthsArr[0].x = loadOffset(tDepthSource1, baseCoord, 0, ivec2(0, 0)).x;
    depthsArr[0].y = loadOffset(tDepthSource1, baseCoord, 0, ivec2(1, 0)).x;
    depthsArr[0].z = loadOffset(tDepthSource1, baseCoord, 0, ivec2(0, 1)).x;
    depthsArr[0].w = loadOffset(tDepthSource1, baseCoord, 0, ivec2(1, 1)).x;
    
    depthsArr[1].x = loadOffset(tDepthSource2, baseCoord, 0, ivec2(0, 0)).x;
    depthsArr[1].y = loadOffset(tDepthSource2, baseCoord, 0, ivec2(1, 0)).x;
    depthsArr[1].z = loadOffset(tDepthSource2, baseCoord, 0, ivec2(0, 1)).x;
    depthsArr[1].w = loadOffset(tDepthSource2, baseCoord, 0, ivec2(1, 1)).x;
    
    depthsArr[2].x = loadOffset(tDepthSource3, baseCoord, 0, ivec2(0, 0)).x;
    depthsArr[2].y = loadOffset(tDepthSource3, baseCoord, 0, ivec2(1, 0)).x;
    depthsArr[2].z = loadOffset(tDepthSource3, baseCoord, 0, ivec2(0, 1)).x;
    depthsArr[2].w = loadOffset(tDepthSource3, baseCoord, 0, ivec2(1, 1)).x;
    
    depthsArr[3].x = loadOffset(tDepthSource4, baseCoord, 0, ivec2(0, 0)).x;
    depthsArr[3].y = loadOffset(tDepthSource4, baseCoord, 0, ivec2(1, 0)).x;
    depthsArr[3].z = loadOffset(tDepthSource4, baseCoord, 0, ivec2(0, 1)).x;
    depthsArr[3].w = loadOffset(tDepthSource4, baseCoord, 0, ivec2(1, 1)).x;
    
    const uvec2 SVPosui         = uvec2(inFragCoord.xy);
    const uint pseudoRandomA    = (SVPosui.x) + 2 * (SVPosui.y);
    
    float dummyUnused1;
    float dummyUnused2;
    float falloffCalcMulSq, falloffCalcAdd;
 
    unroll
    for(int i = 0; i < 4; i++)
    {
        vec4 depths = depthsArr[i];
        float closest = min(min(depths.x, depths.y), min(depths.z, depths.w));

        CalculateRadiusParameters(abs(closest), vec2(1.0, 1.0), dummyUnused1, dummyUnused2, falloffCalcMulSq);

        vec4 dists = depths - closest.xxxx;
        vec4 weights = saturate(dists * dists * falloffCalcMulSq + 1.0);

        float smartAvg = dot(weights, depths) / dot(weights, vec4(1.0, 1.0, 1.0, 1.0));
        const uint pseudoRandomIndex = (pseudoRandomA + i) % 4;

        //depthsOutArr[i] = closest;
        //depthsOutArr[i] = depths[ pseudoRandomIndex ];
        depthsOutArr[i] = smartAvg;
    }
    
    outVarPixelTarget(output, 0) = depthsOutArr[0].xxxx;
    outVarPixelTarget(output, 1) = depthsOutArr[1].xxxx;
    outVarPixelTarget(output, 2) = depthsOutArr[2].xxxx;
    outVarPixelTarget(output, 3) = depthsOutArr[3].xxxx;
#else
    
    ivec2 baseCoord = ivec2(inVar(input, out_texcoord) * ScreenSize());
    float a = loadOffset(tDepthSource1, baseCoord, 0, ivec2(0, 0)).x;
    float b = loadOffset(tDepthSource1, baseCoord, 0, ivec2(1, 0)).x;
    float c = loadOffset(tDepthSource1, baseCoord, 0, ivec2(0, 1)).x;
    float d = loadOffset(tDepthSource1, baseCoord, 0, ivec2(1, 1)).x;
    
    outVarPixelTarget(output, 0) = ScreenSpaceToViewSpaceDepth(a).xxxx;
    outVarPixelTarget(output, 1) = ScreenSpaceToViewSpaceDepth(b).xxxx;
    outVarPixelTarget(output, 2) = ScreenSpaceToViewSpaceDepth(c).xxxx;
    outVarPixelTarget(output, 3) = ScreenSpaceToViewSpaceDepth(d).xxxx;
#endif
    outReturn(output)
}

#endif
