#ifndef Crowd2D_H_
#define Crowd2D_H_


#if defined(USE_Crowd2D)

/*
struct Crowd
{
	int pnum;
	int particle_offset;
	int softbody_offset;
	int _p;
	vec2 center;
	ivec2 center_work;
};
*/

layout(set=USE_Crowd2D, binding=0, std430) restrict buffer SoftbodyInfo {
	uint b_grid_counter[];
};
layout(set=USE_Crowd2D, binding=1, std430) restrict buffer SoftbodyBuffer {
	vec2 b_softbody_pos[];
};
#endif


#endif