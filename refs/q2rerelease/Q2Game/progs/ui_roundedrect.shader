//
// Copyright(C) 2019-2020 Nightdive Studios, LLC
//

#include "progs/common.inc"

#ifdef SHADER_VERTEX

begin_cbuffer(UiTransforms, 0)
    cbuffer_member(mat3, uUiTransform);
    cbuffer_member(mat4, uUiProjection);
end_cbuffer()

//AspectRatio, BorderSize, CornerRadius, Fill
struct extraData_s
{
    float   aspectRatio;
    float   borderSize;
    float   cornerRadius;
    float   fill;
};
def_structuredBuffer(extraData_s, sbExtraData, 0);

//----------------------------------------------------
// input
begin_input(inVertex)
    var_attrib(0, vec2); //Position
    var_attrib(1, vec2); //Texcoord
    var_attrib(2, vec4); //Color0 - border
    var_attrib(3, vec4); //Color1 - fill
    var_attrib(4, int); //Data index
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(psPosition)
    def_var_out(vec2, psTexcoord, TEXCOORD0)
    def_var_out(float, psAspectRatio, TEXCOORD1)
    def_var_out(float, psBorderSize, TEXCOORD2)
    def_var_out(float, psCornerRadius, TEXCOORD3)
    def_var_out(float, psFill, TEXCOORD4)
    def_var_out(vec4, psBorderColor, COLOR0)
    def_var_out(vec4, psFillColor, COLOR1)
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output);

    //Unpack
    vec2 inPosition = inVarAttrib(0, input);
    vec2 inTexcoord = inVarAttrib(1, input);
    vec4 inColor0 = inVarAttrib(2, input);
    vec4 inColor1 = inVarAttrib(3, input);
    int inExtraDataIndex = inVarAttrib(4, input);

    extraData_s extraData = sbExtraData[inExtraDataIndex];

    vec3 transformedPosition = mul(uUiTransform, vec3(inPosition, 1.0f));
    vec4 projectedPosition = mul(uUiProjection, vec4(transformedPosition.xy, 0.0f, 1.0f));

    outVarPosition(output, psPosition) = projectedPosition;
    outVar(output, psTexcoord) = inTexcoord * vec2(extraData.aspectRatio, 1.0f);
    outVar(output, psBorderColor) = inColor0;
    outVar(output, psFillColor) = inColor1;
    outVar(output, psAspectRatio) = extraData.aspectRatio;
    outVar(output, psBorderSize) = extraData.borderSize * extraData.aspectRatio;
    outVar(output, psCornerRadius) = extraData.cornerRadius * extraData.aspectRatio;
    outVar(output, psFill) = extraData.fill;

    outReturn(output);
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(psPosition)
    def_var_in(vec2, psTexcoord, TEXCOORD0)
    def_var_in(float, psAspectRatio, TEXCOORD1)
    def_var_in(float, psBorderSize, TEXCOORD2)
    def_var_in(float, psCornerRadius, TEXCOORD3)
    def_var_in(float, psFill, TEXCOORD4)
    def_var_in(vec4, psBorderColor, COLOR0)
    def_var_in(vec4, psFillColor, COLOR1)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

float Sdf_RoundedBox(vec2 p, vec2 boxCenter, vec2 size, float radius)
{
    vec2 q = abs(p - boxCenter) - size + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
    /*float2 q = abs(p - boxCenter) - size + radius;

    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - radius;*/
}

float Sdf_Annular(float d, float r)
{
    return abs(d) - r;
}

float Sdf_AntiAlias(float d)
{
    float dist = length(vec2(dFdx(d), dFdy(d)));

    return saturate(0.5f - (d / dist));
}

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output);

    vec2 texCoord = inVar(input, psTexcoord);
    vec4 borderColor = inVar(input, psBorderColor);
    vec4 fillColor = inVar(input, psFillColor);
    float aspectRatio = inVar(input, psAspectRatio);
    float borderSize = inVar(input, psBorderSize);
    float cornerRadius = inVar(input, psCornerRadius);
    float fill = inVar(input, psFill);

    vec2 scale = vec2(aspectRatio, 1.0f);

    float fillDist = Sdf_RoundedBox(texCoord, vec2(0.5f, 0.5f) * scale,
        vec2(0.5f, 0.5f) * scale - borderSize * 0.5f, cornerRadius);

    float borderDist = Sdf_Annular(fillDist, borderSize / 2.0f);
    float borderAlpha = Sdf_AntiAlias(borderDist);
    float fillAlpha = Sdf_AntiAlias(fillDist);

    float alpha = max(borderAlpha, fillAlpha * fill);
    vec4 color = lerp(fillColor, borderColor, borderAlpha);

    outVarFragment(output, fragment) = vec4(color.rgb, 1.0f) * (color.a * alpha);
    outReturn(output)
}

#endif
