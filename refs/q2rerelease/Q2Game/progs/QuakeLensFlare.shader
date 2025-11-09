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
    var_attrib(1,  vec2);
    
    def_var_instanceID()
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec2, out_texcoord, TEXCOORD0)
    def_var_out(vec4, out_color, COLOR0)
end_output

#include "progs/includes/QuakeTransformStruct.inc"

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    quakeModelTransform_s transform     = uTransform[inVar(input, inInstanceID)];
    mat4 objTransform                   = MakeMatrix(transform.fTransform1, transform.fTransform2, transform.fTransform3, transform.fTransform4);
    
    vec4 vertex                         = mul(objTransform, vec4(inVarAttrib(0, input).xy, 0.0, 1.0));
    vertex                              = mul(uTransposedRotationMatrix, vertex);
    
    vertex.xyz                          += transform.fTransform8.xyz;
    
    vec4 vPosition                      = mul(uModelViewMatrix, vertex);
    outVarPosition(output, position)    = mul(uProjectionMatrix, vPosition);
    outVar(output, out_texcoord)        = inVarAttrib(1, input);
    outVar(output, out_color)           = mix(transform.fBlendParams1.rgba, transform.fShadeParams2.rgba, 1.0 - inVarAttrib(0, input).w);
    outVar(output, out_color).a         = inVarAttrib(0, input).z;
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
    def_var_in(vec4, out_color, COLOR0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_pixelTarget(vec4, 0)
end_output

def_sampler(2D, tQuakeSpriteImage, 0);

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec4 color = sample(tQuakeSpriteImage, inVar(input, out_texcoord));
    color.rgb *= ((color.r + color.g + color.b) / 3.0);
    color.rgb *= inVar(input, out_color).a;
    
    outVarPixelTarget(output, 0) = color * inVar(input, out_color);
    outReturn(output)
}

#endif
