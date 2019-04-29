#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#define USE_Rigidbody2D 0
#include "Rigidbody2D.glsl"

layout(points, invocations = VoronoiVertex_MAX) in;
layout(triangle_strip, max_vertices = 3) out;


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
	vec2 center = vec2(b_voronoi_cell[constant.id].point) / 1024. * 2. - 1.;
	int num = b_voronoi_vertex[constant.id].num;
//	for(int i = 0; i < num; i+=1)
	int i = gl_InvocationID;
	if(i >= num) { return; }
	{
		gl_Position = vec4(center, 0., 1.);
		EmitVertex();

		vec2 v1 = vec2(b_voronoi_vertex[constant.id].vertex[i]) / 1024.;
		gl_Position = vec4(v1*2. - 1., 0., 1.);
		EmitVertex();

		vec2 v2 = vec2(b_voronoi_vertex[constant.id].vertex[(i+1)%num]) / 1024.;
		gl_Position = vec4(v2*2. - 1., 0., 1.);
		EmitVertex();
	}

	EndPrimitive();
/*
	gl_Position = vec4(0., 0., 0., 1.);
	EmitVertex();
	gl_Position = vec4(0., 1., 0., 1.);
	EmitVertex();
	gl_Position = vec4(1., 0., 0., 1.);
	EmitVertex();
*/

}
