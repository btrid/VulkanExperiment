#ifndef Movable_
#define Movable_

#ifdef USE_Movable

struct Movable
{
	vec2 pos;
	vec2 dir;
	float scale;
	uint rb_id;
};


layout(std430, set=USE_Movable, binding=0) buffer MovableBuffer {
	Movable b_movable[];
};
layout(std430, set=USE_Movable, binding=1) buffer MovableCounter {
	uvec4 b_movable_counter;
};
#endif


#endif