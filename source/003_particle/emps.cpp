#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <003_particle/emps.h>

#define PCL_DST 0.02f					//平均粒子間距離
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


std::vector<int> GridLinkHead;
std::vector<int> GridLinkNext;
std::vector<float> wallPrs;
std::vector<vec3> wallVsc;

std::array<float, 16> wallweight;

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
bool validCheck(FluidData& dFluid, const vec3& pos)
{
	return all(greaterThanEqual(pos, dFluid.m_constant.GridMin)) && all(lessThan(pos, dFluid.m_constant.GridMax));
}

ivec3 ToGridCellIndex(FluidData& dFluid, const vec3& p)
{
	return ivec3((p - dFluid.m_constant.GridMin) * dFluid.m_constant.GridCellSizeInv); 
}
int ToGridIndex(FluidData& dFluid, const ivec3& i){
//	assert(all(greaterThanEqual(i, ivec3(0))) && all(lessThan(i, dFluid.m_constant.GridCellNum)));
	return i.x + i.y * dFluid.m_constant.GridCellNum.x + i.z * dFluid.m_constant.GridCellNum.x * dFluid.m_constant.GridCellNum.y;
}
int ToGridIndex(FluidData& dFluid, const vec3& p){	return ToGridIndex(dFluid, ToGridCellIndex(dFluid, p));}
void AlcBkt(FluidData& dFluid)
{
	dFluid.m_constant.GridMin = vec3(0.0 - PCL_DST * 3);
	dFluid.m_constant.GridMax = vec3(1.0 + PCL_DST * 3, 0.6 + PCL_DST * 20, 1.0 + PCL_DST * 3);
	dFluid.m_constant.GridCellSize = radius * (1.0 + CRT_NUM);
	dFluid.m_constant.GridCellSizeInv = 1.0 / dFluid.m_constant.GridCellSize;
	dFluid.m_constant.GridCellNum = ivec3((dFluid.m_constant.GridMax - dFluid.m_constant.GridMin) * dFluid.m_constant.GridCellSizeInv) + ivec3(3);
	dFluid.m_constant.GridCellTotal = dFluid.m_constant.GridCellNum.x * dFluid.m_constant.GridCellNum.y * dFluid.m_constant.GridCellNum.z;

	dFluid.PNum = 20000;
	dFluid.Acc.resize(dFluid.PNum);
	dFluid.Pos.resize(dFluid.PNum);
	dFluid.Vel.resize(dFluid.PNum);
	dFluid.Prs.resize(dFluid.PNum);
	dFluid.PType.resize(dFluid.PNum);
	std::fill(dFluid.PType.begin(), dFluid.PType.end(), PT_Ghost);
	std::fill(dFluid.Pos.begin(), dFluid.Pos.end(), dFluid.m_constant.GridMin - vec3(999));


	GridLinkHead.resize(dFluid.m_constant.GridCellTotal);
	GridLinkNext.resize(dFluid.PNum);

	dFluid.wallenable.resize(dFluid.m_constant.GridCellTotal);
	wallPrs.resize(dFluid.m_constant.GridCellTotal);
	wallVsc.resize(dFluid.m_constant.GridCellTotal);
	ivec3 n = dFluid.m_constant.GridCellNum;
	// 壁の設定
	for(int iz=0;iz<n.z;iz++){
	for(int iy=0;iy<n.y;iy++){
	for(int ix=0;ix<n.x;ix++){
		int ip = iz*n.x* n.y + iy*n.x + ix;
		auto p = dFluid.m_constant.GridMin + dFluid.m_constant.GridCellSize * (vec3(ivec3(ix, iy, iz)) + 0.5f);
		float d = dFluid.triangle.getDistance(p);
		if (d < radius) {
			dFluid.wallenable[ip] = true;
		}
	}}}

	// 初期化
	for(int iz=0;iz<n.z;iz++){
	for(int iy=0;iy<n.y;iy++){
	for(int ix=0;ix<n.x;ix++){
		int ip = iz*n.x* n.y + iy*n.x + ix;
		dFluid.PType[ip] = PT_Ghost;
		dFluid.Pos[ip] = dFluid.m_constant.GridMin + PCL_DST*vec3(ivec3(ix,iy,iz));
	}}}

#define WAVE_HEIGHT 0.3
#define WAVE_WIDTH 0.1
	for(int iz=0;iz<n.z;iz++){
	for(int iy=0;iy<n.y;iy++){
	for(int ix=0;ix<n.x;ix++){
		int ip = iz * n.x * n.y + iy * n.x + ix;
		if(ix<3 || ix >=n.x-3 || iz<3 || iz>=n.z-3 || iy<3){
			// 壁を作る
			dFluid.PType[ip] = PT_Wall;
		}
		else if(dFluid.Pos[ip].y >= WAVE_HEIGHT && dFluid.Pos[ip].x <= WAVE_WIDTH){
			// ダムを造る
			dFluid.PType[ip] = PT_Fluid;
		}
	}}}

}

void SetPara(FluidData& dFluid){
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


void MkBkt(FluidData& dFluid)
{
	std::fill(GridLinkHead.begin(), GridLinkHead.end(), -1);
	for (int i = 0; i < dFluid.PNum; i++) 
	{
		if (dFluid.PType[i] == PT_Ghost)continue;
		int ib = ToGridIndex(dFluid, dFluid.Pos[i]);
		int pnumber = GridLinkHead[ib];
		GridLinkHead[ib] = i;
		GridLinkNext[i] = pnumber;
	}
}

void VscTrm(FluidData& dFluid){
	for(int i=0;i<dFluid.PNum;i++)
	{
		if (dFluid.PType[i] != PT_Fluid) 
		{
			continue;
		}
		auto viscosity = vec3(0.0);
		const auto& pos = dFluid.Pos[i];
		const auto& vel = dFluid.Vel[i];
		auto idx = ToGridCellIndex(dFluid, pos);
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=dFluid.m_constant.GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=dFluid.m_constant.GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=dFluid.m_constant.GridCellNum.x){continue;}
			int jb = ToGridIndex(dFluid, ivec3(jx, jy, jz));
			for (int j = GridLinkHead[jb]; j != -1; j = GridLinkNext[j])
			{
				if (i==j) { continue;}
				double dist2 = distance2(dFluid.Pos[j], pos);
				if (dist2 >= radius2) {continue;}

				double dist = sqrt(dist2);
				double w =  WEI(dist, radius);
				viscosity += (dFluid.Vel[j] - vel) * w;
			}

			if (dFluid.wallenable[jb]) {
				auto a = dFluid.m_constant.GridMin + vec3(jx, jy, jz) * dFluid.m_constant.GridCellSize;
				auto b = a + vec3(dFluid.m_constant.GridCellSize);
				float dist = AABB(a, b).distance(pos);
				double w = WEI(dist, radius);
				viscosity += (- vel) * w;
			}
		}}}
		dFluid.Acc[i]=viscosity* (dFluid.viscosity / n0 / lmd);
	}

// 	for (int z = 0; z < dFluid.m_constant.GridCellNum.z; z++){
// 	for (int y = 0; y < dFluid.m_constant.GridCellNum.y; y++){
// 	for (int x = 0; x < dFluid.m_constant.GridCellNum.x; x++){
// 		int i = ToGridIndex(dFluid, ivec3(x, y, z));
// 		if (dFluid.wallenable[i])
// 		{
// 			auto viscosity = vec3(0.0);
// 			auto a = dFluid.m_constant.GridMin + vec3(x, y, z) * dFluid.m_constant.GridCellSize;
// 			auto b = a + vec3(dFluid.m_constant.GridCellSize);
// 			AABB aabb(a, b);
// 			for (int j = GridLinkHead[i]; j != -1; j = GridLinkNext[j])
// 			{
// 				float dist = aabb.distance(dFluid.Pos[j]);
// 				double w = WEI(dist, radius);
// 				viscosity += (-vel) * w;
// 			}
// 			wallVsc[i] = viscosity * (dFluid.viscosity / n0 / lmd);
// 		}
// 	}}}

//	dFluid.triangle.
//	wallPrs.
}

void UpPcl1(FluidData& dFluid)
{
	for(int i=0;i<dFluid.PNum;i++)
	{
		if(dFluid.PType[i] == PT_Fluid)
		{
			dFluid.Vel[i] += (dFluid.Acc[i] + Gravity) *DT;
			dFluid.Pos[i] +=dFluid.Vel[i]*DT;
			dFluid.Acc[i]=vec3(0.0);
			if (!validCheck(dFluid, dFluid.Pos[i]))
			{
				dFluid.PType[i] = PT_Ghost;
				dFluid.Prs[i] = 0.0;
				dFluid.Vel[i] = {};
			}
		}
	}
}

void ChkCol(FluidData& dFluid)
{
	for(int i=0;i<dFluid.PNum;i++)
	{
		if (dFluid.PType[i] != PT_Fluid) {
			continue;
		}
		double mi = Dns[dFluid.PType[i]];
		const auto& pos = dFluid.Pos[i];
		const auto& vel= dFluid.Vel[i];
		auto vec_ix2 = vel;
		auto idx = ToGridCellIndex(dFluid, pos);
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=dFluid.m_constant.GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=dFluid.m_constant.GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=dFluid.m_constant.GridCellNum.x){continue;}
			int jb = ToGridIndex(dFluid, ivec3(jx, jy, jz));
			for (int j = GridLinkHead[jb]; j != -1; j = GridLinkNext[j])
			{
				if (i==j){ continue;}

				auto v = dFluid.Pos[j] - pos;
				auto dist2 = dot(v, v);
				if(dist2>=rlim2){ continue;}

				double fDT = dot((vel-dFluid.Vel[j])*v, vec3(1.));
				if(fDT > 0.0){
					double mj = Dns[dFluid.PType[j]];
					fDT *= COL*mj/(mi+mj)/dist2;
					vec_ix2 -= v*fDT;
				}
			}

			if (dFluid.PType[i] == PT_Fluid) 
			{
				if (dFluid.wallenable[jb]) {
					auto a = dFluid.m_constant.GridMin + vec3(jx, jy, jz) * dFluid.m_constant.GridCellSize;
					auto b = a + vec3(dFluid.m_constant.GridCellSize);
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
		dFluid.Acc[i]=vec_ix2;
	}
	for(int i=0;i<dFluid.PNum;i++)
	{
		dFluid.Vel[i]=dFluid.Acc[i];
	}
}

void MkPrs(FluidData& dFluid){
	for(int i=0;i<dFluid.PNum;i++)
	{
		if (dFluid.PType[i] == PT_Ghost)
		{
			continue;
		}

		const auto& pos = dFluid.Pos[i];
		double ni = 0.0;

		auto idx = ToGridCellIndex(dFluid, pos);
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=dFluid.m_constant.GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=dFluid.m_constant.GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=dFluid.m_constant.GridCellNum.x){continue;}
			int jb = ToGridIndex(dFluid, ivec3(jx, jy, jz));
			for(int j = GridLinkHead[jb];j!=-1; j = GridLinkNext[j])
			{
				if (i == j) { continue; }
				double dist2 = distance2(dFluid.Pos[j], pos);
				if (dist2 >= radius2) { continue;}
				double dist = sqrt(dist2);
				double w =  WEI(dist, radius);
				ni += w;
			}
			if (dFluid.wallenable[jb] && dFluid.PType[i] == PT_Fluid)
			{
				auto a = dFluid.m_constant.GridMin + vec3(jx, jy, jz) * dFluid.m_constant.GridCellSize;
				auto b = a + vec3(dFluid.m_constant.GridCellSize);
				auto dist = AABB(a, b).distance(pos);
				if (dist < radius)
				{
					double w = WEI(dist, radius);
					ni += w;
				}
			}

		}}}

		double mi = Dns[dFluid.PType[i]];
		double pressure = (ni > n0)*(ni - n0) * mi;
		dFluid.Prs[i] = pressure;
	}

	for (int z = 0; z < dFluid.m_constant.GridCellNum.z; z++){
	for (int y = 0; y < dFluid.m_constant.GridCellNum.y; y++){
	for (int x = 0; x < dFluid.m_constant.GridCellNum.x; x++){
		int i = ToGridIndex(dFluid, ivec3(x, y, z));
		if (!dFluid.wallenable[i])
		{
			continue;
		}
		auto a = dFluid.m_constant.GridMin + vec3(x, y, z) * dFluid.m_constant.GridCellSize;
		auto b = a + vec3(dFluid.m_constant.GridCellSize);
		double ni = 0.0;

		for(int jz=z-1;jz<=z+1;jz++){ if(jz<0||jz>=dFluid.m_constant.GridCellNum.z){continue;}
		for(int jy=y-1;jy<=y+1;jy++){ if(jy<0||jy>=dFluid.m_constant.GridCellNum.y){continue;}
		for(int jx=x-1;jx<=x+1;jx++){ if(jx<0||jx>=dFluid.m_constant.GridCellNum.x){continue;}
			int jb = ToGridIndex(dFluid, ivec3(jx, jy, jz));
			for (int j = GridLinkHead[jb]; j != -1; j = GridLinkNext[j])
			{
				if (dFluid.PType[j] != PT_Fluid) {
					continue;
				}
				double dist = AABB(a, b).distance(dFluid.Pos[j]);
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

void PrsGrdTrm(FluidData& dFluid)
{
	for(int i=0;i<dFluid.PNum;i++)
	{
		if (dFluid.PType[i] != PT_Fluid) {
			continue;
		}
		vec3 acc = vec3(0.0);
		const auto& pos = dFluid.Pos[i];
		auto pre_min = dFluid.Prs[i];
		auto idx = ToGridCellIndex(dFluid, pos);
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=dFluid.m_constant.GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=dFluid.m_constant.GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=dFluid.m_constant.GridCellNum.x){continue;}
			int jb = ToGridIndex(dFluid, ivec3(jx, jy, jz));
			for (int j = GridLinkHead[jb]; j != -1; j = GridLinkNext[j])
			{
				if (i == j) { continue; }
				if (distance2(dFluid.Pos[j], pos) >= radius2) {continue;}
				if (pre_min > dFluid.Prs[j]) 
				{
					pre_min = dFluid.Prs[j]; 
				}
			}

			if (dFluid.wallenable[jb] && pre_min > wallPrs[jb])
			{
				auto a = dFluid.m_constant.GridMin + vec3(jx, jy, jz) * dFluid.m_constant.GridCellSize;
				auto b = a + vec3(dFluid.m_constant.GridCellSize);
				auto dist = AABB(a, b).distance(pos);
				if (dist < radius)
				{
					pre_min = wallPrs[jb];
				}
			}

		}}}
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=dFluid.m_constant.GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=dFluid.m_constant.GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=dFluid.m_constant.GridCellNum.x){continue;}
			int jb = ToGridIndex(dFluid, ivec3(jx, jy, jz));
			for (int j = GridLinkHead[jb]; j != -1; j = GridLinkNext[j])
			{
				if (i == j) { continue; }
				auto v = dFluid.Pos[j] - pos;
				double dist2 = dot(v, v);
				if (dist2 >= radius2) { continue; }

				double dist = sqrt(dist2);
				double w =  WEI(dist, radius);
				w *= (dFluid.Prs[j] - pre_min)/dist2;
				acc += v*w;;
			}
			if (dFluid.wallenable[jb])
			{
				auto a = dFluid.m_constant.GridMin + vec3(jx, jy, jz) * dFluid.m_constant.GridCellSize;
				auto b = a + vec3(dFluid.m_constant.GridCellSize);
				auto dist = AABB(a, b).distance(pos);
				if (dist < radius)
				{
					double w = WEI(dist, radius);
					w *= (wallPrs[jb] - pre_min) / dist*dist;
					acc += (AABB(a, b).nearest(pos) - pos) * w;;
				}
			}
		}}}
		dFluid.Acc[i]=acc*invDns[PT_Fluid]*-1.f;
	}
}

void UpPcl2(FluidData& dFluid)
{
	for(int i=0;i<dFluid.PNum;i++)
	{
		if(dFluid.PType[i] == PT_Fluid)
		{
			dFluid.Vel[i] +=dFluid.Acc[i]*DT;
//			dFluid.Pos[i] +=dFluid.Acc[i]*DT*DT;
			dFluid.Acc[i] = vec3(0.0);
			if (!validCheck(dFluid, dFluid.Pos[i]))
			{
				dFluid.PType[i] = PT_Ghost;
			}
		}
	}
}

void init(FluidData& dFluid)
{
	AlcBkt(dFluid);
	SetPara(dFluid);
}
void run(FluidData& dFluid)
{
	MkBkt(dFluid);
	VscTrm(dFluid);
	UpPcl1(dFluid);
	ChkCol(dFluid);
	MkPrs(dFluid);
	PrsGrdTrm(dFluid);
	UpPcl2(dFluid);
//	MkPrs(dFluid);
}
