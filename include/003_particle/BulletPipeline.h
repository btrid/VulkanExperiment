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


struct BulletInfo
{
	uint32_t m_max_num;
};

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
			COMPUTE_EMIT,
			COMPUTE_NUM,
		};
		enum : uint32_t {
			GRAPHICS_RENDER,
			GRAPHICS_NUM,
		};
		enum : uint32_t {
			GRAPHICS_SHADER_VERTEX_PARTICLE,
//			GRAPHICS_SHADER_GEOMETRY,
			GRAPHICS_SHADER_FRAGMENT_PARTICLE,
			GRAPHICS_SHADER_NUM,
		};

		enum : uint32_t
		{
			PIPELINE_LAYOUT_UPDATE,
			PIPELINE_LAYOUT_BULLET_DRAW,
			PIPELINE_LAYOUT_NUM,
		};
		enum : uint32_t
		{
			DESCRIPTOR_UPDATE,
			DESCRIPTOR_NUM,
		};

		btr::AllocatedMemory m_bullet;
		btr::AllocatedMemory m_bullet_info;
		std::array<std::vector<BulletData>, 2> m_append_buffer;
		btr::AllocatedMemory m_bullet_counter;
		btr::AllocatedMemory m_emit;

		btr::AllocatedMemory m_bullet_draw_indiret_info;
		std::array<vk::DescriptorSetLayout, DESCRIPTOR_NUM> m_descriptor_set_layout;
		std::array<vk::DescriptorSet, DESCRIPTOR_NUM> m_descriptor_set;
		std::array<vk::PipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

		std::vector<vk::Pipeline> m_compute_pipeline;
		std::array<vk::PipelineShaderStageCreateInfo, COMPUTE_NUM> m_compute_shader_info;

		std::vector<vk::Pipeline> m_graphics_pipeline;
		std::array<vk::PipelineShaderStageCreateInfo, GRAPHICS_SHADER_NUM> m_graphics_shader_info;

		BulletInfo m_bullet_info_cpu;
		void setup(btr::Loader& loader);

		void execute(std::shared_ptr<btr::Executer>& executer)
		{
			vk::CommandBuffer cmd = executer->m_cmd;

			uint src_offset = sGlobal::Order().getCPUIndex() == 0 ? (m_bullet.getBufferInfo().range / sizeof(BulletData) / 2) : 0;
			uint dst_offset = sGlobal::Order().getCPUIndex() == 1 ? (m_bullet.getBufferInfo().range / sizeof(BulletData) / 2) : 0;
			auto& data = m_append_buffer[sGlobal::Order().getGPUIndex()];

			{
				// transfer
				std::vector<vk::BufferMemoryBarrier> to_transfer = {
					m_bullet_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

				cmd.updateBuffer<vk::DrawIndirectCommand>(m_bullet_counter.getBufferInfo().buffer, m_bullet_counter.getBufferInfo().offset, vk::DrawIndirectCommand(0, 1, 0, 0));

				std::vector<vk::BufferMemoryBarrier> to_update_barrier = {
					m_bullet_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_update_barrier }, {});
			}
			{
				// update
				struct UpdateConstantBlock
				{
					float m_deltatime;
					uint m_src_offset;
					uint m_dst_offset;
					uint m_emit_num;
				};
				UpdateConstantBlock block;
				block.m_deltatime = sGlobal::Order().getDeltaTime();
				block.m_src_offset = src_offset;
				block.m_dst_offset = dst_offset;
				block.m_emit_num = data.size();
				cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE], vk::ShaderStageFlagBits::eCompute, 0, block);
				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline[PIPELINE_LAYOUT_UPDATE]);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE], 0, m_descriptor_set[DESCRIPTOR_UPDATE], {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE], 1, sScene::Order().m_descriptor_set[sScene::DESCRIPTOR_LAYOUT_MAP], {});
				auto groups = app::calcDipatchGroups(glm::uvec3(8192 / 2, 1, 1), glm::uvec3(1024, 1, 1));
				cmd.dispatch(groups.x, groups.y, groups.z);

			}
			// エミット
			{
				if (!data.empty())
				{
					std::vector<vk::BufferMemoryBarrier> to_emit_barrier =
					{
						m_bullet.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite),
						m_bullet_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_emit_barrier, {});


					// emit

					auto to_transfer = m_emit.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite);
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

					btr::BufferMemory::Descriptor emit_desc;
					emit_desc.size = vector_sizeof(data);
					emit_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
					btr::AllocatedMemory bullet_emit = executer->m_staging_memory.allocateMemory(emit_desc);
					memcpy(bullet_emit.getMappedPtr(), data.data(), emit_desc.size);


					vk::BufferCopy copy;
					copy.dstOffset = m_emit.getBufferInfo().offset;
					copy.setSrcOffset(bullet_emit.getBufferInfo().offset);
					copy.setSize(emit_desc.size);
					cmd.copyBuffer(bullet_emit.getBufferInfo().buffer, m_emit.getBufferInfo().buffer, copy);

					auto to_read = m_emit.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

					cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline[COMPUTE_EMIT]);
					auto groups = app::calcDipatchGroups(glm::uvec3(data.size(), 1, 1), glm::uvec3(1024, 1, 1));
					cmd.dispatch(groups.x, groups.y, groups.z);

					data.clear();
				}
			}


			auto particle_barrier = m_bullet.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader, {}, {}, particle_barrier, {});

			auto to_draw = m_bullet_counter.makeMemoryBarrier(vk::AccessFlagBits::eIndirectCommandRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { to_draw }, {});

		}

		void draw(vk::CommandBuffer cmd)
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline[GRAPHICS_RENDER]);
			uint dst_offset = sGlobal::Order().getCPUIndex() == 1 ? (m_bullet.getBufferInfo().range / sizeof(BulletData) / 2) : 0;
			cmd.pushConstants<uint>(m_pipeline_layout[PIPELINE_LAYOUT_BULLET_DRAW], vk::ShaderStageFlagBits::eVertex, 0, dst_offset);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_BULLET_DRAW], 0, m_descriptor_set[DESCRIPTOR_UPDATE], {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_BULLET_DRAW], 1, sScene::Order().m_descriptor_set[sScene::DESCRIPTOR_LAYOUT_CAMERA], {});
			cmd.drawIndirect(m_bullet_counter.getBufferInfo().buffer, m_bullet_counter.getBufferInfo().offset, 1, sizeof(vk::DrawIndirectCommand));
		}

	};
	std::unique_ptr<Private> m_private;

	void setup(btr::Loader& loader)
	{
		auto p = std::make_unique<Private>();
		p->setup(loader);

		m_private = std::move(p);
	}

	void execute(std::shared_ptr<btr::Executer>& executer)
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

