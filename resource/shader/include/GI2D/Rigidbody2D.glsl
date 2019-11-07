#ifndef Rigidbody2D_H_
#define Rigidbody2D_H_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require


#if defined(USE_Rigidbody2D)

#define RB_PARTICLE_BLOCK_SIZE (64)
#define COLLIDABLE_NUM (4)
#define RB_DT (0.016)
#define RB_GRAVITY_DT (vec2(0., 0.0025))
//#define RB_DT (0.0016)
//#define RB_GRAVITY_DT (vec2(0., 0.000025))


#define k_radius (0.5)
#define k_delimiter (1.)


float calcWeightImpl(in float distance, in float influenceRadius)
{
	return max((distance / influenceRadius) - 1., 0.);
}
float calcWeight(in float distance)
{
	return calcWeightImpl(distance, 0.5);
}


struct rbWorld
{
	float DeltaTime;
	uint step;
	uint STEP;
	uint rigidbody_num_notuse;

	uint rigidbody_max;
	uint particle_block_max;
	uint gpu_index;
	uint cpu_index;

};

#define RB_FLAG_FLUID (1<<0)
#define RB_FLAG_USER_CONTROL (1<<1)

#define CM_WORK_PRECISION (65535.)
struct Rigidbody
{
	uint pnum;
	float life;
	vec2 cm;

	vec2 cm_old;
	uint flag;
	uint _p1;

	vec4 R;
	vec4 R_old;

	i64vec2 cm_work;
	i64vec4 Apq_work;
};

#define RBP_FLAG_ACTIVE (1<<0)
#define RBP_FLAG_COLLIDABLE (1<<1)
#define _RBP_FLAG_COLLIDED (1<<7)
struct rbParticle
{
	f16vec2 relative_pos;
	f16vec2 sdf;

	f16vec2 local_pos;
	f16vec2 local_sdf;

	vec2 pos;
	vec2 pos_old;


	float density;
	uint flag_color; // u8vec4[flag, r, g, b]

};
struct rbCollidable
{
	uint r_id;
	float mass_inv;
	vec2 pos;

	vec2 vel;
	vec2 sdf;
};
struct rbCollidableWall
{
	f16vec2 sdf;
};

struct BufferManage
{
	uint rb_list_size;
	uint pb_list_size;
	uint rb_active_index;
	uint rb_free_index;
	uint pb_active_index;
	uint pb_free_index;
};

layout(set=USE_Rigidbody2D, binding=0, std430) buffer WorldData {
	rbWorld b_world;
};
layout(set=USE_Rigidbody2D, binding=1, std430) buffer rbInfoBuffer {
	Rigidbody b_rigidbody[];
};
layout(set=USE_Rigidbody2D, binding=2, std430) buffer rbParticleBuffer {
	rbParticle b_rbParticle[];
};
layout(set=USE_Rigidbody2D, binding=3, std430) buffer rbParticleMappingBuffer {
	uint b_rbParticle_map[];
};
layout(set=USE_Rigidbody2D, binding=4, std430) buffer rbCollidableCounter {
	uint b_collidable_counter[];
};
layout(set=USE_Rigidbody2D, binding=5, std430) buffer rbCollidableBuffer {
	rbCollidable b_collidable[];
};
layout(set=USE_Rigidbody2D, binding=6, std430) buffer rbCollidableWallBuffer {
	rbCollidableWall b_collidable_wall[];
};
layout(set=USE_Rigidbody2D, binding=7, std430) buffer rbFluidCounter {
	uint b_fluid_counter[];
};
layout(set=USE_Rigidbody2D, binding=8, std430) buffer rbBufferManageData {
	BufferManage b_manager;
};
layout(set=USE_Rigidbody2D, binding=9, std430) buffer rbRigidbodyFreelist {
	uint b_rb_memory_list[];
};
layout(set=USE_Rigidbody2D, binding=10, std430) buffer rbParticleBlockFreelist {
	uint b_pb_memory_list[];
};
layout(set=USE_Rigidbody2D, binding=11, std430) buffer rbActiveCounter {
	uvec4 b_update_counter[];
};
layout(set=USE_Rigidbody2D, binding=12, std430) buffer rbRBActiveBuffer {
	uint b_rb_update_list[];
};
layout(set=USE_Rigidbody2D, binding=13, std430) buffer rbPBActiveBuffer {
	uint b_pb_update_list[];
};

#endif
#if defined(USE_MakeRigidbody)

struct RBMakeParam
{
	uvec4 pb_num;
	uvec4 registered_num;
	ivec4 rb_aabb;
	uint destruct_voronoi_id;
};

struct RBMakeCallback
{
	uint rb_id;
};

layout(set=USE_MakeRigidbody, binding=0, std430) restrict buffer MakeRigidbodyBuffer {
	Rigidbody b_make_rigidbody;
};
layout(set=USE_MakeRigidbody, binding=1, std430) restrict buffer MakeParticleBuffer {
	rbParticle b_make_particle[];
};
layout(set=USE_MakeRigidbody, binding=2, std430) restrict buffer MakeJFABuffer {
	i16vec2 b_make_jfa_cell[];
};
layout(set=USE_MakeRigidbody, binding=3, std430) restrict buffer RBMakeParamBuffer {
	RBMakeParam b_make_param;
};
layout(set=USE_MakeRigidbody, binding=4, std430) restrict buffer RBMakeCallbackBuffer {
	RBMakeCallback b_make_callback;
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