#ifndef FLUID_H_
#define FLUID_H_

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

#if defined(USE_Fluid2D)

//#define Grid_Size (2)
#define SND (22.)
#define Scale (3.)
//#define DT 0.016
  #define DT 0.0005
#define DefaultDensity (0.1)
#define InfluenceRadius (1.*Scale)
#define CollisionRadius (1.*Scale)
#define WallPressure (3.)
const float p_mass = 1000.; // 質量
const float w_mass = 1000.;

/*
layout(std140, binding=0) uniform ParticleInfoUniform {
	ParticleInfo uParticleInfo;
};
layout(std140, binding = 1) uniform  GridInfoUniform {
	GridInfo uGridInfo;
};
*/
layout(set=USE_Fluid2D, binding=0, std430) restrict buffer PosBuffer {
	vec2 b_pos[];
};
layout(set=USE_Fluid2D, binding=1, std430) restrict buffer VelBuffer {
	vec2 b_vel[];
};
layout(set=USE_Fluid2D, binding=2, std430) restrict buffer AccBuffer {
	vec2 b_acc[];
};
layout(set=USE_Fluid2D, binding=3, std430) restrict buffer TypeBuffer {
	int b_type[];
};
layout(set=USE_Fluid2D, binding=4, std430) restrict buffer PressureBuffer {
	float b_pressure[];
};
layout(set=USE_Fluid2D, binding=5, std430) restrict buffer MinimumPressureBuffer {
	float b_minimum_pressure[];
};
layout(set=USE_Fluid2D, binding=6, std430) restrict buffer GridCellBuffer {
	int b_grid_head[];
};
layout(set=USE_Fluid2D, binding=7, std430) restrict buffer GridNodeBuffer {
	int b_grid_node[];
};
#endif

float calcWeight(in float distance, in float influenceRadius)
{
	return max((influenceRadius / (distance+0.001)) - 1., 0.);
}

bvec4 getWall(in ivec2 index)
{
	ivec4 _fi = ivec4(index/8, index%8);
	ivec4 f_neighbor = ivec4(_fi.z==0 ? -1 : 0, _fi.z==7 ? 1 : 0, _fi.w==0 ? -1 : 0, _fi.w==7 ? 1 : 0);
	ivec4 fi = (_fi.xxxx+f_neighbor.xxzz) + (_fi.yyyy+f_neighbor.ywyw)*(u_gi2d_info.m_resolution.x/8);
	ivec4 bit = (_fi.zzzz+ivec2(0,1).xxyy)%8 + ((_fi.wwww+ivec2(0,1).xyxy)%8)*8;
	bvec4 is_wall;
	is_wall.x = (b_diffuse_map[fi.x] & (1ul<<bit.x)) != 0;
	is_wall.y = (b_diffuse_map[fi.y] & (1ul<<bit.y)) != 0;
	is_wall.z = (b_diffuse_map[fi.z] & (1ul<<bit.z)) != 0;
	is_wall.w = (b_diffuse_map[fi.w] & (1ul<<bit.w)) != 0;
	return is_wall;
}
#endif