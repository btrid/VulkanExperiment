#ifndef Path2D_H_
#define Path2D_H_


#if defined(USE_PathFinding)


struct PathNode
{
	uint cost;
	uint norton_index;
	uint child_node_index;
};

layout(set=USE_PathFinding, binding=0, std430) restrict buffer PathNodeBuffer {
	PathNode b_node[];
};
layout(set=USE_PathFinding, binding=1, std430) restrict buffer OpenNodeBuffer {
	uint b_node_counter;
};
layout(set=USE_PathFinding, binding=2, std430) restrict buffer OpenNodeCounter {
	uvec4 b_node_hierarchy_counter[];
};


#endif
#endif