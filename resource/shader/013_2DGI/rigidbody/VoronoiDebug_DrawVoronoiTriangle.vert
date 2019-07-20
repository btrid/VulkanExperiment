#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#define USE_Voronoi 0
#include "Voronoi.glsl"


layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out A
{
	flat uint vertex_index;
};


void main()
{
	gl_Position = vec4(1.0);
	vertex_index = gl_VertexIndex;
}
