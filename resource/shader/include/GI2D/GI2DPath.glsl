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
layout(std430, set=USE_GI2D_Path, binding=0) restrict buffer PathNeibghborStateBuffer {
	// 壁ならbitが立つ
	uint8_t b_neighbor[];
};
layout(std430, set=USE_GI2D_Path, binding=1) restrict buffer PathCostBuffer {
	PathData b_path_data[];
};

#endif


#endif //GI2D_Path_