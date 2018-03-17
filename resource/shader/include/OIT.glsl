#ifndef _OIT_
#define _OIT_

struct OITInfo
{
	mat4 m_camera_PV;
	uvec3 m_resolution;
	vec3 m_position;
};
struct Fragment
{
	vec3 albedo;
};

#ifdef USE_OIT
layout(std140, set=USE_OIT, binding=0) uniform OITInfoUniform {
	OITInfo u_OIT_info;
};
layout(std430, set=USE_OIT, binding=1) restrict buffer FragmentCounter {
	int b_fragment_counter;
};
layout(std430, set=USE_OIT, binding=2) restrict buffer FragmentBuffer {
	Fragment b_fragment[];
};
layout(std430, set=USE_OIT, binding=3) restrict buffer FragmentMapBuffer {
	int b_fragment_map[];
};
layout(std430, set=USE_OIT, binding=10) restrict buffer EmissiveCounter {
	ivec3 b_emissive_counter[];
};
layout(std430, set=USE_OIT, binding=11) restrict buffer EmissiveBuffer {
	vec3 b_emissive[];
};
layout(std430, set=USE_OIT, binding=12) restrict buffer EmissiveMapBuffer {
	int b_emissive_map[];
};
layout(std430, set=USE_OIT, binding=20) restrict buffer ColorBuffer {
	vec4 b_color[];
};

#endif

#endif //_OIT_