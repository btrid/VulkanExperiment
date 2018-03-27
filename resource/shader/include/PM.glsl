#ifndef _PM_
#define _PM_

#extension GL_ARB_gpu_shader_int64 : require
//#extension GL_NV_shader_atomic_float : require
//#extension GL_NV_shader_atomic_int64 : require
struct PMInfo
{
	mat4 m_camera_PV;
	uvec2 m_resolution;
	uvec2 m_emission_tile_size;
	uvec2 m_emission_tile_num;
	uvec2 _p;
	vec4 m_position;
	ivec4 m_emission_buffer_size;
	ivec4 m_emission_buffer_offset;
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
	ivec4 b_emission_counter[];
};
layout(std430, set=USE_PM, binding=11) restrict buffer EmissiveBuffer {
	Emission b_emission[];
};
/*layout(std430, set=USE_PM, binding=10) restrict buffer BounceCounter {
	ivec4 b_bounce_counter[];
};
layout(std430, set=USE_PM, binding=11) restrict buffer BounceBuffer {
	Emission b_bounce[];
};
layout(std430, set=USE_PM, binding=11) restrict buffer BounceMapBuffer {
	int b_bounce_map[];
};
*/
layout(std430, set=USE_PM, binding=20) restrict buffer ColorBuffer {
	uvec4 b_color[];
};
layout (set=USE_PM, binding=21, r32ui) uniform uimage2DArray t_color[5];
layout (set=USE_PM, binding=30) uniform usampler2DArray s_color[5];

#endif

#endif //_PM_