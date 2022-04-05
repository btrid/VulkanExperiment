﻿#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <003_particle/emps.h>

#define PCL_DST 0.02					//平均粒子間距離
dvec3 GridMin = dvec3(0.0 - PCL_DST * 3);
dvec3 GridMax = dvec3(1.0 + PCL_DST * 3, 0.2 + PCL_DST * 3, 0.6 + PCL_DST * 30);
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
#define DT 0.0005			//時間刻み幅
#define SND 22.0			//音速
#define KNM_VSC 0.000001	//動粘性係数
#define DIM 3				//次元数
#define CRT_NUM 0.1		//クーラン条件数
#define COL_RAT 0.2		//接近した粒子の反発率
#define DST_LMT_RAT 0.9	//これ以上の粒子間の接近を許さない距離の係数
#define WEI(dist, re) ((re/dist) - 1.0)	//重み関数
#define Gravity dvec3(0.f, 0.f, -9.8);//重力加速度

double radius = PCL_DST * 2.1;
double radius2 = radius*radius;

double GridCellSize; //GridCellの大きさ(バケット1辺の長さ)
double GridCellSizeInv;
ivec3 GridCellNum;
int GridCellTotal;

std::vector<int> GridLinkHead;
std::vector<int> GridLinkNext;

double n0; //初期粒子数密度
double lmd;	//ラプラシアンモデルの係数λ
double A1;//粘性項の計算に用いる係数
double A2;//圧力の計算に用いる係数
double A3;//圧力勾配項の計算に用いる係数
double rlim; //これ以上の粒子間の接近を許さない距離
double rlim2;
double COL; // 反発率？

double Dns[PT_MAX],invDns[PT_MAX];

bool validCheck(const dvec3& pos)
{
	if (any(greaterThan(pos, GridMax)) || any(lessThan(pos, GridMin)))
	{
		return false;
	}
	return true;
}

ivec3 ToGridCellIndex(const dvec3& p){	return ivec3((p - GridMin) * GridCellSizeInv) + 1;}
int ToGridIndex(const ivec3& i){	return i.x + i.y * GridCellNum.x + i.z * GridCellNum.x * GridCellNum.y;}
int ToGridIndex(const dvec3& p){	return ToGridIndex(ToGridCellIndex(p));}
void AlcBkt(FluidContext& cFluid)
{
	cFluid.PNum = 20000;
	cFluid.Acc.resize(cFluid.PNum);
	cFluid.Pos.resize(cFluid.PNum);
	cFluid.Vel.resize(cFluid.PNum);
	cFluid.Prs.resize(cFluid.PNum);
	cFluid.PType.resize(cFluid.PNum);

	GridCellSize = radius*(1.0+CRT_NUM);
	GridCellSizeInv = 1.0/ GridCellSize;
	GridCellNum = ivec3((GridMax - GridMin)*GridCellSizeInv) + ivec3(3);
	GridCellTotal = GridCellNum.x * GridCellNum.y * GridCellNum.z;

	GridLinkHead.resize(GridCellTotal);
	GridLinkNext.resize(cFluid.PNum);

	ivec3 n = GridCellNum;
	for (auto& p : cFluid.PType)
	{
		p = PT_Ghost;
	}


#define WAVE_HEIGHT 0.5
#define WAVE_WIDTH 0.25
	for (int iz = 0; iz < n.z; iz++) {
		for (int iy = 0; iy < n.y; iy++) {
			for (int ix = 0; ix < n.x; ix++) {
				int ip = iz * n.x * n.y + iy * n.x + ix;
				cFluid.Pos[ip] = GridMin + PCL_DST * dvec3(ivec3(ix, iy, iz) - 3);
				if (ix < 3 || ix >= n.x - 3 || iy < 3 || iy >= n.y - 3 || iz < 3) {
					// 箱を作る
					cFluid.PType[ip] = PT_Wall;
				}
				else if (cFluid.Pos[ip].y <= WAVE_HEIGHT && cFluid.Pos[ip].x <= WAVE_WIDTH) {
					// ダムを造る
					cFluid.PType[ip] = PT_Fluid;
				}
			}
		}
	}

}

void SetPara(FluidContext& cFluid){
	double tn0 =0.0;
	double tlmd =0.0;
	for(int ix= -4;ix<5;ix++){
	for(int iy= -4;iy<5;iy++){
	for(int iz= -4;iz<5;iz++){
		dvec3 p = PCL_DST* dvec3(ix, iy, iz);
		double dist2 = dot(p,p);
		if(dist2 <= radius2){
			if(dist2==0.0)continue;
			double dist = sqrt(dist2);
			tn0 += WEI(dist, radius);
			tlmd += dist2 * WEI(dist, radius);
		}
	}}}
	n0 = tn0;			//初期粒子数密度
	lmd = tlmd/tn0;	//ラプラシアンモデルの係数λ
	A1 = 2.0*KNM_VSC*DIM/n0/lmd;//粘性項の計算に用いる係数
	A2 = SND*SND/n0;				//圧力の計算に用いる係数
	A3 = -DIM/n0;					//圧力勾配項の計算に用いる係数
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
		GridLinkNext[ib] = pnumber;
	}
}

void VscTrm(FluidContext& cFluid){
	for(int i=0;i<cFluid.PNum;i++)
	{
		if (cFluid.PType[i] != PT_Fluid) 
		{
			continue;
		}
		auto acc = dvec3(0.0);
		auto pos = cFluid.Pos[i];
		auto vel = cFluid.Vel[i];
		auto idx = ToGridCellIndex(pos);
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=GridCellNum.x){continue;}
			int jb = ToGridIndex(ivec3(jx, jy, jz));
			int j = GridLinkHead[jb];
			if(j == -1) continue;
			for(;;){
				auto v = cFluid.Pos[j] - pos;
				double dist2 = dot(v, v);
				if(dist2<radius2){
				if(j!=i && cFluid.PType[j]!=PT_Ghost){
					double dist = sqrt(dist2);
					double w =  WEI(dist, radius);
					acc += (cFluid.Vel[j] - vel) * w;
				}}
				j = GridLinkNext[j];
				if(j==-1) break;
			}
		}}}
		cFluid.Acc[i]=acc*A1 + Gravity;
	}
}

void UpPcl1(FluidContext& cFluid)
{
	for(int i=0;i<cFluid.PNum;i++)
	{
		if(cFluid.PType[i] == PT_Fluid)
		{
			cFluid.Vel[i] +=cFluid.Acc[i]*DT;
			cFluid.Pos[i] +=cFluid.Vel[i]*DT;
			cFluid.Acc[i]=dvec3(0.0);
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
		auto pos = cFluid.Pos[i];
		auto vec= cFluid.Vel[i];
		auto vec_ix2 = vec;
		auto idx = ToGridCellIndex(pos);
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=GridCellNum.x){continue;}
			int jb = ToGridIndex(ivec3(jx, jy, jz));
			int j = GridLinkHead[jb];
			if(j == -1) continue;
			for(;;){
				auto v = cFluid.Pos[j] - pos;
				auto dist2 = dot(v, v);
				if(dist2<rlim2)
				{
					if(j!=i && cFluid.PType[j]!=PT_Ghost)
					{
						double fDT = dot(vec-cFluid.Vel[j], dvec3(1.));
						if(fDT > 0.0){
							double mj = Dns[cFluid.PType[j]];
							fDT *= COL*mj/(mi+mj)/dist2;
							vec_ix2 -= v*fDT;;
						}
					}
				}
				j = GridLinkNext[j];
				if(j==-1) break;
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

		auto pos = cFluid.Pos[i];
		double ni = 0.0;
		auto idx = ToGridCellIndex(pos);

		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=GridCellNum.x){continue;}
			int jb = ToGridIndex(ivec3(jx, jy, jz));
			for(int j = GridLinkHead[jb];j!=-1; j = GridLinkNext[j])
			{
				auto v = cFluid.Pos[j] - pos;
				double dist2 = dot(v,v);
				if (dist2 >= radius2) { continue;}
				if(j!=i && cFluid.PType[j]!=PT_Ghost){
					double dist = sqrt(dist2);
					double w =  WEI(dist, radius);
					ni += w;
				}
			}
		}}}
		double mi = Dns[cFluid.PType[i]];
		double pressure = (ni > n0)*(ni - n0) * A2 * mi;
		cFluid.Prs[i] = pressure;
	}
}

void PrsGrdTrm(FluidContext& cFluid){
	for(int i=0;i<cFluid.PNum;i++){
	if(cFluid.PType[i] == PT_Fluid){
		dvec3 acc = dvec3(0.0);
		auto pos= cFluid.Pos[i];
		auto pre_min = cFluid.Prs[i];
		auto idx = ToGridCellIndex(pos);
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=GridCellNum.x){continue;}
			int jb = ToGridIndex(ivec3(jx, jy, jz));
			int j = GridLinkHead[jb];
			if(j == -1) continue;
			for(;;){
				auto v = cFluid.Pos[j] - pos;
				double dist2 = dot(v, v);
				if(dist2<radius2){
				if(j!=i && cFluid.PType[j]!=PT_Ghost){
					if (pre_min > cFluid.Prs[j]) { pre_min = cFluid.Prs[j]; }
				}}
				j = GridLinkNext[j];
				if(j==-1) break;
			}
		}}}
		for(int jz=idx.z-1;jz<=idx.z+1;jz++){ if(jz<0||jz>=GridCellNum.z){continue;}
		for(int jy=idx.y-1;jy<=idx.y+1;jy++){ if(jy<0||jy>=GridCellNum.y){continue;}
		for(int jx=idx.x-1;jx<=idx.x+1;jx++){ if(jx<0||jx>=GridCellNum.x){continue;}
			int jb = ToGridIndex(ivec3(jx, jy, jz));
			int j = GridLinkHead[jb];
			if(j == -1) continue;
			for(;;){
				auto v = cFluid.Pos[j] - pos;
				double dist2 = dot(v, v);
				if(dist2<radius2){
				if(j!=i && cFluid.PType[j]!=PT_Ghost){
					double dist = sqrt(dist2);
					double w =  WEI(dist, radius);
					w *= (cFluid.Prs[j] - pre_min)/dist2;
					acc += v*w;;
				}}
				j = GridLinkNext[j];
				if(j==-1) break;
			}
		}}}
		cFluid.Acc[i]=acc*invDns[PT_Fluid]*A3;
	}}
}

void UpPcl2(FluidContext& cFluid)
{
	for(int i=0;i<cFluid.PNum;i++)
	{
		if(cFluid.PType[i] == PT_Fluid)
		{
			cFluid.Vel[i] +=cFluid.Acc[i]*DT;
			cFluid.Pos[i] +=cFluid.Acc[i]*DT*DT;
			cFluid.Acc[i] = dvec3(0.0);
			if (!validCheck(cFluid.Pos[i]))
			{
				cFluid.PType[i] = PT_Ghost;
				cFluid.Prs[i] = 0.0;
				cFluid.Vel[i] = {};
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
	MkPrs(cFluid);
}
