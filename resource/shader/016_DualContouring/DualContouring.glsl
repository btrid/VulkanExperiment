#ifndef LDC_H_
#define LDC_H_

//#extension SPV_KHR_ray_tracing : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

#define Voxel_Reso uvec3(256)
#define Voxel_Block_Size vec3(512.)

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
	int next;
	uint flag;
};
#define LDCFlag_Incident 1
#define LDCFlag_Exit 2
struct DCCell
{
	uvec3 normal;
//	u8vec3 dist;
//	uint8_t axis;
	uint axis_dist;
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


#if defined(USE_Model)
layout(set=USE_Model,binding=0, std140) uniform InfoUniform {Info u_info; };
layout(set=USE_Model,binding=1, scalar) buffer Vertices { vec3 b_vertex[]; };
layout(set=USE_Model,binding=2, scalar) buffer Normals { vec3 b_normal[]; };
layout(set=USE_Model,binding=3, scalar) buffer Indices { uvec3 b_index[]; };
layout(set=USE_Model,binding=4, scalar) buffer DrawCmd { VkDrawIndexedIndirectCommand b_draw_cmd[]; };
layout(set=USE_Model,binding=5) uniform accelerationStructureEXT topLevelAS;
#endif

#if defined(USE_DC)
layout(set=USE_DC,binding=0, std430) buffer LDCCounter { int b_ldc_counter; };
layout(set=USE_DC,binding=1, std430) buffer LDCPointLinkHead { int b_ldc_point_link_head[]; };
layout(set=USE_DC,binding=2, scalar) buffer LDCPointBuffer { LDCPoint b_ldc_point[]; };
layout(set=USE_DC,binding=3, scalar) buffer DCVertex { u8vec4 b_dc_vertex[]; };
layout(set=USE_DC,binding=4, scalar) buffer DCIndexCounter { VkDrawIndirectCommand b_dc_index_counter; };
layout(set=USE_DC,binding=5, scalar) buffer DCIndexBuffer { u8vec4 b_dc_index[]; };
#endif

#if defined(USE_DC_Boolean)
layout(set=USE_DC_Boolean,binding=0, std430) buffer LDCCounter_B { int b_ldc_counter_b; };
layout(set=USE_DC_Boolean,binding=1, std430) buffer LDCPointLinkHead_B { int b_ldc_point_link_head_b[]; };
layout(set=USE_DC_Boolean,binding=2, scalar) buffer LDCPointBuffer_B { LDCPoint b_ldc_point_b[]; };
layout(set=USE_DC_Boolean,binding=3, scalar) buffer DCVertex_B { u8vec4 b_dc_vertex_b[]; };
layout(set=USE_DC_Boolean,binding=4, scalar) buffer DCIndexCounter_B { VkDrawIndirectCommand b_dc_index_counter_b; };
layout(set=USE_DC_Boolean,binding=5, scalar) buffer DCIndexBuffer_B { u8vec4 b_dc_index_b[]; };
#endif

#if defined(USE_DC_WorkBuffer)
layout(set=USE_DC_WorkBuffer,binding=0, scalar) buffer DCCellBuffer { DCCell b_dc_cell[]; };
layout(set=USE_DC_WorkBuffer,binding=1, scalar) buffer DCCellHashmap { uint b_dc_cell_hashmap[]; };
#endif

#if defined(USE_Rendering)
layout(set=USE_Rendering,binding=0) uniform sampler2D s_albedo[100];
#endif


// http://jcgt.org/published/0003/02/01/paper.pdf
// https://discourse.panda3d.org/t/glsl-octahedral-normal-packing/15233
vec2 sign_not_zero(in vec2 v) 
{
    return fma(step(vec2(0.0), v), vec2(2.0), vec2(-1.0));
}
uint pack_normal_octahedron(in vec3 v)
{
	v = vec3(v.xy / dot(abs(v), vec3(1)), v.z);
	return packHalf2x16(mix(v.xy, (1.0 - abs(v.yx)) * sign_not_zero(v.xy), step(v.z, 0.0)));

}
vec3 unpack_normal_octahedron(in uint packed_nrm)
{
	vec2 nrm = unpackHalf2x16(packed_nrm);
	vec3 v = vec3(nrm.xy, 1.0 - abs(nrm.x) - abs(nrm.y));
	v.xy = mix(v.xy, (1.0 - abs(v.yx)) * sign_not_zero(v.xy), step(v.z, 0.));
	return normalize(v);
}
#define pack_normal(_v) pack_normal_octahedron(_v)
#define unpack_normal(_v) unpack_normal_octahedron(_v)
#endif // LDC_H_

