

struct ParticleInfo
{
	uint m_max_num;
	uint m_emit_max_num;
};
struct ParticleData
{
	vec4 m_pos;	//!< xyz:pos w:scale
	vec4 m_vel;	//!< xyz:dir w:not use
	ivec4 m_map_index;
	uint m_type;
	uint m_flag;
	float m_life;
	uint _p;
};

#ifdef SETPOINT_PARTICLE
layout(std140, set=SETPOINT_PARTICLE, binding=0) uniform ParticleInfoUniform {
	ParticleInfo u_particle_info;
};

layout(std430, set=SETPOINT_PARTICLE, binding=1) restrict buffer ParticleDataBuffer {
	ParticleData b_particle[];
};
layout(std430, set=SETPOINT_PARTICLE, binding=2) restrict buffer CounterBuffer {
	DrawIndirectCommand b_draw_cmd;
};
layout(std430, set=SETPOINT_PARTICLE, binding=3) restrict buffer ParticleDataBuffer {
	ParticleData b_particle_emit[];
};
#endif