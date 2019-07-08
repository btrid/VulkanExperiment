#ifndef Radiosity2_
#define Radiosity2_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#ifdef USE_GI2D_Radiosity2

#define Bounce_Num (3)

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
	uint64_t radiance;
};

layout(set=USE_GI2D_Radiosity, binding=0, std430) restrict buffer SegmentCounter {
	SegmentCounter b_segment_counter;
};
layout(set=USE_GI2D_Radiosity, binding=1, std430) restrict buffer SegmentBuffer {
	Segment b_segment[];
};

#endif
#endif //Radiosity2_