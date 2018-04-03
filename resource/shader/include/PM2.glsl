#ifndef _PM2_
#define _PM2_

#extension GL_ARB_gpu_shader_int64 : require
//#extension GL_NV_shader_atomic_float : require
//#extension GL_NV_shader_atomic_int64 : require

#define debug() for(int i = 0; i < 100000000; i++)
#define heavy_loop(_loop) for(int i = 0; i < _loop; i++)
struct PMInfo
{
	ivec4 m_resolution;
	ivec4 m_fragment_hierarchy_offset[2];
	ivec4 m_fragment_map_hierarchy_offset[2];
	ivec4 m_emission_buffer_size;
	ivec4 m_emission_buffer_offset;
};
struct Fragment
{
	vec3 albedo;
};
struct Emission 
{
	vec4 value;
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
layout(std430, set=USE_PM, binding=10) restrict buffer EmissionCounter {
	ivec4 b_emission_counter[];
};
layout(std430, set=USE_PM, binding=11) restrict buffer EmissionBuffer {
	Emission b_emission[];
};
layout(std430, set=USE_PM, binding=12) restrict buffer EmissionListBuffer {
	int b_emission_list[];
};
layout(std430, set=USE_PM, binding=13) restrict buffer EmissionMapBuffer {
	int b_emission_map[];
};

layout(std430, set=USE_PM, binding=20) restrict buffer ColorBuffer {
	uvec4 b_color[];
};
layout (set=USE_PM, binding=21, r32ui) uniform uimage2DArray t_color[4];
layout (set=USE_PM, binding=30) uniform sampler2DArray s_color[4];

#define getFragmentHierarchyOffset(_i) (u_pm_info.m_fragment_hierarchy_offset[((_i)-1)/4][((_i)-1)%4])
#define getFragmentMapHierarchyOffset(_i) (u_pm_info.m_fragment_map_hierarchy_offset[((_i)-1)/4][((_i)-1)%4])

#endif

#endif //_PM_