#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D/GI2D.glsl"
#include "GI2D/Radiosity.glsl"

layout(push_constant) uniform Constant
{
	vec4 pos;
	vec4 color;
} constant;

layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};

void main()
{
	const ivec4 reso = u_gi2d_info.m_resolution;
	vec2 pos = constant.pos.xy;
	vec2 target;
//	pos = vec2(333.);
//	target = vec2(888.);
	if(gl_VertexIndex==0||gl_VertexIndex==1024*4+1)
	{
		gl_Position = vec4(pos / reso.xy * 2. - 1., 0., 1.);
		return;
	}
	else 
	{
		uint targetID = (gl_VertexIndex-1)%1023;
		uint targetType = (gl_VertexIndex-1)/1023;
		switch(targetType)
		{
			case 0:
				target = vec2(targetID, 0);
				break;
			case 1:
				target = vec2(1023, targetID);
				break;
			case 2:
				target = vec2((reso.x-1)-targetID, 1023);
				break;
			case 3:
				target = vec2(0, (reso.y-1)-targetID);
				break;
		}	
	}
	target += 0.5;

	if(distance(target, pos) <= 0.00001)
	{ 
		gl_Position = vec4(target / reso.xy * 2. - 1., 0., 1.);
		return;
	}
	vec2 dir = normalize(target - pos);
	for(int march = 1; march <2000; )
	{
		ivec2 mi = ivec2(pos + dir*march);
		ivec2 cell = mi>>3;
		uint64_t map = b_fragment_map[cell.x + cell.y * reso.z];

		for(;;)
		{
			ivec2 cell_sub = mi%8;
			bool attr = (map & (1ul<<(cell_sub.x+cell_sub.y*8))) != 0ul;
			if(attr)
			{
				gl_Position = vec4((pos + dir*march) / vec2(reso.xy) * 2. - 1., 0., 1.);
				return;
			}
			march++;

			mi = ivec2(pos + dir*march);
			if(any(notEqual(cell, mi>>3)))
			{
				break;
			}
		}
	}
//	while(true);

}