#ifndef Softbody2D_H_
#define Softbody2D_H_


#if defined(USE_Softbody2D)

struct Softbody
{
	int pnum;
	int particle_offset;
	int softbody_offset;
	int _p;
	vec2 center;
	ivec2 center_work;
};


layout(set=USE_Softbody2D, binding=0, std430) restrict buffer SoftbodyInfo {
	Softbody b_softbody[];
};
layout(set=USE_Softbody2D, binding=1, std430) restrict buffer SoftbodyBuffer {
	vec2 b_softbody_pos[];
};
#endif


#endif