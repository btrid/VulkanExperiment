#ifndef _PM_
#define _PM_

#extension GL_ARB_gpu_shader_int64 : require
//#extension GL_NV_shader_atomic_int64 : require
struct PMInfo
{
	mat4 m_camera_PV;
	ivec2 m_resolution;
	uvec2 m_emission_tile_size;
	uvec2 m_emission_tile_num;
	uvec2 _p;
	vec4 m_position;
	ivec4 m_fragment_hierarchy_offset[2];
	ivec4 m_fragment_map_hierarchy_offset[2];
	ivec4 m_emission_buffer_size;
	ivec4 m_emission_buffer_offset;
	int m_emission_tile_linklist_max;
	int m_sdf_work_num;
};
struct Fragment
{
	vec3 albedo;
};
struct Emission 
{
	vec4 value;
};
struct LinkList
{
	int next;
	int target;
};

struct SDFWork
{
	uint map_index;
	uint hierarchy;
	uint fragment_idx;
	uint _p;
};

#ifdef USE_PM
layout(std140, set=USE_PM, binding=0) uniform PMInfoUniform {
	PMInfo u_pm_info;
};
layout(std430, set=USE_PM, binding=1) restrict buffer FragmentBuffer {
	Fragment b_fragment[];
};
layout(std430, set=USE_PM, binding=2) restrict buffer FragmentHierarchyBuffer {
	uint64_t b_fragment_map[];
};
layout(std430, set=USE_PM, binding=3) restrict buffer FragmentHierarchyMapBuffer {
	int b_fragment_hierarchy[];
};
layout(std430, set=USE_PM, binding=4) restrict buffer SDFWorkerBuffer {
	SDFWork b_sdf_work[];
};
layout(std430, set=USE_PM, binding=5) restrict buffer SDFCounter {
//	ivec4 b_sdf_counter[];
	int b_sdf_counter[];
};
layout(std430, set=USE_PM, binding=6) restrict buffer SignedDistanceFieldBuffer {
	uint b_sdf[]; // atomic_floatがないのでuint.単位はmm
};

layout(std430, set=USE_PM, binding=10) restrict buffer EmissiveCounter {
	ivec4 b_emission_counter[];
};
layout(std430, set=USE_PM, binding=11) restrict buffer EmissiveBuffer {
	Emission b_emission[];
};
layout(std430, set=USE_PM, binding=12) restrict buffer EmissiveListBuffer {
	int b_emission_list[];
};
layout(std430, set=USE_PM, binding=13) restrict buffer EmissiveMapBuffer {
	int b_emission_map[];
};
layout(std430, set=USE_PM, binding=14) restrict buffer EmissiveTileLinkListCounter {
	int b_emission_tile_counter;
};
layout(std430, set=USE_PM, binding=15) restrict buffer EmissiveTileLinkHeadBuffer {
	int b_emission_tile_linkhead[];
};
layout(std430, set=USE_PM, binding=16) restrict buffer EmissiveTileLinkListBuffer {
	LinkList b_emission_tile_linklist[];
};
layout (set=USE_PM, binding=20, r32ui) uniform uimage2DArray t_color[4];
layout (set=USE_PM, binding=30) uniform sampler2DArray s_color[4];

#define getFragmentHierarchyOffset(_i) (u_pm_info.m_fragment_hierarchy_offset[(_i)/4][(_i)%4])
#define getFragmentMapHierarchyOffset(_i) (u_pm_info.m_fragment_map_hierarchy_offset[(_i)/4][(_i)%4])
#define culling_light_power() (0.001)
#define light_area() (33.)
#endif

#endif //_PM_