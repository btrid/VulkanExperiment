
#version 450

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shading_language_include : enable

#include "config.h"
#extension GL_NV_mesh_shader : require

// one of them provides uint8_t
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_NV_gpu_shader5 : enable
  
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_vote : require

//////////////////////////////////////

#include "common.h"

//////////////////////////////////////////////////
// MESH CONFIG

#define GROUP_SIZE    WARP_SIZE

layout(local_size_x=GROUP_SIZE) in;
layout(max_vertices=NVMESHLET_VERTEX_COUNT, max_primitives=NVMESHLET_PRIMITIVE_COUNT) out;
layout(triangles) out;

// do primitive culling in the shader, output reduced amount of primitives
#ifndef USE_MESH_SHADERCULL
#define USE_MESH_SHADERCULL     0
#endif

// no cull-before-fetch, always load all attributes
#ifndef USE_EARLY_ATTRIBUTES
#define USE_EARLY_ATTRIBUTES    1
#endif

// task shader is used in advance, doing early cluster culling
#ifndef USE_TASK_STAGE
#define USE_TASK_STAGE          1
#endif

// get compiler to do batch loads
#ifndef USE_BATCHED_LATE_FETCH
#define USE_BATCHED_LATE_FETCH  1
#endif


/////////////////////////////////////
// UNIFORMS
#if USE_PER_GEOMETRY_VIEWS
  uvec4 geometryOffsets = uvec4(0, 0, 0, 0);
#else
  layout(push_constant) uniform pushConstant{
    uvec4     geometryOffsets;
    // x: mesh, y: prim, z: index, w: vertex
  };
#endif

layout(std140, binding = SCENE_UBO_VIEW, set = DSET_SCENE) uniform sceneBuffer {
  SceneData scene;
};
layout(std430, binding = SCENE_SSBO_STATS, set = DSET_SCENE) buffer statsBuffer {
  CullStats stats;
};

layout(std140, binding= 0, set = DSET_OBJECT) uniform objectBuffer {
  ObjectData object;
};

layout(std430, binding = GEOMETRY_SSBO_MESHLETDESC, set = DSET_GEOMETRY) buffer meshletDescBuffer {
  uvec4 meshletDescs[];
};
layout(std430, binding = GEOMETRY_SSBO_PRIM, set = DSET_GEOMETRY) buffer primIndexBuffer1 {
  uint  primIndices1[];
};
layout(std430, binding = GEOMETRY_SSBO_PRIM, set = DSET_GEOMETRY) buffer primIndexBuffer2 {
  uvec2 primIndices2[];
};

layout(binding=GEOMETRY_TEX_IBO,  set=DSET_GEOMETRY)  uniform usamplerBuffer texIbo;
layout(binding=GEOMETRY_TEX_VBO,  set=DSET_GEOMETRY)  uniform samplerBuffer  texVbo;
layout(binding=GEOMETRY_TEX_ABO,  set=DSET_GEOMETRY)  uniform samplerBuffer  texAbo;

/////////////////////////////////////////////////

#include "nvmeshlet_utils.glsl"

/////////////////////////////////////////////////
// MESH INPUT

  taskNV in Task {
    uint    baseID;
    uint8_t subIDs[GROUP_SIZE];
  } IN;
  // gl_WorkGroupID.x runs from [0 .. parentTask.gl_TaskCountNV - 1]
  uint meshletID = IN.baseID + IN.subIDs[gl_WorkGroupID.x];
  uint laneID = gl_LocalInvocationID.x;


////////////////////////////////////////////////////////////
// INPUT

// If you work from fixed vertex definitions and don't need dynamic 
// format conversions by texture formats, or don't mind
// creating multiple shader permutations, you may want to
// use ssbos here, instead of tbos for a bit more performance.

vec3 getPosition( uint vidx ){
  return texelFetch(texVbo, int(vidx)).xyz;
}

vec3 getNormal( uint vidx ){
  return texelFetch(texAbo, int(vidx * NORMAL_STRIDE)).xyz;
}

vec4 getExtra( uint vidx, uint xtra ){
  return texelFetch(texAbo, int(vidx * NORMAL_STRIDE + 1 + xtra));
}

////////////////////////////////////////////////////////////
// OUTPUT

layout(location=0) out Interpolants {
  vec3  wPos;
  float dummy;
  vec3  wNormal;
  flat uint meshletID;
#if EXTRA_ATTRIBUTES
  vec4 xtra[EXTRA_ATTRIBUTES];
#endif
} OUT[];

//////////////////////////////////////////////////
// EXECUTION

vec4 procVertex(const uint vert, uint vidx)
{
  vec3 oPos = getPosition(vidx);
  vec3 wPos = (object.worldMatrix  * vec4(oPos,1)).xyz;
  vec4 hPos = (scene.viewProjMatrix * vec4(wPos,1));
  
  gl_MeshVerticesNV[vert].gl_Position = hPos;
  
  OUT[vert].wPos = wPos;
  OUT[vert].dummy = 0;
  OUT[vert].meshletID = meshletID;
  
#if USE_CLIPPING
  // spir-v annoyance, doesn't unroll the loop and therefore cannot derive the number of clip distances used
  gl_MeshVerticesNV[vert].gl_ClipDistance[0] = dot(scene.wClipPlanes[0], vec4(wPos,1));
  gl_MeshVerticesNV[vert].gl_ClipDistance[1] = dot(scene.wClipPlanes[1], vec4(wPos,1));
  gl_MeshVerticesNV[vert].gl_ClipDistance[2] = dot(scene.wClipPlanes[2], vec4(wPos,1));
#endif
  
  return hPos;
}

// To benefit from batched loading, and reduce latency 
// let's make use of a dedicated load phase.
// (explained at the end of the file in the USE_BATCHED_LATE_FETCH section)

#if USE_BATCHED_LATE_FETCH
  struct TempAttributes {
    vec3 normal;
  #if EXTRA_ATTRIBUTES
    vec4 xtra[EXTRA_ATTRIBUTES];
  #endif
  };

  void fetchAttributes(inout TempAttributes temp, uint vert, uint vidx)
  {
    temp.normal = getNormal(vidx);
  #if EXTRA_ATTRIBUTES
    for (int i = 0; i < EXTRA_ATTRIBUTES; i++){
      temp.xtra[i] = getExtra(vidx, i);
    }
  #endif
  }

  void storeAttributes(inout TempAttributes temp, const uint vert, uint vidx)
  {
    vec3 oNormal = temp.normal;
    vec3 wNormal = mat3(object.worldMatrixIT) * oNormal;
    OUT[vert].wNormal = wNormal;
  #if EXTRA_ATTRIBUTES
    for (int i = 0; i < EXTRA_ATTRIBUTES; i++){
      OUT[vert].xtra[i] = temp.xtra[i];
    }
  #endif
  }

  void procAttributes(const uint vert, uint vidx)
  {
    TempAttributes  temp;
    fetchAttributes(temp, vert, vidx);
    storeAttributes(temp, vert, vidx);
  }

#else
  
  // if you never intend to use the above mechanism,
  // you can express the attribute processing more like a regular
  // vertex shader

  void procAttributes(const uint vert, uint vidx)
  {
    vec3 oNormal = getNormal(vidx);
    vec3 wNormal = mat3(object.worldMatrixIT) * oNormal;
    OUT[vert].wNormal = wNormal;
  #if EXTRA_ATTRIBUTES
    for (int i = 0; i < EXTRA_ATTRIBUTES; i++) {
      vec4 xtra = getExtra(vidx, i);
      OUT[vert].xtra[i] = xtra;
    }
  #endif
  }
#endif

//////////////////////////////////////////////////
// MESH EXECUTION

  uint getVertexClip(uint vert) {
    return getCullBits(gl_MeshVerticesNV[vert].gl_Position);
  }
  vec2 getVertexScreen(uint vert) {
    return getScreenPos(gl_MeshVerticesNV[vert].gl_Position);
  }

  #define NVMSH_BARRIER() \
    memoryBarrierShared(); \
    barrier();
    
  #define NVMSH_INDEX_BITS      8
  #define NVMSH_PACKED4X8_GET(packed, idx)   (((packed) >> (NVMSH_INDEX_BITS * (idx))) & 255)
  
  
  // only for tight packing case, 8 indices are loaded per thread
  #define NVMSH_PRIMITIVE_INDICES_RUNS  ((NVMESHLET_PRIMITIVE_COUNT * 3 + GROUP_SIZE * 8 - 1) / (GROUP_SIZE * 8))

  // processing loops
  #define NVMSH_VERTEX_RUNS     ((NVMESHLET_VERTEX_COUNT + GROUP_SIZE - 1) / GROUP_SIZE)
  #define NVMSH_PRIMITIVE_RUNS  ((NVMESHLET_PRIMITIVE_COUNT + GROUP_SIZE - 1) / GROUP_SIZE)
  
#if 1
  #define nvmsh_writePackedPrimitiveIndices4x8NV writePackedPrimitiveIndices4x8NV
#else
  #define nvmsh_writePackedPrimitiveIndices4x8NV(idx, topology) {\
        gl_PrimitiveIndicesNV[ (idx) + 0 ] = (NVMSH_PACKED4X8_GET((topology), 0)); \
        gl_PrimitiveIndicesNV[ (idx) + 1 ] = (NVMSH_PACKED4X8_GET((topology), 1)); \
        gl_PrimitiveIndicesNV[ (idx) + 2 ] = (NVMSH_PACKED4X8_GET((topology), 2)); \
        gl_PrimitiveIndicesNV[ (idx) + 3 ] = (NVMSH_PACKED4X8_GET((topology), 3));} 
#endif
  
///////////////////////////////////////////////////////////////////////////////

void main()
{
  // decode meshletDesc
  uvec4 desc = meshletDescs[meshletID + geometryOffsets.x];
  uint vertMax;
  uint primMax;
  uint vertBegin;
  uint primBegin;
  decodeMeshlet(desc, vertMax, primMax, vertBegin, primBegin);

  vertBegin += geometryOffsets.z;
  primBegin += geometryOffsets.y;


  uint primCount = primMax + 1;
  uint vertCount = vertMax + 1;
  

  // LOAD PHASE
  // VERTEX PROCESSING
  for (uint i = 0; i < uint(NVMSH_VERTEX_RUNS); i++) 
  {
    
    uint vert = laneID + i * GROUP_SIZE;
    
    // Use "min" to avoid branching
    // this ensures the compiler can batch loads
    // prior writes/processing
    //
    // Most of the time we will have fully saturated vertex utilization,
    // but we may compute the last vertex redundantly.
    {
      uint vidx = texelFetch(texIbo, int(vertBegin + min(vert,vertMax))).x;
      
      vidx += geometryOffsets.w;
      
      vec4 hPos = procVertex(vert, vidx);
    
      procAttributes(vert, vidx);
    }
  }
  // PRIMITIVE TOPOLOGY
  // 
  // FITTED_UINT8 gives fastest code and best bandwidth usage, at the sacrifice of
  // not maximizing actual primCount (primCount within meshlet may be always smaller than NVMESHLET_MAX_PRIMITIVES)

  // each run does read 8 single byte indices per thread
  // the number of primCount was clamped in such fashion in advance
  // that it's guaranteed that gl_PrimitiveIndicesNV is sized big enough to allow the full 32-bit writes
  {
    uint readBegin = primBegin / 8;
    uint readIndex = primCount * 3 - 1;
    uint readMax = readIndex / 8;

    for (uint i = 0; i < uint(NVMSH_PRIMITIVE_INDICES_RUNS); i++)
    {
      uint read = laneID + i * GROUP_SIZE;
      uint readUsed = min(read, readMax);
      uvec2 topology = primIndices2[readBegin + readUsed];
      nvmsh_writePackedPrimitiveIndices4x8NV(readUsed * 8 + 0, topology.x);
      nvmsh_writePackedPrimitiveIndices4x8NV(readUsed * 8 + 4, topology.y);
    }
  }

  if (laneID == 0) 
  {
    gl_PrimitiveCountNV = primCount;
  #if USE_STATS
    atomicAdd(stats.meshletsOutput, 1);
    atomicAdd(stats.trisOutput, primCount);
    atomicAdd(stats.attrInput,  vertCount);
    atomicAdd(stats.attrOutput, vertCount);
  #endif
  }
  uint outTriangles = 0;
  
  NVMSH_BARRIER();
  
  // PRIMITIVE PHASE
 
  const uint primRuns = (primCount + GROUP_SIZE - 1) / GROUP_SIZE;
  for (uint i = 0; i < primRuns; i++) {
    uint triCount = 0;
    uint topology = 0;
    
    uint prim = laneID + i * GROUP_SIZE;
    
    if (prim <= primMax) 
    {
      uint idx = prim * 3;
      uint ia = gl_PrimitiveIndicesNV[idx + 0];
      uint ib = gl_PrimitiveIndicesNV[idx + 1];
      uint ic = gl_PrimitiveIndicesNV[idx + 2];
      topology = ia | (ib << NVMSH_INDEX_BITS) | (ic << (NVMSH_INDEX_BITS*2));
      
      // build triangle
      vec2 a = getVertexScreen(ia);
      vec2 b = getVertexScreen(ib);
      vec2 c = getVertexScreen(ic);

      triCount = testTriangle(a.xy, b.xy, c.xy, 1.0, false);
    }
    
    uvec4 vote = subgroupBallot(triCount == 1);
    uint  tris = subgroupBallotBitCount(vote);
    uint  idxOffset = outTriangles + subgroupBallotExclusiveBitCount(vote);
  
    if (triCount != 0) {
      uint idx = idxOffset * 3;
      gl_PrimitiveIndicesNV[idx + 0] = NVMSH_PACKED4X8_GET(topology, 0);
      gl_PrimitiveIndicesNV[idx + 1] = NVMSH_PACKED4X8_GET(topology, 1);
      gl_PrimitiveIndicesNV[idx + 2] = NVMSH_PACKED4X8_GET(topology, 2);
    }
    
    outTriangles += tris;
  }

  
  NVMSH_BARRIER();
  
  if (laneID == 0) 
  {
    gl_PrimitiveCountNV = outTriangles;
  #if USE_STATS
    atomicAdd(stats.meshletsOutput, 1);
    atomicAdd(stats.trisOutput, outTriangles);
    #if USE_EARLY_ATTRIBUTES
      atomicAdd(stats.attrInput,  vertCount);
      atomicAdd(stats.attrOutput, vertCount);
    #endif
  #endif
  }

#endif // !USE_MESH_SHADERCULL
}
