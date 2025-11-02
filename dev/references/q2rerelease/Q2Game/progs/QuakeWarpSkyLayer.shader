//
// Copyright(C) 2022-2023 Nightdive Studios, LLC
//
// ORIGINAL AUTHOR
//      Samuel Villarreal
//

#include "progs/common.inc"

#ifdef SHADER_VERTEX

#if defined(__ORBIS__)
    #pragma argument(indirectdraw)
#endif

//----------------------------------------------------
// input
begin_input(inVertex)
    var_attrib(0, vec4);
    var_attrib(1, vec3);
    var_attrib(2, vec2);
    var_attrib(3, vec4);
    var_attrib(4, vec4);
    var_attrib(5, uint);
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec3, out_position, POSITION0)
    def_var_out(vec4, out_curPosition,  POSITION1)
    def_var_out(vec4, out_prevPosition, POSITION2)
end_output

#define QUAKE_NO_TRANSFORM_UNIFORM_BUFFER   1
#include "progs/includes/QuakeTransformStruct.inc"
def_structuredBuffer(quakeModelTransform_s, sbTransform, 0);

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    vec4 vertex                         = vec4(inVarAttrib(0, input).xyz, 1.0);
    vec4 vPosition                      = mul(uModelViewMatrix, vertex);
    outVarPosition(output, position)    = mul(uProjectionMatrix, vPosition);
    outVar(output, out_position)        = vPosition.xyz; 
    outVar(output, out_curPosition)     = outVarPosition(output, position);
    outVar(output, out_prevPosition)    = mul(uPrevProjection, mul(uPrevModelView, vertex)); 
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL
#define QUAKE_UNIFORM_BUFFER_USE_SKY_PARAMS 1
#include "progs/includes/QuakeUtils.inc"

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_in(vec3, out_position, POSITION0)
    def_var_in(vec4, out_curPosition,   POSITION1)
    def_var_in(vec4, out_prevPosition,  POSITION2)
end_input

#include "progs/includes/QuakeCommonDefines.inc"

//----------------------------------------------------
// output
begin_output(outPixel)
    RT_DIFFUSE_TARGET
#ifndef GLSLC_NX
    RT_MOTIONBLUR_TARGET
#endif
    RT_NORMALS_TARGET
    RT_LIGHTBUFFER_TARGET
    RT_GLOWBUFFER_TARGET
end_output

def_sampler(2D, tQuakeImage, 0);

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    const float fFragZ = -(inVar(input, out_position).z);
    vec2 vCoords = ComputeSkyLayerCoords(fFragZ, ScreenCoords(), uSpeedScale);
    vec4 color = sample(tQuakeImage, vCoords) * 0.65;
    
    color *= uSkyAlpha;
    
    //color.rgb = mix(color.rgb, uFogColor.rgb, uSkyFogFactor);
    RT_DIFFUSE = color;
    RT_NORMALS = vec4(0.0, 0.0, 0.0, 0.0);
    RT_GLOWBUFFER = vec4(0.0, 0.0, 0.0, 1.0);
#ifndef GLSLC_NX
    RT_MOTIONBLUR.a = 1.0;
    RT_MOTIONBLUR.rg = 
        ComputeMotionVector(inVar(input, out_curPosition), inVar(input, out_prevPosition));
#endif
        
    outReturn(output)
}

#endif
