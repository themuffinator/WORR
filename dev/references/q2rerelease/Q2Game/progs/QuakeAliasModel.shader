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
    var_attrib(0,  vec4);
    var_attrib(1,  vec2);
    // --
    var_attrib(2,  vec4);
    
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

#include "progs/includes/QuakeTransformStruct.inc"

//----------------------------------------------------
float ComputeShade(vec3 vertexnormal, vec4 shadeParams)
{
    float fDot = dot(vertexnormal, shadeParams.xyz);
    return fDot < 0.0 ? (1.0 + fDot * (13.0 / 44.0)) : 1.0 + fDot;
}

#define NUMVERTEXNORMALS    162
static const vec3 r_avertexnormals[NUMVERTEXNORMALS] =
#if defined(GLSL_VERSION) && !defined(VULKAN)
vec3[]
(
#else
{
#endif
    vec3(-0.525731, 0.000000, 0.850651),
    vec3(-0.442863, 0.238856, 0.864188),
    vec3(-0.295242, 0.000000, 0.955423),
    vec3(-0.309017, 0.500000, 0.809017),
    vec3(-0.162460, 0.262866, 0.951056),
    vec3(0.000000, 0.000000, 1.000000),
    vec3(0.000000, 0.850651, 0.525731),
    vec3(-0.147621, 0.716567, 0.681718),
    vec3(0.147621, 0.716567, 0.681718),
    vec3(0.000000, 0.525731, 0.850651),
    vec3(0.309017, 0.500000, 0.809017),
    vec3(0.525731, 0.000000, 0.850651),
    vec3(0.295242, 0.000000, 0.955423),
    vec3(0.442863, 0.238856, 0.864188),
    vec3(0.162460, 0.262866, 0.951056),
    vec3(-0.681718, 0.147621, 0.716567),
    vec3(-0.809017, 0.309017, 0.500000),
    vec3(-0.587785, 0.425325, 0.688191),
    vec3(-0.850651, 0.525731, 0.000000),
    vec3(-0.864188, 0.442863, 0.238856),
    vec3(-0.716567, 0.681718, 0.147621),
    vec3(-0.688191, 0.587785, 0.425325),
    vec3(-0.500000, 0.809017, 0.309017),
    vec3(-0.238856, 0.864188, 0.442863),
    vec3(-0.425325, 0.688191, 0.587785),
    vec3(-0.716567, 0.681718, -0.147621),
    vec3(-0.500000, 0.809017, -0.309017),
    vec3(-0.525731, 0.850651, 0.000000),
    vec3(0.000000, 0.850651, -0.525731),
    vec3(-0.238856, 0.864188, -0.442863),
    vec3(0.000000, 0.955423, -0.295242),
    vec3(-0.262866, 0.951056, -0.162460),
    vec3(0.000000, 1.000000, 0.000000),
    vec3(0.000000, 0.955423, 0.295242),
    vec3(-0.262866, 0.951056, 0.162460),
    vec3(0.238856, 0.864188, 0.442863),
    vec3(0.262866, 0.951056, 0.162460),
    vec3(0.500000, 0.809017, 0.309017),
    vec3(0.238856, 0.864188, -0.442863),
    vec3(0.262866, 0.951056, -0.162460),
    vec3(0.500000, 0.809017, -0.309017),
    vec3(0.850651, 0.525731, 0.000000),
    vec3(0.716567, 0.681718, 0.147621),
    vec3(0.716567, 0.681718, -0.147621),
    vec3(0.525731, 0.850651, 0.000000),
    vec3(0.425325, 0.688191, 0.587785),
    vec3(0.864188, 0.442863, 0.238856),
    vec3(0.688191, 0.587785, 0.425325),
    vec3(0.809017, 0.309017, 0.500000),
    vec3(0.681718, 0.147621, 0.716567),
    vec3(0.587785, 0.425325, 0.688191),
    vec3(0.955423, 0.295242, 0.000000),
    vec3(1.000000, 0.000000, 0.000000),
    vec3(0.951056, 0.162460, 0.262866),
    vec3(0.850651, -0.525731, 0.000000),
    vec3(0.955423, -0.295242, 0.000000),
    vec3(0.864188, -0.442863, 0.238856),
    vec3(0.951056, -0.162460, 0.262866),
    vec3(0.809017, -0.309017, 0.500000),
    vec3(0.681718, -0.147621, 0.716567),
    vec3(0.850651, 0.000000, 0.525731),
    vec3(0.864188, 0.442863, -0.238856),
    vec3(0.809017, 0.309017, -0.500000),
    vec3(0.951056, 0.162460, -0.262866),
    vec3(0.525731, 0.000000, -0.850651),
    vec3(0.681718, 0.147621, -0.716567),
    vec3(0.681718, -0.147621, -0.716567),
    vec3(0.850651, 0.000000, -0.525731),
    vec3(0.809017, -0.309017, -0.500000),
    vec3(0.864188, -0.442863, -0.238856),
    vec3(0.951056, -0.162460, -0.262866),
    vec3(0.147621, 0.716567, -0.681718),
    vec3(0.309017, 0.500000, -0.809017),
    vec3(0.425325, 0.688191, -0.587785),
    vec3(0.442863, 0.238856, -0.864188),
    vec3(0.587785, 0.425325, -0.688191),
    vec3(0.688191, 0.587785, -0.425325),
    vec3(-0.147621, 0.716567, -0.681718),
    vec3(-0.309017, 0.500000, -0.809017),
    vec3(0.000000, 0.525731, -0.850651),
    vec3(-0.525731, 0.000000, -0.850651),
    vec3(-0.442863, 0.238856, -0.864188),
    vec3(-0.295242, 0.000000, -0.955423),
    vec3(-0.162460, 0.262866, -0.951056),
    vec3(0.000000, 0.000000, -1.000000),
    vec3(0.295242, 0.000000, -0.955423),
    vec3(0.162460, 0.262866, -0.951056),
    vec3(-0.442863, -0.238856, -0.864188),
    vec3(-0.309017, -0.500000, -0.809017),
    vec3(-0.162460, -0.262866, -0.951056),
    vec3(0.000000, -0.850651, -0.525731),
    vec3(-0.147621, -0.716567, -0.681718),
    vec3(0.147621, -0.716567, -0.681718),
    vec3(0.000000, -0.525731, -0.850651),
    vec3(0.309017, -0.500000, -0.809017),
    vec3(0.442863, -0.238856, -0.864188),
    vec3(0.162460, -0.262866, -0.951056),
    vec3(0.238856, -0.864188, -0.442863),
    vec3(0.500000, -0.809017, -0.309017),
    vec3(0.425325, -0.688191, -0.587785),
    vec3(0.716567, -0.681718, -0.147621),
    vec3(0.688191, -0.587785, -0.425325),
    vec3(0.587785, -0.425325, -0.688191),
    vec3(0.000000, -0.955423, -0.295242),
    vec3(0.000000, -1.000000, 0.000000),
    vec3(0.262866, -0.951056, -0.162460),
    vec3(0.000000, -0.850651, 0.525731),
    vec3(0.000000, -0.955423, 0.295242),
    vec3(0.238856, -0.864188, 0.442863),
    vec3(0.262866, -0.951056, 0.162460),
    vec3(0.500000, -0.809017, 0.309017),
    vec3(0.716567, -0.681718, 0.147621),
    vec3(0.525731, -0.850651, 0.000000),
    vec3(-0.238856, -0.864188, -0.442863),
    vec3(-0.500000, -0.809017, -0.309017),
    vec3(-0.262866, -0.951056, -0.162460),
    vec3(-0.850651, -0.525731, 0.000000),
    vec3(-0.716567, -0.681718, -0.147621),
    vec3(-0.716567, -0.681718, 0.147621),
    vec3(-0.525731, -0.850651, 0.000000),
    vec3(-0.500000, -0.809017, 0.309017),
    vec3(-0.238856, -0.864188, 0.442863),
    vec3(-0.262866, -0.951056, 0.162460),
    vec3(-0.864188, -0.442863, 0.238856),
    vec3(-0.809017, -0.309017, 0.500000),
    vec3(-0.688191, -0.587785, 0.425325),
    vec3(-0.681718, -0.147621, 0.716567),
    vec3(-0.442863, -0.238856, 0.864188),
    vec3(-0.587785, -0.425325, 0.688191),
    vec3(-0.309017, -0.500000, 0.809017),
    vec3(-0.147621, -0.716567, 0.681718),
    vec3(-0.425325, -0.688191, 0.587785),
    vec3(-0.162460, -0.262866, 0.951056),
    vec3(0.442863, -0.238856, 0.864188),
    vec3(0.162460, -0.262866, 0.951056),
    vec3(0.309017, -0.500000, 0.809017),
    vec3(0.147621, -0.716567, 0.681718),
    vec3(0.000000, -0.525731, 0.850651),
    vec3(0.425325, -0.688191, 0.587785),
    vec3(0.587785, -0.425325, 0.688191),
    vec3(0.688191, -0.587785, 0.425325),
    vec3(-0.955423, 0.295242, 0.000000),
    vec3(-0.951056, 0.162460, 0.262866),
    vec3(-1.000000, 0.000000, 0.000000),
    vec3(-0.850651, 0.000000, 0.525731),
    vec3(-0.955423, -0.295242, 0.000000),
    vec3(-0.951056, -0.162460, 0.262866),
    vec3(-0.864188, 0.442863, -0.238856),
    vec3(-0.951056, 0.162460, -0.262866),
    vec3(-0.809017, 0.309017, -0.500000),
    vec3(-0.864188, -0.442863, -0.238856),
    vec3(-0.951056, -0.162460, -0.262866),
    vec3(-0.809017, -0.309017, -0.500000),
    vec3(-0.681718, 0.147621, -0.716567),
    vec3(-0.681718, -0.147621, -0.716567),
    vec3(-0.850651, 0.000000, -0.525731),
    vec3(-0.688191, 0.587785, -0.425325),
    vec3(-0.587785, 0.425325, -0.688191),
    vec3(-0.425325, 0.688191, -0.587785),
    vec3(-0.425325, -0.688191, -0.587785),
    vec3(-0.587785, -0.425325, -0.688191),
    vec3(-0.688191, -0.587785, -0.425325)
#if defined(GLSL_VERSION) && !defined(VULKAN)
);
#else
};
#endif

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
    vec4 blendParams                    = transform.fBlendParams1;
    
    uint flags                          = uint(shadeParams2.w); // Yuck... need to use w component of shadeparams2 for a bitflag
    outVar(output, out_immediateColor)  = vec4(0.0, 0.0, 0.0, 0.0);
    
    vec3 modelVertex1                   = inVarAttrib(0, input).xyz;
    vec3 modelVertex2                   = inVarAttrib(2, input).xyz;
    vec4 vertex                         = vec4((modelVertex2 - modelVertex1) * blendParams.x + modelVertex1, 1.0);
#ifndef GLSLC_NX
    vec4 prevVertex                     = vec4((modelVertex2 - modelVertex1) * blendParams.y + modelVertex1, 1.0);
#endif
    
    int lnIndex1                        = int(inVarAttrib(0, input).w);
    int lnIndex2                        = int(inVarAttrib(2, input).w);
    
    vec3 modelNormals1                  = r_avertexnormals[lnIndex1];
    vec3 modelNormals2                  = r_avertexnormals[lnIndex2];
    
    vec3 combinedNormals1               = (modelNormals2 - modelNormals1) * blendParams.x + modelNormals1;
    
    // ---------------------------------------------------------------------------------
    // Powersuit effect - set color and scale vertices
    if((flags & 2) != 0)
    {
        vec3 combinedNormals2 = (modelNormals2 - modelNormals1) * blendParams.x + modelNormals1;
        
        outVar(output, out_immediateColor).rgb = shadeParams2.rgb;
        outVar(output, out_immediateColor).a = 1.0;

        float scale = ((flags & 4) != 0) ? 0.25 : 4.0;

        vertex.xyz += (combinedNormals1 * scale);
#ifndef GLSLC_NX
        prevVertex.xyz += (combinedNormals2 * scale);
#endif
    }
    
    mat4 mtxProjection                  = uProjectionMatrix;
    
    // ---------------------------------------------------------------------------------
    // Set fixed FOV
    if((flags & 4) != 0)
    {
        const float fFOV                = tan((FIXED_WEAPON_FOV * 0.01745329251994329576923690768489) * 0.5);
        const float fA                  = 1.0 / (blendParams.w * fFOV);
        const float fB                  = 1.0 / fFOV;
        
        M(mtxProjection, 0, 0)          -= (M(mtxProjection, 0, 0) - fA);
        M(mtxProjection, 1, 1)          -= (M(mtxProjection, 1, 1) - fB);
    }
    
    mat4 mtxTransform                   = mul(uModelViewMatrix, objTransform);
    vec4 vPosition                      = mul(mtxTransform, vertex);
    outVarPosition(output, position)    = mul(mtxProjection, vPosition);
    outVar(output, out_texcoord)        = inVarAttrib(1, input).xy;
    outVar(output, out_position)        = vPosition.xyz;
    
    float fDot                          = ComputeShade(combinedNormals1, shadeParams1);
    vec3 vShadeColor                    = min(shadeParams2.xyz * fDot, vec3(1.0, 1.0, 1.0));
    
    // ---------------------------------------------------------------------------------
    // Apply monochrome scale to shade color
    float fIntensity                    = max(vShadeColor.r, max(vShadeColor.g, vShadeColor.b));
    vec3 vMonochrome                    = vec3(fIntensity, fIntensity, fIntensity);
    outVar(output, out_color)           = vec4(mix(vShadeColor, vMonochrome, blendParams.z), shadeParams1.w);   
    
    outVar(output, out_curPosition)     = outVarPosition(output, position);
    outVar(output, out_normals)         = normalize(mul(cast(mat3, mtxTransform), combinedNormals1));
    
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
#ifndef QUAKE_ALIAS_PREVIEW
def_sampler(2D, tQuakeGlowImage, 1);
#endif

#ifdef QUAKE_ALIAS_PREVIEW
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
    
#if QUAKE_ALIAS_PREVIEW == 1
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
#ifndef QUAKE_ALIAS_PREVIEW
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
