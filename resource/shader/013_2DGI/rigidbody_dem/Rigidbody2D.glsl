#ifndef Rigidbody2D_H_
#define Rigidbody2D_H_

#if defined(USE_Rigidbody2D)

#define RB_PARTICLE_BLOCK_SIZE (64)
#define FLUID_NUM (8)
#define RB_DT (0.016)
#define RB_GRAVITY_DT (vec2(0., 0.0025))
//#define RB_DT (0.0016)
//#define RB_GRAVITY_DT (vec2(0., 0.000025))


#define k_radius (0.5)
#define k_delimiter (1.)
struct rbWorld
{
	float DeltaTime;
	uint step;
	uint STEP;
	uint rigidbody_num;
//	uint particle_block_num;
};

struct Rigidbody
{
	uint pnum;
	uint solver_count;
	float inertia;
	float mass;

	vec2 center;
	vec2 size;

	vec2 pos;
	vec2 pos_old;

	float angle;
	float angle_old;
	ivec2 exclusion;

	int exclusion_angle;
	int is_exclusive;
	vec2 pos_predict;

	float angle_predict;
	float _p1;
	vec2 cm;

	vec4 R;

	i64vec2 cm_work;
	i64vec4 Apq_work;



};

struct rbParticle
{
	vec2 relative_pos;
	vec2 sdf;

	vec2 pos;
	vec2 pos_predict;

	vec2 local_pos;
	vec2 local_sdf;

	uint contact_index;
	uint is_contact;
	uint r_id;
	uint is_active;

	uint f_id;
	uint _p1;
	vec2 pos_old;
};

struct rbFluid
{
	uint r_id;
	uint p_id;
	vec2 pos;

	uint solver_count;
	uint _p1;
	vec2 sdf;

	float mass;
	uint is_active;
	ivec2 move;
};

struct rbConstraint
{
	uint f_id1;
	uint f_id2;
};

layout(set=USE_Rigidbody2D, binding=0, std430) restrict buffer WorldData {
	rbWorld b_world;
};
layout(set=USE_Rigidbody2D, binding=1, std430) restrict buffer rbInfoBuffer {
	Rigidbody b_rigidbody[];
};
layout(set=USE_Rigidbody2D, binding=2, std430) restrict buffer rbParticleBuffer {
	rbParticle b_rbParticle[];
};
layout(set=USE_Rigidbody2D, binding=3, std430) restrict buffer rbParticleMappingBuffer {
	uint b_rbParticle_map[];
};
layout(set=USE_Rigidbody2D, binding=4, std430) restrict buffer rbFluidCounter {
	uint b_fluid_count[];
};
layout(set=USE_Rigidbody2D, binding=5, std430) restrict buffer rbFluidData {
	rbFluid b_fluid[];
};
layout(set=USE_Rigidbody2D, binding=6, std430) restrict buffer rbConstraintCounter {
	uvec4 b_constraint_counter;
};
layout(set=USE_Rigidbody2D, binding=7, std430) restrict buffer rbConstraintData {
	rbConstraint b_constraint[];
};

#endif


vec2 rotateRBParticle(in vec2 v, in float angle)
{
	float c = cos(angle);
	float s = sin(angle);

	vec2 Result;
	Result.x = v.x * c - v.y * s;
	Result.y = v.x * s + v.y * c;
	return Result;
}



float closestPointSegment(in vec2 origin, in vec2 dir, in vec2 p)
{
	float t = dot(p - origin, dir) / dot(dir, dir);
	vec2 l = origin + dir * clamp(t, 0., 1.); 
	return distance(l, p);
}
#endif