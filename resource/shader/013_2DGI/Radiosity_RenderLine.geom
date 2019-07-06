#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

#define invocation_num (3)
layout(points, invocations = invocation_num) in;
layout(line_strip, max_vertices = Vertex_Num/invocation_num+2) out;
//layout(triangle_strip, max_vertices = 3) out;


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
	if(dot(gs_in[0].color, gs_in[0].color) <= 0.001){ return ;}
	uint index = gs_in[0].vertex_index;

	for(uint i = gl_InvocationID; i < Dir_Num; i+=invocation_num)
	{
		uint angle_index = i;
		u16vec2 target0 = b_vertex_array[index].vertex[angle_index].pos;
		u16vec2 target1 = b_vertex_array[index].vertex[angle_index+Dir_Num].pos;
		vec4 vertex = ((vec4(target0, target1) + vec4(0.5)) / vec4(u_gi2d_info.m_resolution.xyxy)) * 2. - 1.;

		gl_Position = vec4(vertex.xy, 0., 1.);
		gs_out.color = gs_in[0].color;
		EmitVertex();

		gl_Position = vec4(vertex.zw, 0., 1.);
		gs_out.color = gs_in[0].color;
		EmitVertex();
		EndPrimitive();

	}

}
