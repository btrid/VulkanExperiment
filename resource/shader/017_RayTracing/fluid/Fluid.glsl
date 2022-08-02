#ifndef FLUID_GLSL_H_
#define FLUID_GLSL_H_
#define PT_Ghost 0
#define PT_Fluid 1
#define PT_Smoke 2
#define PT_Wall 3
#define PT_MAX 4

#define DNS_FLD 1000		//流体粒子の密度
#define DNS_SMORK 1		//流体粒子の密度
#define DNS_WLL 1000		//壁粒子の密度
#define DT 0.0005f			//時間刻み幅
#define CRT_NUM 0.1f		//クーラン条件数
#define COL_RAT 0.2f		//接近した粒子の反発率
#define DST_LMT_RAT 0.9f	//これ以上の粒子間の接近を許さない距離の係数
#define WEI(dist, re) ((re/(dist)) - 1.0f)	//重み関数
#define Gravity vec3(0.f, -9.8f, 0.f) //重力加速度
float radius = PCL_DST * 2.1f;
float radius2 = radius * radius;
#define PCL_DST 0.02f					//平均粒子間距離

float Dns[PT_MAX] = {DNS_FLD, 1., DNS_SMORK, DNS_WLL};
float invDns[PT_MAX] = {1./DNS_FLD, 1., 1./DNS_SMORK, 1./DNS_WLL};

float safeDistance(in vec3 a, in vec3 b)
{
	return abs(dot(a, b)) < 0.000001f ? 0. : distance(a, b);
}
bool validCheck(in FluidData dFluid, in vec3 pos)
{
	return all(greaterThanEqual(pos, dFluid.m_constant.GridMin)) && all(lessThan(pos, dFluid.m_constant.GridMax));
}

ivec3 ToGridCellIndex(in vec3 p)
{
	return ivec3((p - u_info.GridMin) * u_info.GridCellSizeInv);
}
int ToGridIndex(in ivec3 i) 
{
	//	assert(all(greaterThanEqual(i, ivec3(0))) && all(lessThan(i, dFluid.m_constant.GridCellNum)));
	return i.x + i.y * u_info.GridCellNum.x + i.z * u_info.GridCellNum.x * u_info.GridCellNum.y;
}
ivec4 ToGridIndex(in vec3 p) 
{
	ivec3 i = ivec3((p - u_info.GridMin) * u_info.GridCellSizeInv);
    return ivec4(i, i.x + i.y * u_info.GridCellNum.x + i.z * u_info.GridCellNum.x * u_info.GridCellNum.y); 
}

struct Constant
{
    float n0; //初期粒子数密度
    float lmd;	//ラプラシアンモデルの係数λ
    float rlim; //これ以上の粒子間の接近を許さない距離
    float COL; // 反発率？

    float viscosity; //!< 粘度
    float GridCellSize; //GridCellの大きさ(バケット1辺の長さ)
    float GridCellSizeInv;
    int GridCellTotal;

    ivec3 GridCellNum;
    float p1;

    vec3 GridMin;
    float p2;

    vec3 GridMax;
    float p3;

    ivec3 WallCellNum;
    int WallCellTotal;
};
    
layout(set=USE_Fluid,binding=0, std140) uniform U0 {VoxelInfo u_info; };
layout(set=USE_Fluid,binding=2, scalar) buffer B1 { int GridLinkHead[]; };
layout(set=USE_Fluid,binding=3, scalar) buffer B2 { int GridLinkNext[]; };
layout(set=USE_Fluid,binding=4, scalar) buffer B3 { int wallPrs[]; };
layout(set=USE_Fluid,binding=5, scalar) buffer B4 { int wallVsc[]; };
layout(set=USE_Fluid,binding=10, scalar) buffer B10 { vec3 b_pos[]; };
layout(set=USE_Fluid,binding=11, scalar) buffer B11 { vec3 b_vel[]; };
layout(set=USE_Fluid,binding=12, scalar) buffer B12 { vec3 b_acc[]; };
layout(set=USE_Fluid,binding=13, scalar) buffer B13 { float b_sdf[]; };
layout(set=USE_Fluid,binding=14, scalar) buffer B14 { vec3 b_sdf_normal[]; };
layout(set=USE_Fluid,binding=15, scalar) buffer B15 { float b_prs[]; };
layout(set=USE_Fluid,binding=16, scalar) buffer B16 { int b_type[]; };

#endif