#ifndef GI2D_Path_
#define GI2D_Path_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#ifdef USE_GI2D_Path


const i16vec2 g_neighor_list[8] = 
{
	i16vec2(-1,-1), 
	i16vec2( 0,-1),
	i16vec2( 1,-1),
	i16vec2( 1, 0),
	i16vec2( 1, 1),
	i16vec2( 0, 1),
	i16vec2(-1, 1),
	i16vec2(-1, 0),
};

struct OpenNode
{
	i16vec2 pos;
	uint cost;
	uint dir_type;
};

struct Node
{
	uint cost;
	i16vec2 parent;
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
layout(std430, set=USE_GI2D_Path, binding=3) restrict buffer PathOpenNodeBuffer {
	uint b_cost[];
};
layout(std430, set=USE_GI2D_Path, binding=4) restrict buffer PathNodeBuffer {
	i16vec2 b_parent[];
};
#endif

#endif //GI2D_Path_