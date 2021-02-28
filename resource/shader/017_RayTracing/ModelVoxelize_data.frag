#version 460
#extension GL_GOOGLE_include_directive : require


#define USE_Voxel 0
#define USE_Model 1
#include "Voxel.glsl"

layout(location=1)in Transform
{
	vec3 Position;
	vec3 Albedo;
	vec3 Normal;
}transform;



void main()
{
	ivec3 pos = ivec3(transform.Position.xyz);
	if(any(lessThan(pos, ivec3(0))) || any(greaterThanEqual(pos, u_info.reso.xyz))){ return; }

	ivec3 p = pos>>4;
	ivec3 reso = u_info.reso.xyz>>4;
	int index = p.x+p.y*reso.x+p.z*reso.x*reso.y;
	{
		int top_index = b_hashmap[index];
		ivec3 top_bit = ToTopBit(pos);
		int tb = top_bit.x + top_bit.y*4+ top_bit.z*16;

		uint mid_index = b_interior[b_interior[top_index].child + bitcount(b_interior[top_index].bitmask, tb)].child;
		ivec3 mid_bit = ToMidBit(pos);
		int mb = mid_bit.x+mid_bit.y*4+mid_bit.z*16;

		uint leaf_index = b_interior[b_interior[mid_index].child + bitcount(b_interior[mid_index].bitmask, mb)].child;
		b_leaf[leaf_index].normal = pack_normal(transform.Normal);
		b_leaf[leaf_index].albedo = 0u;

	}


}