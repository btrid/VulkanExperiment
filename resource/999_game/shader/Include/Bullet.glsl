
#ifndef BULLET_GLSL
#define BULLET_GLSL

#include <btrlib/common.glsl>

struct BulletInfo
{
	uint m_max_num;
};
struct BulletData
{
	vec4 m_pos;	//!< xyz:pos w:scale
	vec4 m_vel;	//!< xyz:dir

	uint m_ll_next;
	float m_power;
	ivec2 m_map_index;

	uint m_type;
	uint m_flag;
	float m_life;
	float m_atk;

};

#ifdef SETPOINT_BULLET
layout(std140, set=SETPOINT_BULLET, binding=0) uniform ParticleInfoUniform {
	BulletInfo u_bullet_info;
};

layout(std430, set=SETPOINT_BULLET, binding=1) restrict buffer BulletDataBuffer {
	BulletData b_bullet[];
};
layout(std430, set=SETPOINT_BULLET, binding=2) restrict buffer BulletCounterBuffer {
	DrawIndirectCommand b_bullet_draw_cmd;
};
layout(std430, set=SETPOINT_BULLET, binding=3) readonly restrict buffer BulletEmitBuffer {
	BulletData b_bullet_emit[];
};
layout(std430, set=SETPOINT_BULLET, binding=4) restrict buffer BulletLLHeadBuffer {
	uint b_bullet_head[];
};
layout(std430, set=SETPOINT_BULLET, binding=5) readonly restrict buffer BulletEmitCountBuffer {
	uint b_bullet_emit_count;
};


#endif

#endif