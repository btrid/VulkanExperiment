#ifndef GI2D_Radiosity3_
#define GI2D_Radiosity3_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#ifdef USE_GI2D_Radiosity3
layout(std430, set=USE_GI2D_Radiosity3, binding=0) restrict buffer PathAccessBuffer {
	uint b_closed[];
};
layout(std430, set=USE_GI2D_Radiosity3, binding=1) restrict buffer PathNeibghborStateBuffer {
	// 壁ならbitが立つ
	uint8_t b_neighbor[];
};
layout(std430, set=USE_GI2D_Radiosity3, binding=2) restrict buffer RadianceBuffer {
	f16vec3 b_radiance[];
};
#endif

#endif //GI2D_Radiosity3_