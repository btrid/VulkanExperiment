#ifndef GI2D_Path_
#define GI2D_Path_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#ifdef USE_GI2D_Path

struct PathNode
{
//	uint cost:28;
//	uint dir_type:4;
	uint data;
};

layout(std430, set=USE_GI2D_Path, binding=0) restrict buffer PathConnectBuffer {
	uint b_connect;
};
layout(std430, set=USE_GI2D_Path, binding=1) restrict buffer PathAccessBuffer {
	uint b_closed[];
};
layout(std430, set=USE_GI2D_Path, binding=2) restrict buffer PathNeibghborStateBuffer {
	// 壁ならbitが立つ
	uint8_t b_neighbor[];
};
layout(std430, set=USE_GI2D_Path, binding=3) restrict buffer PathCostBuffer {
	uint b_cost[];
};
layout(std430, set=USE_GI2D_Path, binding=4) restrict buffer PathParentBuffer {
	i16vec2 b_parent[];
};
layout(std430, set=USE_GI2D_Path, binding=5) restrict buffer PathNodeBuffer {
	PathNode b_node[];
};
layout(std430, set=USE_GI2D_Path, binding=6) buffer NeighborTableBuffer {
	// 壁ならbitが立つ
	uint8_t b_neighbor_table[2048];
};
#endif

#endif //GI2D_Path_