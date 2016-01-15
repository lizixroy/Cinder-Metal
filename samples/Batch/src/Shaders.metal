//
//  Shaders.metal
//  MetalTemplate
//
//  Created by William Lindmeier on 11/26/15.
//
//

#include <metal_stdlib>
#include <simd/simd.h>
#include "MetalConstants.h"
#include "SharedTypes.h"

using namespace metal;

using namespace metal;
using namespace cinder::mtl;

// Variables in constant address space
constant float3 light_position = float3(0.0, 1.0, -1.0);
//constant float4 ambient_color_blue = float4(0.18, 0.24, 0.8, 1.0);
//constant float4 ambient_color_green = float4(0.24, 0.8, 0.18, 1.0);
//constant float4 diffuse_color  = float4(0.4, 0.4, 1.0, 1.0);
// NOTE: samplers defined in the shader don't appear to have an anisotropy param
constexpr sampler shaderSampler( coord::normalized, // normalized (0-1) or coord::pixel (0-width,height)
                                 address::repeat, // repeat, clamp_to_zero, clamp_to_edge,
                                 filter::linear, // nearest or linear
                                 mip_filter::linear ); // nearest or linear or none

typedef struct
{
    packed_float3 ciPosition;
    packed_float3 ciNormal;
} InterleavedVertex;

typedef struct
{
    float4 position [[position]];
    float4 color;
    float2 texCoords;
} VertOut;

// Vertex shader function
//vertex ColorInOut lighting_vertex_interleaved( device const InterleavedVertex* vertex_array [[ buffer(ciBufferIndexInterleavedVerts) ]],
//                                               constant ciUniforms_t& uniforms [[ buffer(ciBufferIndexUniforms) ]],
//                                               unsigned int vid [[ vertex_id ]] )
//{
//    ColorInOut out;
//    
//    float3 offsetPosition = vertex_array[vid].ciPosition + float3(0,0,1.5);
//    float4 in_position = float4(offsetPosition, 1.0);
//    out.position = uniforms.ciModelViewProjectionMatrix * in_position;
//    
//    float3 normal = vertex_array[vid].ciNormal;
//    float4 eye_normal = normalize(uniforms.ciNormalMatrix * float4(normal, 0.0));
//    float n_dot_l = dot(eye_normal.rgb, normalize(light_position));
//    n_dot_l = fmax(0.0, n_dot_l);
//    
//    out.color = ambient_color_green + diffuse_color * n_dot_l;
//    
//    return out;
//}

// Vertex Bhader using an interleaved geom::Source
// CubeVertex is found in SharedData.h
vertex VertOut lighting_vertex_interleaved_src( device const CubeVertex* verts [[ buffer(ciBufferIndexInterleavedVerts) ]],
                                                device const uint* indices [[ buffer(ciBufferIndexIndicies) ]],
                                                constant ciUniforms_t& uniforms [[ buffer(ciBufferIndexUniforms) ]],
                                                unsigned int vid [[ vertex_id ]] )
{
    VertOut out;
    
    const uint idx = indices[vid];
    CubeVertex vert = verts[idx];
    float4 in_position = float4(vert.ciPosition, 1.0);
    out.position = uniforms.ciModelViewProjectionMatrix * in_position;
    
    float4 eye_normal = normalize(uniforms.ciNormalMatrix * float4(vert.ciNormal, 0.0));
    float n_dot_l = dot(eye_normal.rgb, normalize(light_position));
    n_dot_l = fmax(0.0, n_dot_l);
    
    out.texCoords = vert.ciTexCoord0;
    out.color = n_dot_l;
    
    return out;
}

//// Vertex Buffer using attrib buffers
//vertex ColorInOut lighting_vertex_attrib_buffers( device const packed_float3* positions [[ buffer(ciBufferIndexPositions) ]],
//                                                  device const packed_float3* normals [[ buffer(ciBufferIndexNormals) ]],
//                                                  constant ciUniforms_t& uniforms [[ buffer(ciBufferIndexUniforms) ]],
//                                                  unsigned int vid [[ vertex_id ]] )
//{
//    ColorInOut out;
//    
//    float4 in_position = float4(positions[vid], 1.0);
//    out.position = uniforms.ciModelViewProjectionMatrix * in_position;
//    
//    float3 normal = normals[vid];
//    float4 eye_normal = normalize(uniforms.ciNormalMatrix * float4(normal, 0.0));
//    float n_dot_l = dot(eye_normal.rgb, normalize(light_position));
//    n_dot_l = fmax(0.0, n_dot_l);
//    
//    out.color = ambient_color_blue + diffuse_color * n_dot_l;
//    
//    return out;
//}

// Fragment shader function
fragment float4 lighting_texture_fragment( VertOut in [[ stage_in ]],
                                           texture2d<float> textureCube [[ texture(ciTextureIndex0) ]]) //,
                                          //sampler objcSampler [[ sampler(ciSamplerIndex0) ]] )
{
    // Use the shader sampler
    float4 texColor = textureCube.sample(shaderSampler, in.texCoords);
    // Use the sampler passed in from the app:
    //float4 texColor = textureCube.sample(objcSampler, in.texCoords);
    return float4(texColor.rgb * in.color.rgb, texColor.a);
}

//fragment float4 lighting_fragment( ColorInOut in [[stage_in]] )
//{
//    return in.color;
//}

