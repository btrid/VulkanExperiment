#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

#define invocation_num 4
layout(points, invocations = invocation_num) in;
layout(triangle_strip, max_vertices = 2*Vertex_Num/invocation_num+2+2) out;


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

	uint num = Vertex_Num / invocation_num;

	for(uint _i = 0; _i < num; _i++)
	{
		uint angle_index = _i + num * gl_InvocationID;
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

	uint angle_index = (num * (gl_InvocationID+1)) % Vertex_Num;
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

	if(gl_InvocationID == invocation_num-1 && Vertex_Num%invocation_num != 0)
	{
		uint angle_index = 0;
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

	EndPrimitive();

}
