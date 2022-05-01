#pragma once

#include <vector>
#include <btrlib/DefineMath.h>

namespace imps
{
	struct dFluid
	{
		std::vector<vec3> Acceleration;
		std::vector<int>    ParticleType;
		std::vector<vec3> Position;
		std::vector<vec3> Velocity;
		std::vector<float> Pressure;
		std::vector<float> NumberDensity;
		std::vector<int>    BoundaryCondition;
		std::vector<float> SourceTerm;
		std::vector<int>    FlagForCheckingBoundaryCondition;
		std::vector<float> CoefficientMatrix;
		std::vector<float> MinimumPressure;

		int PNum;
	};

	void init(dFluid& dfluid);
	void run(dFluid& dfluid);
}