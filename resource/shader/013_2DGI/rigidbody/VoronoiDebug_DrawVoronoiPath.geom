#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#define USE_Voronoi 0
#include "Voronoi.glsl"

layout(points) in;
layout(line_strip, max_vertices = 2) out;


layout(location=0)in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) in A
{
	flat uint vertex_index;
}gsin[];

layout(push_constant) uniform Input
{
	int id;
} constant;

void main() 
{

	{
		int16_t a = int16_t(gsin[0].vertex_index);
		int16_t b = b_voronoi_path[gsin[0].vertex_index];
		b = b == -1s ? a : b;

		vec2 v1 = vec2(b_voronoi_cell[int(a)].point) / 1024.;
		vec2 v2 = vec2(b_voronoi_cell[int(b)].point) / 1024.;

		gl_Position = vec4(v1*2. - 1., 0., 1.);
		EmitVertex();

		gl_Position = vec4(v2*2. - 1., 0., 1.);
		EmitVertex();
	}
	EndPrimitive();

}
