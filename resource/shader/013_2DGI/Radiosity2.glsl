#ifndef Radiosity2_
#define Radiosity2_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#ifdef USE_GI2D_Radiosity2

#define Dir_Num (33)
#define Bounce_Num (3)

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

layout(set=USE_GI2D_Radiosity2, binding=0, std140) uniform GI2DRadiosityInfoUniform {
	GI2DRadiosityInfo u_radiosity_info;
};
layout(set=USE_GI2D_Radiosity2, binding=1, std430) restrict buffer SegmentCounterBuffer {
	SegmentCounter b_segment_counter;
};
layout(set=USE_GI2D_Radiosity2, binding=2, std430) restrict buffer SegmentBuffer {
	Segment b_segment[];
};
layout(set=USE_GI2D_Radiosity2, binding=3, std430) restrict buffer RadianceBuffer {
	uint64_t b_radiance[];
};
layout(set=USE_GI2D_Radiosity2, binding=4, std430) restrict buffer MapEdgeBuffer {
	uint64_t b_edge[];
};

layout(set=USE_GI2D_Radiosity2, binding=5) uniform sampler2D s_radiosity[4];

#endif
#endif //Radiosity2_