//
// Copyright(C) 2007-2014 Samuel Villarreal
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

#define GL_EXT_control_flow_attributes 1

#define mul(m1, m2) (m1 * m2)
#define static
#define saturate(a) clamp(a, 0.0, 1.0)
#define cast(type, n) type(n)
#define fmod(x, y) mod(x, y)

#ifndef GLSLC_NX
    #define unroll  [[unroll]]
    #define flatten [[flatten]]
    #define branch  [[dont_flatten]]
#else
    #define unroll
    #define flatten
    #define branch
#endif

#define inline
#define frac(x) fract(x)
#define ddx(x) dFdx(x)
#define ddy(y) dFdy(y)
#define lerp(x, y, a) mix(x, y, a)

#define rcp(x) (1.0 / (x))

#define vec2op(v, op, val) (v.x op val && v.y op val)
#define vec3op(v, op, val) (v.x op val && v.y op val && v.z op val)

#ifdef SHADER_PIXEL
void clip(float x)  { if(x     < 0.0)                           { discard; } }
void clip(vec2 x)   { if(any(lessThan(x, vec2(0.0))) == true)   { discard; } }
void clip(vec3 x)   { if(any(lessThan(x, vec3(0.0))) == true)   { discard; } }
void clip(vec4 x)   { if(any(lessThan(x, vec4(0.0))) == true)   { discard; } }
#endif    //SHADER_PIXEL

#if GLSL_VERSION >= 330
    #define glslAttrib(idx, type)                   layout(location = idx) in type attrib##idx
#elif GLSL_VERSION >= 130
    #define glslAttrib(idx, type)                   in type attrib##idx
#else
    #define glslAttrib(idx, type)                   attribute type attrib##idx
#endif

#define glslAttribName(idx)                         attrib##idx

#define begin_input(name)
#define end_input

#define begin_output(name)
#define end_output

#if defined(SHADER_PIXEL)
    #define CBUFFER_SET     3
    #define RESOURCE_SET    4
    #define UAV_SET         5
#elif defined(SHADER_GEOMETRY)
    #define CBUFFER_SET     6
    #define RESOURCE_SET    7
    #define UAV_SET         8
#else
    #define CBUFFER_SET     0
    #define RESOURCE_SET    1
    #define UAV_SET         2
#endif

#define uniform_buffer(name, block)                 layout (set = CBUFFER_SET, binding = __binding__) uniform name

#define var_attrib(idx, type)                       glslAttrib(idx, type)
#define inVarAttrib(idx, structName)                glslAttribName(idx)

#define glsl_out                                    __out__
#define glsl_in                                     __in__

#define def_var_out(type, name, gpuName)            glsl_out type name;

#ifndef SHADER_GEOMETRY
#define def_var_in(type, name, gpuName)             glsl_in type name;
#else
#define def_var_in(type, name, gpuName)             glsl_in type name[];
#endif

#define def_flat_var_out(type, name, gpuName)       flat glsl_out type name;

#ifndef SHADER_GEOMETRY
#define def_flat_var_in(type, name, gpuName)        flat glsl_in type name;
#else
#define def_flat_var_in(type, name, gpuName)        flat glsl_in type name[];
#endif

#define def_var_outPosition(name)                   out gl_PerVertex { vec4 gl_Position; };

#ifndef SHADER_GEOMETRY
#define def_var_position(name)
#else
#define def_var_position(name)                      in gl_PerVertex { vec4 gl_Position; } gl_in[];
#endif

#define def_var_fragment(name)                      layout (location = 0) out vec4 outFragment0;
#define def_var_pixelTarget(type, idx)              layout (location = idx) out vec4 outFragment##idx;

#ifndef SHADER_GEOMETRY
#define shader_main(outStruct, inStruct, inName)    void main()
#define shader_pixel(outStruct, inStruct, inName)   void main()
#else
#define shader_main(inType, inStruct, inName, maxCount, inCount, outType, outStruct)    \
layout(inType) in;  \
layout(outType, max_vertices = maxCount) out;   \
void main()
#endif

#define declareOutVar(outStruct, name)
#define outVar(name, field)                         field
#define outVarPosition(name, field)                 gl_Position

#define outVarFragment(name, field)                 outFragment0
#define outVarPixelTarget(name, idx)                outFragment##idx

#ifndef SHADER_GEOMETRY
#define inVar(inName, field)                        field
#else
#define inVar(name, field, index)                   field[index]
#endif

#define outReturn(name)                             return;

#define def_sampler(dim, name, idx) \
    layout (set = RESOURCE_SET, binding = idx) uniform sampler##dim uTex_##name
#define def_samplerArray(dim, name, idx)    \
    layout (set = RESOURCE_SET, binding = idx) uniform sampler##dim##Array uTex_##name
#define def_samplerCompare(dim, name, idx)  \
    layout (set = RESOURCE_SET, binding = idx) uniform sampler##dim##Shadow uTex_##name
#define def_samplerUint(dim, name, idx) \
    layout (set = RESOURCE_SET, binding = idx) uniform usampler##dim uTex_##name
#define def_samplerArrayUint(dim, name, idx)    \
    layout (set = RESOURCE_SET, binding = idx) uniform usampler##dim##Array uTex_##name

#define sampleLevelZero(name, coord)                texture(uTex_##name, coord)
#define sample(name, coord)                         texture(uTex_##name, coord)
#define sampleLOD(name, coord, lod)                 textureLod(uTex_##name, coord, float(lod))
#define sampleLODOffset(name, coord, lod, offset)   textureLodOffset(uTex_##name, coord, float(lod), offset)
#define load(name, coord, mip)                      texelFetch(uTex_##name, coord, mip)
#define loadOffset(name, coord, mip, offset)        texelFetchOffset(uTex_##name, coord, mip, offset)
#define loadArray(name, coord, layer, mip)          texelFetch(uTex_##name, ivec3(coord, layer), mip)
#define gather(name, coord)                         textureGather(uTex_##name, coord)   
#define gatherOffset(name, coord, offset)           textureGatherOffset(uTex_##name, coord, offset)
#define gatherRed(name, coord)                      textureGather(uTex_##name, coord, 0)
#define gatherGreen(name, coord)                    textureGather(uTex_##name, coord, 1)
#define gatherBlue(name, coord)                     textureGather(uTex_##name, coord, 2)
#define gatherAlpha(name, coord)                    textureGather(uTex_##name, coord, 3)
#define gatherRedOffset(name, coord, offset)        textureGatherOffset(uTex_##name, coord, offset, 0)
#define gatherGreenOffset(name, coord, offset)      textureGatherOffset(uTex_##name, coord, offset, 1)
#define gatherBlueOffset(name, coord, offset)       textureGatherOffset(uTex_##name, coord, offset, 2)
#define gatherAlphaOffset(name, coord, offset)      textureGatherOffset(uTex_##name, coord, offset, 3)
#define sampleCmpLevelZero(name, coord, cmp)        texture(uTex_##name, vec3(coord, cmp))  

#define inFragCoord                                 gl_FragCoord

#define M(mtx, col, row)                            mtx[col][row]
#define MV4(mtx, col)                               mtx[col]
#define MV3(mtx, col)                               mtx[col].xyz
#define MV2(mtx, col)                               mtx[col].xy

#define def_var_instanceID()
#define def_var_vertexID()
#define def_var_clipDistance()

#define inClipDistance                              gl_ClipDistance
#define inInstanceID                                gl_InstanceIndex
#define inVertexID                                  gl_VertexIndex

#define def_var_dispatchThreadID()
#define def_var_groupID()
#define def_var_groupIndex()
#define def_var_groupThreadID()

#define inNumThreads(x, y, z)                       layout(local_size_x = x, local_size_y = y, local_size_z = z) in;

#define inDispatchThreadID                          gl_GlobalInvocationID
#define inGroupID                                   gl_WorkGroupID
#define inGroupIndex                                gl_LocalInvocationIndex
#define inGroupThreadID                             gl_LocalInvocationID

#define def_structuredBuffer(type, name, idx) \
    layout(std430, set = RESOURCE_SET, binding = idx) buffer _##name  \
    {   \
        restrict readonly type name[ ];   \
    }

#define def_rwStructuredBuffer(type, name, idx) \
    layout(std430, set = UAV_SET, binding = idx) buffer _##name  \
    {   \
        type name[ ];   \
    }

#define def_rwTexture(type, dims, name, idx)    \
    layout (set = UAV_SET, binding = idx, type) uniform image##dims name 
    
#define def_rwTextureArray(type, dims, name, idx)   \
    layout (set = UAV_SET, binding = idx, type) uniform image##dims##Array name
    
#define def_roBuffer(type, name, idx)               def_structuredBuffer(type, name, idx)
#define def_rwBuffer(type, name, idx)               def_rwStructuredBuffer(type, name, idx)

#define groupShared                                 shared
#define groupMemoryBarrierWithGroupSync()   \
    groupMemoryBarrier();   \
    barrier()
#define groupMemoryBarrierAll()   \
    groupMemoryBarrier();  \
    barrier();  \
    memoryBarrier()
#define kAtomicAdd(mem, data, orig)                 (orig = atomicAdd(mem, data))

#define inVarPosition(name, field, index)           gl_in[index].gl_Position

#define in_point                                    points
#define in_line                                     lines
#define in_triangles                                triangles
#define in_lineadj                                  line_adjacency
#define triangleadj                                 triangle_adjacency

#define out_points                                  points
#define out_lines                                   line_strip
#define out_triangles                               triangle_strip

#define emitVertex(outName)                         EmitVertex()
#define endPrimitive(outName)                       EndPrimitive()

#define inPrimitiveID                               gl_PrimitiveID
