#ifndef GI2D_SDF_
#define GI2D_SDF_

#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_shader_draw_parameters : require
#extension GL_NV_shader_atomic_int64 : require

#if defined(USE_GI2D_SDF)
layout(set=USE_GI2D_SDF, binding=0, std430) restrict buffer JFABuffer {
#if defined(USE_GI2D_SDFEX) 
	ivec4 b_jfa_ex[];
#else
	i16vec2 b_jfa[];
#endif
};
layout(set=USE_GI2D_SDF, binding=1, std430) restrict buffer SDFBuffer {
	float b_sdf[];
};

layout(set=USE_GI2D_SDF, binding=2, std430) restrict buffer EdgeBuffer {
	uint8_t b_edge[];
};

#endif

#endif //GI2D_SDF_
