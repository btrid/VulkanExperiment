#ifndef Accessor_
#define Accessor_

#ifdef USE_Accessor

struct cMemoryManager
{
	uint block_max;
	uint active_index;
	uint free_index;
	uint allocate_address;
};

layout(std430, set=USE_Accessor, binding=0) buffer MemoryManagerBuffer {
	cMemoryManager b_memory_manager;
};
layout(std430, set=USE_Accessor, binding=1) buffer MemoryListBuffer {
	uint b_memory_list[];
};

#endif


#endif