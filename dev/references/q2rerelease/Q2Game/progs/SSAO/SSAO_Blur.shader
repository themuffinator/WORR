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
vec2 SampleBlurred(vec2 inPos, vec2 coord)
{
    float packedEdges   = loadArray(tBlurInput, ivec2(inPos), float(g_ASSAOConsts.PassIndex), 0).y;
    vec4 edgesLRTB      = UnpackEdges(packedEdges);
    
    vec4 valuesUL       = gatherRed(tBlurInput, vec3(coord - g_ASSAOConsts.HalfViewportPixelSize * 0.5, float(g_ASSAOConsts.PassIndex)));
    vec4 valuesBR       = gatherRed(tBlurInput, vec3(coord + g_ASSAOConsts.HalfViewportPixelSize * 0.5, float(g_ASSAOConsts.PassIndex)));

    float ssaoValue     = valuesUL.y;
    float ssaoValueL    = valuesUL.x;
    float ssaoValueT    = valuesUL.z;
    float ssaoValueR    = valuesBR.z;
    float ssaoValueB    = valuesBR.x;

    float sumWeight = 0.5f;
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
    vec2 inPos = inFragCoord.xy;
    outVarPixelTarget(output, 0) = vec4(SampleBlurred(inPos, inVar(input, out_texcoord)), 0.0, 0.0); 
    outReturn(output)
}

#endif
