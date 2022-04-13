#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <tuple>
#include <003_particle/emps.h>

//#define _check(_n) { assert(!any(isnan(_n))); }
//#define _check(_n) { assert(!any(isnan(vec3(_n)))); }
#define _check(_n) { }
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
#define WEI(dist, re) ((re/(dist)) - 1.0f)	//重み関数
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

float Dns[PT_MAX],invDns[PT_MAX];

float safeDistance(const vec3& a, const vec3& b)
{
	return abs(dot(a, b)) < 0.000001f ? 0. : distance(a, b);
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
//#define REFERENCE
//	dFluid.m_constant.GridMin = vec3(0.0 - PCL_DST * 3);
//	dFluid.m_constant.GridMax = vec3(1.0 + PCL_DST * 3, 0.6 + PCL_DST * 20, 1.0 + PCL_DST * 3);
	dFluid.m_constant.GridMin = vec3(0.0);
	dFluid.m_constant.GridMax = vec3(0.5, 0.5, 0.5);
//	dFluid.m_constant.GridMax = vec3(1.0, 0.5, 1.0);
	dFluid.m_constant.GridCellSize = radius * (1.0 + CRT_NUM);
	dFluid.m_constant.GridCellSizeInv = 1.0 / dFluid.m_constant.GridCellSize;
	dFluid.m_constant.GridCellNum = ivec3((dFluid.m_constant.GridMax - dFluid.m_constant.GridMin) * dFluid.m_constant.GridCellSizeInv) + ivec3(4);
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

	dFluid.wallesdf.resize(dFluid.m_constant.GridCellTotal);
	dFluid.wallenable.resize(dFluid.m_constant.GridCellTotal);
	dFluid.walln.resize(dFluid.m_constant.GridCellTotal);
	
	wallPrs.resize(dFluid.m_constant.GridCellTotal);
	wallVsc.resize(dFluid.m_constant.GridCellTotal);
	ivec3 n = dFluid.m_constant.GridCellNum;
	// 壁の設定
	for(int iz=0;iz<n.z;iz++){
	for(int iy=0;iy<n.y;iy++){
	for(int ix=0;ix<n.x;ix++){
		int ip = iz*n.x* n.y + iy*n.x + ix;
		auto p = dFluid.m_constant.GridMin + dFluid.m_constant.GridCellSize * vec3(ix, iy, iz);
		float d = 99999.f;
		vec3 n;
		for (auto& t : dFluid.triangles)
		{
			auto new_d = t.getDistance(p);
			if (new_d <= d )
			{

#if !defined(REFERENCE)
				d = new_d;
#endif
				n = t.getNormal();
			}
		}
		if (d<radius)
		{
			dFluid.wallenable[ip] = true;
		}
		dFluid.wallesdf[ip] = d;
		dFluid.walln[ip] = n;
	}}}
#define WAVE_HEIGHT 0.3
#define WAVE_WIDTH 0.1

	n = (dFluid.m_constant.GridMax - dFluid.m_constant.GridMin) / PCL_DST;
	int count = 0;
	for(int iz=0;iz<n.z;iz++){
	for(int iy=0;iy<n.y;iy++){
	for(int ix=0;ix<n.x;ix++){
		int ip = iz * n.x * n.y + iy * n.x + ix;
		if(ix<3 || ix >=n.x-3 || iz<3 || iz>=n.z-3 || iy<3){
			// 壁を作る
#if defined(REFERENCE)
			dFluid.PType[count] = PT_Wall;
			dFluid.Pos[count] = dFluid.m_constant.GridMin + PCL_DST * vec3(ivec3(ix, iy, iz));
			count++;
#endif
		}
		else if(iy >= 4 && iy <= 8 && ix <= 15 && iz <= 15){
			// ダムを造る
			dFluid.PType[count] = PT_Fluid;
			dFluid.Pos[count] = dFluid.m_constant.GridMin + PCL_DST * vec3(ivec3(ix, iy, iz));
			count++;
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
			ww += WEI(dist, radius)*4.f;
		}
		wallweight[iy] = ww;
//		wallweight[iy] = 1.f;
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

float sdf(FluidData& dFluid, const ivec3& idx)
{
	return dFluid.wallesdf[ToGridIndex(dFluid, idx)];
}

float prs(FluidData& dFluid, const ivec3& idx)
{
	return wallPrs[ToGridIndex(dFluid, idx)];
}

std::tuple<float, vec3> getSDF(FluidData& dFluid, ivec3 idx, const vec3& pos)
{
	idx = min(idx, dFluid.m_constant.GridCellNum - 1);
	vec3 pa = dFluid.m_constant.GridMin + dFluid.m_constant.GridCellSize * vec3(idx);
	vec3 fp = (pos - pa) * dFluid.m_constant.GridCellSizeInv;
	auto value = sdf(dFluid, idx);

	// check
	if (value > radius + dFluid.m_constant.GridCellSize*sqrt(3.f)) { return { 999.f, vec3(0.) }; }

	ivec2 offset = ivec2(0, 1);
	vec4 a = vec4(value, sdf(dFluid, idx + offset.yxx()), sdf(dFluid, idx + offset.xyx()), sdf(dFluid, idx + offset.yyx));
	vec4 b = vec4(sdf(dFluid, idx + offset.xxy()), sdf(dFluid, idx + offset.yxy()), sdf(dFluid, idx + offset.xyy()), sdf(dFluid, idx + offset.yyy));
	auto z = mix(a, b, fp.z);
	auto xy = vec2(mix(z.xy(), z.zw(), fp.yy()));
	auto dist = mix(vec1(xy.x), vec1(xy.y), vec1(fp.x)).x;

	auto zs = dot(b - a, vec4(1.f));
	auto ys = dot(vec4(a.zw(), b.zw()) - vec4(a.xy(), b.xy()), vec4(1.f));
	auto xs = dot(vec4(a.yw(), b.yw()) - vec4(a.xz(), b.xz()), vec4(1.f));
	vec3 n = normalize(vec3(xs, ys, zs));
	_check(dist);
	_check(n);
	return { glm::max(dist, 0.f), n };
}

float getPrs(FluidData& dFluid, ivec3 idx, const vec3& pos)
{
	idx = min(idx, dFluid.m_constant.GridCellNum - 1);
	vec3 pa = dFluid.m_constant.GridMin + dFluid.m_constant.GridCellSize * vec3(idx);
	vec3 fp = (pos - pa) * dFluid.m_constant.GridCellSizeInv;
	auto value = sdf(dFluid, idx);

	ivec2 offset = ivec2(0, 1);
	vec4 a = vec4(prs(dFluid, idx + offset.xxx()), prs(dFluid, idx + offset.yxx()), prs(dFluid, idx + offset.xyx()), prs(dFluid, idx + offset.yyx));
	vec4 b = vec4(prs(dFluid, idx + offset.xxy()), prs(dFluid, idx + offset.yxy()), prs(dFluid, idx + offset.xyy()), prs(dFluid, idx + offset.yyy));
	auto z = mix(a, b, fp.z);
	auto xy = vec2(mix(z.xy(), z.zw(), fp.yy()));
	auto dist = mix(vec1(xy.x), vec1(xy.y), vec1(fp.x)).x;

	return dist;
}

void VscTrm(FluidData& dFluid) {
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

		}}}

		auto sdf = getSDF(dFluid, idx, pos);
		if (std::get<0>(sdf) < radius)
		{
			double w = WEI(std::get<0>(sdf), radius);
			viscosity += (-vel) * w;
		}

		dFluid.Acc[i]=viscosity* (dFluid.viscosity / n0 / lmd);
		if (abs(dFluid.Acc[i].y) >= 1000.f)
		{
			int a = 0;
		}
		_check(viscosity);
	}

}

void UpPcl1(FluidData& dFluid)
{
	for(int i=0;i<dFluid.PNum;i++)
	{
		if(dFluid.PType[i] == PT_Fluid)
		{
			dFluid.Vel[i] += (dFluid.Acc[i] + Gravity) *DT;
			dFluid.Pos[i] += dFluid.Vel[i]*DT;
			dFluid.Acc[i]=vec3(0.0);

			if (!validCheck(dFluid, dFluid.Pos[i]))
			{
				dFluid.PType[i] = PT_Ghost;
				dFluid.Prs[i] = 0.0;
				dFluid.Vel[i] = {};
//				DebugBreak();
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
				v = normalize(v);
				double fDT = dot((vel-dFluid.Vel[j])*v, vec3(1.));
				if(fDT > 0.0){
					double mj = Dns[dFluid.PType[j]];
					fDT *= COL*mj/(mi+mj);
					vec_ix2 -= v*fDT;
				}
			}

		}}}
		auto sdf = getSDF(dFluid, idx, pos);
		if (std::get<0>(sdf) < radius)
		{
			auto wallp = pos - std::get<1>(sdf) * std::get<0>(sdf);
//			auto v = wallp - pos;
//			v = normalize(v);

			double fDT = dot(vel* -std::get<1>(sdf), vec3(1.));
			if (fDT > 0.0) {
				double mj = Dns[PT_Wall];
				fDT *= COL * mj / (mi + mj);
				vec_ix2 -= -std::get<1>(sdf) * fDT;
			}
		}
		_check(vec_ix2);
		dFluid.Acc[i]=vec_ix2;
		if (abs(vec_ix2.y) >= 1000.f)
		{
			int a = 0;
		}
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
		}}}
		auto sdf = getSDF(dFluid, idx, pos);
		if (std::get<0>(sdf) < radius)
		{
			double w = WEI(std::get<0>(sdf), radius);
		}

		double mi = Dns[dFluid.PType[i]];
		double pressure = (ni > n0)*(ni - n0) * mi;
		dFluid.Prs[i] = pressure;
		_check(pressure);
	}

	for (int z = 0; z < dFluid.m_constant.GridCellNum.z; z++){
	for (int y = 0; y < dFluid.m_constant.GridCellNum.y; y++){
	for (int x = 0; x < dFluid.m_constant.GridCellNum.x; x++){
		int i = ToGridIndex(dFluid, ivec3(x, y, z));
		{
			if (dFluid.wallesdf[i] > radius *2*sqrt(3.f))
			{
				continue;
			}

		}

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
				auto sdf = getSDF(dFluid, ivec3(jx, jy, jz), dFluid.Pos[j]);
				if (std::get<0>(sdf) >= radius) { continue; }
				double w = WEI(std::get<0>(sdf), radius);
				ni += w;
			}
		}}}
		float mi = Dns[PT_Wall];
		float pressure = (ni > n0) * (ni - n0) * mi;
		wallPrs[i] = pressure;
		_check(pressure);

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

			if (pre_min > wallPrs[jb])
			{
				auto sdf = getSDF(dFluid, idx, pos);
				if (std::get<0>(sdf) < radius)
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
				w *= (dFluid.Prs[j] - pre_min)/(dist2);
				acc += v*w;
			}
		}}}
		auto sdf = getSDF(dFluid, idx, pos);
		if (std::get<0>(sdf) < radius)
		{
			double w = WEI(std::get<0>(sdf), radius);
			w *= (wallPrs[ToGridIndex(dFluid, idx)] - pre_min)/* / (std::get<0>(sdf) * std::get<0>(sdf))*/;

			auto wallp = pos - std::get<1>(sdf) * std::get<0>(sdf);
			acc += -std::get<1>(sdf) * w;// * //-wallweight[int(std::get<0>(sdf)/radius*16.f) ];
		}
		_check(acc);

		dFluid.Acc[i]=acc*invDns[PT_Fluid]*-1.f;
		if (abs(dFluid.Acc[i].y) >= 100.f)
		{
			int a = 0;
		}
	}
}

void UpPcl2(FluidData& dFluid)
{
	for(int i=0;i<dFluid.PNum;i++)
	{
		if(dFluid.PType[i] == PT_Fluid)
		{
			if (abs(dFluid.Acc[i].y) >= 100.f)
			{
				int a = 0;
			}
			dFluid.Vel[i] +=dFluid.Acc[i]*DT;
//			dFluid.Pos[i] +=dFluid.Acc[i]*DT*DT;
			dFluid.Acc[i] = vec3(0.0);
			if (!validCheck(dFluid, dFluid.Pos[i]))
			{
				dFluid.PType[i] = PT_Ghost;
				//				DebugBreak();
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
}

#include <applib/App.h>
#include <applib/sAppImGui.h>
void gui(FluidData& cFluid)
{
	app::g_app_instance->m_window->getImgui()->pushImguiCmd([&]()
		{
// 			ImGui::SetNextWindowSize(ImVec2(200.f, 40.f), ImGuiCond_Once);
// 			static bool is_open;
// 			if (ImGui::Begin("FluidConfig", &is_open, ImGuiWindowFlags_NoSavedSettings))
// 			{
// 				ImGui::SliderFloat("viscosity", &cFluid.viscosity, 0.f, 30.f);
// 
// 				static ivec3 pos;
// 				ImGui::SliderInt3("test sdf", &pos.x, 0, cFluid.m_constant.GridCellNum.x);
// 				auto sdf = getSDF(cFluid, pos, (vec3(pos)+vec3(0.5f))*cFluid.m_constant.GridCellSize);
// 				ImGui::InputFloat("sdf", &std::get<0>(sdf));
// 				ImGui::InputFloat3("normal", &std::get<1>(sdf).x);
// 			}
// 
// 			ImGui::End();
		});

}