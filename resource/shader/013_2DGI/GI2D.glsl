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
struct GI2DScene
{
	int m_frame;
	int m_hierarchy;
};
struct Fragment
{
	vec4 albedo;
};

#define Grid_Size (1.)
  #define DT 0.016
//#define DT 0.0005

#ifdef USE_GI2D
layout(std140, set=USE_GI2D, binding=0) uniform GI2DInfoUniform {
	GI2DInfo u_gi2d_info;
};
layout(std140, set=USE_GI2D, binding=1) uniform GI2DSceneUniform {
	GI2DScene u_gi2d_scene;
};
layout(std430, set=USE_GI2D, binding=2) restrict buffer FragmentBuffer {
	Fragment b_fragment[];
};
layout(std430, set=USE_GI2D, binding=3) restrict buffer DiffuseMapBuffer {
	uint64_t b_diffuse_map[];
};
layout(std430, set=USE_GI2D, binding=4) restrict buffer EmissiveMapBuffer {
	uint64_t b_emissive_map[];
};
layout(set=USE_GI2D, binding=5) restrict buffer GridCounter {
	int b_grid_counter[];
};
layout(set=USE_GI2D, binding=6) restrict buffer LightBuffer {
	uint b_light[];
};
layout(set=USE_GI2D, binding=7) restrict buffer LightCounter {
	uvec4 b_light_counter;
};
layout(set=USE_GI2D, binding=8) restrict buffer LightIndexBuffer {
	uint b_light_index[];
};

ivec2 frame_offset(){
	return ivec2(u_gi2d_scene.m_frame%2,u_gi2d_scene.m_frame/2);
}

#define getFragmentMapHierarchyOffset(_i) (u_gi2d_info.m_fragment_map_hierarchy_offset[(_i)/4][(_i)%4])

#endif


#define LightPower (0.15)
#define Ray_Density (1)
#define Block_Size (1)
#define denominator (512.)
//#define denominator (2048.)
//#define denominator (16.)


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

/*uvec4 packEmissiveR4(in vec3 rgb)
{
	ivec3 irgb = ivec3(rgb*denominator*(1.+1./denominator*0.5));
	irgb <<= ivec3(20, 10, 0);
	return irgb.x | irgb.y | irgb.z;
}
*/
vec3 unpackEmissive4(in uvec4 irgb)
{
	vec4 rrrr = vec4((irgb >> uvec4(20)) & ((uvec4(1)<<uvec4(10))-1));
	vec4 gggg = vec4((irgb >> uvec4(10)) & ((uvec4(1)<<uvec4(10))-1));
	vec4 bbbb = vec4((irgb >> uvec4(0)) & ((uvec4(1)<<uvec4(10))-1));
	vec3 rgb = vec3(dot(rrrr, vec4(1.)), dot(gggg, vec4(1.)),dot(bbbb, vec4(1.)));
	return rgb / denominator;
}

#ifdef USE_GI2D_Radiosity
/*struct D2Ray
{
	vec2 origin;
	vec2 dir;
};
*/struct D2Ray
{
	vec2 origin;
	float angle;
	uint march;
};

layout(set=USE_GI2D_Radiosity, binding=0) restrict buffer RadianceMapBuffer {
	uint b_radiance[];
};
layout(set=USE_GI2D_Radiosity, binding=1) restrict buffer BounceMapBuffer {
	uint64_t b_bounce_map[];
};
layout(set=USE_GI2D_Radiosity, binding=2, std430) restrict buffer RayBuffer {
	D2Ray b_ray[];
};
layout(set=USE_GI2D_Radiosity, binding=3) restrict buffer RayCounter {
	ivec4 b_ray_counter;
};


#endif

#ifdef USE_GI2D_Radiosity2

layout(set=USE_GI2D_Radiosity2, binding=0) restrict buffer RadianceMapBuffer {
	int64_t b_radiance[];
};
layout(set=USE_GI2D_Radiosity2, binding=1) restrict buffer BounceMapBuffer {
	uint64_t b_bounce_map[];
};

int64_t packEmissive(in vec3 rgb)
{
	i64vec3 irgb = i64vec3(rgb*denominator*(1.+1./denominator*0.5));
	irgb <<= i64vec3(40, 20, 0);
	return irgb.x | irgb.y | irgb.z;
}
vec3 unpackEmissive(in int64_t irgb)
{
//	vec3 rgb = vec3((uvec3(irgb) >> uvec3(21, 10, 0)) & ((uvec3(1)<<uvec3(11, 11, 10))-1));
	vec3 rgb = vec3((uvec3(irgb) >> u64vec3(40, 20, 0)) & ((u64vec3(1)<<u64vec3(20, 20, 20))-1));
	return rgb / denominator;
}
#endif

uint getMemoryOrder(in uvec2 xy)
{
	xy = (xy ^ (xy << 8 )) & 0x00ff00ff;
	xy = (xy ^ (xy << 4 )) & 0x0f0f0f0f;
	xy = (xy ^ (xy << 2 )) & 0x33333333;
	xy = (xy ^ (xy << 1 )) & 0x55555555;
	
	return (xy.y<<1)|xy.x;
}
uvec4 getMemoryOrder4(in uvec4 x, in uvec4 y)
{
	x = (x ^ (x << 8 )) & 0x00ff00ff;
	x = (x ^ (x << 4 )) & 0x0f0f0f0f;
	x = (x ^ (x << 2 )) & 0x33333333;
	x = (x ^ (x << 1 )) & 0x55555555;

	y = (y ^ (y << 8 )) & 0x00ff00ff;
	y = (y ^ (y << 4 )) & 0x0f0f0f0f;
	y = (y ^ (y << 2 )) & 0x33333333;
	y = (y ^ (y << 1 )) & 0x55555555;
	
	return (y<<1)|x;
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

bool intersectRayAABB(in vec2 p, in vec2 d, in vec2 bmin, in vec2 bmax, out float d_min, out float d_max)
{
	vec2 tmin = vec2(0.);
	vec2 tmax = vec2(0.);
	for (int i = 0; i < 2; i++)
	{
		if (abs(d[i]) < 10e-6)
		{
			// 光線はスラブに対して平行。原点がスラブの中になければ交点無し。
			if (p[i] < bmin[i] || p[i] > bmax[i])
			{
				p = vec2(-9999999999.f);
				return false;
			}
			tmin[i] = 0.;
			tmax[i] = 99999.;
		}
		else
		{
			float ood = 1. / d[i];
			float t1 = (bmin[i] - p[i]) * ood;
			float t2 = (bmax[i] - p[i]) * ood;

			tmin[i] = min(t1, t2);
			tmax[i] = max(t1, t2);
		}
	}
	d_min = max(tmin[0], tmin[1]);
	d_max = min(tmax[0], tmax[1]);
	return d_min <= d_max;
}
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