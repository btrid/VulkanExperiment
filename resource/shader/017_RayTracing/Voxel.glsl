#ifndef VOXEL_H_
#define VOXEL_H_

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
// #extension GL_KHR_shader_subgroup : require
#extension GL_KHR_shader_subgroup_ballot : require
#if defined(USE_Voxel)

struct VoxelInfo
{
	ivec4 reso;

};

struct InteriorNode
{
	uvec2 bitmask;
	uint child;
};
struct LeafNode
{
	uint16_t normal;
	uint32_t albedo;
};

struct LeafData
{
	uvec2 bitmask;
	u16vec3 pos;
	uint leaf_index;
};

layout(set=USE_Voxel,binding=0, std140) uniform V0 {VoxelInfo u_info; };
layout(set=USE_Voxel,binding=1, scalar) buffer V1 { int b_hashmap[]; };
layout(set=USE_Voxel,binding=2, scalar) buffer V2 { ivec4 b_interior_counter[]; };
layout(set=USE_Voxel,binding=3, scalar) buffer V3 { int b_leaf_counter; };
layout(set=USE_Voxel,binding=4, scalar) buffer V4 { InteriorNode b_interior[]; };
layout(set=USE_Voxel,binding=5, scalar) buffer V5 { LeafNode b_leaf[]; };
layout(set=USE_Voxel,binding=6, scalar) buffer V6 { ivec4 b_leaf_data_counter; };
layout(set=USE_Voxel,binding=7, scalar) buffer V7 { LeafData b_leaf_data[]; };

layout(set=USE_Voxel,binding=10, scalar) buffer V10 { ivec4 b_fragment_counter; };
layout(set=USE_Voxel,binding=11, scalar) buffer V11 { i16vec4 b_fragment_data[]; };

layout(set=USE_Voxel,binding=20, std140) uniform V10 {mat4 u_voxelize_pvmat[3]; };
#endif


float sdBox( vec3 p, vec3 b )
{
	vec3 d = abs(p) - b;
	return min(max(d.x,max(d.y,d.z)),0.0) + length(max(d,0.0));
}
float sdSphere( vec3 p, float s )
{
  return length( p ) - s;
}

float sdf(in vec3 p)
{
	float d = 9999999.;
	{
//		d = min(d, sdSphere(p-vec3(1000., 250., 1000.), 1000.));

		d = min(d, sdBox(p-vec3(500., 200., 700.), vec3(300.)));
		d = min(d, sdSphere(p-vec3(1200., 190., 1055.), 200.));
	}
	return d;
}

bool map(in vec3 p)
{
	p-=0.5;
	float d = sdf(p);
	for(int z = 0; z < 2; z++)
	for(int y = 0; y < 2; y++)
	for(int x = 0; x < 2; x++)
	{
		if(z==0 && y==0 && x==0){continue;}
		if(sign(d) != sign(sdf(p + vec3(x, y, z))))
		{
			return true;
		}
	}
	return false;
}

vec3 normal(in vec3 p)
{
	p-=0.5;
	float x = sdf(p-vec3(0.1, 0.0, 0.0)) - sdf(p+vec3(0.1, 0.0, 0.0));
	float y = sdf(p-vec3(0.0, 0.1, 0.0)) - sdf(p+vec3(0.0, 0.1, 0.0));
	float z = sdf(p-vec3(0.0, 0.0, 0.1)) - sdf(p+vec3(0.0, 0.0, 0.1));
	return normalize(vec3(x, y, z));
}

ivec3 ToTopIndex(in ivec3 p){ return p >> 4; }
ivec3 ToMidIndex(in ivec3 p){ return p >> 2; }
ivec3 ToTopBit(in ivec3 p){ return (p>>2)-(p>>4<<2); }
ivec3 ToMidBit(in ivec3 p){ return p - (p>>2<<2); }

uint bitcount(in uvec2 bitmask, in int bit)
{
	uvec2 mask = uvec2((i64vec2(1l) << clamp(i64vec2(bit+1) - i64vec2(0, 32), i64vec2(0), i64vec2(32))) - i64vec2(1l));
	uvec2 c = bitCount(bitmask & mask);
	return c.x+c.y;
}
bool isBitOn(in uvec2 bitmask, in int bit)
{
	return (bitmask[bit/32] & (1<<(bit%32))) != 0;
}


// http://jcgt.org/published/0003/02/01/paper.pdf
// https://discourse.panda3d.org/t/glsl-octahedral-normal-packing/15233
vec2 sign_not_zero(in vec2 v) 
{
    return fma(step(vec2(0.0), v), vec2(2.0), vec2(-1.0));
}
uint16_t pack_normal(in vec3 v)
{
	v = vec3(v.xy / dot(abs(v), vec3(1)), v.z);
	v.xy = mix(v.xy, (1.0 - abs(v.yx)) * sign_not_zero(v.xy), step(v.z, 0.0))*vec2(127.)+127.;
	ivec2 i = ivec2(round(clamp(v.xy, 0., 255.)));
	return uint16_t(i.x | (i.y<<8));

}
vec3 unpack_normal(in uint16_t packed_nrm)
{
	vec2 nrm = vec2((packed_nrm.xx>>uvec2(0,8))&0xff) -127.;
	nrm = clamp(nrm / 127.0, -1.0, 1.0);
	vec3 v = vec3(nrm.xy, 1.0 - abs(nrm.x) - abs(nrm.y));
	v.xy = mix(v.xy, (1.0 - abs(v.yx)) * sign_not_zero(v.xy), step(v.z, 0.));
	return normalize(v);
}
#endif // VOXEL_H_