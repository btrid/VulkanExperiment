#pragma once

#include <vector>
#include <btrlib/DefineMath.h>
struct FluidContext
{
	int PNum;
	std::vector<dvec3> Acc, Pos, Vel;
	std::vector<double> Prs;
	std::vector<int> PType;
};
void init(FluidContext& cFluid); 
void run(FluidContext& cFluid);