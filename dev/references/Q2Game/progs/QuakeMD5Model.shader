//
// Copyright(C) 2022-2023 Nightdive Studios, LLC
//
// ORIGINAL AUTHOR
//      Samuel Villarreal
//

#include "progs/common.inc"

#ifdef SHADER_VERTEX

#define FIXED_WEAPON_FOV    74.0

//----------------------------------------------------
// input
begin_input(inVertex)
    var_attrib(0,  vec3);   // normal
    var_attrib(1,  vec2);   // coord
    var_attrib(2,  int);    // start weight
    var_attrib(3,  int);    // weight count

    def_var_instanceID()
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec2, out_texcoord, TEXCOORD0)
    def_var_out(vec4, out_color,    COLOR0)
    def_var_out(vec4, out_immediateColor,    COLOR1)
    def_var_out(vec3, out_position, POSITION0)
    def_var_out(vec4, out_curPosition,  POSITION1)
#ifndef GLSLC_NX
    def_var_out(vec4, out_prevPosition, POSITION2)
#endif
    def_var_out(vec3, out_normals, NORMALS)
end_output

//----------------------------------------------------
float ComputeShade(vec3 vertexnormal, vec4 shadeParams)
{
    float fDot = dot(vertexnormal, shadeParams.xyz);
    return fDot < 0.0 ? (1.0 + fDot * (13.0 / 44.0)) : 1.0 + fDot;
}

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
    
    mat4 objTransform, objPrevTransform;
    
    quakeModelTransform_s transform     = uTransform[inVar(input, inInstanceID)];
    
    objTransform                        = MakeMatrix(transform.fTransform1, transform.fTransform2, transform.fTransform3, transform.fTransform4);

#ifndef GLSLC_NX
    objPrevTransform                    = MakeMatrix(transform.fTransform5, transform.fTransform6, transform.fTransform7, transform.fTransform8);
#endif
    
    vec4 shadeParams1                   = transform.fShadeParams1;
    vec4 shadeParams2                   = transform.fShadeParams2;
    vec4 blendParams1                   = transform.fBlendParams1;
    vec4 blendParams2                   = transform.fBlendParams2;
    
    uint flags                          = uint(shadeParams2.w); // Yuck... need to use w component of shadeparams2 for a bitflag
    outVar(output, out_immediateColor)  = vec4(0.0, 0.0, 0.0, 0.0);
    
    uint boneOffset                     = uint(blendParams2.x);
    
    vec3 vNormals                       = inVarAttrib(0, input);
    vec2 vCoords                        = inVarAttrib(1, input);
    int iStartWeight                    = inVarAttrib(2, input);
    int iWeightCount                    = inVarAttrib(3, input);
    
    vec3 vVertexPos                     = vec3(0.0, 0.0, 0.0);
    vec3 vVertexNormal                  = vec3(0.0, 0.0, 0.0);
    
#ifndef GLSLC_NX
    vec3 vPrevVertexPos                 = vec3(0.0, 0.0, 0.0);
    vec3 vPrevVertexNormal              = vec3(0.0, 0.0, 0.0);
#endif
    
    for(int i = 0; i < iWeightCount; ++i)
    {
        quakeMD5Weight_s weight         = sbWeights[iStartWeight + i];
        
        quakeMD5BoneTransform_s frameC  = sbBoneTransforms[boneOffset + ((weight.iJointID * 2) + 0)];
        vec3 vPos1                      = frameC.vPos;
        vec4 qRot1                      = frameC.qRot;
        
        mat4 mtxRot1                    = QToMatrix(qRot1);
        vec3 vRotPos1                   = mul(cast(mat3, mtxRot1), weight.vPos) * frameC.fScale;
        
        vVertexPos                      += (vPos1 + vRotPos1) * weight.fBias;
        vVertexNormal                   += mul(cast(mat3, mtxRot1), vNormals) * weight.fBias;
        
#ifndef GLSLC_NX
        quakeMD5BoneTransform_s frameP  = sbBoneTransforms[boneOffset + ((weight.iJointID * 2) + 1)];
        vec3 vPos2                      = frameP.vPos;
        vec4 qRot2                      = frameP.qRot;
        
        mat4 mtxRot2                    = QToMatrix(qRot2);
        vec3 vRotPos2                   = mul(cast(mat3, mtxRot2), weight.vPos) * frameP.fScale;
        
        vPrevVertexPos                  += (vPos2 + vRotPos2) * weight.fBias;
        vPrevVertexNormal               += mul(cast(mat3, mtxRot2), vNormals) * weight.fBias;
#endif
    }
    
    vec4 vertex                         = vec4(vVertexPos, 1.0);
#ifndef GLSLC_NX
    vec4 prevVertex                     = vec4(vPrevVertexPos, 1.0);
#endif
    
    // ---------------------------------------------------------------------------------
    // Powersuit effect - set color and scale vertices
    if((flags & 2) != 0)
    {
        outVar(output, out_immediateColor).rgb = shadeParams2.rgb;
        outVar(output, out_immediateColor).a = 1.0;

        float scale = ((flags & 4) != 0) ? 0.25 : 4.0;

        vertex.xyz += (vVertexNormal * scale);
#ifndef GLSLC_NX
        prevVertex.xyz += (vPrevVertexNormal * scale);
#endif
    }
    
    mat4 mtxProjection                  = uProjectionMatrix;
    
    // ---------------------------------------------------------------------------------
    // Set fixed FOV
    if((flags & 4) != 0)
    {
        const float fFOV                = tan((FIXED_WEAPON_FOV * 0.01745329251994329576923690768489) * 0.5);
        const float fA                  = 1.0 / (blendParams1.w * fFOV);
        const float fB                  = 1.0 / fFOV;
        
        M(mtxProjection, 0, 0)          -= (M(mtxProjection, 0, 0) - fA);
        M(mtxProjection, 1, 1)          -= (M(mtxProjection, 1, 1) - fB);
    }
    
    mat4 mtxTransform                   = mul(uModelViewMatrix, objTransform);
    vec4 vPosition                      = mul(mtxTransform, vertex);
    outVarPosition(output, position)    = mul(mtxProjection, vPosition);
    outVar(output, out_texcoord)        = inVarAttrib(1, input);
    outVar(output, out_position)        = vPosition.xyz;
    
    float fDot                          = ComputeShade(vVertexNormal, shadeParams1);
    vec3 vShadeColor                    = min(shadeParams2.xyz * fDot, vec3(1.0, 1.0, 1.0));
    
    // ---------------------------------------------------------------------------------
    // Apply monochrome scale to shade color
    float fIntensity                    = max(vShadeColor.r, max(vShadeColor.g, vShadeColor.b));
    vec3 vMonochrome                    = vec3(fIntensity, fIntensity, fIntensity);
    outVar(output, out_color)           = vec4(mix(vShadeColor, vMonochrome, blendParams1.z), shadeParams1.w);   
    
    outVar(output, out_curPosition)     = outVarPosition(output, position);
    outVar(output, out_normals)         = normalize(mul(cast(mat3, mtxTransform), vVertexNormal));
    
#ifndef GLSLC_NX
    if((flags & 1) != 0) // no motion blur on view model (except for vertices)
    {
        outVar(output, out_prevPosition)
            = mul(mtxProjection, mul(mul(uModelViewMatrix, objTransform), prevVertex));
    }
    else
    {
        outVar(output, out_prevPosition)
            = mul(uPrevProjection, mul(mul(uPrevModelView, objPrevTransform), prevVertex));
    }
#endif
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
    def_var_in(vec4, out_color,    COLOR0)
    def_var_in(vec4, out_immediateColor,    COLOR1)
    def_var_in(vec3, out_position, POSITION0)
    def_var_in(vec4, out_curPosition,   POSITION1)
#ifndef GLSLC_NX
    def_var_in(vec4, out_prevPosition,  POSITION2)
#endif
    def_var_in(vec3, out_normals, NORMALS)
end_input

#include "progs/includes/QuakeCommonDefines.inc"

//----------------------------------------------------
// output
begin_output(outPixel)
    RT_DIFFUSE_TARGET
#ifndef QUAKE_ALIAS_PREVIEW
#ifndef GLSLC_NX
    RT_MOTIONBLUR_TARGET
#endif
    RT_NORMALS_TARGET
    RT_LIGHTBUFFER_TARGET
    RT_GLOWBUFFER_TARGET
#endif
end_output

def_sampler(2D, tQuakeImage, 0);
#ifndef QUAKE_MD5_PREVIEW
def_sampler(2D, tQuakeGlowImage, 1);
#endif

#ifdef QUAKE_MD5_PREVIEW
    #define QUAKE_HAS_CLUSTERED_SHADING     1
#endif

#include "progs/includes/QuakeUtils.inc"

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec4 color = sample(tQuakeImage, inVar(input, out_texcoord));
    vec4 shadeColor = inVar(input, out_color);
    vec4 immediateColor = inVar(input, out_immediateColor);
    
    const vec3 vNormals = inVar(input, out_normals);
    const float fAlpha = color.a * shadeColor.a;
    
    // Sponge: NO TEXTURED POWERSUITS!!!
#if 0
    color.rgb += (inVar(input, out_immediateColor).rgb * 8.0);
#else
    color.r = lerp(color.r, immediateColor.r, immediateColor.a);
    color.g = lerp(color.g, immediateColor.g, immediateColor.a);
    color.b = lerp(color.b, immediateColor.b, immediateColor.a);
#endif

#if QUAKE_MD5_PREVIEW == 1
    // ---------------------------------------------------------------------------------
    // Apply dynamic lighting and shadows
    const float fFragZ = -(inVar(input, out_position).z);
    vec4 vOutColor = vec4(1.0, 1.0, 1.0, 1.0);
    
    if(ComputeShadedColor(vOutColor, inVar(input, out_texcoord), shadeColor, true, fFragZ, vNormals, inFragCoord) == false)
    {
        discard;
        outReturn(output)
    }
    
    color.rgb *= vOutColor.rgb;
#endif
    
    RT_DIFFUSE = vec4(color.rgb, fAlpha);
#ifndef QUAKE_MD5_PREVIEW
    RT_NORMALS = vec4(vNormals, 1.0);
    RT_LIGHTBUFFER = shadeColor;
    RT_GLOWBUFFER = sample(tQuakeGlowImage, inVar(input, out_texcoord)) + immediateColor;
    RT_GLOWBUFFER.rgb *= RT_GLOWBUFFER.a * fAlpha;
    RT_LIGHTBUFFER.rgb = mix(RT_LIGHTBUFFER.rgb, vec3(1.0, 1.0, 1.0), RT_GLOWBUFFER.a);
    
#ifndef GLSLC_NX
    const float fMask = fAlpha <= 0.999 ? 0.0 : 1.0;
    RT_MOTIONBLUR.a = fMask;
    RT_MOTIONBLUR.rg = 
        ComputeMotionVector(inVar(input, out_curPosition), inVar(input, out_prevPosition));
#endif
#endif
    outReturn(output)
}

#endif
