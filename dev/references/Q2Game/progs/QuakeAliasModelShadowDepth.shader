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
    var_attrib(0,  vec4);
    // --
    var_attrib(1,  vec4);
    
    def_var_instanceID()
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
end_output

#include "progs/includes/QuakeTransformStruct.inc"

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    mat4 objTransform;
    
    quakeModelTransform_s transform     = uTransform[inVar(input, inInstanceID)];
    objTransform                        = MakeMatrix(transform.fTransform1, transform.fTransform2, transform.fTransform3, transform.fTransform4);
    
    vec4 blendParams                    = transform.fBlendParams1;
    
    vec3 modelVertex1                   = inVarAttrib(0, input).xyz;
    vec3 modelVertex2                   = inVarAttrib(1, input).xyz;
    vec4 vertex                         = vec4((modelVertex2 - modelVertex1) * blendParams.x + modelVertex1, 1.0);
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
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
end_output

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    outReturn(output)
}

#endif
