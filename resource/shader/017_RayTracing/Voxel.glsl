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


layout(set=USE_Voxel,binding=0, std140) uniform V0 {VoxelInfo u_info; };
layout(set=USE_Voxel,binding=1, scalar) buffer V1 { int b_hashmap[]; };
layout(set=USE_Voxel,binding=2, scalar) buffer V2 { uvec2 b_hashmap_mask[]; };
layout(set=USE_Voxel,binding=3, scalar) buffer V3 { ivec4 b_interior_counter[]; };
layout(set=USE_Voxel,binding=4, scalar) buffer V4 { int b_leaf_counter; };
layout(set=USE_Voxel,binding=5, scalar) buffer V5 { InteriorNode b_interior[]; };
layout(set=USE_Voxel,binding=6, scalar) buffer V6 { LeafNode b_leaf[]; };

#endif

ivec3 ToTopIndex(in ivec3 p){ return p >> 4; }
ivec3 ToMidIndex(in ivec3 p){ return p >> 2; }
ivec3 ToTopBit(in ivec3 p){ return (p>>2)-(p>>4<<2); }
ivec3 ToMidBit(in ivec3 p){ return p - (p>>2<<2); }
uvec3 ToTopIndex(in uvec3 p){ return p >> 4; }
uvec3 ToMidIndex(in uvec3 p){ return p >> 2; }
uvec3 ToTopBit(in uvec3 p){ return (p>>2)-(p>>4<<2); }
uvec3 ToMidBit(in uvec3 p){ return p - (p>>2<<2); }

//#define ToHierarchyIndex(in ivec3 p, in int level) { return p >> (level*2)}
uint bitcount(in uvec2 bitmask, in int bit)
{
	uvec2 mask = uvec2((i64vec2(1l) << clamp(i64vec2(bit+1) - i64vec2(0, 32), i64vec2(0), i64vec2(32))) - i64vec2(1l));
	uvec2 c = bitCount(bitmask & mask);
	return c.x+c.y;
}
uint bitcount(in uvec2 bitmask, in uint bit)
{
	uvec2 mask = uvec2((i64vec2(1l) << clamp(i64vec2(bit+1) - i64vec2(0, 32), i64vec2(0), i64vec2(32))) - i64vec2(1l));
	uvec2 c = bitCount(bitmask & mask);
	return c.x+c.y;
}
bool isBitOn(in uvec2 bitmask, in int bit)
{
	return (bitmask[bit/32] & (1<<(bit%32))) != 0;
}
bool isBitOn(in uvec2 bitmask, in uint bit)
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