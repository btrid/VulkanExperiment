#ifndef GameObject_
#define GameObject_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

struct cState
{
	uint64_t alive;
};

struct cMovable
{
	vec2 pos;
	vec2 dir;
	float scale;
	uint rb_address;
	uint _p;
	uint _p1;
};

#ifdef USE_GameObject_Buffer
layout(std430, set=USE_GameObject_Buffer, binding=0) buffer StateBuffer {
	cState b_state_buffer[];
};

layout(std430, set=USE_GameObject_Buffer, binding=1) buffer MovableBuffer {
	cMovable b_movable_buffer[];
};
#endif
#ifdef USE_GameObject
layout(std430, set=USE_GameObject, binding=0) buffer StateData {
	cState b_state[];
};

layout(std430, set=USE_GameObject, binding=1) buffer MovableData {
	cMovable b_movable[];
};
#endif

#endif