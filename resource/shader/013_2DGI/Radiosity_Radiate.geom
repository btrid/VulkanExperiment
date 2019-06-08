#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

layout(points, invocations = 1) in;
layout(triangle_strip, max_vertices = 2*DIR_NUM) out;
//layout(points, invocations = DIR_NUM) in;
//layout(triangle_strip, max_vertices = 3) out;


layout(location=0)in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location=1) in Data
{
	flat uint index;
} gs_in[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1)out OutData
{
	vec4 color;
};
void main() 
{
	uint index = gs_in[0].index;
	u16vec2 pos = b_vertex_array[index].pos;
	vec2 center = (pos / 1024.) * 2. - 1.;
#if 0
	int i = gl_InvocationID;
	{
		gl_Position = vec4(center, 0., 1.);
		EmitVertex();

		vec2 v1 = vec2(b_vertex_array[index].vertex[i]) / 1024.;
		vec2 v2 = vec2(b_vertex_array[index].vertex[(i+1)%32]) / 1024.;

		gl_Position = vec4(v1*2. - 1., 0., 1.);
		EmitVertex();

		gl_Position = vec4(v2*2. - 1., 0., 1.);
		EmitVertex();
	}
	EndPrimitive();
#else
	for(int i = 0; i < DIR_NUM; i++)
	{
		vec2 v1 = vec2(b_vertex_array[index].vertex[i]) / 1024.;
		gl_Position = vec4(v1*2. - 1., 0., 1.);
		EmitVertex();

		gl_Position = vec4(center, 0., 1.);
		EmitVertex();
	}
	EndPrimitive();
#endif

}
