#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

layout(points, invocations = Dir_Num) in;
layout(line_strip, max_vertices = 3) out;


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
	if(dot(gs_in[0].color, gs_in[0].color) == 0.){ return ;}
	uint index = gs_in[0].vertex_index;
	uint angle_index = gl_InvocationID;

	u16vec2 pos = b_vertex_array[index].pos;
	i16vec2 target0 = b_vertex_array[index].vertex[angle_index*2] & i16vec2(0x7fff);
	i16vec2 target1 = b_vertex_array[index].vertex[angle_index*2+1] & i16vec2(0x7fff);
	vec2 center = (vec2(pos) + vec2(0.5)) / vec2(u_gi2d_info.m_resolution.xy) * 2. - 1.;
	vec4 vertex = ((vec4(target0, target1) + vec4(0.5)) / vec4(u_gi2d_info.m_resolution.xyxy)) * 2. - 1.;

	gl_Position = vec4(vertex.xy, 0., 1.);
	gs_out.color = gs_in[0].color;
	EmitVertex();

	gl_Position = vec4(center.xy, 0., 1.);
	gs_out.color = gs_in[0].color;
	EmitVertex();

	gl_Position = vec4(vertex.zw, 0., 1.);
	gs_out.color = gs_in[0].color;
	EmitVertex();
	EndPrimitive();

}
