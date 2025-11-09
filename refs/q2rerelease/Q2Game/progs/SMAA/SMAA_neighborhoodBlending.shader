//
// Copyright(C) 2016-2017 Samuel Villarreal
// Copyright(C) 2016 Night Dive Studios, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include "progs/common.inc"

begin_cbuffer(PostProcess_MSAA, 0)
    cbuffer_member(vec4, uRTSize);
end_cbuffer()

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
    def_var_out(vec2, out_texcoord,     TEXCOORD0)
    def_var_out(vec4, out_smaaCoord1,   TEXCOORD1)
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    outVarPosition(output, position)    = inVarAttrib(0, input);
    outVar(output, out_texcoord)        = inVarAttrib(1, input);

    outVar(output, out_smaaCoord1) =
        mad(uRTSize.xyxy, vec4( 1.0, 0.0, 0.0,  1.0), inVarAttrib(1, input).xyxy);
        
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

/**
 * Conditional move:
 */
void SMAAMovc(bvec2 cond, inout vec2 variable, vec2 value)
{
    flatten if (cond.x) variable.x = value.x;
    flatten if (cond.y) variable.y = value.y;
}

void SMAAMovc(bvec4 cond, inout vec4 variable, vec4 value)
{
    SMAAMovc(cond.xy, variable.xy, value.xy);
    SMAAMovc(cond.zw, variable.zw, value.zw);
}

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2, out_texcoord,      TEXCOORD0)
    def_var_in(vec4, out_smaaCoord1,    TEXCOORD1)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

def_sampler(2D, tSMAADest, 0);
def_sampler(2D, tBlendWeights, 1);

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec4 vOffset;
    vOffset = inVar(input, out_smaaCoord1);
    
    // Fetch the blending weights for current pixel:
    vec4 a;
    a.x = sampleLevelZero(tBlendWeights, vOffset.xy).a; // Right
    a.y = sampleLevelZero(tBlendWeights, vOffset.zw).g; // Top
    a.wz = sampleLevelZero(tBlendWeights, inVar(input, out_texcoord)).xz; // Bottom / Left
    
    vec4 color;
    
    // Is there any blending weight with a value greater than 0.0?
    branch
    if(dot(a, vec4(1.0, 1.0, 1.0, 1.0)) < 1e-5)
    {
        color = sampleLevelZero(tSMAADest, inVar(input, out_texcoord));
    }
    else
    {
        bool h = max(a.x, a.z) > max(a.y, a.w); // max(horizontal) > max(vertical)

        // Calculate the blending offsets:
        vec4 blendingOffset = vec4(0.0, a.y, 0.0, a.w);
        vec2 blendingWeight = a.yw;
        SMAAMovc(bvec4(h, h, h, h), blendingOffset, vec4(a.x, 0.0, a.z, 0.0));
        SMAAMovc(bvec2(h, h), blendingWeight, a.xz);
        blendingWeight /= dot(blendingWeight, vec2(1.0, 1.0));

        // Calculate the texture coordinates:
        vec4 blendingCoord = mad(blendingOffset, vec4(uRTSize.xy, -uRTSize.xy), inVar(input, out_texcoord).xyxy);

        // We exploit bilinear filtering to mix current pixel with the chosen
        // neighbor:
        color = blendingWeight.x * sampleLevelZero(tSMAADest, blendingCoord.xy);
        color += blendingWeight.y * sampleLevelZero(tSMAADest, blendingCoord.zw);
    }

    outVarFragment(output, fragment) = color;
    outReturn(output)
}

#endif
