#ifndef LayerdDepth_H_
#define LayerdDepth_H_

#extension GL_EXT_ray_query : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

#if defined(USE_LayeredDepth)

struct LDPoint
{
	float p;
	int flag;
};
LDPoint g_invalid_point = {99999999., 0xffffffff};
bool is_valid(in LDPoint p){ return p.flag!=g_invalid_point.flag; }
#define LDFlag_Incident 1
#define LDFlag_Exit 2

layout(set=USE_LayeredDepth,binding=0, scalar) buffer LayerdDepth_Counter { int b_LayeredDepth_active_counter; int b_LayeredDepth_free_counter; };
layout(set=USE_LayeredDepth,binding=1, scalar) buffer LayerdDepth_PointLinkHead { int b_LayeredDepth_point_link_head[]; };
layout(set=USE_LayeredDepth,binding=2, scalar) buffer LayerdDepth_PointLinkNext { int b_LayeredDepth_point_link_next[]; };
layout(set=USE_LayeredDepth,binding=3, scalar) buffer LayerdDepth_PointBuffer { LDPoint b_LayeredDepth_point[]; };
layout(set=USE_LayeredDepth,binding=4, scalar) buffer LayerdDepth_PointFreeBuffer { int b_LayeredDepth_point_free[]; };

#define ld_point_num (1024*1024*64)
int allocate_LayeredDepth_point()
{
	return atomicAdd(b_LayeredDepth_active_counter, 1);
}

#endif // USE_LayeredDepth

#if defined(USE_Model)

struct Info
{
	vec4 m_aabb_min;
	vec4 m_aabb_max;
	uint m_vertex_num;
};
struct VkDrawIndexedIndirectCommand 
{
    uint32_t    indexCount;
    uint32_t    instanceCount;
    uint32_t    firstIndex;
    int32_t     vertexOffset;
    uint32_t    firstInstance;
};
layout(set=USE_Model,binding=0, std140) uniform InfoUniform {Info u_info; };
layout(set=USE_Model,binding=1, scalar) buffer Vertices { vec3 b_vertex[]; };
layout(set=USE_Model,binding=2, scalar) buffer Normals { vec3 b_normal[]; };
layout(set=USE_Model,binding=3, scalar) buffer Indices { uvec3 b_index[]; };
layout(set=USE_Model,binding=4, scalar) buffer DrawCmd { VkDrawIndexedIndirectCommand b_draw_cmd[]; };
layout(set=USE_Model,binding=5) uniform accelerationStructureEXT topLevelAS;
#endif

#endif // LayerdDepth_H_

