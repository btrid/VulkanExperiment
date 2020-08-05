#ifndef Crowd2D_H_
#define Crowd2D_H_


#if defined(USE_Crowd2D)

struct CrowdInfo
{
	uint crowd_info_max;
	uint unit_info_max;
	uint crowd_data_max;
	uint unit_data_max;

	uint ray_num_max;
	uint ray_frame_max; //!< 1frameにおけるRayの最大の数
	uint frame_max;
	uint ray_angle_num;

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
	float m_rot;
	float m_rot_prev;

//	float m_move;
//	int link_next;
};

struct Info
{
	int crowd_type;
	int unit_type;
};
struct CrowdScene
{
	float m_deltatime;
	int m_frame;
	int m_hierarchy;
	uint m_skip;
	int _p2;
};



layout(set=USE_Crowd2D, binding=0, std140) uniform CrowdInfoUniform {
	CrowdInfo u_crowd_info;
};
layout(set=USE_Crowd2D, binding=1, std140) uniform CrowdSceneUniform {
	CrowdScene u_crowd_scene;
};
layout(set=USE_Crowd2D, binding=2, std140) uniform UnitInfoUniform {
	UnitInfo u_unit_info[16];
};
layout(set=USE_Crowd2D, binding=3, std430) restrict buffer CrowdBuffer {
	CrowdData b_crowd[];
};
layout(set=USE_Crowd2D, binding=4, std430) restrict buffer UnitCounter {
	ivec4 b_unit_counter;
};
layout(set=USE_Crowd2D, binding=5, std430) restrict buffer UnitLinkList {
	int b_unit_link_head[];
};
layout(set=USE_Crowd2D, binding=20, std430) restrict buffer UnitPosBuffer 
{
	vec4 b_unit_pos[];
};
layout(set=USE_Crowd2D, binding=21, std430) restrict buffer UnitMoveBuffer 
{
	vec2 b_unit_move[];
};
layout(set=USE_Crowd2D, binding=23, std430) restrict buffer UnitInfoBuffer 
{
	Info b_unit_info[];
};

const int g_crowd_density_cell_size = 64;
#endif


#endif