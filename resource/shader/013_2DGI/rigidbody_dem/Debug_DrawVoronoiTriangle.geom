#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#define USE_Rigidbody2D 0
#include "Rigidbody2D.glsl"

layout(points) in;
layout(triangle_strip, max_vertices = 16) out;


layout(location=0)in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(push_constant) uniform Input
{
	int id;
} constant;

void main() 
{
	for(int i = 0; i < b_voronoi_vertex[constant.id].num; i++)
	{
		vec2 v = vec2(b_voronoi_vertex[constant.id].vertex[i]) / 1024.;
		gl_Position = vec4(v*2. - 1., 0., 1.);
		EmitVertex();
	}
/*
	gl_Position = vec4(0., 0., 0., 1.);
	EmitVertex();
	gl_Position = vec4(0., 1., 0., 1.);
	EmitVertex();
	gl_Position = vec4(1., 0., 0., 1.);
	EmitVertex();
*/
	EndPrimitive();

}
