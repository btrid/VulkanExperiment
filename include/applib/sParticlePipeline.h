#pragma once

#include <array>
#include <memory>
#include <btrlib/BufferMemory.h>
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
			PIPELINE_CMD_TRANSFAR,
			PIPELINE_GENERATE,
			PIPELINE_DRAW,
			PIPELINE_NUM,
		};
		enum : uint32_t {
			SHADER_UPDATE,
			SHADER_EMIT,
			SHADER_GENERATE_TRANSFAR_CPU,
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

		btr::AllocatedMemory m_particle_info;
		btr::AllocatedMemory m_particle;
		btr::AllocatedMemory m_particle_update_param;
		btr::AllocatedMemory m_particle_counter;
		btr::AllocatedMemory m_particle_generate_cmd;
		btr::AllocatedMemory m_generate_cmd_counter;
		btr::AllocatedMemory m_particle_emitter;
		btr::AllocatedMemory m_particle_emitter_counter;

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

		vk::CommandBuffer execute(std::shared_ptr<btr::Executer>& executer)
		{
			auto cmd = executer->m_cmd_pool->allocCmdOnetime(0);

			struct UpdateConstantBlock
			{
				float m_deltatime;
				uint m_double_buffer_dst_index;
			};
			UpdateConstantBlock constant;
			constant.m_deltatime = sGlobal::Order().getDeltaTime();
			constant.m_double_buffer_dst_index = sGlobal::Order().getCPUIndex();


			// debug
			static int a;
			a++;
			if (a == 100)
			{
				a = 0;
				vk::BufferMemoryBarrier to_read = m_generate_cmd_counter.makeMemoryBarrierEx();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_CMD_TRANSFAR].get());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_PARTICLE].get(), {});
				cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), vk::ShaderStageFlagBits::eCompute, 0, constant);
				cmd.dispatch(1, 1, 1);
			}

			// generate particle
			{
				vk::BufferMemoryBarrier to_read = m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				to_read.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_GENERATE].get());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_PARTICLE].get(), {});
				cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), vk::ShaderStageFlagBits::eCompute, 0, constant);
				cmd.dispatchIndirect(m_generate_cmd_counter.getBufferInfo().buffer, m_generate_cmd_counter.getBufferInfo().offset);
			}
			{
				// パーティクル数初期化
				vk::BufferMemoryBarrier to_transfer = m_particle_counter.makeMemoryBarrierEx();
				to_transfer.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

				cmd.updateBuffer<vk::DrawIndirectCommand>(m_particle_counter.getBufferInfo().buffer, m_particle_counter.getBufferInfo().offset, vk::DrawIndirectCommand(4, 0, 0, 0));
				cmd.updateBuffer<glm::uvec3>(m_generate_cmd_counter.getBufferInfo().buffer, m_generate_cmd_counter.getBufferInfo().offset, glm::uvec3(0, 1, 1));

			}

			// update
			{
				vk::BufferMemoryBarrier to_read = m_particle_counter.makeMemoryBarrierEx();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_UPDATE].get());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_PARTICLE].get(), {});
				constant.m_double_buffer_dst_index = sGlobal::Order().getGPUIndex();
				cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), vk::ShaderStageFlagBits::eCompute, 0, constant);
				auto groups = app::calcDipatchGroups(glm::uvec3(m_particle_info_cpu.m_particle_max_num, 1, 1), glm::uvec3(1024, 1, 1));
				cmd.dispatch(groups.x, groups.y, groups.z);
			}

			cmd.end();
			return cmd;
		}

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

