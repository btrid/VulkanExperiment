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

vec2 calcDirEx(in vec2 pos, out vec2 target)
{
	#define GI2D_FLT_EPSILON 0.00001
	vec2 f_dir = target - pos;
	f_dir.x = abs(f_dir.x)<GI2D_FLT_EPSILON ? (f_dir.x >= 0. ? GI2D_FLT_EPSILON : -GI2D_FLT_EPSILON) : f_dir.x;
	f_dir.y = abs(f_dir.y)<GI2D_FLT_EPSILON ? (f_dir.y >= 0. ? GI2D_FLT_EPSILON : -GI2D_FLT_EPSILON) : f_dir.y;
	vec2 f_inv_dir = 1./f_dir;
	f_dir = f_dir * min(abs(f_inv_dir.x), abs(f_inv_dir.y));

	return f_dir;
}

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
//				gl_Position = vec4(target / reso.xy * 2. - 1., 0., 1.);return;
				break;
			case 1:
				target = vec2(1023, targetID);
//				gl_Position = vec4(target / reso.xy * 2. - 1., 0., 1.);return;
				break;
			case 2:
				target = vec2((reso.x-1)-targetID, 1023);
//				gl_Position = vec4(target / reso.xy * 2. - 1., 0., 1.);return;
				break;
			case 3:
				target = vec2(0, (reso.y-1)-targetID);
//				gl_Position = vec4(target / reso.xy * 2. - 1., 0., 1.);return;
				break;
		}	
	}
	target += 0.5;

	vec2 dir = normalize(target - pos);
	gl_Position = vec4(pos / reso.xy * 2. - 1., 0., 1.);

	for(int march = 1; march <2000; march++)
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
				gl_Position = vec4(ivec2(pos + dir*march) / vec2(reso.xy) * 2. - 1., 0., 1.);
				return;
			}
			march++;

			mi = ivec2(pos + dir*march);
//			gl_Position = vec4(vec2(pos + dir*march) / reso.xy * 2. - 1., 0., 1.);
			if(any(notEqual(cell, mi>>3)))
			{
				break;
			}
		}
	}

}