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

#define REVERSE_NDC             1
#define CLIP_SPACE_ZERO_TO_ONE  1

#define vec2 float2
#define vec3 float3
#define vec4 float4
#define mat2 float2x2
#define mat3 float3x3
#define mat4 float4x4
#define ivec2 int2
#define ivec3 int3
#define ivec4 int4
#define bvec2 bool2
#define bvec3 bool3
#define bvec4 bool4
#define uvec2 uint2
#define uvec3 uint3
#define uvec4 uint4

#define rcp(x) (1.0 / (x))

#define flatten [flatten]
#define branch [branch]
#define unroll [unroll]

#define flat

#define cast(type, n) (type)(n)

#define fract(x) ((x) - floor((x)))
#define mix(x, y, a) lerp(x, y, a)
#define dFdx(x) ddx(x)
#define dFdy(y) ddy(y)

#define vec2op(v, op, val) (v op val)
#define vec3op(v, op, val) (v op val)

#define inversesqrt(x) rsqrt(x)

#define begin_input(name)                           struct name {
#define end_input                                   };

#define begin_output(name)                          struct name {
#define end_output                                  };

#define uniform_buffer(name, block)                 cbuffer name

#define hlslAttribName1(idx)                        attrib##idx
#define hlslAttribName2(idx)                        ATTRIB##idx

#define var_attrib(idx, type)                       type hlslAttribName1(idx) : hlslAttribName2(idx)
#define inVarAttrib(idx, structName)                structName.hlslAttribName1(idx)

#define def_var_out(type, name, gpuName)            type name : gpuName;
#define def_var_in(type, name, gpuName)             type name : gpuName;

#define def_flat_var_out(type, name, gpuName)       type name : gpuName;
#define def_flat_var_in(type, name, gpuName)        type name : gpuName;

#define def_var_outPosition(name)                   def_var_in(vec4, name, SV_POSITION)
#define def_var_position(name)                      def_var_out(vec4, name, SV_POSITION)
#define def_var_fragment(name)                      def_var_out(vec4, name, SV_TARGET)
#define def_var_pixelTarget(type, idx)              type outPixelTarget##idx : SV_Target##idx;

#ifndef SHADER_GEOMETRY
#define shader_main(outStruct, inStruct, inName)    outStruct main(inStruct inName)
#define shader_pixel(outStruct, inStruct, inName)   outStruct main(float4 inFragCoord : SV_Position, inStruct inName)
#else    
#define shader_main(inType, inStruct, inName, maxCount, inCount, outType, outStruct)    \
    [maxvertexcount(maxCount)]  \
    void main(inType inStruct inName[inCount], uint inPrimitiveID : SV_PrimitiveID, inout outType<outStruct> outputStream)
#endif

#define declareOutVar(outStruct, name)              outStruct name;
#define outVar(name, field)                         name.field
#define outVarPosition(name, field)                 name.field
#define outVarFragment(name, field)                 name.field
#define outVarPixelTarget(name, idx)                name.outPixelTarget##idx

#ifndef SHADER_GEOMETRY
#define inVar(inName, field)                        inName.field
#else
#define inVar(inName, field, index)                 inName[index].field
#define inVarPosition(inName, field, index)         inVar(inName, field, index)
#endif

#define outReturn(name)                             return name;

#define def_sampler(dim, name, idx)   \
    Texture##dim uTex_##name : register(t##idx); \
    SamplerState uSample##name : register(s##idx)
    
#define def_samplerArray(dim, name, idx)    \
    Texture##dim##Array uTex_##name : register(t##idx);   \
    SamplerState uSample##name : register(s##idx)
    
#define def_samplerCompare(dim, name, idx)   \
    Texture##dim uTex_##name : register(t##idx); \
    SamplerComparisonState uSample##name : register(s##idx)

#define def_samplerUint(dim, name, idx)   \
    Texture##dim<uint> uTex_##name : register(t##idx); \
    SamplerState uSample##name : register(s##idx)
    
#define def_samplerArrayUint(dim, name, idx)    \
    Texture##dim##Array<uint> uTex_##name : register(t##idx);   \
    SamplerState uSample##name : register(s##idx)
    
#define sampleLevelZero(name, coord)                uTex_##name.SampleLevel(uSample##name, coord, 0)
#define sample(name, coord)                         uTex_##name.Sample(uSample##name, coord)
#define sampleLOD(name, coord, lod)                 uTex_##name.SampleLevel(uSample##name, coord, lod)
#define sampleLODOffset(name, coord, lod, offset)   uTex_##name.SampleLevel(uSample##name, coord, lod, offset)
#define load(name, coord, mip)                      uTex_##name.Load(ivec3(coord, mip))
#define loadArray(name, coord, layer, mip)          uTex_##name.Load(ivec4(coord, layer, mip))
#define loadOffset(name, coord, mip, offset)        uTex_##name.Load(ivec3(coord, mip), offset)
#define gather(name, coord)                         uTex_##name.Gather(uSample##name, coord)
#define gatherOffset(name, coord, offset)           uTex_##name.Gather(uSample##name, coord, offset)
#define gatherRed(name, coord)                      uTex_##name.GatherRed(uSample##name, coord)
#define gatherGreen(name, coord)                    uTex_##name.GatherGreen(uSample##name, coord)
#define gatherBlue(name, coord)                     uTex_##name.GatherBlue(uSample##name, coord)
#define gatherAlpha(name, coord)                    uTex_##name.GatherAlpha(uSample##name, coord)
#define gatherRedOffset(name, coord, offset)        uTex_##name.GatherRed(uSample##name, coord, offset)
#define gatherGreenOffset(name, coord, offset)      uTex_##name.GatherGreen(uSample##name, coord, offset)
#define gatherBlueOffset(name, coord, offset)       uTex_##name.GatherBlue(uSample##name, coord, offset)
#define gatherAlphaOffset(name, coord, offset)      uTex_##name.GatherAlpha(uSample##name, coord, offset)
#define sampleCmpLevelZero(name, coord, cmp)        uTex_##name.SampleCmpLevelZero(uSample##name, coord, cmp)    

#define M(mtx, col, row)                            mtx[row][col]
#define MV4(mtx, col)                               vec4(mtx[0][col], mtx[1][col], mtx[2][col], mtx[3][col])
#define MV3(mtx, col)                               vec3(mtx[0][col], mtx[1][col], mtx[2][col])
#define MV2(mtx, col)                               vec2(mtx[0][col], mtx[1][col])

#define inNumThreads(x, y, z)                       [numthreads(x, y, z)]

#define def_var_instanceID()                        def_var_out(uint, inInstanceID, SV_InstanceID)
#define def_var_vertexID()                          def_var_out(uint, inVertexID, SV_VertexID)
#define def_var_clipDistance()                      def_var_out(float, inClipDistance, SV_ClipDistance)
#define def_var_dispatchThreadID()                  def_var_out(uint3, inDispatchThreadID, SV_DispatchThreadID)
#define def_var_groupID()                           def_var_out(uint3, inGroupID, SV_GroupID)
#define def_var_groupIndex()                        def_var_out(uint, inGroupIndex, SV_GroupIndex)
#define def_var_groupThreadID()                     def_var_out(uint3, inGroupThreadID, SV_GroupThreadID)

#define groupShared                                 groupshared

#define r32f                                        float
#define rg32f                                       float2
#define rgba32f                                     float4
#define r16f                                        half
#define rg16f                                       half2
#define rgba16f                                     half4
#define r8                                          float
#define rg8                                         float2
#define rgba8                                       float4
#define r16                                         half
#define rg16                                        half2
#define rgba16                                      half4    

#define imageStore(tex, coord, value)               tex[coord] = value
#define imageLoad(tex, coord)                       tex[coord]

#define def_structuredBuffer(type, name, idx)       StructuredBuffer<type> name : register(t##idx)
#define def_rwStructuredBuffer(type, name, idx)     RWStructuredBuffer<type> name : register(u##idx)
#define def_rwTexture(type, dims, name, idx)        RWTexture##dims<type> name : register(u##idx)
#define def_roBuffer(type, name, idx)               Buffer<type> name : register(t##idx)
#define def_rwBuffer(type, name, idx)               RWBuffer<type> name : register(u##idx)
#define def_rwTextureArray(type, dims, name, idx)   RWTexture##dims##Array<type> name : register(u##idx)

#define groupMemoryBarrierWithGroupSync             GroupMemoryBarrierWithGroupSync
#define groupMemoryBarrier                          GroupMemoryBarrier
#define groupMemoryBarrierAll                       AllMemoryBarrierWithGroupSync
#define kAtomicAdd(mem, data, orig)                 InterlockedAdd(mem, data, orig)

#define in_point                                    point
#define in_line                                     line
#define in_triangles                                triangle
#define in_lineadj                                  lineadj
#define triangleadj                                 triangleadj

#define out_points                                  PointStream
#define out_lines                                   LineStream
#define out_triangles                               TriangleStream

#define emitVertex(outName)                         outputStream.Append(outName)
#define endPrimitive(outName)                       outputStream.RestartStrip()
