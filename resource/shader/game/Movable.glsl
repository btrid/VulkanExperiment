#ifndef Movable_
#define Movable_

#ifdef USE_Movable

struct Movable
{
	vec2 pos;
	vec2 pos_predict;
	vec2 dir;
	float scale;
	uint rb_id;
};


layout(std140, set=USE_Movable, binding=0) uniform MovableBuffer {
	Movable b_movable[];
};
#endif


#endif