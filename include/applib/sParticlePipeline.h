#pragma once

#include <array>
#include <memory>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cCamera.h>
#include <btrlib/Context.h>
#include <btrlib/Singleton.h>

#include <applib/App.h>

struct ParticleInfo
{
	uint32_t m_particle_max_num;
	uint32_t m_emitter_max_num;
	uint32_t m_generate_cmd_max_num;
};

struct ParticleData
{
	vec4 m_position;		//!< 位置
	vec4 m_velocity;		//!< 回転

	vec4 m_rotate;
	vec4 m_scale;
	vec4 m_color;		//!< 色

	uint32_t m_type;		//!< パーティクルのタイプ
	uint32_t m_flag;
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
	uint32_t m_generate_num;
	uint32_t m_type;
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

	uint32_t m_type;		//!< Emitterのタイプ
	uint32_t m_flag;
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

struct sParticlePipeline : Singleton<sParticlePipeline>
{
	friend Singleton<sParticlePipeline>;
	enum : uint32_t {
		PIPELINE_UPDATE,
		PIPELINE_EMIT,
		PIPELINE_GENERATE_DEBUG,
		PIPELINE_GENERATE,
		PIPELINE_DRAW,
		PIPELINE_NUM,
	};
	enum : uint32_t {
		SHADER_UPDATE,
//		SHADER_EMIT,
		SHADER_GENERATE_TRANSFAR_DEBUG,
		SHADER_GENERATE,
		SHADER_DRAW_VERTEX,
		SHADER_DRAW_FRAGMENT,
		SHADER_NUM,
	};

	enum PipelineLayout : uint32_t
	{
		PIPELINE_LAYOUT_UPDATE,
		PIPELINE_LAYOUT_DRAW,
		PIPELINE_LAYOUT_NUM,
	};

	enum DescriptorSetLayout : uint32_t
	{
		DESCRIPTOR_SET_LAYOUT_PARTICLE,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet : uint32_t
	{
		DESCRIPTOR_SET_PARTICLE,
		DESCRIPTOR_SET_NUM,
	};

	btr::BufferMemoryEx<ParticleInfo> m_particle_info;
	btr::BufferMemoryEx<ParticleData> m_particle;
	btr::BufferMemoryEx<ParticleUpdateParameter> m_particle_update_param;
	btr::BufferMemoryEx<vk::DrawIndirectCommand> m_particle_counter;
	btr::BufferMemoryEx<ParticleGenerateCommand> m_particle_generate_cmd;
	btr::BufferMemoryEx<uvec3> m_particle_generate_cmd_counter;
	btr::BufferMemoryEx<EmitterData> m_particle_emitter;
	btr::BufferMemoryEx<uvec3> m_particle_emitter_counter;

 	vk::UniqueRenderPass m_render_pass;
 	vk::UniqueFramebuffer m_framebuffer;

	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;

	ParticleInfo m_particle_info_cpu;

	void setup(std::shared_ptr<btr::Context>& context);
	vk::CommandBuffer execute(std::shared_ptr<btr::Context>& context);
	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context);

	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_descriptor_set_layout[desctiptor].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_descriptor_set[i].get(); }

};

