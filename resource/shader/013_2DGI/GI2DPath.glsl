#ifndef GI2D_Path_
#define GI2D_Path_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#ifdef USE_GI2D_Path
layout(std430, set=USE_GI2D_Path, binding=0) restrict buffer PathConnectBuffer {
	uint b_connect;
};
layout(std430, set=USE_GI2D_Path, binding=1) restrict buffer PathAccessBuffer {
	uint b_close[];
};

#endif

#endif //GI2D_Path_