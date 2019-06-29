#ifndef GI2D_
#define GI2D_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

struct GI2DInfo
{
	mat4 m_camera_PV;
	ivec4 m_resolution;
	vec4 m_position;
	ivec4 m_fragment_map_size_hierarchy;
	uint m_hierarchy_num;
};
struct GI2DScene
{
	int m_frame;
	int m_hierarchy;
	uint m_skip;
	int _p2;

	uint m_radiance_offset;
	uint m_map_offset;
	uvec2 m_map_reso;
};

#define _maxf (1023.)
#define _maxi (1023)
uint packRGB(in vec3 rgb)
{
	ivec3 irgb = ivec3(rgb*_maxf);
	irgb = min(irgb, ivec3(_maxi));
	irgb <<= ivec3(20, 10, 0);
	return irgb.x | irgb.y | irgb.z;
}
vec3 unpackRGB(in uint irgb)
{
	vec3 rgb = vec3((uvec3(irgb) >> uvec3(20, 10, 0)) & ((uvec3(1) << uvec3(10, 10, 10)) - uvec3(1)));
	return rgb / _maxf;
}
struct Fragment
{
//	uint32_t data : 30;
//	uint32_t is_diffuse : 1;
//	uint32_t is_emissive : 1;

	uint data;
};
void setRGB(inout Fragment f, in vec3 rgb)
{
	f.data = (f.data & (3<<30)) | packRGB(rgb);
}
vec3 getRGB(in Fragment f)
{
	return unpackRGB(f.data);
}
void setEmissive(inout Fragment f, in bool b)
{
	f.data = (f.data & ~(1<<31)) | (b?(1<<31):0);
}
void setDiffuse(inout Fragment f, in bool b)
{
	f.data = (f.data & ~(1<<30)) | (b?(1<<30):0);
}
bool isEmissive(in Fragment f)
{
	return (f.data & (1<<31)) != 0;
}
bool isDiffuse(in Fragment f)
{
	return (f.data & (1<<30)) != 0;
}

Fragment makeFragment(in vec3 color, in bool d, in bool e)
{
	Fragment f;
	setRGB(f, color);
	setDiffuse(f, d);
	setEmissive(f, e);
	return f;
}

struct D2Ray
{
	vec2 origin;
	float angle;
	uint march;
};
struct D2Segment
{
	uint ray_index;
	uint begin;
	uint march;
	uint radiance;
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
layout(std430, set=USE_GI2D, binding=3) restrict buffer FragmentMapBuffer {
	u64vec2 b_fragment_map[];
};
layout(set=USE_GI2D, binding=4) restrict buffer GridCounter {
	int b_grid_counter[];
};
layout(set=USE_GI2D, binding=5) restrict buffer LightBuffer {
	uint b_light[];
};
layout(set=USE_GI2D, binding=6, std430) restrict buffer DiffuseMapBuffer {
	uint64_t b_diffuse_map[];
};
layout(set=USE_GI2D, binding=7, std430) restrict buffer EmissiveMapBuffer {
	uint64_t b_emissive_map[];
};


ivec2 frame_offset(){
	return ivec2(u_gi2d_scene.m_frame%2,u_gi2d_scene.m_frame/2);
}


#define getFragmentMapHierarchyOffset(_i) (((_i)==0) ? 0 : u_gi2d_info.m_fragment_map_size_hierarchy[(_i)-1])
#endif


#define LightPower (0.035)
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

vec3 unpackEmissive4(in uvec4 irgb)
{
	vec4 rrrr = vec4((irgb >> uvec4(20)) & ((uvec4(1)<<uvec4(10))-1));
	vec4 gggg = vec4((irgb >> uvec4(10)) & ((uvec4(1)<<uvec4(10))-1));
	vec4 bbbb = vec4((irgb >> uvec4(0)) & ((uvec4(1)<<uvec4(10))-1));
	vec3 rgb = vec3(dot(rrrr, vec4(1.)), dot(gggg, vec4(1.)),dot(bbbb, vec4(1.)));
	return rgb / denominator;
}

#ifdef USE_GI2D_Radiosity
struct GI2DRadiosityInfo
{
	uint ray_num_max;
	uint ray_frame_max;
	uint frame_max;
	uint a2;
};

struct DrawCommand
{
	uint vertexCount;
	uint instanceCount;
	uint firstVertex;
	uint firstInstance;

	uvec4 bounce_cmd;

};
#define Dir_Num (31)
#define Vertex_Num (Dir_Num*2)
#define Bounce_Num (3)

struct VertexInfo
{
	u16vec2 pos;
	uint id;
};
struct RadiosityVertex
{
//	u16vec2 vertex_pos[Vertex_Num];
//	uint vertex_id[Vertex_Num];
	VertexInfo vertex[Vertex_Num];
	u16vec2 pos;
	u16vec2 _p;
	f16vec3 radiance[2];
	f16vec3 albedo;

};

layout(set=USE_GI2D_Radiosity, binding=0, std140) uniform GI2DRadiosityInfoUniform {
	GI2DRadiosityInfo u_radiosity_info;
};
layout(set=USE_GI2D_Radiosity, binding=1, std430) restrict buffer VertexArrayCounter {
	DrawCommand b_vertex_array_counter;
};
layout(set=USE_GI2D_Radiosity, binding=2, std430) restrict buffer VertexArrayIndexBuffer {
	uint b_vertex_array_index[];
};
layout(set=USE_GI2D_Radiosity, binding=3, std430) restrict buffer VertexArrayBuffer {
	RadiosityVertex b_vertex_array[];
};
layout(set=USE_GI2D_Radiosity, binding=4, std430) restrict buffer MapEdgeBuffer {
	uint64_t b_edge[];
};


#endif

uint getMemoryOrder(in uvec2 xy)
{
//	xy = (xy ^ (xy << 8 )) & 0x00ff00ff;
//	xy = (xy ^ (xy << 4 )) & 0x0f0f0f0f;
//	xy = (xy ^ (xy << 2 )) & 0x33333333;
//	xy = (xy ^ (xy << 1 )) & 0x55555555;

	xy = (xy | (xy << 8 )) & 0x00ff00ff;
	xy = (xy | (xy << 4 )) & 0x0f0f0f0f;
	xy = (xy | (xy << 2 )) & 0x33333333;
	xy = (xy | (xy << 1 )) & 0x55555555;

	return (xy.y<<1)|xy.x;
}
uvec4 getMemoryOrder4(in uvec4 x, in uvec4 y)
{
	x = (x | (x << 8 )) & 0x00ff00ff;
	x = (x | (x << 4 )) & 0x0f0f0f0f;
	x = (x | (x << 2 )) & 0x33333333;
	x = (x | (x << 1 )) & 0x55555555;

	y = (y | (y << 8 )) & 0x00ff00ff;
	y = (y | (y << 4 )) & 0x0f0f0f0f;
	y = (y | (y << 2 )) & 0x33333333;
	y = (y | (y << 1 )) & 0x55555555;
	
	return (y<<1)|x;
}

vec2 intersectRayRay(in vec2 as, in vec2 ad, in vec2 bs, in vec2 bd)
{
	float u = (as.y*bd.x + bd.y*bs.x - bs.y*bd.x - bd.y*as.x) / (ad.x*bd.y - ad.y*bd.x);
	return as + u * ad;
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
vec2 calcDir(in float angle)
{
#define GI2D_FLT_EPSILON 0.0001
	vec2 dir = rotate(angle);
	dir.x = abs(dir.x)<GI2D_FLT_EPSILON ? 0.0001 : dir.x;
	dir.y = abs(dir.y)<GI2D_FLT_EPSILON ? 0.0001 : dir.y;
	vec2 inv_dir = 1./dir;
	return dir * min(abs(inv_dir.x), abs(inv_dir.y));
}
void calcDir2(in float angle, out vec2 dir, out vec2 inv_dir)
{
	dir = rotate(angle);
	dir.x = abs(dir.x)<GI2D_FLT_EPSILON ? 0.0001 : dir.x;
	dir.y = abs(dir.y)<GI2D_FLT_EPSILON ? 0.0001 : dir.y;
	inv_dir = 1./dir;
	dir = dir * min(abs(inv_dir.x), abs(inv_dir.y));
	inv_dir = 1./dir;
}

#endif //GI2D_