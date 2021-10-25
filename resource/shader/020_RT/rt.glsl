#ifndef RT_GLSL_
#define RT_GLSL_
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

#if defined(USE_SceneModel)
struct Entity
{
	uint64_t vertices;
	uint64_t indices;
	uint64_t materials;
	uint64_t materialIndices;
};
layout(set=USE_SceneModel, binding=0, scalar) buffer EntityBuffer { Entity b_entity[]; };
layout(set=USE_SceneModel, binding=1, buffer_reference, scalar) buffer V {vec3 b_v[]; };
layout(set=USE_SceneModel, binding=2, buffer_reference, scalar) buffer I {vec3 b_i[]; };
layout(set=USE_SceneModel, binding=3, buffer_reference, scalar) buffer M {int m[]; };
layout(set=USE_SceneModel, binding=4, buffer_reference, scalar) buffer MI {int mi[]; };
#endif // RT_GLSL_