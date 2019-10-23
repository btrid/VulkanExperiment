#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D/GI2D.glsl"
#include "GI2D/Radiosity.glsl"

layout(location = 0)in ivec2 in_pos;
layout(location = 1)in vec4 in_color;

layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location=1) out Vertex{
	flat ivec2 pos;
	flat vec3 color;
}vs_out;

void main()
{
	const ivec4 reso = u_gi2d_info.m_resolution;
	vs_out.pos = ivec2(in_pos);
	vs_out.color = vec3(in_color.xyz);

	ivec2 pos = in_pos;
	if(gl_VertexIndex==0)
	{
		gl_Position = vec4(vec2(pos) / reso.xy * 2. - 1., 0., 1.);
		return;
	}

	ivec2 target = ivec2(0);
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
	pos += sign(target - pos);
	ivec2 delta = abs(target - pos);

	// ライトの影響が小さすぎるところはしない
//	float cutoff = 0.001;
//	float dist = distance(vec2(target),vec2(pos));
//	int p = int(1.+ dist * dist * 0.01)+1;
	int p = 200;

	ivec3 _dir = sign(ivec3(target, 0) - ivec3(pos, 0));
	ivec2 d[2];
	d[0] = _dir.xz;
	d[1] = _dir.zy;

	int axis = delta.x > delta.y ? 0 : 1;
	ivec2 dir[2];
	dir[0] = d[1-axis]+d[axis];
	dir[1] = d[axis];
	int	D = 2 * delta[1 - axis] - delta[axis];
	int deltax = 2 * delta[1 - axis] - 2 * delta[axis];
	int deltay = 2 * delta[1 - axis];

	uint64_t map = 0ul;
	ivec2 cell = ivec2(999999999);
	for(;p>=0;)
	{
		if(any(notEqual(cell, pos>>3)))
		{
			cell = pos>>3;
			map = b_fragment_map[cell.x + cell.y * reso.z];
		}

		ivec2 cell_sub = pos-(cell<<3);
		if((map & (1ul<<(cell_sub.x+cell_sub.y*8))) != 0ul)
		{
			break;
		}

		D += D>0 ? deltax : deltay;
		p -= D>0 ? 2 : 1;
		pos += D>0 ? dir[0] : dir[1];
	}
	gl_Position = vec4((pos/dvec2(reso.xy)) * 2. -1., 0., 1.);

}