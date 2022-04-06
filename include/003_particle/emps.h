#pragma once

#include <vector>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Shape.h>
struct FluidContext
{
	int PNum;
	std::vector<vec3> Acc, Pos, Vel;
	std::vector<float> Prs;
	std::vector<int> PType;

	float viscosity = 0.000001f; //!< ”S“x

	Triangle triangle;
};
void init(FluidContext& cFluid); 
void run(FluidContext& cFluid);
void gui(FluidContext& cFluid);

