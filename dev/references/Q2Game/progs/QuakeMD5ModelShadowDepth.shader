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
    var_attrib(0,  int);    // start weight
    var_attrib(1,  int);    // weight count

    def_var_instanceID()
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
end_output

struct quakeMD5Weight_s
{
    vec3 vPos;
    float fBias;
    int iJointID;
    float padding0[3];
};

struct quakeMD5AnimFrame_s
{
    vec4 qRot;
    vec3 vPos;
    float padding0;
    int iParent;
    float padding1[3];
};

struct quakeMD5BoneTransform_s
{
    vec4 qRot;
    vec3 vPos;
    float fScale;
};

def_structuredBuffer(quakeMD5Weight_s, sbWeights, 0);
def_structuredBuffer(quakeMD5BoneTransform_s, sbBoneTransforms, 1);

#include "progs/includes/QuakeTransformStruct.inc"
#include "progs/includes/QuakeQuaternionMath.inc"

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    mat4 objTransform;
    
    quakeModelTransform_s transform     = uTransform[inVar(input, inInstanceID)];
    objTransform                        = MakeMatrix(transform.fTransform1, transform.fTransform2, transform.fTransform3, transform.fTransform4);
    
    vec4 blendParams1                   = transform.fBlendParams1;
    vec4 blendParams2                   = transform.fBlendParams2;
    
    uint boneOffset                     = uint(blendParams2.x);
    
    int iStartWeight                    = inVarAttrib(0, input);
    int iWeightCount                    = inVarAttrib(1, input);
    
    vec3 vVertexPos                     = vec3(0.0, 0.0, 0.0);
    
    for(int i = 0; i < iWeightCount; ++i)
    {
        quakeMD5Weight_s weight         = sbWeights[iStartWeight + i];
        quakeMD5BoneTransform_s frameC  = sbBoneTransforms[boneOffset + ((weight.iJointID * 2) + 0)];
        
        vec3 vPos1                      = frameC.vPos;
        vec4 qRot1                      = frameC.qRot;
        vec3 vRotPos1                   = QRotateVec3(weight.vPos, qRot1) * frameC.fScale;
        vVertexPos                      += (vPos1 + vRotPos1) * weight.fBias;
    }
    
    vec4 vertex                         = vec4(vVertexPos, 1.0);
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
