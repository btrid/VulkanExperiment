#ifndef FLUID_GLSL_
#define FLUID_GLSL_

#extension GL_EXT_scalar_block_layout : require

struct Constant
{
	float GridCellSize; //GridCellの大きさ(バケット1辺の長さ)
	float GridCellSizeInv;
	int GridCellTotal;
	float p1;
	ivec3 GridCellNum;
	float p2;
	vec3 GridMin;
	float p3;
	vec3 GridMax;
	float p4;
};

layout(set=Use_Fluid, binding=0, scalar) uniform FluidConstant { Constant u_constant; };
layout(set=Use_Fluid, binding=10, scalar) buffer FluidWall { int b_WallEnable[]; };


#endif