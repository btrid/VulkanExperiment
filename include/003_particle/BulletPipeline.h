#pragma once

#include <vector>
#include <array>
#include <memory>
#include <btrlib/Singleton.h>
#include <btrlib/BufferMemory.h>
#include <btrlib/cCamera.h>

#include <applib/App.h>
#include <btrlib/Loader.h>
#include <003_particle/GameDefine.h>
#include <003_particle/MazeGenerator.h>
#include <003_particle/Geometry.h>
#include <003_particle/CircleIndex.h>


struct BulletData
{
	glm::vec4 m_pos;	//!< xyz:pos w:scale
	glm::vec4 m_vel;	//!< xyz:dir
	glm::ivec4 m_map_index;

	uint32_t m_type;
	uint32_t m_flag;
	float m_life;
	float m_atk;
	float m_power;
	float m_1;
	float m_2;
	float m_3;
};



struct sBulletSystem : public Singleton<sBulletSystem>
{
	friend sBulletSystem;
	struct Private 
	{
		enum : uint32_t {
			COMPUTE_UPDATE,
			COMPUTE_NUM,
		};
		enum : uint32_t {
			GRAPHICS_SHADER_VERTEX_PARTICLE,
			GRAPHICS_SHADER_FRAGMENT_PARTICLE,
			GRAPHICS_SHADER_NUM,
		};

		enum : uint32_t
		{
			PIPELINE_LAYOUT_UPDATE,
			PIPELINE_LAYOUT_PARTICLE_DRAW,
			PIPELINE_LAYOUT_NUM,
		};
		enum : uint32_t
		{
			DESCRIPTOR_UPDATE,
			DESCRIPTOR_NUM,
		};

		btr::AllocatedMemory m_particle;
		btr::AllocatedMemory m_particle_info;
		std::array<std::vector<BulletData>, 2> m_append_buffer;
		btr::AllocatedMemory m_particle_counter;

		btr::AllocatedMemory m_particle_draw_indiret_info;
		std::array<vk::DescriptorSetLayout, DESCRIPTOR_NUM> m_descriptor_set_layout;
		std::array<vk::DescriptorSet, DESCRIPTOR_NUM> m_descriptor_set;
		std::array<vk::PipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

		std::vector<vk::Pipeline> m_compute_pipeline;
		std::array<vk::PipelineShaderStageCreateInfo, COMPUTE_NUM> m_compute_shader_info;

		std::vector<vk::Pipeline> m_graphics_pipeline;
		std::array<vk::PipelineShaderStageCreateInfo, GRAPHICS_SHADER_NUM> m_graphics_shader_info;

		ParticleInfo m_particle_info_cpu;
		void setup(btr::Loader& loader);

		void execute(btr::Executer& executer)
		{
			vk::CommandBuffer cmd = executer.m_cmd;


			// エミット
			{
				auto& emit_data = m_append_buffer[sGlobal::Order().getGPUIndex()];
				if (!emit_data.empty())
				{
					auto particle_barrier = m_particle.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite);
					std::vector<vk::BufferMemoryBarrier> to_emit_barrier =
					{
						particle_barrier,
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_emit_barrier, {});

					btr::BufferMemory::Descriptor desc;
					desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
					desc.size = vector_sizeof(emit_data);
					auto emit_staging_memory = executer.m_staging_memory.allocateMemory(desc);

					// emit
					vk::BufferCopy copy_info;
					copy_info.setSize(desc.size);
					copy_info.setSrcOffset(emit_staging_memory.getBufferInfo().offset);
					copy_info.setDstOffset(m_particle.getBufferInfo().offset + sizeof(BulletData)*sGlobal::Order().getGPUIndex());
					cmd.copyBuffer(emit_staging_memory.getBufferInfo().buffer, m_particle.getBufferInfo().buffer, copy_info);

				}

				{
					// パーティクル数更新
					std::vector<vk::BufferMemoryBarrier> to_transfer = {
						m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

					cmd.updateBuffer<vk::DrawIndirectCommand>(m_particle_counter.getBufferInfo().buffer, m_particle_counter.getBufferInfo().offset, vk::DrawIndirectCommand(emit_data.size(), 1, 0, 0));

					std::vector<vk::BufferMemoryBarrier> to_update_barrier = {
						m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_update_barrier }, {});
				}
				emit_data.clear();
			}
			uint src_offset = sGlobal::Order().getCPUIndex() == 0 ? (m_particle.getBufferInfo().range / sizeof(BulletData) / 2) : 0;
			uint dst_offset = sGlobal::Order().getCPUIndex() == 1 ? (m_particle.getBufferInfo().range / sizeof(BulletData) / 2) : 0;
			{
				// update
				struct UpdateConstantBlock
				{
					float m_deltatime;
					uint m_src_offset;
					uint m_dst_offset;
				};
				UpdateConstantBlock block;
				block.m_deltatime = sGlobal::Order().getDeltaTime();
				block.m_src_offset = src_offset;
				block.m_dst_offset = dst_offset;
				cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[COMPUTE_UPDATE], vk::ShaderStageFlagBits::eCompute, 0, block);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline[COMPUTE_UPDATE]);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE], 0, m_descriptor_set[DESCRIPTOR_UPDATE], {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE], 1, Scene::Order().m_descriptor_set[Scene::DESCRIPTOR_LAYOUT_MAP], {});
				auto groups = app::calcDipatchGroups(glm::uvec3(8192 / 2, 1, 1), glm::uvec3(1024, 1, 1));
				cmd.dispatch(groups.x, groups.y, groups.z);

			}

			auto particle_barrier = m_particle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader, {}, {}, particle_barrier, {});

			auto to_draw = m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eIndirectCommandRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { to_draw }, {});

		}

		void draw(vk::CommandBuffer cmd)
		{
// 			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline[0]);
// 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_PARTICLE_DRAW], 0, m_descriptor_set[DESCRIPTOR_UPDATE], {});
// 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_PARTICLE_DRAW], 1, m_descriptor_set[DESCRIPTOR_DRAW_CAMERA], {});
// 			cmd.drawIndirect(m_particle_counter.getBufferInfo().buffer, m_particle_counter.getBufferInfo().offset, 1, sizeof(vk::DrawIndirectCommand));
		}

	};
	std::unique_ptr<Private> m_private;

	void setup(btr::Loader& loader)
	{
		auto p = std::make_unique<Private>();
		p->setup(loader);

		m_private = std::move(p);
	}

	void execute(btr::Executer& executer)
	{
		m_private->execute(executer);
	}

	void draw(vk::CommandBuffer cmd)
	{
		m_private->draw(cmd);
	}

	void shoot(const std::vector<BulletData>& param)
	{
		auto& data = m_private->m_append_buffer[sGlobal::Order().getCPUIndex()];
		data.insert(data.end(), param.begin(), param.end());
	}
};

