#pragma once

#include <array>
#include <memory>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cCamera.h>
#include <btrlib/Loader.h>
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
	//	vec4 m_offset;		//!< 位置のオフセット エミットした位置
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
	struct Private 
	{
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
			SHADER_EMIT,
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

		btr::BufferMemory m_particle_info;
		btr::BufferMemory m_particle;
		btr::BufferMemory m_particle_update_param;
		btr::BufferMemory m_particle_counter;
		btr::BufferMemory m_particle_generate_cmd;
		btr::BufferMemory m_particle_generate_cmd_counter;
		btr::BufferMemory m_particle_emitter;
		btr::BufferMemory m_particle_emitter_counter;

		vk::UniqueRenderPass m_render_pass;
		std::vector<vk::UniqueFramebuffer> m_framebuffer;

		std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
		std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
		std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

		std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
		std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
		std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;

		ParticleInfo m_particle_info_cpu;



		void setup(std::shared_ptr<btr::Loader>& loader);
		vk::CommandBuffer execute(std::shared_ptr<btr::Executer>& executer);
		vk::CommandBuffer draw(std::shared_ptr<btr::Executer>& executer);

	};
	std::unique_ptr<Private> m_private;

	void setup(std::shared_ptr<btr::Loader>& loader)
	{
		auto p = std::make_unique<Private>();
		p->setup(loader);

		m_private = std::move(p);
	}

	vk::CommandBuffer execute(std::shared_ptr<btr::Executer>& executer)
	{
		return m_private->execute(executer);
	}

	vk::CommandBuffer draw(std::shared_ptr<btr::Executer>& executer)
	{
		return m_private->draw(executer);
	}
};

