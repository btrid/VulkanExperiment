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

};

struct rbParticle
{
	uint use_collision_detective;
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
layout(set=USE_Rigidbody2D, binding=3, std430) restrict buffer rbParticleBuffer {
	rbParticle b_rbParticle[];
};

vec2 rotateRBParticle(in vec2 v, in float angle)
{
	float c = cos(angle);
	float s = sin(angle);

	vec2 Result;
	Result.x = v.x * c - v.y * s;
	Result.y = v.x * s + v.y * c;
	return Result;
}
#endif


float closestPointSegment(in vec2 origin, in vec2 dir, in vec2 p)
{
	float t = dot(p - origin, dir) / dot(dir, dir);
	vec2 l = origin + dir * clamp(t, 0., 1.); 
	return distance(l, p);
}
#endif