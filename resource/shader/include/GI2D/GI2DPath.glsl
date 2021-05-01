#ifndef GI2D_Path_
#define GI2D_Path_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require
#extension GL_EXT_scalar_block_layout : require
#ifdef USE_GI2D_Path

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
uint PathData_cost(in PathData d)
{
	return (d.data>>4)&((1<<28)-1);
}

layout(set=USE_GI2D_Path, binding=0, scalar) restrict buffer PathNeibghborStateBuffer {
	// 壁ならbitが立つ
	uint8_t b_neighbor[];
};
layout(set=USE_GI2D_Path, binding=1, std430) restrict buffer PathCostBuffer {
	PathData b_path_data[];
};
layout(set=USE_GI2D_Path, binding=2, std430) restrict buffer PathOpenBuffer {
	i16vec2 b_open[];
};
layout(set=USE_GI2D_Path, binding=3, std430) restrict buffer PathOpenCount {
	int b_open_counter;
};

#endif


#endif //GI2D_Path_