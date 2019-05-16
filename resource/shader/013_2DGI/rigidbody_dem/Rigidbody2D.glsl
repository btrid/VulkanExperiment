#ifndef Rigidbody2D_H_
#define Rigidbody2D_H_

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
	return max((influenceRadius / (distance+0.001)) - 1., 0.);
}
float calcWeight(in float distance)
{
	return calcWeightImpl(distance, k_delimiter);
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

#define RB_FLAG_FLUID (1)
struct Rigidbody
{
	uint pnum;
	float life;
	vec2 cm;

	uint flag;
	uint _p1;
	uint _p2;
	uint _p3;
	ivec2 size_min;
	ivec2 size_max;

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
	float density;
	uint color;
	uint is_active;

};

struct rbCollidable
{
	uint r_id;
	float mass_inv;
	vec2 pos;

	vec2 vel;
	vec2 sdf;
};

struct rbFluid
{
	vec2 pos;
	uint p_id;
	float density;
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


struct VoronoiCell
{
	i16vec2 point;
};

#define VoronoiVertex_MAX (10)
struct VoronoiPolygon
{
	int16_t vertex_index[VoronoiVertex_MAX];
	int num;
	i16vec4 minmax;
};

struct VoronoiVertex
{
	i16vec2 point;
	int16_t	cell[4];
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
layout(set=USE_Rigidbody2D, binding=4, std430) restrict buffer rbCollidableCounter {
	uint b_collidable_counter[];
};
layout(set=USE_Rigidbody2D, binding=5, std430) restrict buffer rbCollidableBuffer {
	rbCollidable b_collidable[];
};
layout(set=USE_Rigidbody2D, binding=6, std430) restrict buffer rbFluidCounter {
	uint b_fluid_counter[];
};
layout(set=USE_Rigidbody2D, binding=7, std430) restrict buffer rbFluidBuffer {
	rbFluid b_fluid[];
};
layout(set=USE_Rigidbody2D, binding=8, std430) restrict buffer rbBufferManageData {
	BufferManage b_manager;
};
layout(set=USE_Rigidbody2D, binding=9, std430) restrict buffer rbRigidbodyFreelist {
	uint b_rb_memory_list[];
};
layout(set=USE_Rigidbody2D, binding=10, std430) restrict buffer rbParticleBlockFreelist {
	uint b_pb_memory_list[];
};
layout(set=USE_Rigidbody2D, binding=11, std430) restrict buffer rbActiveCounter {
	uvec4 b_update_counter[];
};
layout(set=USE_Rigidbody2D, binding=12, std430) restrict buffer rbRBActiveBuffer {
	uint b_rb_update_list[];
};
layout(set=USE_Rigidbody2D, binding=13, std430) restrict buffer rbPBActiveBuffer {
	uint b_pb_update_list[];
};
layout(set=USE_Rigidbody2D, binding=14, std430) restrict buffer rbVoronoiCellBuffer {
	VoronoiCell b_voronoi_cell[];
};
layout(set=USE_Rigidbody2D, binding=15, std430) restrict buffer rbVoronoiPolygonBuffer {
	VoronoiPolygon b_voronoi_polygon[];
};
layout(set=USE_Rigidbody2D, binding=16, std430) restrict buffer rbVoronoiBuffer {
	int16_t b_voronoi[];
};
layout(set=USE_Rigidbody2D, binding=17, std430) restrict buffer rbVoronoiVertexCounter {
	uvec4 b_voronoi_vertex_counter;
};
layout(set=USE_Rigidbody2D, binding=18, std430) restrict buffer rbVoronoiVertexBuffer {
	VoronoiVertex b_voronoi_vertex[];
};
layout(set=USE_Rigidbody2D, binding=19, std430) restrict buffer rbVoronoiPathBuffer {
	int16_t b_voronoi_path[];
};

#endif
#if defined(USE_MakeRigidbody)

layout(set=USE_MakeRigidbody, binding=0, std430) restrict buffer MakeRigidbodyBuffer {
	Rigidbody b_make_rigidbody;
};
layout(set=USE_MakeRigidbody, binding=1, std430) restrict buffer MakeParticleBuffer {
	rbParticle b_make_particle[];
};
layout(set=USE_MakeRigidbody, binding=2, std430) restrict buffer MakeJFABuffer {
	i16vec2 b_make_jfa_cell[];
};
layout(set=USE_MakeRigidbody, binding=3, std430) restrict buffer MakeDispatchParam {
	uvec4 b_make_dispatch_param;
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