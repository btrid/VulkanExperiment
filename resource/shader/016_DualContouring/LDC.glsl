#ifndef LDC_H_
#define LDC_H_

#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

#define Voxel_Reso uvec3(64)
struct Info
{
	vec4 m_aabb_min;
	vec4 m_aabb_max;
	uint m_vertex_num;
};
struct LDCPoint
{
	float p;
	uint normal;
	int inout_next;
};
struct LDCCell
{
	uvec3 normal;
	uint useaxis_xyz;
//	uvec4 useaxis_xyz;
//	uvec3 normal;
};

struct VkDrawIndexedIndirectCommand 
{
    uint32_t    indexCount;
    uint32_t    instanceCount;
    uint32_t    firstIndex;
    int32_t     vertexOffset;
    uint32_t    firstInstance;
};
struct VkDrawIndirectCommand {
    uint32_t    vertexCount;
    uint32_t    instanceCount;
    uint32_t    firstVertex;
    uint32_t    firstInstance;
};


#if defined(USE_AS)
layout(set=USE_AS,binding=0) uniform accelerationStructureEXT topLevelAS;
#endif
#if defined(USE_LDC)
layout(set=USE_LDC,binding=0, std140) uniform InfoUniform {Info u_info; };
layout(set=USE_LDC,binding=1, scalar) buffer Vertices { vec3 b_vertex[]; };
layout(set=USE_LDC,binding=2, scalar) buffer Normals { vec3 b_normal[]; };
layout(set=USE_LDC,binding=3, scalar) buffer Indices { uvec3 b_index[]; };
layout(set=USE_LDC,binding=10, std430) buffer LDCCounter { int b_ldc_counter; };
layout(set=USE_LDC,binding=11, std430) buffer LDCPointLinkHead { int b_ldc_point_link_head[]; };
layout(set=USE_LDC,binding=12, scalar) buffer LDCPointBuffer { LDCPoint b_ldc_point[]; };
layout(set=USE_LDC,binding=13, scalar) buffer LDCCellBuffer { LDCCell b_ldc_cell[]; };
layout(set=USE_LDC,binding=14, scalar) buffer DCVVertex { vec3 b_dcv_vertex[]; };
layout(set=USE_LDC,binding=15, scalar) buffer DCVCounter { int b_dcv_counter; };
layout(set=USE_LDC,binding=16, scalar) buffer DCVHashMap { int b_dcv_hashmap[]; };
layout(set=USE_LDC,binding=17, scalar) buffer DCVIndexCounter { VkDrawIndirectCommand b_dcv_index_counter; };
layout(set=USE_LDC,binding=18, scalar) buffer DCVIndexBuffer { uvec3 b_dcv_index[]; };
#endif


// https://discourse.panda3d.org/t/glsl-octahedral-normal-packing/15233
f16vec2 sign_not_zero(in f16vec2 v) 
{
    return fma(step(f16vec2(0.0), v), f16vec2(2.0), f16vec2(-1.0));
}
uint pack_normal_octahedron(in vec3 _v)
{
//	v.xy /= dot(abs(v), vec3(1));
	f16vec3 v = f16vec3(_v.xy / dot(abs(_v), vec3(1)), _v.z);
//	return mix(v.xy, (float16_t(1.0) - abs(v.yx)) * sign_not_zero(v.xy), step(v.z, float16_t(0.0)));
	return packHalf2x16(mix(v.xy, (float16_t(1.0) - abs(v.yx)) * sign_not_zero(v.xy), step(v.z, float16_t(0.0))));

}
vec3 unpack_normal_octahedron(in uint packed_nrm)
{
	vec2 nrm = unpackHalf2x16(packed_nrm);
	f16vec3 v = f16vec3(nrm.xy, float16_t(1.0) - abs(nrm.x) - abs(nrm.y));
	v.xy = mix(v.xy, (float16_t(1.0) - abs(v.yx)) * sign_not_zero(v.xy), step(v.z, float16_t(0)));
	return normalize(v);
}
#endif // LDC_H_

