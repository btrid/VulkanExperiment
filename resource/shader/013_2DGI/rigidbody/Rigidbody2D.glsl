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

	vec2 center;
	vec2 size;

	vec2 pos;
	vec2 vel;

	ivec2 pos_work;
	ivec2 vel_work;

	float angle;
	float angle_vel;
	int angle_vel_work;
	float _pp1;

	vec2 vel_delta;
	float angle_vel_delta;
	float _pp2;

};

struct Constraint
{
	int r_id;
	int p_id;
	float impulse;
	int _p3;
	vec2 ri;
	vec2 rj;
	vec2 vi;
	vec2 vj;
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
layout(set=USE_Rigidbody2D, binding=5, std430) restrict buffer rbConstraintCounter {
	ivec4 b_constraint_count;
};
layout(set=USE_Rigidbody2D, binding=6, std430) restrict buffer rbConstraintBuffer {
	Constraint b_constraint[];
};

#endif

#endif