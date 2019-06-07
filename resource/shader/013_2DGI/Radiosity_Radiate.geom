#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

layout(points, invocations = 32) in;
layout(triangle_strip, max_vertices = 3) out;
//layout(points, invocations = 1) in;
//layout(triangle_strip, max_vertices = 32*3) out;


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

layout(location=0)out OutData
{
	vec4 color;
};
void main() 
{
	u16vec2 pos = b_vertex_array[gs_in[0].index].pos;
	vec2 center = (pos / 1024.) * 2. - 1.;

//	for(int i = 0; i < 32; i++)
	int i = gl_InvocationID;
	{
		gl_Position = vec4(center, 0., 1.);
		EmitVertex();

		vec2 v1 = vec2(b_vertex_array[gs_in[0].index].vertex[i]) / 1024.;
		vec2 v2 = vec2(b_vertex_array[gs_in[0].index].vertex[(i+1)%32]) / 1024.;

		gl_Position = vec4(v1*2. - 1., 0., 1.);
		EmitVertex();

		gl_Position = vec4(v2*2. - 1., 0., 1.);
		EmitVertex();
	}
	EndPrimitive();

}
