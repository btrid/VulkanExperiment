#pragma once

#include <vector>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
struct FluidContext
{
	int PNum;
	std::vector<vec3> Acc, Pos, Vel;
	std::vector<float> Prs;
	std::vector<int> PType;
};
void init(FluidContext& cFluid); 
void run(FluidContext& cFluid);


