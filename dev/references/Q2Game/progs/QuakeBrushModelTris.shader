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
    
    mat4 objTransform, objPrevTransform;
    
    const uint dwMask = inVarAttrib(5, input);
    uint index = (dwMask >> 12) & 0x7FF;
    
    quakeModelTransform_s transform = sbTransform[index];
    
    for(int i = 0; i < 4; ++i)
    {
        M(objTransform, 0, i) = transform.fTransform1[i];
        M(objTransform, 1, i) = transform.fTransform2[i];
        M(objTransform, 2, i) = transform.fTransform3[i];
        M(objTransform, 3, i) = transform.fTransform4[i];
        
        M(objPrevTransform, 0, i) = transform.fTransform5[i];
        M(objPrevTransform, 1, i) = transform.fTransform6[i];
        M(objPrevTransform, 2, i) = transform.fTransform7[i];
        M(objPrevTransform, 3, i) = transform.fTransform8[i];
    }
    
    vec4 vertex                         = vec4(inVarAttrib(0, input).xyz, 1.0);
    mat4 mtxTransform                   = mul(uModelViewMatrix, objTransform);
    mat4 mtxPrevTransform               = mul(uPrevModelView, objPrevTransform);
    vec4 vPosition                      = mul(mtxTransform, vertex);
    vec4 vPrevPosition                  = mul(mtxPrevTransform, vertex);
    outVarPosition(output, position)    = mul(uProjectionMatrix, vPosition);
    outVar(output, out_position)        = vPosition.xyz;
    outVar(output, out_curPosition)     = outVarPosition(output, position);
    outVar(output, out_prevPosition)    = mul(uPrevProjection, vPrevPosition);
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

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

def_samplerArray(2D, tQuakeImage, 0);

#include "progs/includes/QuakeUtils.inc"

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    RT_DIFFUSE = vec4(1.0, 1.0, 1.0, 1.0);
    RT_GLOWBUFFER = vec4(0.0, 0.0, 0.0, 1.0);
    
#ifndef GLSLC_NX
    RT_MOTIONBLUR.a = 1.0;
    RT_MOTIONBLUR.rg = 
        ComputeMotionVector(inVar(input, out_curPosition), inVar(input, out_prevPosition));
#endif
    outReturn(output)
}

#endif
