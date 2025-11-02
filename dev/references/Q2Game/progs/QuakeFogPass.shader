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
#include "progs/includes/QuakeFogCommon.inc"

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

def_sampler(2D, tSceneDepth, 0);
def_sampler(2D, tSceneColor, 1);

//----------------------------------------------------
float ScreenSpaceToViewSpaceDepth(float screenDepth)
{
    const float fM22 = -(uZFar) / (uZFar - uZNear);
    const float fM32 = -(uZFar * uZNear) / (uZFar - uZNear);
    
    float fDepthLinearizeMul = -fM32;
    float fDepthLinearizeAdd = fM22;
    if(fDepthLinearizeMul * fDepthLinearizeAdd < 0)
    {
        fDepthLinearizeAdd = -fDepthLinearizeAdd;
    }
    
    return fDepthLinearizeMul / (fDepthLinearizeAdd - screenDepth);
}

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec4 vSceneColor = sampleLevelZero(tSceneColor, inVar(input, out_texcoord));
    const float fDistance = ScreenSpaceToViewSpaceDepth(sampleLevelZero(tSceneDepth, inVar(input, out_texcoord)).r);
    
    vec3 vColor = ComputeFogColor(vSceneColor, fDistance, inVar(input, out_texcoord));
    
    outVarFragment(output, fragment) = vec4(vColor, vSceneColor.a);
    outReturn(output)
}

#endif
