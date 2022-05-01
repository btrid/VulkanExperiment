#pragma once

#include <vector>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Shape.h>
#include <btrlib/Context.h>
#include <btrlib/AllocatedMemory.h>

namespace sph
{
struct dFluid
{
	int PNum;
	std::vector<vec3> pos;
	std::vector<vec3> vel;
	std::vector<vec3> f;
	std::vector<float> prs;
	std::vector<float> rho;

	std::vector<int> GridLinkHead;
	std::vector<int> GridLinkNext;

};

void init(dFluid& dfluid);
void run(dFluid& dfluid);
}