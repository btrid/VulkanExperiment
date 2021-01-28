#ifndef LDC_H_
#define LDC_H_

#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

#define Voxel_Reso uvec3(256)
#define Voxel_Block_Size vec3(512.)
#define Voxel_Size (Voxel_Block_Size/Voxel_Reso)

struct Info
{
	vec4 m_aabb_min;
	vec4 m_aabb_max;
	uint m_vertex_num;
};
struct LDCPoint
{
	float p;
	uint16_t flag;
	uint16_t normal;
};
LDCPoint g_invalid_point = {999999., uint16_t(0xffff), -1us};
bool is_valid(in LDCPoint p){ return p.flag!=g_invalid_point.flag; }
#define LDCFlag_Incident 1s
#define LDCFlag_Exit 2s
struct DCCell
{
	uint axis_dist;
	u16vec3 normal;
	uint16_t flag;
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

struct LDCInfo
{
	uvec4 m_voxel_reso;
	vec4 m_voxel_size;
	int m_ldc_point_num;
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
layout(set=USE_DC,binding=0, std430) uniform LDCInfo_ { LDCInfo u_LDCModel_info; };
layout(set=USE_DC,binding=1, std430) buffer LDCCounter { int b_ldc_active_counter; int b_ldc_free_counter; };
layout(set=USE_DC,binding=2, scalar) buffer LDCPointLinkHead { int b_ldc_point_link_head[]; };
layout(set=USE_DC,binding=3, scalar) buffer LDCPointLinkNext { int b_ldc_point_link_next[]; };
layout(set=USE_DC,binding=4, scalar) buffer LDCPointBuffer { LDCPoint b_ldc_point[]; };
layout(set=USE_DC,binding=5, scalar) buffer LDCPointFreeBuffer { int b_ldc_point_free[]; };
layout(set=USE_DC,binding=6, scalar) buffer DCVertex { u8vec4 b_dc_vertex[]; };
layout(set=USE_DC,binding=7, scalar) buffer DCIndexCounter { VkDrawIndirectCommand b_dc_index_counter; };
layout(set=USE_DC,binding=8, scalar) buffer DCIndexBuffer { u8vec4 b_dc_index[]; };

int allocate_ldc_point()
{
	int index = atomicAdd(b_ldc_active_counter, 1);
	if(index==u_LDCModel_info.m_ldc_point_num)
	{
		atomicAdd(b_ldc_active_counter, -u_LDCModel_info.m_ldc_point_num);
	}
	index %= u_LDCModel_info.m_ldc_point_num;
	int free_index = b_ldc_point_free[index];
	return free_index < 0 ? index : free_index;
}

void free_ldc_point(in int i)
{
	int index = atomicAdd(b_ldc_free_counter, 1);
	if(index==u_LDCModel_info.m_ldc_point_num)
	{
		atomicAdd(b_ldc_free_counter, -u_LDCModel_info.m_ldc_point_num);
	}
	b_ldc_point_free[index%u_LDCModel_info.m_ldc_point_num] = i;
}

#endif

#if defined(USE_DC_WorkBuffer)
layout(set=USE_DC_WorkBuffer,binding=0, scalar) buffer DCCellBuffer { DCCell b_dc_cell[]; };
layout(set=USE_DC_WorkBuffer,binding=1, scalar) buffer DCCellHashmap { uint b_dc_cell_hashmap[]; };
layout(set=USE_DC_WorkBuffer,binding=2, scalar) buffer DCCellCounter { uvec4 b_dc_cell_counter; };
layout(set=USE_DC_WorkBuffer,binding=3, scalar) buffer DCCellList { u8vec4 b_dc_cell_list[]; };
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
uint16_t pack_normal_octahedron(in vec3 v)
{
	v = vec3(v.xy / dot(abs(v), vec3(1)), v.z);
	v.xy = mix(v.xy, (1.0 - abs(v.yx)) * sign_not_zero(v.xy), step(v.z, 0.0))*vec2(127.)+127.;
	ivec2 i = ivec2(round(clamp(v.xy, 0., 255.)));
	return uint16_t(i.x | (i.y<<8));

}
vec3 unpack_normal_octahedron(in uint16_t packed_nrm)
{
	vec2 nrm = vec2((packed_nrm.xx>>uvec2(0,8))&0xff) -127.;
	nrm = clamp(nrm / 127.0, -1.0, 1.0);
	vec3 v = vec3(nrm.xy, 1.0 - abs(nrm.x) - abs(nrm.y));
	v.xy = mix(v.xy, (1.0 - abs(v.yx)) * sign_not_zero(v.xy), step(v.z, 0.));
	return normalize(v);
}
#define pack_normal(_v) pack_normal_octahedron(_v)
#define unpack_normal(_v) unpack_normal_octahedron(_v)
#endif // LDC_H_

