#ifndef _PM_
#define _PM_

#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_NV_shader_atomic_int64 : require
struct PMInfo
{
	mat4 m_camera_PV;
	ivec2 m_resolution;
	uvec2 m_emission_tile_size;
	uvec2 m_emission_tile_num;
	uvec2 _p;
	vec4 m_position;
	ivec4 m_fragment_map_hierarchy_offset[2];
	int m_emission_tile_linklist_max;
	int m_emission_buffer_max;
};
struct Fragment
{
	vec4 albedo;
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

struct PM2DLightData
{
	vec2 pos;
	float dir; // atan
	float angle;
	vec4 emission;
	int level;
	int _p1;
	int _p2;
	int _p3;
};
struct PM2DStaticLightData
{
	vec2 pos;
	float dir; // atan
	float angle;
	vec4 emission;
	int level;
	int m_is_enable;
	int m_;
	int _p3;
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
layout(std430, set=USE_PM, binding=3) restrict coherent buffer FragmentChangeMapBuffer {
	uint64_t b_fragment_change_map[];
};

layout(std430, set=USE_PM, binding=20) restrict buffer EmissiveCounter {
	ivec4 b_emission_counter[];
};
layout(std430, set=USE_PM, binding=21) restrict buffer EmissiveBuffer {
	Emission b_emission_buffer[];
};
layout(std430, set=USE_PM, binding=22) restrict buffer EmissiveTileLinkListCounter {
	int b_emission_tile_counter;
};
layout(std430, set=USE_PM, binding=23) restrict buffer EmissiveTileLinkHeadBuffer {
	int b_emission_tile_linkhead[];
};
layout(std430, set=USE_PM, binding=24) restrict buffer EmissiveTileLinkListBuffer {
	LinkList b_emission_tile_linklist[];
};
layout(std430, set=USE_PM, binding=25) restrict buffer EmmisiveReachedBuffer {
	uint64_t b_emission_reached[];
};
layout(std430, set=USE_PM, binding=26) restrict buffer EmmisiveOcclusionBuffer {
	uint64_t b_emission_occlusion[];
};
layout(std430, set=USE_PM, binding=27) restrict buffer LightWorkBuffer {
	uint64_t b_light_work[];
};

#define getFragmentMapHierarchyOffset(_i) (u_pm_info.m_fragment_map_hierarchy_offset[(_i)/4][(_i)%4])

//int _map_offset[8] = {0, 16384, 20480, 21504, 21760, 21824, 21840, 21844};
//#define getFragmentMapHierarchyOffset(_i) (_map_offset[_i])
#endif

#ifdef USE_PM_RENDER
layout (set=USE_PM_RENDER, binding=0, rgba16f) uniform image2D t_color[4];
layout (set=USE_PM_RENDER, binding=1) uniform sampler2D s_color[4];
#endif //_PM_RENDER_

#ifdef USE_PM_LIGHT
layout(std430, set=USE_PM_LIGHT, binding=0) restrict buffer LightCounter {
	uvec4 b_light_count;
};
layout(std430, set=USE_PM_LIGHT, binding=1) restrict buffer LightDataBuffer {
	Emission b_light_data[];
};
#endif
#ifdef USE_PM_SDF
layout(/*std430,*/ set=USE_PM_SDF, binding=0) restrict buffer SDFBuffer {
	float b_light_count;
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

#endif //_PM_