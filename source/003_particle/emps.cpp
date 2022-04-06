#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <003_particle/emps.h>

#define PCL_DST 0.02f					//平均粒子間距離
vec3 GridMin = vec3(0.0-PCL_DST*3);
vec3 GridMax = vec3(1.0 + PCL_DST * 3, 0.6 + PCL_DST * 20, 1.0 + PCL_DST * 3);
struct ConstantParam
{
};

enum ParticleType
{
	PT_Ghost,
	PT_Fluid,
	PT_Wall,
	PT_MAX,
};

#define DNS_FLD 1000		//流体粒子の密度
#define DNS_WLL 1000		//壁粒子の密度
#define DT 0.0005f			//時間刻み幅
#define DIM 3				//次元数
#define CRT_NUM 0.1f		//クーラン条件数
#define COL_RAT 0.2f		//接近した粒子の反発率
#define DST_LMT_RAT 0.9f	//これ以上の粒子間の接近を許さない距離の係数
#define WEI(dist, re) ((re/dist) - 1.0f)	//重み関数
#define Gravity vec3(0.f, -9.8f, 0.f) //重力加速度
double radius = PCL_DST * 2.1f;
double radius2 = radius*radius;

float GridCellSize; //GridCellの大きさ(バケット1辺の長さ)
float GridCellSizeInv;
ivec3 GridCellNum;
int GridCellTotal;

std::vector<int> GridLinkHead;
std::vector<int> GridLinkNext;
std::array<float, 16> wallweight;
std::vector<float> wallPrs;
std::vector<vec3> wallVsc;
std::vector<int> wallenable;

double n0; //初期粒子数密度
double lmd;	//ラプラシアンモデルの係数λ
double A2;//圧力の計算に用いる係数
double rlim; //これ以上の粒子間の接近を許さない距離
double rlim2;
double COL; // 反発率？

double Dns[PT_MAX],invDns[PT_MAX];

float safeDistance(const vec3& a, const vec3& b)
{
	return abs(dot(a, b)) < 0.000001 ? 0. : distance(a, b);
}
bool validCheck(const vec3& pos)
{
	return all(greaterThanEqual(pos, GridMin)) && all(lessThan(pos, GridMax));
}

ivec3 ToGridCellIndex(const vec3& p) 
{
	return ivec3((p - GridMin) * GridCellSizeInv); 
}
int ToGridIndex(const ivec3& i){
//	assert(all(greaterThanEqual(i, ivec3(0))) && all(lessThan(i, GridCellNum)));
	return i.x + i.y * GridCellNum.x + i.z * GridCellNum.x * GridCellNum.y;
}
int ToGridIndex(const vec3& p){	return ToGridIndex(ToGridCellIndex(p));}
void AlcBkt(FluidContext& cFluid)
{
	cFluid.PNum = 20000;
	cFluid.Acc.resize(cFluid.PNum);
	cFluid.Pos.resize(cFluid.PNum);
	cFluid.Vel.resize(cFluid.PNum);
	cFluid.Prs.resize(cFluid.PNum);
	cFluid.PType.resize(cFluid.PNum);
	std::fill(cFluid.PType.begin(), cFluid.PType.end(), PT_Ghost);
	std::fill(cFluid.Pos.begin(), cFluid.Pos.end(), GridMin - vec3(999));

	GridCellSize = radius*(1.0+CRT_NUM);
	GridCellSizeInv = 1.0/ GridCellSize;
	GridCellNum = ivec3((GridMax - GridMin)*GridCellSizeInv) + ivec3(3);
	GridCellTotal = GridCellNum.x * GridCellNum.y * GridCellNum.z;

	GridLinkHead.resize(GridCellTotal);
	GridLinkNext.resize(cFluid.PNum);

	wallenable.resize(GridCellTotal);
	wallPrs.resize(GridCellTotal);
	wallVsc.resize(GridCellTotal);
	ivec3 n = GridCellNum;
	// 壁の設定
	for(int iz=0;iz<n.z;iz++){
	for(int iy=0;iy<n.y;iy++){
	for(int ix=0;ix<n.x;ix++){
		int ip = iz*n.x* n.y + iy*n.x + ix;
		auto p = GridMin + GridCellSize * (vec3(ivec3(ix, iy, iz)) + 0.5f);
		float d = cFluid.triangle.getDistance(p);
		if (d < radius) {
			wallenable[ip] = true;
		}
	}}}

	// 初期化
	for(int iz=0;iz<n.z;iz++){
	for(int iy=0;iy<n.y;iy++){
	for(int ix=0;ix<n.x;ix++){
		int ip = iz*n.x* n.y + iy*n.x + ix;
		cFluid.PType[ip] = PT_Ghost;
		cFluid.Pos[ip] = GridMin + PCL_DST*vec3(ivec3(ix,iy,iz));
	}}}

#define WAVE_HEIGHT 0.3
#define WAVE_WIDTH 0.1
	for(int iz=0;iz<n.z;iz++){
	for(int iy=0;iy<n.y;iy++){
	for(int ix=0;ix<n.x;ix++){
		int ip = iz * n.x * n.y + iy * n.x + ix;
		if(ix<3 || ix >=n.x-3 || iz<3 || iz>=n.z-3 || iy<3){
			// 壁を作る
			cFluid.PType[ip] = PT_Wall;
		}
		else if(cFluid.Pos[ip].y >= WAVE_HEIGHT && cFluid.Pos[ip].x <= WAVE_WIDTH){
			// ダムを造る
			cFluid.PType[ip] = PT_Fluid;
		}
	}}}

}

void SetPara(FluidContext& cFluid){
	double tn0 =0.0;
	double tlmd =0.0;
	for(int ix= -4;ix<5;ix++){
	for(int iy= -4;iy<5;iy++){
	for(int iz= -4;iz<5;iz++){
		vec3 p = PCL_DST* vec3(ix, iy, iz);
		double dist2 = dot(p,p);
		if(dist2 <= radius2){
			if(dist2==0.0)continue;
			double dist = sqrt(dist2);
			tn0 += WEI(dist, radius);
			tlmd += dist2 * WEI(dist, radius);
		}
	}}}
	for(int iy= 0;iy<16;iy++)
	{
		float ww = 0.f;
		vec3 p = vec3(0, (float)iy/16.f*radius, 0);
		for (int w = 0; w < 9; w++)
		{
			vec3 wp = PCL_DST * -vec3(w%3, (w/3)%3, w/9);
			double dist2 = distance2(p, wp) + 0.0001;
			if (dist2 >= radius2) { continue; }
			double dist = sqrt(dist2);
			ww += WEI(dist, radius);
		}
		wallweight[iy] = ww;
	}

	n0 = tn0;			//初期粒子数密度
	lmd = tlmd/tn0;	//ラプラシアンモデルの係数λ
	rlim = PCL_DST * DST_LMT_RAT;//これ以上の粒子間の接近を許さない距離
	rlim2 = rlim*rlim;
	COL = 1.0 + COL_RAT;

	Dns[PT_Fluid] = DNS_FLD;
	Dns[PT_Wall] = DNS_WLL;
	invDns[PT_Fluid] = 1.0 / DNS_FLD;
	invDns[PT_Wall] = 1.0 / DNS_WLL;
}


void MkBkt(FluidContext& cFluid)
{
	std::fill(GridLinkHead.begin(), GridLinkHead.end(), -1);
	for (int i = 0; i < cFluid.PNum; i++) 
	{
		if (cFluid.PType[i] == PT_Ghost)continue;
		int ib = ToGridIndex(cFluid.Pos[i]);
		int pnumber = GridLinkHead[ib];
		GridLinkHead[ib] = i;
		GridLinkNext[i] = pnumber;
	}
}

void VscTrm(FluidContext& cFluid){
	for(int i=0;i<cFluid.PNum;i++)
	{
		if (cFluid.PType[i] != PT_Fluid) 
		{
			continue;
		}
		auto viscosity = vec3(0.0);
		const auto& pos = cFluid.Pos[i];
		const auto& vel = cFluid.Vel[i];
		auto idx = ToGridCellIndex(pos);
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=GridCellNum.x){continue;}
			int jb = ToGridIndex(ivec3(jx, jy, jz));
			for (int j = GridLinkHead[jb]; j != -1; j = GridLinkNext[j])
			{
				if (i==j) { continue;}
				double dist2 = distance2(cFluid.Pos[j], pos);
				if (dist2 >= radius2) {continue;}

				double dist = sqrt(dist2);
				double w =  WEI(dist, radius);
				viscosity += (cFluid.Vel[j] - vel) * w;
			}

			if (wallenable[jb]) {
				auto a = GridMin + vec3(jx, jy, jz) * GridCellSize;
				auto b = a + vec3(GridCellSize);
				float dist = AABB(a, b).distance(pos);
				double w = WEI(dist, radius);
				viscosity += (- vel) * w;
			}
		}}}
		cFluid.Acc[i]=viscosity* (cFluid.viscosity / n0 / lmd);
	}

// 	for (int z = 0; z < GridCellNum.z; z++){
// 	for (int y = 0; y < GridCellNum.y; y++){
// 	for (int x = 0; x < GridCellNum.x; x++){
// 		int i = ToGridIndex(ivec3(x, y, z));
// 		if (wallenable[i])
// 		{
// 			auto viscosity = vec3(0.0);
// 			auto a = GridMin + vec3(x, y, z) * GridCellSize;
// 			auto b = a + vec3(GridCellSize);
// 			AABB aabb(a, b);
// 			for (int j = GridLinkHead[i]; j != -1; j = GridLinkNext[j])
// 			{
// 				float dist = aabb.distance(cFluid.Pos[j]);
// 				double w = WEI(dist, radius);
// 				viscosity += (-vel) * w;
// 			}
// 			wallVsc[i] = viscosity * (cFluid.viscosity / n0 / lmd);
// 		}
// 	}}}

//	cFluid.triangle.
//	wallPrs.
}

void UpPcl1(FluidContext& cFluid)
{
	for(int i=0;i<cFluid.PNum;i++)
	{
		if(cFluid.PType[i] == PT_Fluid)
		{
			cFluid.Vel[i] += (cFluid.Acc[i] + Gravity) *DT;
			cFluid.Pos[i] +=cFluid.Vel[i]*DT;
			cFluid.Acc[i]=vec3(0.0);
			if (!validCheck(cFluid.Pos[i]))
			{
				cFluid.PType[i] = PT_Ghost;
				cFluid.Prs[i] = 0.0;
				cFluid.Vel[i] = {};
			}
		}
	}
}

void ChkCol(FluidContext& cFluid)
{
	for(int i=0;i<cFluid.PNum;i++)
	{
		if (cFluid.PType[i] != PT_Fluid) {
			continue;
		}
		double mi = Dns[cFluid.PType[i]];
		const auto& pos = cFluid.Pos[i];
		const auto& vel= cFluid.Vel[i];
		auto vec_ix2 = vel;
		auto idx = ToGridCellIndex(pos);
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=GridCellNum.x){continue;}
			int jb = ToGridIndex(ivec3(jx, jy, jz));
			for (int j = GridLinkHead[jb]; j != -1; j = GridLinkNext[j])
			{
				if (i==j){ continue;}

				auto v = cFluid.Pos[j] - pos;
				auto dist2 = dot(v, v);
				if(dist2>=rlim2){ continue;}

				double fDT = dot((vel-cFluid.Vel[j])*v, vec3(1.));
				if(fDT > 0.0){
					double mj = Dns[cFluid.PType[j]];
					fDT *= COL*mj/(mi+mj)/dist2;
					vec_ix2 -= v*fDT;
				}
			}

			if (cFluid.PType[i] == PT_Fluid) 
			{
				if (wallenable[jb]) {
					auto a = GridMin + vec3(jx, jy, jz) * GridCellSize;
					auto b = a + vec3(GridCellSize);
					auto v = AABB(a, b).nearest(pos) - pos;
					auto dist2 = dot(v, v);
					if (dist2 < rlim2)
					{
						double fDT = dot(vel * v, vec3(1.));
						if (fDT > 0.0) {
							double mj = Dns[PT_Wall];
							fDT *= COL * mj / (mi + mj) / dist2;
							vec_ix2 -= v * fDT;
						}
					}
				}
			}

		}}}
		cFluid.Acc[i]=vec_ix2;
	}
	for(int i=0;i<cFluid.PNum;i++)
	{
		cFluid.Vel[i]=cFluid.Acc[i];
	}
}

void MkPrs(FluidContext& cFluid){
	for(int i=0;i<cFluid.PNum;i++)
	{
		if (cFluid.PType[i] == PT_Ghost)
		{
			continue;
		}

		const auto& pos = cFluid.Pos[i];
		double ni = 0.0;

		auto idx = ToGridCellIndex(pos);
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=GridCellNum.x){continue;}
			int jb = ToGridIndex(ivec3(jx, jy, jz));
			for(int j = GridLinkHead[jb];j!=-1; j = GridLinkNext[j])
			{
				if (i == j) { continue; }
				double dist2 = distance2(cFluid.Pos[j], pos);
				if (dist2 >= radius2) { continue;}
				double dist = sqrt(dist2);
				double w =  WEI(dist, radius);
				ni += w;
			}
			if (wallenable[jb] && cFluid.PType[i] == PT_Fluid)
			{
				auto a = GridMin + vec3(jx, jy, jz) * GridCellSize;
				auto b = a + vec3(GridCellSize);
				auto dist = AABB(a, b).distance(pos);
				if (dist < radius)
				{
					double w = WEI(dist, radius);
					ni += w;
				}
			}

		}}}

		double mi = Dns[cFluid.PType[i]];
		double pressure = (ni > n0)*(ni - n0) * mi;
		cFluid.Prs[i] = pressure;
	}

	for (int z = 0; z < GridCellNum.z; z++){
	for (int y = 0; y < GridCellNum.y; y++){
	for (int x = 0; x < GridCellNum.x; x++){
		int i = ToGridIndex(ivec3(x, y, z));
		if (!wallenable[i])
		{
			continue;
		}
		auto a = GridMin + vec3(x, y, z) * GridCellSize;
		auto b = a + vec3(GridCellSize);
		double ni = 0.0;

		for(int jz=z-1;jz<=z+1;jz++){ if(jz<0||jz>=GridCellNum.z){continue;}
		for(int jy=y-1;jy<=y+1;jy++){ if(jy<0||jy>=GridCellNum.y){continue;}
		for(int jx=x-1;jx<=x+1;jx++){ if(jx<0||jx>=GridCellNum.x){continue;}
			int jb = ToGridIndex(ivec3(jx, jy, jz));
			for (int j = GridLinkHead[jb]; j != -1; j = GridLinkNext[j])
			{
				if (cFluid.PType[j] != PT_Fluid) {
					continue;
				}
				double dist = AABB(a, b).distance(cFluid.Pos[j]);
				if (dist >= radius) { continue; }
				double w = WEI(dist, radius);
				ni += w;
			}
		}}}
		float mi = Dns[PT_Wall];
		float pressure = (ni > n0) * (ni - n0) * mi;
		wallPrs[i] = pressure;

	}}}
}

void PrsGrdTrm(FluidContext& cFluid)
{
	for(int i=0;i<cFluid.PNum;i++)
	{
		if (cFluid.PType[i] != PT_Fluid) {
			continue;
		}
		vec3 acc = vec3(0.0);
		const auto& pos = cFluid.Pos[i];
		auto pre_min = cFluid.Prs[i];
		auto idx = ToGridCellIndex(pos);
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=GridCellNum.x){continue;}
			int jb = ToGridIndex(ivec3(jx, jy, jz));
			for (int j = GridLinkHead[jb]; j != -1; j = GridLinkNext[j])
			{
				if (i == j) { continue; }
				if (distance2(cFluid.Pos[j], pos) >= radius2) {continue;}
				if (pre_min > cFluid.Prs[j]) 
				{
					pre_min = cFluid.Prs[j]; 
				}
			}

			if (wallenable[jb] && pre_min > wallPrs[jb])
			{
				auto a = GridMin + vec3(jx, jy, jz) * GridCellSize;
				auto b = a + vec3(GridCellSize);
				auto dist = AABB(a, b).distance(pos);
				if (dist < radius)
				{
					pre_min = wallPrs[jb];
				}
			}

		}}}
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=GridCellNum.x){continue;}
			int jb = ToGridIndex(ivec3(jx, jy, jz));
			for (int j = GridLinkHead[jb]; j != -1; j = GridLinkNext[j])
			{
				if (i == j) { continue; }
				auto v = cFluid.Pos[j] - pos;
				double dist2 = dot(v, v);
				if (dist2 >= radius2) { continue; }

				double dist = sqrt(dist2);
				double w =  WEI(dist, radius);
				w *= (cFluid.Prs[j] - pre_min)/dist2;
				acc += v*w;;
			}
			if (wallenable[jb])
			{
				auto a = GridMin + vec3(jx, jy, jz) * GridCellSize;
				auto b = a + vec3(GridCellSize);
				auto dist = AABB(a, b).distance(pos);
				if (dist < radius)
				{
					double w = WEI(dist, radius);
					w *= (wallPrs[jb] - pre_min) / dist*dist;
					acc += (AABB(a, b).nearest(pos) - pos) * w;;
				}
			}
		}}}
		cFluid.Acc[i]=acc*invDns[PT_Fluid]*-1.f;
	}
}

void UpPcl2(FluidContext& cFluid)
{
	for(int i=0;i<cFluid.PNum;i++)
	{
		if(cFluid.PType[i] == PT_Fluid)
		{
			cFluid.Vel[i] +=cFluid.Acc[i]*DT;
//			cFluid.Pos[i] +=cFluid.Acc[i]*DT*DT;
			cFluid.Acc[i] = vec3(0.0);
			if (!validCheck(cFluid.Pos[i]))
			{
				cFluid.PType[i] = PT_Ghost;
			}
		}
	}
}

void init(FluidContext& cFluid)
{
	AlcBkt(cFluid);
	SetPara(cFluid);
}
void run(FluidContext& cFluid)
{
	MkBkt(cFluid);
	VscTrm(cFluid);
	UpPcl1(cFluid);
	ChkCol(cFluid);
	MkPrs(cFluid);
	PrsGrdTrm(cFluid);
	UpPcl2(cFluid);
//	MkPrs(cFluid);
}
