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

#define QUAKE_HAS_CLUSTERED_SHADING     1
#include "progs/includes/QuakeUtils.inc"

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

def_sampler(2D, tDepth, 0);
def_sampler(2D, tNormal, 1);

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec2 vTCoord = (inVar(input, out_texcoord) / uResolutionScale.zw) + uScreenBounds.xy;
    
//#ifdef KEX_PLATFORM_NX
#if 0
    const vec2 vScreenSize = ScreenSize() * 2.0;
#else
    const vec2 vScreenSize = ScreenSize();
#endif
    
    const float fDepth = load(tDepth, ivec2(vTCoord * vScreenSize), 0).r;
    const vec3 vNormal = load(tNormal, ivec2(vTCoord * vScreenSize), 0).xyz;
#if 0
    float fFragZ = (LinearizeDepth(uZNear, uZFar, fDepth) * 0.5) * uZFar;
#else
    float fFragZ = ScreenSpaceToViewSpaceDepth(fDepth);
#endif
    
    vec4 vShadeColor = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 vOutColor = vec4(1.0, 1.0, 1.0, 1.0);
    
    if(ComputeShadedColor(vOutColor, vTCoord, vShadeColor, true, fFragZ, vNormal, inFragCoord) == false)
    {
        discard;
        outReturn(output)
    }
    
    outVarFragment(output, fragment) = vec4(vOutColor.rgb, fFragZ / uZFar);
    outReturn(output)
}

#endif
