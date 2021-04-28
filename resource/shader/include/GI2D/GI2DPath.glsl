#ifndef GI2D_Path_
#define GI2D_Path_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require
#extension GL_EXT_scalar_block_layout : require
#ifdef USE_GI2D_Path

struct PathData_Work
{
	i16vec2 pos;
//	uint dir_type:4;
//	uint cost:28;
	uint data;
};

struct PathData
{
//	uint cost:28;
//	uint dir_type:4;
	uint data;
};

const ivec2 g_neighbor[8] = 
{
	ivec2(-1,-1), 
	ivec2( 0,-1),
	ivec2( 1,-1),
	ivec2( 1, 0),
	ivec2( 1, 1),
	ivec2( 0, 1),
	ivec2(-1, 1),
	ivec2(-1, 0),
};


uint PathData_dirtype(in PathData d)
{
	return d.data&0x07;
}
#define NodeSize (1024 * 16)
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
	PathData b_path_data[];
};
layout(set=USE_GI2D_Path, binding=4, scalar) buffer PathBuffer 
{
	uint8_t b_parent[];
};
layout(set=USE_GI2D_Path, binding=5, scalar) restrict buffer PathNodeOpenBuffer {
	i16vec2 b_open[];
};
layout(std430, set=USE_GI2D_Path, binding=6) restrict buffer PathNodeOpenCounter {
	ivec4 b_open_counter[4];
};
#endif

#endif //GI2D_Path_