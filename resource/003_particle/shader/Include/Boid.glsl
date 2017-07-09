

struct BoidInfo
{
	uint m_brain_max;
	uint m_soldier_max;

};
struct BrainData
{
	uint m_is_alive;
	uint m_type;
	uint _p2;
	uint _p3;
	vec4 m_target;
};

struct SoldierInfo
{
	float m_move_speed;
	float m_turn_speed;
	uint _p2;
	uint _p3;
};
struct SoldierData
{
	vec4 m_pos;	//!< xyz:pos w:scale
	vec4 m_vel;	//!< xyz:dir w:distance
	ivec4 m_map_index;
	uint m_brain_index;
	uint m_type;
	uint m_ll_next;
	float m_life;
	vec4 m_astar_target;
};

#ifdef SETPOINT_BOID
layout(std140, set=SETPOINT_BOID, binding=0) uniform BoidInfoUniform {
	BoidInfo u_boid_info;
};
layout(std140, set=SETPOINT_BOID, binding=1) uniform SoldierInfoUniform {
	SoldierInfo u_soldier_info[16];
};

layout(std430, set=SETPOINT_BOID, binding=2) restrict buffer BrainDataBuffer {
	BrainData b_brain[];
};
layout(std430, set=SETPOINT_BOID, binding=3) restrict buffer SoldierDataBuffer {
	SoldierData b_soldier[];
};
layout(std430, set=SETPOINT_BOID, binding=4) restrict buffer CounterBuffer {
	DrawIndirectCommand b_draw_cmd[];
};
layout(std430, set=SETPOINT_BOID, binding=5) restrict coherent buffer SoldierLLHeadBuffer {
	uint b_soldier_head[];
};
layout(set=SETPOINT_BOID, binding=6, r32ui) uniform readonly uimage2DArray t_astar;
#endif
