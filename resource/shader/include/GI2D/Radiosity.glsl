#ifndef Radiosity_
#define Radiosity_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#ifdef USE_GI2D_Radiosity

#define Dir_Num (45)
#define Bounce_Num (2)
#define Frame_Num (4)

#define ColorShift (20)
#define ColorMask ((1<<ColorShift)-1)
struct GI2DRadiosityInfo
{
	uint ray_num_max;
	uint ray_frame_max;
	uint frame_max;
	uint frame;
};

struct SegmentCounter
{
	uint vertexCount;
	uint instanceCount;
	uint firstVertex;
	uint firstInstance;

	uvec4 bounce_cmd;
};


struct Segment
{
	u16vec4 pos;
};

layout(set=USE_GI2D_Radiosity, binding=0, std140) uniform GI2DRadiosityInfoUniform {
	GI2DRadiosityInfo u_radiosity_info;
};
layout(set=USE_GI2D_Radiosity, binding=1, std140) uniform MeshVertexCount {
	uvec4 u_circle_mesh_count[4];
};
layout(set=USE_GI2D_Radiosity, binding=2, std140) uniform MeshVertexBuffer {
	i16vec2 u_circle_mesh_vertex[512][16];
};
layout(set=USE_GI2D_Radiosity, binding=3, std430) restrict buffer SegmentCounterBuffer {
	SegmentCounter b_segment_counter;
};
layout(set=USE_GI2D_Radiosity, binding=4, std430) restrict buffer SegmentBuffer {
	Segment b_segment[];
};
layout(set=USE_GI2D_Radiosity, binding=5, std430) restrict buffer RadianceBuffer {
	uint64_t b_radiance[];
};
layout(set=USE_GI2D_Radiosity, binding=6, std430) restrict buffer MapEdgeBuffer {
	uint64_t b_edge[];
};
layout(set=USE_GI2D_Radiosity, binding=7, std430) restrict buffer AlbedoBuffer {
	f16vec4 b_albedo[];
};

struct EmissiveCounter
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};
struct Emissive
{
	i16vec2 pos;
	i16vec2 _p;
	f16vec4 color;
};

layout(set=USE_GI2D_Radiosity, binding=8, std430) restrict buffer EmissiveBuffer {
	Emissive b_emissive[];
};

layout(set=USE_GI2D_Radiosity, binding=9) uniform sampler2D s_radiosity[Frame_Num];


#endif
#endif //Radiosity2_