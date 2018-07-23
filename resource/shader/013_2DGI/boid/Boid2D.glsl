#ifndef Boid_H_
#define Boid_H_

//#extension GL_NV_shader_atomic_float : require

//#define BOUNDS_CHECK(v) 

struct ParticleInfo
{
	float lambdaN0;
	float numberDensityN0;
	float influenceRadius;
	float collisionRadius;

	int particleNum;
	int fluidNum;
};

#if defined(USE_Boid2D)

#define Scale (1.)
#define Grid_Size (1.)
  #define DT 0.016
//#define DT 0.0005

layout(set=USE_Boid2D, binding=0, std430) restrict buffer PosBuffer {
	vec2 b_pos[];
};
layout(set=USE_Boid2D, binding=1, std430) restrict buffer VelBuffer {
	vec2 b_vel[];
};
layout(set=USE_Boid2D, binding=2, std430) restrict buffer AccBuffer {
	vec2 b_acc[];
};
layout(set=USE_Boid2D, binding=3, std430) restrict buffer TypeBuffer {
	int b_type[];
};
layout(set=USE_Boid2D, binding=4) restrict buffer GridCellBuffer {
	int b_grid_head[];
};
layout(set=USE_Boid2D, binding=5) restrict buffer GridNodeBuffer {
	int b_grid_node[];
};
layout(set=USE_Boid2D, binding=6) restrict buffer GridCounter {
	int b_grid_counter[];
};
#endif

float calcWeight(in float distance, in float influenceRadius)
{
	return max((influenceRadius / (distance+0.001)) - 1., 0.);
}

bvec4 getWall(in ivec2 index)
{
	ivec4 _fi = ivec4(index/8, index%8);
	ivec4 f_neighbor = ivec4(0, 0, _fi.z==7?1:0, _fi.w==7?1:0);
	ivec4 f_bit =      ivec4(_fi.zw, (_fi.zw+1)%8);
//	ivec4 f_neighbor = ivec4(_fi.z==0?-1:0, _fi.w==0?-1:0, 0, 0);
//	ivec4 f_bit =      ivec4(_fi.z==0?7:(_fi.z-1), _fi.w==0?7:(_fi.w-1), _fi.zw);

	ivec4 fi = (_fi.xxxx+f_neighbor.xxzz) + (_fi.yyyy+f_neighbor.ywyw)*(u_gi2d_info.m_resolution.x/8);
	ivec4 bit = f_bit.xxzz + f_bit.ywyw*8;

	bvec4 is_wall;
//	is_wall.x = false;//(b_diffuse_map[fi.x] & (1ul<<bit.x)) != 0;
//	is_wall.y = false;//(b_diffuse_map[fi.y] & (1ul<<bit.y)) != 0;
//	is_wall.z = false;//(b_diffuse_map[fi.z] & (1ul<<bit.z)) != 0;
//	is_wall.w = false;//(b_diffuse_map[fi.w] & (1ul<<bit.w)) != 0;
	is_wall.x = (b_diffuse_map[fi.x] & (1ul<<bit.x)) != 0;
	is_wall.y = (b_diffuse_map[fi.y] & (1ul<<bit.y)) != 0;
	is_wall.z = (b_diffuse_map[fi.z] & (1ul<<bit.z)) != 0;
	is_wall.w = (b_diffuse_map[fi.w] & (1ul<<bit.w)) != 0;
	return is_wall;
}
#endif