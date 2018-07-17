#ifndef FLUID_H_
#define FLUID_H_

#define TYPE_GHOST 0		//!< 無し
#define TYPE_FLUID 1		//!< 流体
#define TYPE_WALL  2		//!< 壁

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

#define SND (22.)
#define DT 0.016
#define DefaultDensity (1.)
#define InfluenceRadius (1.)
#define CollisionRadius (1.)
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
	if (distance >= influenceRadius || distance == 0.){
		// 距離が遠いので無視
		return 0.;
	}
	return (influenceRadius / distance) - 1.;
}

#endif