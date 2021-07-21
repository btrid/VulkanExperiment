#version 460
#extension GL_GOOGLE_include_directive : require
//#extension SPV_EXT_fragment_fully_covered :require


#define USE_Voxel 0
#define USE_Model 1
#include "Voxel.glsl"

layout(location=1)in Transform
{
	vec3 Position;
	vec3 Albedo;
	vec3 Normal;
}transform;

layout(push_constant, std430) uniform PC
{
	int level;
} constant;

void main()
{
	ivec3 pos = ivec3(transform.Position.xyz);
	if(any(lessThan(pos, ivec3(0))) || any(greaterThanEqual(pos, u_info.reso.xyz))){ return; }

	ivec3 p = pos>>4;
	ivec3 reso = u_info.reso.xyz>>4;
	int hash_index = p.x + p.y*reso.x + p .z*reso.x*reso.y;
	switch(constant.level)
	{
		// make hash
		case 0:
		{
			if(atomicCompSwap(b_hashmap[hash_index], -1, 0) == -1)
			{
				int top_index = atomicAdd(b_interior_counter[0].w, 1);
				if((top_index%64)==0){ atomicAdd(b_interior_counter[0].x, 1); }

				b_hashmap[hash_index] = top_index;
				b_interior[top_index].bitmask = uvec2(0);
			}
		}
		break;

		// make top
		case 1:
		{
			int index = b_hashmap[hash_index];
			ivec3 bit = ToTopBit(pos);
			int b = bit.x + bit.y*4 + bit.z*16;
			atomicOr(b_interior[index].bitmask[b/32], 1<<(b%32));
		}
		break;

		// make mid
		case 2:
		{
			uint index = b_hashmap[hash_index];
			ivec3 bit = ToTopBit(pos);
			int b = bit.x + bit.y*4 + bit.z*16;

			index = b_interior[index].child + bitcount(b_interior[index].bitmask, b);
			bit = ToMidBit(pos);
			b = bit.x + bit.y*4 + bit.z*16;

			atomicOr(b_interior[index].bitmask[b/32], 1<<(b%32));
		}
		break;

		// make leaf
		case 3:
		{
			uint index = b_hashmap[hash_index];
			ivec3 bit = ToTopBit(pos);
			int b = bit.x + bit.y*4+ bit.z*16;

			index = b_interior[index].child + bitcount(b_interior[index].bitmask, b);
			bit = ToMidBit(pos);
			b = bit.x+bit.y*4+bit.z*16;

			uint leaf_index = b_interior[index].child + bitcount(b_interior[index].bitmask, b);

			b_leaf[leaf_index].normal = pack_normal(transform.Normal);
			b_leaf[leaf_index].albedo = 0u;
		}

		break;
	}


}