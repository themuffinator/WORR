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
    def_var_out(vec4, out_smaaCoord2,   TEXCOORD2)
    def_var_out(vec4, out_smaaCoord3,   TEXCOORD3)
end_output

begin_cbuffer(PostProcess_MSAA, 0)
    cbuffer_member(vec4, uRTSize);
end_cbuffer()

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    outVarPosition(output, position)    = inVarAttrib(0, input);
    outVar(output, out_texcoord)        = inVarAttrib(1, input);
    
    outVar(output, out_smaaCoord1) = mad(uRTSize.xyxy, vec4(-1.0, 0.0, 0.0, -1.0), inVarAttrib(1, input).xyxy);
    outVar(output, out_smaaCoord2) = mad(uRTSize.xyxy, vec4( 1.0, 0.0, 0.0,  1.0), inVarAttrib(1, input).xyxy);
    outVar(output, out_smaaCoord3) = mad(uRTSize.xyxy, vec4(-2.0, 0.0, 0.0, -2.0), inVarAttrib(1, input).xyxy);
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2, out_texcoord,      TEXCOORD0)
    def_var_in(vec4, out_smaaCoord1,    TEXCOORD1)
    def_var_in(vec4, out_smaaCoord2,    TEXCOORD2)
    def_var_in(vec4, out_smaaCoord3,    TEXCOORD3)
end_input

//----------------------------------------------------
// output
begin_output(outPixel) 
    def_var_fragment(fragment)
end_output

def_sampler(2D, tEdgeRenderTarget, 0);

#if defined (__ORBIS__) || defined(__PROSPERO__)
    #define SMAA_THRESHOLD 0.01
#else
    #define SMAA_THRESHOLD 0.05
#endif

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec4 vOffset[3];
    vOffset[0] = inVar(input, out_smaaCoord1);
    vOffset[1] = inVar(input, out_smaaCoord2);
    vOffset[2] = inVar(input, out_smaaCoord3);
    
    vec4 vColor = vec4(0.0, 0.0, 0.0, 0.0);
    vec2 threshold = vec2(SMAA_THRESHOLD, SMAA_THRESHOLD);
    vec3 weights = vec3(0.2126, 0.7152, 0.0722);
    float L = dot(sample(tEdgeRenderTarget, inVar(input, out_texcoord)).rgb, weights);
    
    float Lleft = dot(sample(tEdgeRenderTarget, vOffset[0].xy).rgb, weights);
    float Ltop  = dot(sample(tEdgeRenderTarget, vOffset[0].zw).rgb, weights);
    
    // We do the usual threshold:
    vec4 delta;
    delta.xy = abs(L - vec2(Lleft, Ltop));
    vec2 edges = step(threshold, delta.xy);
    
    // Then discard if there is no edge:
    if(dot(edges, vec2(1.0, 1.0)) == 0.0)
    {
        discard;
    }
    
    // Calculate right and bottom deltas:
    float Lright = dot(sample(tEdgeRenderTarget, vOffset[1].xy).rgb, weights);
    float Lbottom  = dot(sample(tEdgeRenderTarget, vOffset[1].zw).rgb, weights);
    delta.zw = abs(L - vec2(Lright, Lbottom));

    // Calculate the maximum delta in the direct neighborhood:
    vec2 maxDelta = max(delta.xy, delta.zw);

    // Calculate left-left and top-top deltas:
    float Lleftleft = dot(sample(tEdgeRenderTarget, vOffset[2].xy).rgb, weights);
    float Ltoptop = dot(sample(tEdgeRenderTarget, vOffset[2].zw).rgb, weights);
    delta.zw = abs(vec2(Lleft, Ltop) - vec2(Lleftleft, Ltoptop));

    // Calculate the final maximum delta:
    maxDelta = max(maxDelta.xy, delta.zw);
    float finalDelta = max(maxDelta.x, maxDelta.y);

    // Local contrast adaptation:
    vColor.rg = edges.xy * step(finalDelta, 2.0 * delta.xy);

    outVarFragment(output, fragment) = vColor;
    outReturn(output)
}

#endif
