#ifndef Crowd2D_H_
#define Crowd2D_H_


#if defined(USE_Crowd2D)

struct CrowdInfo
{
	uint crowd_type_max;
	uint crowd_max;
	uint unit_max;
};
struct CrowdData
{
	vec2 target;
	vec2 _p;
};
struct UnitInfo
{
	float linear_speed;
	float angler_speed;
	float _p2;
	float _p3;
};

struct UnitData
{
	vec2 m_pos;
	vec2 m_dir;
	int crowd_type;
	int unit_type;
	int _p2;
	int _p3;
};

layout(set=USE_Crowd2D, binding=0, std140) uniform CrowdInfoUniform {
	CrowdInfo u_crowd_info;
};
layout(set=USE_Crowd2D, binding=1, std140) uniform UnitInfoUniform {
	UnitInfo u_unit_info[16];
};
layout(set=USE_Crowd2D, binding=2, std430) restrict buffer CrowdBuffer {
	CrowdData b_crowd[];
};
layout(set=USE_Crowd2D, binding=3, std430) restrict buffer UnitBuffer {
	UnitData b_unit[];
};
layout(set=USE_Crowd2D, binding=4, std430) restrict buffer UnitCounter {
	ivec4 b_unit_counter;
};
layout(set=USE_Crowd2D, binding=5, std430) restrict buffer SoftbodyInfo {
	uint b_grid_counter[];
};

#endif


#endif