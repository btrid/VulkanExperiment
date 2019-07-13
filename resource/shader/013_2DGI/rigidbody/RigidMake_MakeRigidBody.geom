#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#define USE_Rigidbody2D 0
#define USE_GI2D 1
#define USE_MakeRigidbody 2
#include "GI2D.glsl"
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

layout(location=1) in GS_IN
{
	flat uint id;
	flat uint vertex_index;
	flat i16vec4 voronoi_minmax;
}
in_data[];

layout(location=1) out GS_OUT
{
	flat i16vec4 voronoi_minmax;
}
out_data;


void main() 
{
	int num = b_voronoi_polygon[in_data[0].id].num;
	int i = gl_InvocationID;

	if(i >= num) { return; }

	out_data.voronoi_minmax = in_data[0].voronoi_minmax;

	{
		vec2 center = vec2(b_voronoi_cell[in_data[0].id].point) / 1024.;
		gl_Position = vec4(center * 2. - 1., 0., 1.);
		EmitVertex();

		vec2 v1 = vec2(b_voronoi_vertex[int(b_voronoi_polygon[in_data[0].id].vertex_index[i])].point) / 1024.;
		vec2 v2 = vec2(b_voronoi_vertex[int(b_voronoi_polygon[in_data[0].id].vertex_index[(i+1)%num])].point) / 1024.;

		gl_Position = vec4(v1* 2. - 1., 0., 1.);
		EmitVertex();

		gl_Position = vec4(v2*2. - 1., 0., 1.);
		EmitVertex();
	}

	EndPrimitive();

}
