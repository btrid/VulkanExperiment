

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
	float m_2;
	float m_3;

};

#ifdef SETPOINT_UPDATE
layout(std140, set=SETPOINT_UPDATE, binding=0) uniform ParticleInfoUniform {
	BulletInfo u_bullet_info;
};

layout(std430, set=SETPOINT_UPDATE, binding=1) restrict buffer ParticleDataBuffer {
	BulletData b_bullet[];
};
layout(std430, set=SETPOINT_UPDATE, binding=2) restrict buffer CounterBuffer {
	DrawIndirectCommand b_draw_cmd;
};
layout(std430, set=SETPOINT_UPDATE, binding=3) readonly buffer EmitBuffer {
	BulletData b_bullet_emit[];
};



#endif
