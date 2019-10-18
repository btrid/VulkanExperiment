#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D/GI2D.glsl"
#include "GI2D/Radiosity.glsl"

layout(points, invocations = 1) in;
layout(line_strip, max_vertices = 2) out;


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
	vec3 color;
}gs_out;

void main()
{
	return;
/*	uint index = gs_in[0].vertex_index;
	uint angle_index = gl_InvocationID;

	u16vec2 pos = b_vertex_array[index].pos;
	u16vec2 target = b_vertex_array[index].vertex[angle_index];
	vec4 line_vertex = ((vec4(pos, target) + vec4(0.5)) / vec4(u_gi2d_info.m_resolution.xyxy)) * 2. - 1.;

	gl_Position = vec4(line_vertex.xy, 0., 1.);
	gs_out.color = gs_in[0].color;
	EmitVertex();

	gl_Position = vec4(line_vertex.zw, 0., 1.);
	gs_out.color = gs_in[0].color;
	EmitVertex();
	EndPrimitive();
*/
}
