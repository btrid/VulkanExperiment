#ifndef Rigidbody2D_H_
#define Rigidbody2D_H_

#if defined(USE_Rigidbody2D)

#define RB_PARTICLE_BLOCK_SIZE (64)
#define FLUID_NUM (4)
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
	uint _p;
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
	vec2 pos_old;

	vec2 local_pos;
	vec2 local_sdf;

	uint contact_index;
	uint is_contact;
	uint color;
	uint is_active;

};

struct rbFluid
{
	uint r_id;
	float mass;
	vec2 pos;

	vec2 vel;
	vec2 sdf;
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


#endif
#if defined(USE_MakeRigidbody)

layout(set=USE_MakeRigidbody, binding=0, std430) restrict buffer MakeParticleBuffer {
	rbParticle b_make_particle;
};
layout(set=USE_MakeRigidbody, binding=1, std430) restrict buffer MakePosBitBuffer {
	uint64_t b_posbit[];
};
layout(set=USE_MakeRigidbody, binding=2, std430) restrict buffer MakeJFABuffer {
	i16vec2 b_jfa_cell[];
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
#endif