#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D/GI2D.glsl"
#include "GI2D/Radiosity.glsl"

layout(location = 0)in ivec2 in_pos;
layout(location = 1)in uvec2 in_flag; //[mesh_index, vertex_num]
layout(location = 2)in vec4 in_color;


layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location=1) out Vertex{
	flat ivec2 pos;
	flat vec3 color;
}vs_out;


bool contains(in ivec4 aabb, in ivec2 p0, in ivec2 p1) 
{
	vec2 line[] = {
		aabb.xy - aabb.zy,
		aabb.zy - aabb.zw,
		aabb.zw - aabb.xw,
		aabb.xw - aabb.xy,
	};
	bool inner = false;
	for (int i = 0; i<4; i++ )
	{
		inner = inner || cross(vec3(line[i], 0.f), vec3(p0, 0.)).z >= 0.;
		inner = inner || cross(vec3(line[i], 0.f), vec3(p1, 0.)).z >= 0.;
	}
	return inner;
}
void main()
{
	const ivec4 reso = u_gi2d_info.m_resolution;
	vs_out.pos = ivec2(in_pos);
	vs_out.color = vec3(in_color.xyz);

	ivec2 pos = in_pos;
	gl_Position = vec4(vec2(pos+0.5) / reso.xy * 2. - 1., 0., 1.);
	if(gl_VertexIndex==0)
	{
		return;
	}

	ivec2 target = ivec2(0);
	{
		if(in_flag.y == 0xffff)
		{
			// 画面端にまで届くような光の場合
			uint target_ID = (gl_VertexIndex-1)%1023;
			uint target_type = (gl_VertexIndex-1)/1023;
			switch(target_type)
			{
				case 0: target = ivec2(target_ID, 0); break;
				case 1: target = ivec2(1023, target_ID); break;
				case 2: target = ivec2((reso.x-1)-target_ID, 1023); break;
				case 3: target = ivec2(0, (reso.y-1)-target_ID); break;
			}
		}
		else
		{
			// ある程度以下の光の場合
			uint vertex_num = in_flag.y;
			uint target_ID = (gl_VertexIndex-1)%vertex_num;
			uint target_type = (gl_VertexIndex-1)/vertex_num;
			int vertex_index = int(((target_type%2)==0)?target_ID:(vertex_num-target_ID));
			int target2 = u_circle_mesh_vertex[in_flag.x][vertex_index/4][vertex_index%4];
			target = (ivec2(target2)>>ivec2(0, 16)) & ivec2(0xffff);
			switch(target_type%8)
			{
				case 0: target = ivec2( target.x, target.y); break;
				case 1: target = ivec2( target.y, target.x); break;
				case 2: target = ivec2(-target.y, target.x); break;
				case 3: target = ivec2(-target.x, target.y); break;
				case 4: target = ivec2(-target.x,-target.y); break;
				case 5: target = ivec2(-target.y,-target.x); break;
				case 6: target = ivec2( target.y,-target.x); break;
				case 7: target = ivec2( target.x,-target.y); break;
			}
			target += pos;

			if(!contains(ivec4(0, 0, reso.xy-1), pos, target))
			{
				// 画面外の場合はレイを飛ばさない
				//return; // 効果少ない？
			}
		}
	}
	pos += sign(target-pos);

	ivec3 _dir = sign(ivec3(target, 0) - ivec3(pos, 0));
	ivec2 dir[2];
	dir[0] = _dir.xz;
	dir[1] = _dir.zy;

	ivec2 delta = abs(target - pos);
	int axis = delta.x > delta.y ? 0 : 1;
	int	D = 2 * delta[1 - axis] - delta[axis];
	int deltax = 2 * delta[1 - axis] - 2 * delta[axis];
	int deltay = 2 * delta[1 - axis];

	uint64_t map = 0ul;
	ivec2 cell = ivec2(999999999);

	for(;!all(equal(pos, target));)
	{
		if(any(notEqual(cell, pos>>3)))
		{
			cell = pos>>3;
			map = b_fragment_map[cell.x + cell.y * reso.z];
		}

		ivec2 cell_sub = pos%8;
		if((map & (1ul<<(cell_sub.x+cell_sub.y*8))) != 0ul)
		{
//			break;
		}

		if (D > 0)
		{
			pos += dir[1 - axis];
		}
		pos += dir[axis];
		D += D>0 ? deltax : deltay;
	}
	gl_Position = vec4(vec2(pos+0.5)/reso.xy * 2. -1., 0., 1.);

}