#version 460
#extension GL_GOOGLE_include_directive : require


#define USE_Voxel 0
#define USE_Model 1
#include "Voxel.glsl"

layout(location=1)in Transform
{
	vec3 Position;
	vec3 Albedo;
}transform;

void main()
{
	ivec3 pos = ivec3(transform.Position.xyz);
	if(any(lessThan(pos, ivec3(0))) || any(greaterThanEqual(pos, u_info.reso.xyz))){ return; }

	ivec3 p = pos>>4;
	ivec3 reso = u_info.reso.xyz>>4;
	int index = p.x+p.y*reso.x+p.z*reso.x*reso.y;
	if(atomicCompSwap(b_hashmap[index], -1, 0) == -1)
	{
		int top_index = atomicAdd(b_interior_counter[0].w, 1);
		if((top_index%64)==0){ atomicAdd(b_interior_counter[0].x, 1); }

		b_hashmap[index] = top_index;
		b_interior[top_index].bitmask = uvec2(0);
	}


}