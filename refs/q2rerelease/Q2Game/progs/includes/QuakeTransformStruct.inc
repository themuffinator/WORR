//
// Copyright(C) 2022-2023 Nightdive Studios, LLC
//
// ORIGINAL AUTHOR
//      Samuel Villarreal
//

struct quakeModelTransform_s
{
    vec4 fTransform1;
    vec4 fTransform2;
    vec4 fTransform3;
    vec4 fTransform4;
    vec4 fTransform5;
    vec4 fTransform6;
    vec4 fTransform7;
    vec4 fTransform8;
    vec4 fShadeParams1;
    vec4 fShadeParams2;
    vec4 fBlendParams1;
    vec4 fBlendParams2;
};

#ifndef QUAKE_NO_TRANSFORM_UNIFORM_BUFFER
//----------------------------------------------------
begin_cbuffer(TransformBuffer, 0)
    cbuffer_member(quakeModelTransform_s, uTransform[256]);
end_cbuffer()
#endif
