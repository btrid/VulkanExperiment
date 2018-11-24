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

	float m_move;
	int crowd_type;
	int unit_type;
	int link_next;
};

struct CrowdRay
{
	vec2 origin;
	float angle;
	uint march;
};
struct CrowdSegment
{
	uint ray_index;
	uint begin_march;
	uint weight_a;
	uint weight_b;
};

struct CrowdScene
{
	int m_frame;
	int m_hierarchy;
	uint m_skip;
	int _p2;
};

struct PathNode
{
	uint value;
};

vec2 MakeRayDir(in float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return vec2(-s, c);
}


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
layout(set=USE_Crowd2D, binding=4, std430) restrict buffer UnitBuffer {
	UnitData b_unit[];
};
layout(set=USE_Crowd2D, binding=5, std430) restrict buffer UnitCounter {
	ivec4 b_unit_counter;
};
layout(set=USE_Crowd2D, binding=6, std430) restrict buffer UnitLinkList {
	int b_unit_link_head[];
};

layout(set=USE_Crowd2D, binding=7, std430) restrict buffer CRayBuffer {
	CrowdRay b_ray[];
};
layout(set=USE_Crowd2D, binding=8) restrict buffer CRayCounter {
	ivec4 b_ray_counter[];
};

layout(set=USE_Crowd2D, binding=9, std430) restrict buffer CSegmentBuffer {
	CrowdSegment b_segment[];
};
layout(set=USE_Crowd2D, binding=10) restrict buffer CSegmentCounter {
	ivec4 b_segment_counter;
};
layout(set=USE_Crowd2D, binding=11) restrict buffer CPathNodeBuffer {
	PathNode b_node[];
};


#endif
const int g_crowd_density_cell_size = 64;


vec2 inverse_safe(in vec2 v)
{
	vec2 inv;
	inv.x = v.x == 0 ? 99999. : 1./v.x;
	inv.y = v.y == 0 ? 99999. : 1./v.y;
	return inv;

}
#endif