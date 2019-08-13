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
	uint64_t b_fragment_map[];
};

#endif

vec2 rotate(in float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return vec2(-s, c);
}

#endif //GI2D_