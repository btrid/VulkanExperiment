
#ifndef APPLIB_PARTICLE_GLSL
#define APPLIB_PARTICLE_GLSL

#include <btrlib/Common.glsl>

struct ParticleInfo
{
	uint m_particle_max_num;
	uint m_emitter_max_num;
	uint m_generate_cmd_max_num;
};

struct ParticleData
{
	//	vec4 m_offset;		//!< 位置のオフセット エミットした位置
	vec4 m_position;		//!< 位置
	vec4 m_velocity;		//!< 回転

	vec4 m_rotate;
	vec4 m_scale;
	vec4 m_color;		//!< 色

	uint m_type;		//!< パーティクルのタイプ
	uint m_flag;
	float m_age;		//!< 年齢
	float m_life;		//!< 寿命
};
struct ParticleUpdateParameter
{
	vec4 m_velocity_add;
	vec4 m_velocity_mul;
	vec4 m_rotate_add;
	vec4 m_rotate_mul;
	vec4 m_scale_add;
	vec4 m_scale_mul;
};

struct EmitParam
{
	vec4 m_value;
	vec4 m_value_rand; // -rand~rand
};

struct ParticleGenerateCommand
{
	uint m_generate_num;
	uint m_type;
	vec2 m_life;
	EmitParam m_position;
	EmitParam m_direction;		//!< xyz:方向 w:移動量
	EmitParam m_rotate;
	EmitParam m_scale;
	EmitParam m_color;

};

struct EmitterData
{
	vec4 m_pos;

	uint m_type;		//!< Emitterのタイプ
	uint m_flag;
	float m_age;		//!< 年齢
	float m_life;		//!< 寿命

	float m_particle_emit_cycle;
	float m_emit_num;
	float m_emit_num_rand;
	float _p;
};
struct EmitterUpdateParameter
{
	vec4 m_pos;
	vec4 m_emit_offset;		//!< パーティクル生成オフセットのランダム値
};

#ifdef USE_PARTICLE
layout(std140, set=USE_PARTICLE, binding=0) uniform ParticleInfoUniform {
	ParticleInfo u_particle_info;
};

layout(std430, set=USE_PARTICLE, binding=1) restrict buffer ParticleDataBuffer {
	ParticleData b_particle[];
};
layout(std430, set=USE_PARTICLE, binding=2) restrict buffer ParticleDrawCmdBuffer {
	DrawIndirectCommand b_particle_draw_cmd;
};
layout(std430, set=USE_PARTICLE, binding=3) restrict buffer EmitterDataBuffer {
	EmitterData b_emitter[];
};
layout(std430, set=USE_PARTICLE, binding=4) restrict buffer ParticleEmitterBuffer {
	uvec3 b_emitter_num;
};
layout(std430, set=USE_PARTICLE, binding=5) restrict buffer ParticleGenerateCommandBuffer {
	ParticleGenerateCommand b_particle_generate_cmd[];
};
layout(std430, set=USE_PARTICLE, binding=6) restrict buffer ParticleGenerateDataBuffer {
	uvec3 b_generate_cmd_num;
};
layout(std430, set=USE_PARTICLE, binding=7) restrict buffer ParticleUpdateParameterBuffer {
	ParticleUpdateParameter b_particle_update_param[];
};
#endif

#endif