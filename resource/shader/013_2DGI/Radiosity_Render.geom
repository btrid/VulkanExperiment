#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

#define max_vertex 8
//#define invocation_num 16
layout(points, invocations = (Vertex_Num+(max_vertex-1))/max_vertex) in;
layout(triangle_strip, max_vertices = max_vertex*2+2) out;


layout(location=0)in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location=1) in InData
{
    flat vec3 color;
	flat uint vertex_index;
} gs_in[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1)out OutData
{
	flat u16vec2 center;
	vec3 color;
}gs_out;

void main()
{
	if(dot(gs_in[0].color, gs_in[0].color) <= 0.01){ return ;}
	uint index = gs_in[0].vertex_index;
	vec3 color = gs_in[0].color;

	u16vec2 pos = b_vertex_array[index].pos;
	vec2 c = vec2(pos) + vec2(0.5);

	vec2 center = c / vec2(u_gi2d_info.m_resolution.xy) * 2. - 1.;

	uint begin = max_vertex * gl_InvocationID;
	uint end = min(max_vertex * (gl_InvocationID+1), Vertex_Num);

	for(uint i = begin; i < end; i++)
	{
		uint angle_index = i;
		u16vec2 target = b_vertex_array[index].vertex[angle_index].pos;
		vec2 v = vec2(target) + vec2(0.5);
		gl_Position = vec4((v / vec2(u_gi2d_info.m_resolution.xy)) * 2. - 1., 0., 1.);
		gs_out.color = color;
		gs_out.center = pos;
		EmitVertex();

		gl_Position = vec4(center, 0., 1.);
		gs_out.color = color;
		gs_out.center = pos;
		EmitVertex();
	}

	uint angle_index = end % Vertex_Num;
	u16vec2 target = b_vertex_array[index].vertex[angle_index].pos;
	vec2 v = vec2(target) + vec2(0.5);
	gl_Position = vec4((v / vec2(u_gi2d_info.m_resolution.xy)) * 2. - 1., 0., 1.);
	gs_out.color = color;
	gs_out.center = pos;
	EmitVertex();

	gl_Position = vec4(center, 0., 1.);
	gs_out.color = color;
	gs_out.center = pos;
	EmitVertex();

	EndPrimitive();

}
