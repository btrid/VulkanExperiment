#ifndef GI2D_
#define GI2D_

#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_shader_draw_parameters : require
#extension GL_NV_shader_atomic_int64 : require
//#extension GL_NV_gpu_program5 : require
struct GI2DInfo
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

struct GI2DLightData
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

#ifdef USE_GI2D
layout(std140, set=USE_GI2D, binding=0) uniform PMInfoUniform {
	GI2DInfo u_gi2d_info;
};
layout(std430, set=USE_GI2D, binding=1) restrict buffer FragmentBuffer {
	Fragment b_fragment[];
};
layout(std430, set=USE_GI2D, binding=2) restrict buffer FragmentMapBuffer {
	uint64_t b_fragment_map[];
};
layout(std430, set=USE_GI2D, binding=3) restrict buffer FragmentChangeMapBuffer {
	uint64_t b_fragment_change_map[];
};
layout(std430, set=USE_GI2D, binding=4) restrict buffer LightMapBuffer {
	uint64_t b_light_map[];
};


layout(std430, set=USE_GI2D, binding=20) restrict buffer EmissiveCounter {
	ivec4 b_emission_counter[];
};
layout(std430, set=USE_GI2D, binding=21) restrict buffer EmissiveBuffer {
	Emission b_emission_buffer[];
};
layout(std430, set=USE_GI2D, binding=22) restrict buffer EmissiveTileLinkListCounter {
	int b_emission_tile_counter;
};
layout(std430, set=USE_GI2D, binding=23) restrict buffer EmissiveTileLinkHeadBuffer {
	int b_emission_tile_linkhead[];
};
layout(std430, set=USE_GI2D, binding=24) restrict buffer EmissiveTileLinkListBuffer {
	LinkList b_emission_tile_linklist[];
};
layout(std430, set=USE_GI2D, binding=25) restrict buffer EmmisiveReachedBuffer {
	uint64_t b_emission_reached[];
};
layout(std430, set=USE_GI2D, binding=26) restrict buffer EmmisiveOcclusionBuffer {
	uint64_t b_emission_occlusion[];
};
layout(std430, set=USE_GI2D, binding=27) restrict buffer LightWorkBuffer {
	uint64_t b_light_work[];
};

#define getFragmentMapHierarchyOffset(_i) (u_gi2d_info.m_fragment_map_hierarchy_offset[(_i)/4][(_i)%4])

//int _map_offset[8] = {0, 16384, 20480, 21504, 21760, 21824, 21840, 21844};
//#define getFragmentMapHierarchyOffset(_i) (_map_offset[_i])
#endif

#ifdef USE_GI2D_RENDER
layout (set=USE_GI2D_RENDER, binding=0, rgba16f) uniform image2D t_color[4];
layout (set=USE_GI2D_RENDER, binding=1) uniform sampler2D s_color[4];
#endif //USE_GI2D_RENDER_

#ifdef USE_GI2D_LIGHT
layout(std430, set=USE_GI2D_LIGHT, binding=0) restrict buffer LightCounter {
	uvec4 b_light_count;
};
layout(std430, set=USE_GI2D_LIGHT, binding=1) restrict buffer LightDataBuffer {
	Emission b_light_data[];
};
#endif
#ifdef USE_RT
layout(set=USE_RT, binding=0) restrict buffer RTBuffer {
	uint64_t b_rt_map[];
};
layout (set=USE_RT, binding=10, rgba16f) uniform image2D t_color;
layout (set=USE_RT, binding=11) uniform sampler2D s_color;

// todo
#define RT_Map_Size uvec2(32, 32)
#endif // USE_RT

#ifdef USE_PM
layout(set=USE_PM, binding=0) restrict buffer PMReachedMapBuffer {
	uint64_t b_reached_map[];
};
layout (set=USE_PM, binding=10, rgba16f) uniform image2D t_color;
layout (set=USE_PM, binding=11) uniform sampler2D s_color;
#endif // USE_PM

#ifdef USE_GI2D_Radiosity
layout(set=USE_GI2D_Radiosity, binding=0) restrict buffer RadianceMapBuffer {
	uint b_radiance[];
};
layout(set=USE_GI2D_Radiosity, binding=1) restrict buffer BounceMapBuffer {
	uint64_t b_bounce_map[];
};

int getMemoryOrder(in ivec2 xy)
{
#if 1
// int getMortonIndex(in ivec2 xy)
	// 8x8のブロック
	ivec2 n = xy>>3;
	xy -= n<<3;
	n = (n ^ (n << 8 )) & 0x00ff00ff;
	n = (n ^ (n << 4 )) & 0x0f0f0f0f;
	n = (n ^ (n << 2 )) & 0x33333333;
	n = (n ^ (n << 1 )) & 0x55555555;
	
	int mi = (n.y<<1)|n.x;
	return mi*64 + xy.x + xy.y*8;
#else
	return xy.x + xy.y * u_gi2d_info.m_resolution.x;
#endif
}

#define LightPower (1.)
#define Attenuation (0.98)
#define denominator (32.)
uint packEmissive(in vec3 rgb)
{
	ivec3 irgb = ivec3(rgb*denominator*(1.+1./denominator*0.5));
	irgb <<= ivec3(20, 10, 0);
	return irgb.x | irgb.y | irgb.z;
}
vec3 unpackEmissive(in uint irgb)
{
//	vec3 rgb = vec3((uvec3(irgb) >> uvec3(21, 10, 0)) & ((uvec3(1)<<uvec3(11, 11, 10))-1));
	vec3 rgb = vec3((uvec3(irgb) >> uvec3(20, 10, 0)) & ((uvec3(1)<<uvec3(10, 10, 10))-1));
	return rgb / denominator;
}

vec2 intersectRayRay(in vec2 as, in vec2 ad, in vec2 bs, in vec2 bd)
{
	float u = (as.y*bd.x + bd.y*bs.x - bs.y*bd.x - bd.y*as.x) / (ad.x*bd.y - ad.y*bd.x);
	return as + u * ad;
}

bool marchToAABB(inout vec2 p, in vec2 d, in vec2 bmin, in vec2 bmax)
{

	if(all(lessThan(p, bmax)) 
	&& all(greaterThan(p, bmin)))
	{
		// AABBの中にいる
		return true;
	}

	float tmin = 0.;
	float tmax = 10e6;
	for (int i = 0; i < 2; i++)
	{
		if (abs(d[i]) < 10e-6)
		{
			// 光線はスラブに対して平行。原点がスラブの中になければ交点無し。
			if (p[i] < bmin[i] || p[i] > bmax[i])
			{
				return false;
			}
		}
		else
		{
			float ood = 1. / d[i];
			float t1 = (bmin[i] - p[i]) * ood;
			float t2 = (bmax[i] - p[i]) * ood;

			// t1が近い平面との交差、t2が遠い平面との交差になる
			float near = min(t1, t2);
			float far = max(t1, t2);

			// スラブの交差している感覚との交差を計算
			tmin = max(near, tmin);
			tmax = max(far, tmax);

			if (tmin > tmax) {
				return false;
			}
		}
	}
	float dist = tmin;
	p += d*dist;
	return true;

}
layout (set=USE_GI2D_Radiosity, binding=10, r16f) uniform image2DArray t_color;
layout (set=USE_GI2D_Radiosity, binding=11) uniform sampler2D s_color;

#endif // USE_PM

vec2 rotate(in float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return vec2(-s, c);
}
vec2 rotateZ(in vec2 dir, in float angle)
{
	float c = cos(angle);
	float s = sin(angle);

	vec2 ret;
	ret.x = dir.x * c - dir.y * s;
	ret.y = dir.x * s + dir.y * c;
	return ret;
}
vec4 rotate2(in vec2 angle)
{
	vec2 c = cos(angle);
	vec2 s = sin(angle);
	return vec4(-s.x, c.x, -s.y, c.y);
}

uint64_t popcnt(in uint64_t n)
{
    uint64_t c = (n & 0x5555555555555555ul) + ((n>>1) & 0x5555555555555555ul);
    c = (c & 0x3333333333333333ul) + ((c>>2) & 0x3333333333333333ul);
    c = (c & 0x0f0f0f0f0f0f0f0ful) + ((c>>4) & 0x0f0f0f0f0f0f0f0ful);
    c = (c & 0x00ff00ff00ff00fful) + ((c>>8) & 0x00ff00ff00ff00fful);
    c = (c & 0x0000ffff0000fffful) + ((c>>16) & 0x0000ffff0000fffful);
    c = (c & 0x00000000fffffffful) + ((c>>32) & 0x00000000fffffffful);
    return c;
}
uint64_t popcnt4(in u64vec4 n)
{
    u64vec4 c = (n & 0x5555555555555555ul) + ((n>>1) & 0x5555555555555555ul);
    c = (c & 0x3333333333333333ul) + ((c>>2) & 0x3333333333333333ul);
    c = (c & 0x0f0f0f0f0f0f0f0ful) + ((c>>4) & 0x0f0f0f0f0f0f0f0ful);
    c = (c & 0x00ff00ff00ff00fful) + ((c>>8) & 0x00ff00ff00ff00fful);
    c = (c & 0x0000ffff0000fffful) + ((c>>16) & 0x0000ffff0000fffful);
    c = (c & 0x00000000fffffffful) + ((c>>32) & 0x00000000fffffffful);
    return c.x+c.y+c.z+c.w;
}
u64vec4 popcnt44(in u64vec4 n)
{
    u64vec4 c = (n & 0x5555555555555555ul) + ((n>>1) & 0x5555555555555555ul);
    c = (c & 0x3333333333333333ul) + ((c>>2) & 0x3333333333333333ul);
    c = (c & 0x0f0f0f0f0f0f0f0ful) + ((c>>4) & 0x0f0f0f0f0f0f0f0ful);
    c = (c & 0x00ff00ff00ff00fful) + ((c>>8) & 0x00ff00ff00ff00fful);
    c = (c & 0x0000ffff0000fffful) + ((c>>16) & 0x0000ffff0000fffful);
    c = (c & 0x00000000fffffffful) + ((c>>32) & 0x00000000fffffffful);
    return c;
}
#endif //GI2D_