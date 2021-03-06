#ifndef FLOWMAP_H_
#define FLOWMAP_H_

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
// #extension GL_KHR_shader_subgroup : require
#extension GL_KHR_shader_subgroup_ballot : require


#if defined(USE_Flowmap)

struct FlowmapInfo
{
	ivec2 reso;
};
struct DropParam
{
	vec2 pos;
	float size;
	float type;
	float time;
//	float seed;
};

layout(set=USE_Flowmap,binding=0, std140) uniform V0 {FlowmapInfo u_info; };
layout(set=USE_Flowmap,binding=1, scalar) buffer V1 { float b_value[]; };
layout(set=USE_Flowmap,binding=2, scalar) buffer V2 { DropParam b_drop[]; };
layout(set=USE_Flowmap,binding=3, scalar) buffer V3 { float b_diffusion[]; };
//layout(set=USE_Flowmap,binding=4, scalar) buffer V4 { int b_drop_num;}
layout(set=USE_Flowmap,binding=10) uniform sampler2D t_floor;

#endif

float mod289(float x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 mod289(vec4 x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 perm(vec4 x){return mod289(((x * 34.0) + 1.0) * x);}

float noise(vec3 p)
{
	vec3 a = floor(p);
	vec3 d = p - a;
	d = d * d * (3.0 - 2.0 * d);

	vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
	vec4 k1 = perm(b.xyxy);
	vec4 k2 = perm(k1.xyxy + b.zzww);

	vec4 c = k2 + a.zzzz;
	vec4 k3 = perm(c);
	vec4 k4 = perm(c + 1.0);

	vec4 o1 = fract(k3 * (1.0 / 41.0));
	vec4 o2 = fract(k4 * (1.0 / 41.0));

	vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
	vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

	return o4.y * d.y + o4.x * (1.0 - d.y);
}


#endif // FLOWMAP_H_