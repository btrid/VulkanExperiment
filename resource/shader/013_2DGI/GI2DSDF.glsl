#ifndef GI2D_SDF_
#define GI2D_SDF_

#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_shader_draw_parameters : require
#extension GL_NV_shader_atomic_int64 : require

struct D2JFACell
{
	i16vec2 nearest[2];
//	i16vec2 e_nearest_index;
//	float distance;
//	float e_distance;
};


#if defined(USE_GI2D_SDF)
layout(set=USE_GI2D_SDF, binding=0, std430) restrict buffer JFABuffer {
	D2JFACell b_jfa[];
};
layout(set=USE_GI2D_SDF, binding=1, std430) restrict buffer SDFBuffer {
	vec2 b_sdf[];
};
#endif

#endif //GI2D_SDF_
