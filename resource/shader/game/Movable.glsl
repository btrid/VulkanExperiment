#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity2 1
#include "GI2D.glsl"
#include "Radiosity2.glsl"

layout (local_size_x=128) in;

void main()
{
	const ivec4 reso = u_gi2d_info.m_resolution;

	vec2 mi = ray.begin * ray.dir.xy + ray.origin;
	if((ray.area&1)!=0) mi.x = (reso.x-1)-mi.x;

	for(int march = ray.begin; march < ray.end; )
	{
		ivec2 cell = ivec2(mi)>>3;
		uint64_t map = b_fragment_map[cell.x + cell.y * reso.z];

		for(;;)
		{
			ivec2 cell_sub = ivec2(mi)%8;
			bool attr = (map & (1ul<<(cell_sub.x+cell_sub.y*8))) != 0ul;
			if(attr)
			{
				break;
			}
			is_inner = attr;
			march++;

			mi = march * ray.dir.xy + ray.origin;
			if((ray.area&1)!=0) mi.x = (reso.x-1)-mi.x;
			if(any(notEqual(cell, mi>>3)))
			{
				break;
			}
		}

	}
}
