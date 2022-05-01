#include <vector>
#include <list>
#include <map>
#include <iostream>
#include <fstream>
#include <cmath>
#include <btrlib/DefineMath.h>
#include <003_particle/sph.h>

static const double SPH_RESTDENSITY = 600.0;
static const double SPH_INTSTIFF = 3.0;
static const double SPH_PMASS = 0.00020543;
static const double SPH_SIMSCALE = 0.004;
static const double H = 0.01;
static const float H2 = H * H;

static const double PI = 3.141592653589793;
static const double DT = 0.004;
static const double SPH_VISC = 0.2;
static const double SPH_LIMIT = 200.0;
static const double SPH_RADIUS = 0.004;
static const double SPH_EPSILON = 0.00001;
static const double SPH_EXTSTIFF = 10000.0;
static const double SPH_EXTDAMP = 256.0;
static const double SPH_PDIST = pow(SPH_PMASS / SPH_RESTDENSITY, 1.0 / 3.0);
static const vec3   MIN = vec3(0.0, 0.0, -10.0);
static const vec3   MAX = vec3(20.0, 50.0, 10.0);
static const vec3   INIT_MIN = vec3(0.0, 0.0, -10.0);
static const vec3   INIT_MAX = vec3(10.0, 20.0, 10.0);
static const double Poly6Kern = 315.0 / (64.0 * PI * pow(H, 9));
static const double SpikyKern = -45.0 / (PI * pow(H, 6));
static const double LapKern = 45.0 / (PI * pow(H, 6));

double CellSize = H / SPH_SIMSCALE;
ivec3 MapSize = floor((MAX - MIN) / CellSize);

static const int s_PNUM = 4000;

namespace sph
{

void init(dFluid& fluid)
{
	fluid.pos.resize(s_PNUM);
	fluid.vel.resize(s_PNUM);
	fluid.prs.resize(s_PNUM);
    fluid.rho.resize(s_PNUM);
    fluid.f.resize(s_PNUM);

    int count = 0;
    vec3 area = INIT_MAX - INIT_MIN;
    double d = SPH_PDIST / SPH_SIMSCALE * 0.95;
    for (double x = INIT_MIN.x + d; x <= INIT_MAX.x - d; x += d)
    for (double y = INIT_MIN.y + d; y <= INIT_MAX.y - d; y += d)
    for (double z = INIT_MIN.z + d; z <= INIT_MAX.z - d; z += d)
    {
		fluid.pos[count++] = vec3(x, y, z);
    }
    fluid.PNum = count;

	fluid.GridLinkHead.resize(MapSize.x* MapSize.y* MapSize.z);
    fluid.GridLinkNext.resize(s_PNUM);
}

ivec3 neighbor_map_idx(vec3& r)
{
    return floor((r - MIN) / CellSize);
}
int neighbor_map_idx(const ivec3& i)
{
	return i.x + i.y * MapSize.x + i.z * MapSize.x * MapSize.y;
}


/*----------------------------
  Functions for simulation
----------------------------*/

void calc_amount(dFluid& fluid);
void calc_force(dFluid& fluid);
void advance(dFluid& fluid);

void calc_grid(dFluid& fluid)
{
	std::fill(fluid.GridLinkHead.begin(), fluid.GridLinkHead.end(), -1);
	for (int i = 0; i < fluid.PNum; i++)
	{
//		if (dFluid.PType[i] == PT_Ghost)continue;
        if (all(greaterThan(fluid.pos[i], MIN)) && all(lessThanEqual(fluid.pos[i], MAX)))
        {
			int ib = neighbor_map_idx(neighbor_map_idx(fluid.pos[i]));
			int pnumber = fluid.GridLinkHead[ib];
			fluid.GridLinkHead[ib] = i;
			fluid.GridLinkNext[i] = pnumber;
        }
	}
}
void run(dFluid& fluid)
{
    calc_grid(fluid);
    calc_amount(fluid);
    calc_force(fluid);
    advance(fluid);
}

void calc_amount(dFluid& fluid)
{
    for (int i = 0; i < fluid.PNum; i++)
    {
        auto& pos = fluid.pos[i];
        auto idx = neighbor_map_idx(pos);
		float sum = 0.0;
		for (int x=idx.x-1; x<=idx.x+1; x++){if(x<0||x>=MapSize.x) { continue; }
		for (int y=idx.y-1; y<=idx.y+1; y++){if(y<0||y>=MapSize.y) { continue; }
		for (int z=idx.z-1; z<=idx.z+1; z++){if(z<0||z>=MapSize.z) { continue; }
            auto jb = neighbor_map_idx(ivec3(x, y, z));
			for (int j = fluid.GridLinkHead[jb]; j != -1; j = fluid.GridLinkNext[j])
			{
				if (i == j) { continue; }
                auto dir = (pos - fluid.pos[j]) * SPH_SIMSCALE;
				float dist2 = length2(dir);
				if (dist2 >= H2) { continue; }

                float c = H2 - dist2;
				sum += c * c * c;
			}

        }}}
        fluid.rho[i] = sum * SPH_PMASS * Poly6Kern;
        fluid.prs[i] = (fluid.rho[i] - SPH_RESTDENSITY) * SPH_INTSTIFF;
        fluid.rho[i] = 1.0 / fluid.rho[i];
    }
}

void calc_force(dFluid& fluid)
{
    for (int i = 0; i < fluid.PNum; i++)
    {
		auto& pos = fluid.pos[i];
		auto& vel = fluid.vel[i];
		auto& prs = fluid.prs[i];
		auto& rho = fluid.rho[i];
        auto idx = neighbor_map_idx(pos);
		vec3 force = vec3(0.0, 0.0, 0.0);
		for (int x=idx.x-1; x<=idx.x+1; x++){if(x<0||x>=MapSize.x) { continue; }
		for (int y=idx.y-1; y<=idx.y+1; y++){if(y<0||y>=MapSize.y) { continue; }
		for (int z=idx.z-1; z<=idx.z+1; z++){if(z<0||z>=MapSize.z) { continue; }
			auto jb = neighbor_map_idx(ivec3(x, y, z));
			for (int j = fluid.GridLinkHead[jb]; j != -1; j = fluid.GridLinkNext[j])
			{
				if (i == j) { continue; }
                auto dir = (pos-fluid.pos[j]) * SPH_SIMSCALE;
				float dist = length(dir);
				if (dist >= H) { continue; }

                float c = H - dist;
				auto pterm = -0.5f * c * SpikyKern * (prs + fluid.prs[j]) / dist;
				auto vterm = LapKern * SPH_VISC;
				auto fcurr = pterm * dir + vterm * (fluid.vel[j] - vel);
				fcurr *= c * rho * fluid.rho[j];
				force += fcurr;

			}
        }}}
        fluid.f[i] = force;
    }
}

void advance(dFluid& fluid)
{
    vec3 norm;
    float diff, adj;

    vec3 g = vec3(0.0, -9.8, 0.0);
	for (int i = 0; i < fluid.PNum; i++)
    {
		auto& pos = fluid.pos[i];
		auto& vel = fluid.vel[i];
        vec3 accel = fluid.f[i] * SPH_PMASS;

        float speed = dot(accel, accel);
        if (speed > SPH_LIMIT * SPH_LIMIT) 
        {
            accel *= SPH_LIMIT / sqrt(speed);
        }

        // Z-axis walls
        diff = 2.0 * SPH_RADIUS - (pos.z - MIN.z) * SPH_SIMSCALE;
        if (diff > SPH_EPSILON)
        {
            norm = vec3(0.0, 0.0, 1.0);
            adj = SPH_EXTSTIFF * diff - SPH_EXTDAMP * dot(norm, vel);
            accel += adj * norm;
        }
        diff = 2.0 * SPH_RADIUS - (MAX.z - pos.z) * SPH_SIMSCALE;
        if (diff > SPH_EPSILON)
        {
            norm = vec3(0.0, 0.0, -1.0);
            adj = SPH_EXTSTIFF * diff - SPH_EXTDAMP * dot(norm, vel);
            accel += adj * norm;
        }

        // X-axis walls
        diff = 2.0 * SPH_RADIUS - (pos.x - MIN.x) * SPH_SIMSCALE;
        if (diff > SPH_EPSILON)
        {
            norm = vec3(1.0, 0.0, 0.0);
            adj = SPH_EXTSTIFF * diff - SPH_EXTDAMP * dot(norm, vel);
            accel += adj * norm;
        }
        diff = 2.0 * SPH_RADIUS - (MAX.x - pos.x) * SPH_SIMSCALE;
        if (diff > SPH_EPSILON)
        {
            norm = vec3(-1.0, 0.0, 0.0);
            adj = SPH_EXTSTIFF * diff - SPH_EXTDAMP * dot(norm, vel);
            accel += adj * norm;
        }

        // Y-axis walls
        diff = 2.0 * SPH_RADIUS - (pos.y - MIN.y) * SPH_SIMSCALE;
        if (diff > SPH_EPSILON)
        {
            norm = vec3(0.0, 1.0, 0.0);
            adj = SPH_EXTSTIFF * diff - SPH_EXTDAMP * dot(norm, vel);
            accel += adj * norm;
        }
        diff = 2.0 * SPH_RADIUS - (MAX.y - pos.y) * SPH_SIMSCALE;
        if (diff > SPH_EPSILON)
        {
            norm = vec3(0.0, -1.0, 0.0);
            adj = SPH_EXTSTIFF * diff - SPH_EXTDAMP * dot(norm, vel);
            accel += adj * norm;
        }

        accel += g;
        fluid.vel[i] += accel * DT;
        fluid.pos[i] += fluid.vel[i] * DT / SPH_SIMSCALE;
    }
}

}