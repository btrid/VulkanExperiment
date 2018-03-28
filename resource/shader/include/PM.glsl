#ifndef _PM_
#define _PM_

#extension GL_ARB_gpu_shader_int64 : require
//#extension GL_NV_shader_atomic_int64 : require
struct PMInfo
{
	mat4 m_camera_PV;
	uvec2 m_resolution;
	uvec2 m_emission_tile_size;
	uvec2 m_emission_tile_num;
	uvec2 _p;
	vec4 m_position;
	int m_emission_tile_map_max;
};
struct Fragment
{
	vec3 albedo;
};
struct Emission 
{
	vec4 pos;
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
	uint64_t b_fragment_hierarchy[];
};
layout(std430, set=USE_PM, binding=10) restrict buffer EmissiveCounter {
	ivec3 b_emission_counter;
};
layout(std430, set=USE_PM, binding=11) restrict buffer EmissiveBuffer {
	Emission b_emission[];
};
layout(std430, set=USE_PM, binding=12) restrict buffer EmissiveTileCounter {
	int b_emission_tile_counter[];
};
layout(std430, set=USE_PM, binding=13) restrict buffer EmissiveTileMapBuffer {
	int b_emission_tile_map[];
};
layout(std430, set=USE_PM, binding=20) restrict buffer ColorBuffer {
	vec4 b_color[];
};

#define culling_light_power() (0.001)
#define light_area() (33.)
#endif

#endif //_PM_