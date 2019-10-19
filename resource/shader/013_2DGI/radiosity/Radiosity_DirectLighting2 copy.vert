#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D/GI2D.glsl"
#include "GI2D/Radiosity.glsl"

#define dir_reso_bit (12)
#define dir_reso (4096)
layout(push_constant) uniform Constant
{
	vec4 pos;
	vec4 color;
} constant;

layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};

ivec2 calcDirEx(in vec2 pos, in vec2 target)
{
	#define GI2D_FLT_EPSILON 0.00001
	vec2 f_dir = target - pos;
	f_dir.x = abs(f_dir.x)<GI2D_FLT_EPSILON ? (f_dir.x >= 0. ? GI2D_FLT_EPSILON : -GI2D_FLT_EPSILON) : f_dir.x;
	f_dir.y = abs(f_dir.y)<GI2D_FLT_EPSILON ? (f_dir.y >= 0. ? GI2D_FLT_EPSILON : -GI2D_FLT_EPSILON) : f_dir.y;
	vec2 f_inv_dir = 1./f_dir;
	f_dir = f_dir * min(abs(f_inv_dir.x), abs(f_inv_dir.y));

	return ivec2(round(f_dir * dir_reso));
//	dir = max(dir, ivec2(1)); // 0除算回避

}

void main()
{
	const ivec4 reso = u_gi2d_info.m_resolution;
	vec2 pos = constant.pos.xy;
	if(gl_VertexIndex==0||gl_VertexIndex==1024*4+1)
	{
		gl_Position = vec4(pos / reso.xy * 2. - 1., 0., 1.);
		return;
	}

	vec2 target;
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
		target += vec2(0.5);
	}

	if(all(equal(target, pos)))
	{ 
		gl_Position = vec4(target / reso.xy * 2. - 1., 0., 1.);
		return;
	}
	ivec2 dir = calcDirEx(pos, target);
	pos = ivec2(pos*dir_reso);

	for(int march = 1; march <3000; )
	{
		ivec2 mi = ivec2(pos + dir * march) >> dir_reso_bit;
		ivec2 cell = mi>>3;
		uint64_t map = b_fragment_map[cell.x + cell.y * reso.z];

		for(;;)
		{
			ivec2 cell_sub = mi%8;
			bool attr = (map & (1ul<<(cell_sub.x+cell_sub.y*8))) != 0ul;
			if(attr)
			{
				gl_Position = vec4(mi / vec2(reso.xy) * 2. - 1., 0., 1.);
				return;
			}
			march++;

			mi = ivec2(pos + dir * march) >> dir_reso_bit;
			if(any(notEqual(cell, mi>>3)))
			{
				break;
			}
		}
	}
//	while(true);

}