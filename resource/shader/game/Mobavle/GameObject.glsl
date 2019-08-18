#ifndef GameObject_
#define GameObject_


struct cMemoryManager
{
	uint block_max;
	uint active_index;
	uint free_index;
	uint allocate_address;
};

struct cResourceAccessorInfo
{
	uint stride;
};
struct cResourceAccessor
{
	uint gameobject_address;
	uint rb_address;
};
struct cMovable
{
	vec2 pos;
	vec2 dir;
	float scale;
};

#ifdef USE_GameObject_Buffer
layout(std430, set=USE_GameObject_Buffer, binding=0) buffer MemoryManagerBuffer {
	cMemoryManager b_memory_manager;
};
layout(std430, set=USE_GameObject_Buffer, binding=1) buffer MemoryListBuffer {
	uint b_memory_list[];
};

layout(std430, set=USE_GameObject_Buffer, binding=2) buffer AccessorInfoBuffer {
	cResourceAccessorInfo b_accessor_info;
};
layout(std430, set=USE_GameObject_Buffer, binding=3) buffer AccessorBuffer {
	cResourceAccessor b_accessor_buffer[];
};

layout(std430, set=USE_GameObject_Buffer, binding=4) buffer MovableBuffer {
	cMovable b_movable_buffer[];
};
#endif
#ifdef USE_GameObject
layout(std430, set=USE_GameObject, binding=0) buffer AccessorData {
	cResourceAccessor b_accessor;
};
#endif

#endif