#ifndef _OIT_
#define _OIT_

#extension GL_ARB_gpu_shader_int64 : require
//#extension GL_NV_shader_atomic_int64 : require
struct OITInfo
{
	mat4 m_camera_PV;
	uvec4 m_resolution;
	vec4 m_position;
	uint m_emissive_tile_map_max;
};
struct Fragment
{
	vec3 albedo;
};
struct Emission
{
	vec3 emissive;
};

#ifdef USE_OIT
layout(std140, set=USE_OIT, binding=0) uniform OITInfoUniform {
	OITInfo u_OIT_info;
};
layout(std430, set=USE_OIT, binding=1) restrict buffer FragmentBuffer {
	Fragment b_fragment[];
};
layout(std430, set=USE_OIT, binding=2) restrict buffer FragmentHierarchyBuffer {
	uint64_t b_fragment_hierarchy[];
};
layout(std430, set=USE_OIT, binding=10) restrict buffer EmissiveCounter {
	ivec3 b_emissive_counter;
};
layout(std430, set=USE_OIT, binding=11) restrict buffer EmissiveBuffer {
	vec3 b_emissive[];
};
layout(std430, set=USE_OIT, binding=12) restrict buffer EmissiveMapBuffer {
	int b_emissive_map[];
};
layout(std430, set=USE_OIT, binding=13) restrict buffer EmissiveTileCounter {
	int b_emissive_tile_counter[];
};
layout(std430, set=USE_OIT, binding=14) restrict buffer EmissiveTileBuffer {
	int b_emissive_tile_map[];
};
layout(std430, set=USE_OIT, binding=20) restrict buffer ColorBuffer {
	vec4 b_color[];
};

#endif

#endif //_OIT_