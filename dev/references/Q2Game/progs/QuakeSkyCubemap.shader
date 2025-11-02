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
    var_attrib(2, vec4);
    var_attrib(3, vec4);
    var_attrib(4, vec4);
    var_attrib(5, uint);
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec3, out_position, POSITION0)
    def_var_out(vec4, out_skyRotator, SKYROTATE)
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
    
    const uint dwMask = inVarAttrib(5, input);
    uint index = (dwMask >> 12) & 0x7FF;
    
    quakeModelTransform_s transform = sbTransform[index];
    
    vec4 vertex                         = vec4(inVarAttrib(0, input).xyz, 1.0);
    vec4 vPosition                      = mul(uModelViewMatrix, vertex);
    outVarPosition(output, position)    = mul(uProjectionMatrix, vPosition);
    outVar(output, out_position)        = vPosition.xyz;
    outVar(output, out_skyRotator).x    = transform.fShadeParams2[1];
    outVar(output, out_skyRotator).y    = transform.fShadeParams2[2];
    outVar(output, out_skyRotator).z    = transform.fShadeParams2[3];
    outVar(output, out_skyRotator).w    = transform.fShadeParams2[0];
    outVar(output, out_curPosition)     = outVarPosition(output, position);
    outVar(output, out_prevPosition)    = mul(uPrevProjection, mul(uPrevModelView, vertex));
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_in(vec3, out_position, POSITION0)
    def_var_in(vec4, out_skyRotator, SKYROTATE)
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

def_sampler(Cube, tQuakeImage, 0);

#include "progs/includes/QuakeUtils.inc"
#include "progs/includes/QuakeFogCommon.inc"

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    const float fFragZ = -(inVar(input, out_position).z);
    vec3 vPos = GetWorldPositionFromDepth(fFragZ, ScreenCoords());
    
    vec3 vDir = normalize(vPos - uViewOrigin);
    
    const vec3 vAxis = inVar(input, out_skyRotator).xyz;
    const float cos_theta = cos(inVar(input, out_skyRotator).w);
    const float sin_theta = sin(inVar(input, out_skyRotator).w);
    vec3 vRotated = (vDir * cos_theta) + (cross(vAxis, vDir) * sin_theta) + (vAxis * dot(vAxis, vDir)) * (1.0 - cos_theta);
    
    vec4 color = sample(tQuakeImage, vRotated) * 0.5;
	
    color.rgb = mix(color.rgb, uFogColor.rgb, uSkyFogFactor);
    
    RT_DIFFUSE = vec4(color.rgb, 1.0);
    RT_NORMALS = vec4(0.0, 0.0, 0.0, 0.0);
    RT_LIGHTBUFFER = vec4(1.0, 1.0, 1.0, 1.0);
    RT_GLOWBUFFER = vec4(0.0, 0.0, 0.0, 1.0);
#ifndef GLSLC_NX
    RT_MOTIONBLUR.a = 1.0;
    RT_MOTIONBLUR.rg = 
        ComputeMotionVector(inVar(input, out_curPosition), inVar(input, out_prevPosition));
#endif
    outReturn(output)
}

#endif
