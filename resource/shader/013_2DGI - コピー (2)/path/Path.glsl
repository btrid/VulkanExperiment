#ifndef Path2D_H_
#define Path2D_H_


#if defined(USE_PathFinding)

#extension GL_ARB_gpu_shader_int64 : require

struct SparseMap
{
	uint norton_index;
	uint child_node_index;
	uint64_t map;
};
struct PathNode
{
	uint cost;
};

layout(set=USE_PathFinding, binding=0, std430) restrict buffer SparseMapBuffer {
	SparseMap b_sparse_map[];
};
layout(set=USE_PathFinding, binding=1, std430) restrict buffer OpenNodeBuffer {
	uint b_sparse_map_counter;
};
layout(set=USE_PathFinding, binding=2, std430) restrict buffer OpenNodeCounter {
	uvec4 b_sparse_map_hierarchy_counter[];
};

layout(set=USE_PathFinding, binding=3, std430) restrict buffer PathNodeBuffer {
	PathNode b_node[];
};


#endif
#endif