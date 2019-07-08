#ifndef Radiosity_
#define Radiosity_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#ifdef USE_GI2D_Radiosity
struct GI2DRadiosityInfo
{
	uint ray_num_max;
	uint ray_frame_max;
	uint frame_max;
	uint a2;
};

struct DrawCommand
{
	uint vertexCount;
	uint instanceCount;
	uint firstVertex;
	uint firstInstance;

	uvec4 bounce_cmd;

};
#define Dir_Num (33)
#define Bounce_Num (3)

struct VertexInfo
{
//	u16vec4 pos;
	uvec2 id;
};
#define RV_FLAG_IS_BOUNDARY 1
struct RadiosityVertex
{
	VertexInfo vertex[Dir_Num];
	u16vec2 pos;
	uint flag;
	f16vec3 radiance[2];
	f16vec3 albedo;

};

layout(set=USE_GI2D_Radiosity, binding=0, std140) uniform GI2DRadiosityInfoUniform {
	GI2DRadiosityInfo u_radiosity_info;
};
layout(set=USE_GI2D_Radiosity, binding=1, std430) restrict buffer VertexArrayCounter {
	DrawCommand b_vertex_array_counter;
};
layout(set=USE_GI2D_Radiosity, binding=2, std430) restrict buffer VertexArrayIndexBuffer {
	uint b_vertex_array_index[];
};
layout(set=USE_GI2D_Radiosity, binding=3, std430) restrict buffer VertexArrayBuffer {
	RadiosityVertex b_vertex_array[];
};
layout(set=USE_GI2D_Radiosity, binding=4, std430) restrict buffer MapEdgeBuffer {
	uint64_t b_edge[];
};


#endif
#endif //Radiosity_