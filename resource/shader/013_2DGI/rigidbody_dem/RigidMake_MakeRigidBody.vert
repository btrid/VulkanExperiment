#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#define USE_Rigidbody2D 0
#define USE_GI2D 1
#define USE_MakeRigidbody 2
#include "GI2D.glsl"
#include "Rigidbody2D.glsl"


layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out A
{
	flat uint id;
	flat uint vertex_index;
	flat i16vec4 voronoi_minmax;
};

uint align(in uint value, in uint align_num)
{
	return (value + (align_num-1)) & ~(align_num -1);
}

void main()
{
	gl_Position = vec4(1.0);

	id = b_make_param.destruct_voronoi_id;
	vertex_index = gl_VertexIndex;
	voronoi_minmax = b_voronoi_polygon[id].minmax;

	b_make_rigidbody.size_min = voronoi_minmax.xy;
	b_make_rigidbody.size_max = voronoi_minmax.zw;

	b_make_param.rb_size = voronoi_minmax.xy - voronoi_minmax.xy;

}
