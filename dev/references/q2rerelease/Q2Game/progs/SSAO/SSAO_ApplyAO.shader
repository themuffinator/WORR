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

def_samplerArray(2D, tFinalSSAO, 0);

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    float ao;
    vec2 inPos          = inFragCoord.xy;
    uvec2 pixPos        = uvec2(inPos.xy);
    uvec2 pixPosHalf    = pixPos / uvec2(2, 2);

    // calculate index in the four deinterleaved source array texture
    int mx = int(pixPos.x % 2);
    int my = int(pixPos.y % 2);
    int ic = mx + my * 2;       // center index
    int ih = (1-mx) + my * 2;   // neighbouring, horizontal
    int iv = mx + (1-my) * 2;   // neighbouring, vertical
    int id = (1-mx) + (1-my)*2; // diagonal
    
    vec2 centerVal = loadArray(tFinalSSAO, pixPosHalf, ic, 0).xy;
    
    ao = centerVal.x;

#if 1   // change to 0 if you want to disable last pass high-res blur (for debugging purposes, etc.)
    vec4 edgesLRTB = UnpackEdges(centerVal.y);

    // return 1.0 - vec4( edgesLRTB.x, edgesLRTB.y * 0.5 + edgesLRTB.w * 0.5, edgesLRTB.z, 0.0 ); // debug show edges

    // convert index shifts to sampling offsets
    float fmx   = float(mx);
    float fmy   = float(my);
    
    // in case of an edge, push sampling offsets away from the edge (towards pixel center)
    float fmxe  = (edgesLRTB.y - edgesLRTB.x);
    float fmye  = (edgesLRTB.w - edgesLRTB.z);

    // calculate final sampling offsets and sample using bilinear filter
    vec2  uvH = (inPos.xy + vec2(fmx + fmxe - 0.5, 0.5 - fmy)) * 0.5 * g_ASSAOConsts.HalfViewportPixelSize;
    float aoH = sampleLevelZero(tFinalSSAO, vec3(uvH, ih)).x;
    vec2  uvV = (inPos.xy + vec2(0.5 - fmx, fmy - 0.5 + fmye)) * 0.5 * g_ASSAOConsts.HalfViewportPixelSize;
    float aoV = sampleLevelZero(tFinalSSAO, vec3(uvV, iv)).x;
    vec2  uvD = (inPos.xy + vec2(fmx - 0.5 + fmxe, fmy - 0.5 + fmye)) * 0.5 * g_ASSAOConsts.HalfViewportPixelSize;
    float aoD = sampleLevelZero(tFinalSSAO, vec3(uvD, id)).x;

    // reduce weight for samples near edge - if the edge is on both sides, weight goes to 0
    vec4 blendWeights;
    blendWeights.x = 1.0;
    blendWeights.y = (edgesLRTB.x + edgesLRTB.y) * 0.5;
    blendWeights.z = (edgesLRTB.z + edgesLRTB.w) * 0.5;
    blendWeights.w = (blendWeights.y + blendWeights.z) * 0.5;

    // calculate weighted average
    float blendWeightsSum = dot(blendWeights, vec4(1.0, 1.0, 1.0, 1.0));
    ao = dot(vec4(ao, aoH, aoV, aoD), blendWeights) / blendWeightsSum;
#endif

    outVarPixelTarget(output, 0) = vec4(ao.xxx, 1.0);
    outReturn(output)
}

#endif
