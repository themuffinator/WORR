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
#if QUAKE_ALPHA_MASK == 1
    def_var_out(vec3, out_texcoord, TEXCOORD0)
#endif
end_output

#define QUAKE_NO_TRANSFORM_UNIFORM_BUFFER   1
#include "progs/includes/QuakeTransformStruct.inc"
def_structuredBuffer(quakeModelTransform_s, sbTransform, 0);

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    mat4 objTransform;
    
    const uint dwMask = inVarAttrib(5, input);
    uint index = (dwMask >> 12) & 0x7FF;
    
    quakeModelTransform_s transform     = sbTransform[index];
    objTransform                        = MakeMatrix(transform.fTransform1, transform.fTransform2, transform.fTransform3, transform.fTransform4);
    
    vec4 vertex                         = vec4(inVarAttrib(0, input).xyz, 1.0);
#if QUAKE_ALPHA_MASK == 1
    outVar(output, out_texcoord)        = inVarAttrib(1, input);
#endif
    mat4 mtxTransform                   = mul(uModelViewMatrix, objTransform);
    vec4 vPosition                      = mul(mtxTransform, vertex);
    outVarPosition(output, position)    = mul(uProjectionMatrix, vPosition);
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
#if QUAKE_ALPHA_MASK == 1
    def_var_in(vec3, out_texcoord, TEXCOORD0)
#endif
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
end_output

#if QUAKE_ALPHA_MASK == 1
    def_samplerArray(2D, tQuakeImage, 0);
#endif

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)

#if QUAKE_ALPHA_MASK == 1
    vec4 color = sampleLevelZero(tQuakeImage, inVar(input, out_texcoord));
    if(color.a <= 0.8)
    {
        discard;
        outReturn(output)
    }
#endif
    
    outReturn(output)
}

#endif
