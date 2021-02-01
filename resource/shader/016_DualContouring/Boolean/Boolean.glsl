#ifndef Boolean_H_
#define Boolean_H_

#extension GL_EXT_ray_query : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

#if defined(USE_Model)
layout(set=USE_Model,binding=0, std140) uniform InfoUniform {Info u_info; };
layout(set=USE_Model,binding=1, scalar) buffer Vertices { vec3 b_vertex[]; };
layout(set=USE_Model,binding=2, scalar) buffer Normals { vec3 b_normal[]; };
layout(set=USE_Model,binding=3, scalar) buffer Indices { uvec3 b_index[]; };
layout(set=USE_Model,binding=4, scalar) buffer DrawCmd { VkDrawIndexedIndirectCommand b_draw_cmd[]; };
layout(set=USE_Model,binding=5) uniform accelerationStructureEXT topLevelAS;
#endif

#if defined(USE_Boolean)
layout(set=USE_Boolean,binding=0) uniform accelerationStructureEXT u_BooleanAdd_TLAS;
layout(set=USE_Boolean,binding=1) uniform accelerationStructureEXT u_BooleanSub_TLAS;
#endif


#endif // Boolean_H_

