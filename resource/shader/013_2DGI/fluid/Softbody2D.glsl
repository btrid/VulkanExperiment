#ifndef Softbody2D_H_
#define Softbody2D_H_


#if defined(USE_Softbody2D)

struct Softbody
{
	int32_t pnum;
	int32_t particle_offset;
	int32_t softbody_offset;
	int32_t _p;
	vec2 center;
	uvec2 center_work;
};


layout(set=USE_Softbody2D, binding=0, std430) restrict buffer SoftbodyInfo {
	Softbody b_softbody[];
};
layout(set=USE_Softbody2D, binding=1, std430) restrict buffer SoftbodyBuffer {
	vec2 b_softbody_pos[];
};
#endif


#endif