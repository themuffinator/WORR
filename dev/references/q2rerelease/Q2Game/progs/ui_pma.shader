//
// Copyright(C) 2019-2020 Nightdive Studios, LLC
//

#include "progs/common.inc"

#ifdef SHADER_VERTEX

begin_cbuffer(UiTransforms, 0)
    cbuffer_member(mat3, uUiTransform);
    cbuffer_member(mat4, uUiProjection);
end_cbuffer()

//----------------------------------------------------
// input
begin_input(inVertex)
    var_attrib(0, vec2); //Position
    var_attrib(1, vec2); //Texcoord
    var_attrib(2, vec4); //Color0
    var_attrib(3, vec4); //Color1
    var_attrib(4, int); //ExtraDataIndex
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(psPosition)
    def_var_out(vec2, psTexcoord, TEXCOORD0)
    def_var_out(vec4, psColor, COLOR0)
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
	declareOutVar(outVertex, output);

	//Unpack
	vec2 inPosition = inVarAttrib(0, input);
	vec2 inTexcoord = inVarAttrib(1, input);
	vec4 inColor = inVarAttrib(2, input);

	//Convert to pre-multiplied alpha
	inColor = vec4(inColor.rgb * inColor.a, inColor.a);

	vec3 transformedPosition = mul(uUiTransform, vec3(inPosition, 1.0f));
	vec4 projectedPosition = mul(uUiProjection, vec4(transformedPosition.xy, 0.0f, 1.0f));

	outVarPosition(output, psPosition) = projectedPosition;
	outVar(output, psTexcoord) = inTexcoord;
	outVar(output, psColor) = inColor;

	outReturn(output);
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
def_var_position(psPosition)
    def_var_in(vec2, psTexcoord, TEXCOORD0)
    def_var_in(vec4, psColor, COLOR0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

def_sampler(2D, tImage, 0);

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
	declareOutVar(outPixel, output);

	vec2 texcoord = inVar(input, psTexcoord);
	vec4 color = inVar(input, psColor);

	vec4 texColor = sample(tImage, texcoord);

	texColor = texColor * color;

	outVarFragment(output, fragment) = texColor;
	outReturn(output)
}

#endif
