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
	vec2 target;
	uint targetID = gl_VertexIndex%1024;
	if(gl_VertexIndex < 1024){
		target = vec2(targetID, 0);
	}
	else if(gl_VertexIndex < 1024+1023){
		target = vec2(1023, 1+targetID);
	}
	else if(gl_VertexIndex < 1024+1023+1023){
		target = vec2(1024-targetID, 1023);
	}
	else if(gl_VertexIndex < 1024+1023+1023+1022){
		target = vec2(0, 1023-targetID);
	}
	target += 0.5;

	vec2 dir = calcDirEx(constant.pos.xy, target);

	for(int i = 1; i++ <10000; i++)
	{
		ivec2 mi = ivec2(constant.pos.xy + dir*i);
		ivec2 cell = mi>>3;
		uint64_t map = b_fragment_map[cell.x + cell.y * reso.z];

		for(;;)
		{
			ivec2 cell_sub = mi%8;
			bool attr = (map & (1ul<<(cell_sub.x+cell_sub.y*8))) != 0ul;
			if(attr)
			{
				gl_Position = vec4(mi, 0., 1.);
				i = 99999999;
				break;
			}
			i++;

			mi = ivec2(constant.pos.xy + dir*i);
			if(any(notEqual(cell, mi>>3)))
			{
				break;
			}
		}
	}

}