

struct BulletInfo
{
	uint m_max_num;
};
struct BulletData
{
	vec4 m_pos;	//!< xyz:pos w:scale
	vec4 m_vel;	//!< xyz:dir
	ivec4 m_map_index;

	uint m_type;
	uint m_flag;
	float m_life;

	float m_atk;
	float m_power;
	float m_1;
	uint m_ll_next;

};

#ifdef SETPOINT_BULLET
layout(std140, set=SETPOINT_BULLET, binding=0) uniform ParticleInfoUniform {
	BulletInfo u_bullet_info;
};

layout(std430, set=SETPOINT_BULLET, binding=1) restrict buffer ParticleDataBuffer {
	BulletData b_bullet[];
};
layout(std430, set=SETPOINT_BULLET, binding=2) restrict buffer BulletCounterBuffer {
	DrawIndirectCommand b_bullet_draw_cmd;
};
layout(std430, set=SETPOINT_BULLET, binding=3) readonly buffer BulletEmitBuffer {
	BulletData b_bullet_emit[];
};
layout(std430, set=SETPOINT_BULLET, binding=4) restrict buffer BulletLLHeadBuffer {
	uint b_bullet_head[];
};


#endif
