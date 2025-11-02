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
    def_var_out(vec3, out_texcoord1, TEXCOORD0)
    def_var_out(vec2, out_texcoord2, TEXCOORD1)
    def_var_out(vec3, out_position, POSITION0)
    def_var_out(vec4, out_curPosition,  POSITION1)
    def_var_out(vec4, out_prevPosition, POSITION2)
    def_var_out(vec4, out_style, STYLES)
    def_var_out(vec4, out_plane, PLANE)
    def_flat_var_out(uint, out_mask, MASK)
    def_var_out(float, out_frame, FRAME)
    def_var_out(float, out_time, TIME)
    def_var_out(float, out_alpha, ALPHA)
    def_var_out(float, out_monochromeScale, MONO)
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
    
    quakeModelTransform_s transform     = sbTransform[index];
    
    objTransform                        = MakeMatrix(transform.fTransform1, transform.fTransform2, transform.fTransform3, transform.fTransform4);
    objPrevTransform                    = MakeMatrix(transform.fTransform5, transform.fTransform6, transform.fTransform7, transform.fTransform8);
    
    vec4 vertex                         = vec4(inVarAttrib(0, input).xyz, 1.0);
    mat4 mtxTransform                   = mul(uModelViewMatrix, objTransform);
    mat4 mtxPrevTransform               = mul(uPrevModelView, objPrevTransform);
    vec4 vPosition                      = mul(mtxTransform, vertex);
    vec4 vPrevPosition                  = mul(mtxPrevTransform, vertex);
    outVarPosition(output, position)    = mul(uProjectionMatrix, vPosition);
    outVar(output, out_position)        = vPosition.xyz;
    outVar(output, out_texcoord1)       = inVarAttrib(1, input);
    outVar(output, out_texcoord2)       = inVarAttrib(2, input).xy;
    outVar(output, out_curPosition)     = outVarPosition(output, position);
    outVar(output, out_prevPosition)    = mul(uPrevProjection, vPrevPosition);
    outVar(output, out_plane)           = inVarAttrib(3, input);
    outVar(output, out_style)           = inVarAttrib(4, input);
    outVar(output, out_mask)            = inVarAttrib(5, input);
    outVar(output, out_time)            = transform.fBlendParams1[1];
    outVar(output, out_monochromeScale) = transform.fShadeParams1[0];
    outVar(output, out_alpha)           = inVarAttrib(0, input).w * transform.fShadeParams1[3];

    int frameIdx = int(((inVarAttrib(5, input) >> 1) & 0x7FF)); // this texinfo's base array index
	int frameOffset = int(transform.fBlendParams1[0]); // brush model frame offset
	int startIdx = frameIdx - ((int(floor(inVarAttrib(1, input).z + 0.5)) >> 9) & 0xFF); // start of animation chain index (always <= frameIdx)
	int maxFrames = max((int(floor(inVarAttrib(1, input).z + 0.5)) >> 1) & 0xFF, 1); // total frames in entire chain
#if HLSL
    int arrayIdx = startIdx + fmod(((frameIdx - startIdx) + frameOffset), maxFrames);
#else
    int arrayIdx = startIdx + (((frameIdx - startIdx) + frameOffset) % maxFrames);
#endif
    outVar(output, out_frame)           = float(arrayIdx);
    
    outVar(output, out_plane).xyz       = normalize(mul(cast(mat3, mtxTransform), outVar(output, out_plane).xyz));
    
    float fScrollAmt = -64.0 * (outVar(output, out_time) / 84.0) - float(int(outVar(output, out_time) / 84.0));
    if(fScrollAmt == 0.0)
    {
        fScrollAmt = -64.0;
    }
    
    outVar(output, out_texcoord1).x += inVarAttrib(2, input).z * fScrollAmt;
    outVar(output, out_texcoord1).y += inVarAttrib(2, input).w * fScrollAmt;
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_in(vec3, out_texcoord1, TEXCOORD0)
    def_var_in(vec2, out_texcoord2, TEXCOORD1)
    def_var_in(vec3, out_position, POSITION0)
    def_var_in(vec4, out_curPosition,   POSITION1)
    def_var_in(vec4, out_prevPosition,  POSITION2)
    def_var_in(vec4, out_style,  STYLES)
    def_var_in(vec4, out_plane, PLANE)
    def_flat_var_in(uint, out_mask, MASK)
    def_var_in(float, out_frame, FRAME)
    def_var_in(float, out_time, TIME)
    def_var_in(float, out_alpha, ALPHA)
    def_var_in(float, out_monochromeScale, MONO)
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
def_samplerArray(2D, tLightmap, 1);

#if QUAKE_FORWARD_SHADE == 1
    #define QUAKE_HAS_CLUSTERED_SHADING     1
#endif

#include "progs/includes/QuakeUtils.inc"

#define MAX_LIGHTSTYLES     256

//----------------------------------------------------
struct quakeLightStyleStruct_s
{
    vec3 rgb;
    float white;
};

//----------------------------------------------------
begin_cbuffer(WorldLights, 0) 
    cbuffer_member(quakeLightStyleStruct_s, uValues[MAX_LIGHTSTYLES]);
end_cbuffer()

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    const uint dwMask = inVar(input, out_mask);
    const vec4 plane = inVar(input, out_plane);
    const vec4 styles = inVar(input, out_style);
    vec2 vTexCoords = inVar(input, out_texcoord1).xy;
    vec4 lightmapColor = vec4(0.0, 0.0, 0.0, 0.0);
    
    const int s1 = int(styles.x+0.5);
    const int s2 = int(styles.y+0.5);
    const int s3 = int(styles.z+0.5);
    const int s4 = int(styles.w+0.5);
    
    // ---------------------------------------------------------------------------------
    // Sample lightmap layers
    vec3 vCoords = vec3(inVar(input, out_texcoord2), float((dwMask >> 23) & 0xF));
    if(s1 != 255)
    {
        lightmapColor += sample(tLightmap, vCoords) * vec4(uValues[s1].rgb, 1.0);
        if(s2 != 255)
        {
            vCoords.z++;
            lightmapColor += sample(tLightmap, vCoords) * vec4(uValues[s2].rgb, 1.0);
            if(s3 != 255)
            {
                vCoords.z++;
                lightmapColor += sample(tLightmap, vCoords) * vec4(uValues[s3].rgb, 1.0);
                if(s4 != 255)
                {
                    vCoords.z++;
                    lightmapColor += sample(tLightmap, vCoords) * vec4(uValues[s4].rgb, 1.0);
                }
            }
        }
    }
    
    lightmapColor.w = 1.0;
    
    // ---------------------------------------------------------------------------------
    // Handle warp effects
    uint flagBits = uint(floor(inVar(input, out_texcoord1).z + 0.5));
    if((flagBits & 0x1) != 0)
    {
        vTexCoords = ComputeWarpCoords(vTexCoords, inVar(input, out_time));
    }
    
    // ---------------------------------------------------------------------------------
    // Sample texture
    vec4 color = sample(tQuakeImage, vec3(vTexCoords, inVar(input, out_frame)));
    
#if QUAKE_ALPHA_MASK == 1
    if(color.a <= 0.8)
    {
        discard;
        outReturn(output)
    }
    
    color.rgb *= color.a;
#endif
    
    vec4 vNewShadeColor = vec4(lightmapColor.rgb, 1.0);
    
    // ---------------------------------------------------------------------------------
    // Apply monochrome scale to lightmap
    float fLightmapIntensity = max(vNewShadeColor.r, max(vNewShadeColor.g, vNewShadeColor.b));
    vNewShadeColor = mix(vNewShadeColor, vec4(fLightmapIntensity, fLightmapIntensity, fLightmapIntensity, 1.0), inVar(input, out_monochromeScale));
    
    color.a *= inVar(input, out_alpha);
    
    const vec3 vNormals = plane.rgb;
    
#if QUAKE_NO_TEXTURE == 1
    color = vec4(0.5, 0.5, 0.5, 1.0);
#endif

#if QUAKE_FORWARD_SHADE == 1

    // ---------------------------------------------------------------------------------
    // Apply dynamic lighting and shadows
    const float fFragZ = -(inVar(input, out_position).z);
    vec4 vOutColor = vec4(1.0, 1.0, 1.0, 1.0);
    
    if(ComputeShadedColor(vOutColor, vTexCoords, vNewShadeColor, true, fFragZ, vNormals, inFragCoord) == false)
    {
        discard;
        outReturn(output)
    }
    
    color.rgb *= vOutColor.rgb;
#endif
    
    RT_DIFFUSE = color;
    RT_LIGHTBUFFER = vNewShadeColor;
    RT_NORMALS = vec4(vNormals, 1.0);
#ifndef QUAKE_GLOW
    RT_GLOWBUFFER = vec4(0.0, 0.0, 0.0, 1.0);
    
    const float fMask = color.a <= 0.999 ? 0.0 : 1.0;
#else
    // Force full opacity on surface. We're using alpha for the glow/fullbright effect.
    RT_DIFFUSE.a = 0xFF;
    
    // Make glow fullbright over lightmap
    RT_LIGHTBUFFER.rgb = mix(RT_LIGHTBUFFER.rgb, vec3(1.0, 1.0, 1.0), color.a);
    
    // Write glowmap to render target
    RT_GLOWBUFFER = color;
    RT_GLOWBUFFER.rgb *= color.a;
    
    const float fMask = 1.0;
#endif

#ifndef GLSLC_NX
    RT_MOTIONBLUR.a = fMask;
    RT_MOTIONBLUR.rg = 
        ComputeMotionVector(inVar(input, out_curPosition), inVar(input, out_prevPosition));
#endif
    outReturn(output)
}

#endif
