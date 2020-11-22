#ifndef LDC_H_
#define LDC_H_

#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : require

struct Info
{
	vec4 m_aabb_min;
	vec4 m_aabb_max;
	uint m_vertex_num;
};

layout(set=0,binding=0) uniform accelerationStructureEXT topLevelAS;
layout(set=1,binding=0, std140) uniform InfoUniform {Info u_info; };
layout(set=1,binding=1, std430) buffer Vertices { float b_vertex[]; };
layout(set=1,binding=2, std430) buffer Indices { uint b_index[]; };
layout(set=1,binding=3, std430) buffer LDCCounter { uint b_ldc_counter; };
layout(set=1,binding=4, std430) buffer LDCArea { uvec2 b_ldc_area[]; };
layout(set=1,binding=5, std430) buffer LDCRange { vec2 b_ldc_range[]; };


#endif // LDC_H_

