#ifndef GI2D_SDF_
#define GI2D_SDF_

#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_shader_draw_parameters : require
#extension GL_NV_shader_atomic_int64 : require

#if defined(USE_GI2D_SDF)
layout(set=USE_GI2D_SDF, binding=0, std430) restrict buffer JFABuffer {
	i16vec2 b_jfa[];
};
layout(set=USE_GI2D_SDF, binding=1, std430) restrict buffer SDFBuffer {
	vec2 b_sdf[];
};
#endif

#endif //GI2D_SDF_
