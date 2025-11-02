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
def_rwTexture(rgba32f, 2D, tNormalsOutputUAV, 4);

//----------------------------------------------------
vec3 CalculateNormal(const vec4 edgesLRTB, vec3 pixCenterPos, vec3 pixLPos, vec3 pixRPos, vec3 pixTPos, vec3 pixBPos)
{
    // Get this pixel's viewspace normal
    vec4 acceptedNormals = vec4(edgesLRTB.x*edgesLRTB.z, edgesLRTB.z*edgesLRTB.y, edgesLRTB.y*edgesLRTB.w, edgesLRTB.w*edgesLRTB.x);

    pixLPos = normalize(pixLPos - pixCenterPos);
    pixRPos = normalize(pixRPos - pixCenterPos);
    pixTPos = normalize(pixTPos - pixCenterPos);
    pixBPos = normalize(pixBPos - pixCenterPos);

    vec3 pixelNormal = vec3(0, 0, -0.0005);
    pixelNormal += (acceptedNormals.x) * cross(pixLPos, pixTPos);
    pixelNormal += (acceptedNormals.y) * cross(pixTPos, pixRPos);
    pixelNormal += (acceptedNormals.z) * cross(pixRPos, pixBPos);
    pixelNormal += (acceptedNormals.w) * cross(pixBPos, pixLPos);
    pixelNormal = normalize(pixelNormal);

    return pixelNormal;
}

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
    
    float pixZs[4][4];
    vec2 upperLeftUV = (inFragCoord.xy - 0.25) * g_ASSAOConsts.Viewport2xPixelSize;

    // middle 4
    pixZs[1][1] = out0.x;
    pixZs[2][1] = out1.x;
    pixZs[1][2] = out2.x;
    pixZs[2][2] = out3.x;
    // left 2
    pixZs[0][1] = ScreenSpaceToViewSpaceDepth(sampleLODOffset(tDepthSource, upperLeftUV, 0, ivec2(-1,  0)).x); 
    pixZs[0][2] = ScreenSpaceToViewSpaceDepth(sampleLODOffset(tDepthSource, upperLeftUV, 0, ivec2(-1,  1)).x); 
    // right 2
    pixZs[3][1] = ScreenSpaceToViewSpaceDepth(sampleLODOffset(tDepthSource, upperLeftUV, 0, ivec2( 2,  0)).x); 
    pixZs[3][2] = ScreenSpaceToViewSpaceDepth(sampleLODOffset(tDepthSource, upperLeftUV, 0, ivec2( 2,  1)).x); 
    // top 2
    pixZs[1][0] = ScreenSpaceToViewSpaceDepth(sampleLODOffset(tDepthSource, upperLeftUV, 0, ivec2( 0, -1)).x);
    pixZs[2][0] = ScreenSpaceToViewSpaceDepth(sampleLODOffset(tDepthSource, upperLeftUV, 0, ivec2( 1, -1)).x);
    // bottom 2
    pixZs[1][3] = ScreenSpaceToViewSpaceDepth(sampleLODOffset(tDepthSource, upperLeftUV, 0, ivec2( 0,  2)).x);
    pixZs[2][3] = ScreenSpaceToViewSpaceDepth(sampleLODOffset(tDepthSource, upperLeftUV, 0, ivec2( 1,  2)).x);

    vec4 edges0 = CalculateEdges(pixZs[1][1], pixZs[0][1], pixZs[2][1], pixZs[1][0], pixZs[1][2]);
    vec4 edges1 = CalculateEdges(pixZs[2][1], pixZs[1][1], pixZs[3][1], pixZs[2][0], pixZs[2][2]);
    vec4 edges2 = CalculateEdges(pixZs[1][2], pixZs[0][2], pixZs[2][2], pixZs[1][1], pixZs[1][3]);
    vec4 edges3 = CalculateEdges(pixZs[2][2], pixZs[1][2], pixZs[3][2], pixZs[2][1], pixZs[2][3]);

    vec3 pixPos[4][4];
    // middle 4
    pixPos[1][1] = NDCToViewspace(upperLeftUV + g_ASSAOConsts.ViewportPixelSize * vec2( 0.0,  0.0), pixZs[1][1]);
    pixPos[2][1] = NDCToViewspace(upperLeftUV + g_ASSAOConsts.ViewportPixelSize * vec2( 1.0,  0.0), pixZs[2][1]);
    pixPos[1][2] = NDCToViewspace(upperLeftUV + g_ASSAOConsts.ViewportPixelSize * vec2( 0.0,  1.0), pixZs[1][2]);
    pixPos[2][2] = NDCToViewspace(upperLeftUV + g_ASSAOConsts.ViewportPixelSize * vec2( 1.0,  1.0), pixZs[2][2]);
    // left 2
    pixPos[0][1] = NDCToViewspace(upperLeftUV + g_ASSAOConsts.ViewportPixelSize * vec2(-1.0,  0.0), pixZs[0][1]);
    pixPos[0][2] = NDCToViewspace(upperLeftUV + g_ASSAOConsts.ViewportPixelSize * vec2(-1.0,  1.0), pixZs[0][2]);
    // right 2                                                                                     
    pixPos[3][1] = NDCToViewspace(upperLeftUV + g_ASSAOConsts.ViewportPixelSize * vec2( 2.0,  0.0), pixZs[3][1]);
    pixPos[3][2] = NDCToViewspace(upperLeftUV + g_ASSAOConsts.ViewportPixelSize * vec2( 2.0,  1.0), pixZs[3][2]);
    // top 2                                                                                       
    pixPos[1][0] = NDCToViewspace(upperLeftUV + g_ASSAOConsts.ViewportPixelSize * vec2( 0.0, -1.0), pixZs[1][0]);
    pixPos[2][0] = NDCToViewspace(upperLeftUV + g_ASSAOConsts.ViewportPixelSize * vec2( 1.0, -1.0), pixZs[2][0]);
    // bottom 2                                                                                    
    pixPos[1][3] = NDCToViewspace(upperLeftUV + g_ASSAOConsts.ViewportPixelSize * vec2( 0.0,  2.0), pixZs[1][3]);
    pixPos[2][3] = NDCToViewspace(upperLeftUV + g_ASSAOConsts.ViewportPixelSize * vec2( 1.0,  2.0), pixZs[2][3]);

    vec3 norm0 = CalculateNormal(edges0, pixPos[1][1], pixPos[0][1], pixPos[2][1], pixPos[1][0], pixPos[1][2]);
    vec3 norm1 = CalculateNormal(edges1, pixPos[2][1], pixPos[1][1], pixPos[3][1], pixPos[2][0], pixPos[2][2]);
    vec3 norm2 = CalculateNormal(edges2, pixPos[1][2], pixPos[0][2], pixPos[2][2], pixPos[1][1], pixPos[1][3]);
    vec3 norm3 = CalculateNormal(edges3, pixPos[2][2], pixPos[1][2], pixPos[3][2], pixPos[2][1], pixPos[2][3]);

    imageStore(tNormalsOutputUAV, baseCoord + ivec2(0, 0), vec4(norm0, 1.0));
    imageStore(tNormalsOutputUAV, baseCoord + ivec2(1, 0), vec4(norm1, 1.0));
    imageStore(tNormalsOutputUAV, baseCoord + ivec2(0, 1), vec4(norm2, 1.0));
    imageStore(tNormalsOutputUAV, baseCoord + ivec2(1, 1), vec4(norm3, 1.0));
    
    outReturn(output)
}

#endif
