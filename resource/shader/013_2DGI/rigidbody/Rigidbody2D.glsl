#ifndef Rigidbody2D_H_
#define Rigidbody2D_H_

#if defined(USE_Rigidbody2D)

//#extension GL_ARB_gpu_shader5 : enable
//#extension GL_NV_shader_atomic_float : require
struct Rigidbody
{
	int pnum;
	int solver_count;
	int inertia;
	int _p2;
	vec2 pos;
	vec2 vel;
	ivec2 pos_work;
	ivec2 vel_work;
	float angle;
	float angle_vel;
	int angle_vel_work;

};

layout(set=USE_Rigidbody2D, binding=0, std430) restrict buffer RigidbodyData {
	Rigidbody b_rigidbody;
};
layout(set=USE_Rigidbody2D, binding=1, std430) restrict buffer rbRelaPosBuffer {
	vec2 b_relative_pos[];
};
layout(set=USE_Rigidbody2D, binding=2, std430) restrict buffer rbPosBuffer {
	vec2 b_rbpos[];
};
layout(set=USE_Rigidbody2D, binding=3, std430) restrict buffer rbVelBuffer {
	vec2 b_rbvel[];
};
layout(set=USE_Rigidbody2D, binding=4, std430) restrict buffer rbAccBuffer {
	vec2 b_rbacc[];
};

#endif

#endif