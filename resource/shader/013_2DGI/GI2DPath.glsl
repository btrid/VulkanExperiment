#ifndef GI2D_Path_
#define GI2D_Path_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#ifdef USE_GI2D_Path
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
layout(std430, set=USE_GI2D_Path, binding=3) restrict buffer PathCloseStateBuffer {
	uint b_closed_state[];
};
#endif

#endif //GI2D_Path_