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

void main()
{
	const ivec4 reso = u_gi2d_info.m_resolution;
	ivec2 pos = ivec2(constant.pos.xy);
	if(gl_VertexIndex==0||gl_VertexIndex==1024*4+1)
	{
		gl_Position = vec4(vec2(pos) / reso.xy * 2. - 1., 0., 1.);
		return;
	}

	ivec2 target;
	{
		uint targetID = (gl_VertexIndex-1)%1023;
		uint targetType = (gl_VertexIndex-1)/1023;
		switch(targetType)
		{
			case 0:
				target = ivec2(targetID, 0);
				break;
			case 1:
				target = ivec2(1023, targetID);
				break;
			case 2:
				target = ivec2((reso.x-1)-targetID, 1023);
				break;
			case 3:
				target = ivec2(0, (reso.y-1)-targetID);
				break;
		}
	}

	gl_Position = vec4(vec2(target) / reso.xy * 2. - 1., 0., 1.);
	if(all(equal(target, pos)))
	{ 
		return;
	}

	// ライトの影響が小さすぎるところはしない
//	float cutoff = 0.001;
//	float dist = distance(vec2(target),vec2(pos));
//	int p = int(1.+ dist * dist * 0.01)+1;
	int p = 200;

	ivec2 delta = abs(target - pos);
	ivec3 _dir = sign(ivec3(target, 0) - ivec3(pos, 0));

	int axis = delta.x >= delta.y ? 0 : 1;
	ivec2 dir[2];
	dir[0] = _dir.xz;
	dir[1] = _dir.zy;
	int	D = 2 * delta[1 - axis] - delta[axis];
	uint64_t map = 0ul;
	ivec2 cell = ivec2(999999999);
	for(;any(notEqual(pos, target));)
//	for(;p!=0;p--)
	{
		for(;;)
		{
			ivec2 cell_sub = pos%8;
			bool attr = (map & (1ul<<(cell_sub.x+cell_sub.y*8))) != 0ul;
			if(attr || p<0)
			{
				gl_Position = vec4(pos / vec2(reso.xy) * 2. - 1., 0., 1.);
				return;
			}

			if (D > 0)
			{
				p--;
				pos += dir[1 - axis];
				D = D + (2 * delta[1 - axis] - 2 * delta[axis]);
			}
			else
			{
				D = D + 2 * delta[1 - axis];
			}
			p--;
			pos += dir[axis];

			if(any(notEqual(cell, pos>>3)))
			{
				break;
			}
		}
		cell = pos>>3;
		map = b_fragment_map[cell.x + cell.y * reso.z];
	}
	gl_Position = vec4(pos / vec2(reso.xy) * 2. - 1., 0., 1.);

}