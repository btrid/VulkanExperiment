#ifndef _SV_
#define _SV_

#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_NV_shader_atomic_int64 : require
#extension GL_ARB_shader_stencil_export : require
#include "btrlib/Common.glsl"

struct SVInfo
{
	mat4 m_camera_PV;
	ivec2 m_resolution;
	uvec2 m_emission_tile_size;
	uvec2 m_emission_tile_num;
	uvec2 _p;
	vec4 m_position;
	int m_emission_tile_linklist_max;
};
struct Fragment
{
	vec3 albedo;
};

struct Emission 
//struct PM2DLightData
{
	vec2 pos;
	float dir; // atan
	float angle;
	vec4 emission;
	int level;
	int _p1;
	int _p2;
	int _p3;
//	int type;
};

struct LinkList
{
	int next;
	uint target;
};

#ifdef USE_SV
layout(std140, set=USE_SV, binding=0) uniform PMInfoUniform {
	SVInfo u_sv_info;
};
layout(std430, set=USE_SV, binding=1) restrict buffer FragmentBuffer {
	Fragment b_fragment[];
};
layout(std430, set=USE_SV, binding=20) restrict buffer EmissiveCounter {
	ivec4 b_emission_counter;
};
layout(std430, set=USE_SV, binding=21) restrict buffer EmissiveBuffer {
	Emission b_emission[];
};
layout(std430, set=USE_SV, binding=22) restrict buffer EmissiveTileLinkListCounter {
	int b_emission_tile_counter;
};
layout(std430, set=USE_SV, binding=23) restrict buffer EmissiveTileLinkHeadBuffer {
	int b_emission_tile_linkhead[];
};
layout(std430, set=USE_SV, binding=24) restrict buffer EmissiveTileLinkListBuffer {
	LinkList b_emission_tile_linklist[];
};
layout(/*std430,*/ set=USE_SV, binding=30) restrict buffer ShadowVolumeBuffer {
	vec2 b_shadow_volume[];
};
layout(std430, set=USE_SV, binding=31) restrict buffer ShadowVolumeCounter {
	DrawIndirectCommand b_shadow_volume_counter;
};

#define getFragmentHierarchyOffset(_i) (u_pm_info.m_fragment_hierarchy_offset[(_i)/4][(_i)%4])
#define getFragmentMapHierarchyOffset(_i) (u_pm_info.m_fragment_map_hierarchy_offset[(_i)/4][(_i)%4])

//int _map_offset[8] = {0, 16384, 20480, 21504, 21760, 21824, 21840, 21844};
//#define getFragmentMapHierarchyOffset(_i) (_map_offset[_i])
#endif

#ifdef USE_SV_RENDER
layout (set=USE_SV_RENDER, binding=0) uniform sampler2D s_color[1];
#endif //_PM_RENDER_

#ifdef USE_SV_LIGHT
layout(std430, set=USE_SV_LIGHT, binding=0) restrict buffer LightCounter {
	uvec4 b_light_count;
};
layout(std430, set=USE_SV_LIGHT, binding=1) restrict buffer LightDataBuffer {
	Emission b_light_data[];
};
#endif

vec2 rotate(in float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return vec2(-s, c);
}
vec4 rotate2(in vec2 angle)
{
	vec2 c = cos(angle);
	vec2 s = sin(angle);
	return vec4(-s.x, c.x, -s.y, c.y);
}
#endif //_SV_