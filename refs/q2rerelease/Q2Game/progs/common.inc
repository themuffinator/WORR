//
// Copyright(C) 2016-2019 Samuel Villarreal
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#pragma once

#define ATTRIB_POSITION     0
#define ATTRIB_TEXCOORD     1
#define ATTRIB_COLOR        2
#define ATTRIB_NORMAL       3

#ifndef mad
#define mad(a, b, c) (a * b + c)
#endif

#if defined(__ORBIS__)
    #include "progs/common_pssl.inc"
#elif defined(__PROSPERO__)
    #include "progs/common_pssl2.inc"
#elif defined(HLSL) || defined(HLSL_TO_GLSLANG)
    #include "progs/common_hlsl.inc"
#else
    #include "progs/common_glsl.inc"
#endif

#ifndef EMULATE_UNIFORM_BUFFERS
    #define begin_cbuffer(name, register)   \
    uniform_buffer(name, register) \
    {
        
    #define cbuffer_member(type, name)  type name
    #define end_cbuffer() };
#else
    #define begin_cbuffer(name, register)
    #define cbuffer_member(type, name) uniform type name
    #define end_cbuffer()
#endif

#include "progs/globals_structs.inc"

#ifdef SHADER_VERTEX
    #include "progs/globals_vertex.inc"
#endif

#ifdef SHADER_PIXEL
    #include "progs/globals_fragment.inc"
#endif

// ---------------------------------------------------------------------------------
//
mat4 MakeMatrix(const in vec4 row1, const in vec4 row2, const in vec4 row3, const in vec4 row4)
{
#if GLSL_VERSION
    return mat4(row1, row2, row3, row4);
#else
    return mat4(
        vec4(row1.x, row2.x, row3.x, row4.x),
        vec4(row1.y, row2.y, row3.y, row4.y),
        vec4(row1.z, row2.z, row3.z, row4.z),
        vec4(row1.w, row2.w, row3.w, row4.w));
#endif
}

// ---------------------------------------------------------------------------------
//
mat3 Mat4ToMat3(const in mat4 mtx)
{
    mat3 temp;
    M(temp, 0, 0) = M(mtx, 0, 0);
    M(temp, 0, 1) = M(mtx, 0, 1);
    M(temp, 0, 2) = M(mtx, 0, 2);
    
    M(temp, 1, 0) = M(mtx, 1, 0);
    M(temp, 1, 1) = M(mtx, 1, 1);
    M(temp, 1, 2) = M(mtx, 1, 2);
    
    M(temp, 2, 0) = M(mtx, 2, 0);
    M(temp, 2, 1) = M(mtx, 2, 1);
    M(temp, 2, 2) = M(mtx, 2, 2);
    
    return temp;
}
