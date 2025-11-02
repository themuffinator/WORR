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
end_output

def_samplerArray(2D, tBlurInput, 0);

//----------------------------------------------------
void AddSample(float ssaoValue, float edgeValue, inout float sum, inout float sumWeight)
{
    float weight = edgeValue;

    sum += (weight * ssaoValue);
    sumWeight += weight;
}

//----------------------------------------------------
vec2 SampleBlurredWide(vec2 coord)
{
    vec3 fullCoord = vec3(coord, float(g_ASSAOConsts.PassIndex));
    vec2 vC = sampleLODOffset(tBlurInput, fullCoord, 0, ivec2(0, 0)).xy;
    vec2 vL = sampleLODOffset(tBlurInput, fullCoord, 0, ivec2(-2, 0)).xy;
    vec2 vT = sampleLODOffset(tBlurInput, fullCoord, 0, ivec2(0, -2)).xy;
    vec2 vR = sampleLODOffset(tBlurInput, fullCoord, 0, ivec2(2, 0)).xy;
    vec2 vB = sampleLODOffset(tBlurInput, fullCoord, 0, ivec2(0, 2)).xy;

    float packedEdges = vC.y;
    vec4 edgesLRTB = UnpackEdges(packedEdges);
    edgesLRTB.x *= UnpackEdges(vL.y).y;
    edgesLRTB.z *= UnpackEdges(vT.y).w;
    edgesLRTB.y *= UnpackEdges(vR.y).x;
    edgesLRTB.w *= UnpackEdges(vB.y).z;

    float ssaoValue = vC.x;
    float ssaoValueL = vL.x;
    float ssaoValueT = vT.x;
    float ssaoValueR = vR.x;
    float ssaoValueB = vB.x;

    float sumWeight = 0.8f;
    float sum = ssaoValue * sumWeight;

    AddSample(ssaoValueL, edgesLRTB.x, sum, sumWeight);
    AddSample(ssaoValueR, edgesLRTB.y, sum, sumWeight);
    AddSample(ssaoValueT, edgesLRTB.z, sum, sumWeight);
    AddSample(ssaoValueB, edgesLRTB.w, sum, sumWeight);

    float ssaoAvg = sum / sumWeight;

    ssaoValue = ssaoAvg; //min( ssaoValue, ssaoAvg ) * 0.2 + ssaoAvg * 0.8;
    return vec2(ssaoValue, packedEdges);
}

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    outVarPixelTarget(output, 0) = vec4(SampleBlurredWide(inVar(input, out_texcoord)), 0.0, 0.0); 
    outReturn(output)
}

#endif
