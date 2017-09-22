#pragma once

#include <vector>
#include <array>
#include <memory>
#include <btrlib/Singleton.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cCamera.h>
#include <btrlib/Loader.h>

#include <applib/App.h>
#include <999_game/MazeGenerator.h>
#include <999_game/sScene.h>


struct BulletInfo
{
	uint32_t m_max_num;
};

struct BulletData
{
	glm::vec4 m_pos;	//!< xyz:pos w:scale
	glm::vec4 m_vel;	//!< xyz:dir

	glm::ivec2 m_map_index;
	uint32_t m_ll_next;
	float m_power;

	uint32_t m_type;
	uint32_t m_flag;
	float m_life;
	float m_atk;

};


class sBulletSystem : public Singleton<sBulletSystem>
{
	friend Singleton<sBulletSystem>;
public:
	enum : uint32_t {
		PIPELINE_COMPUTE_UPDATE,
		PIPELINE_COMPUTE_EMIT,
		PIPELINE_GRAPHICS_RENDER,
		PIPELINE_NUM
	};
	enum : uint32_t {
		SHADER_COMPUTE_UPDATE,
		SHADER_COMPUTE_EMIT,
		SHADER_VERTEX_PARTICLE,
		SHADER_FRAGMENT_PARTICLE,
		SHADER_NUM,
	};

	enum PipelineLayout : uint32_t
	{
		PIPELINE_LAYOUT_UPDATE,
		PIPELINE_LAYOUT_BULLET_DRAW,
		PIPELINE_LAYOUT_NUM,
	};
	enum DescriptorSetLayout : uint32_t
	{
		DESCRIPTOR_SET_LAYOUT_UPDATE,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet : uint32_t
	{
		DESCRIPTOR_SET_UPDATE,
		DESCRIPTOR_SET_NUM,
	};

	struct Private 
	{

		btr::BufferMemory m_bullet;
		btr::BufferMemory m_bullet_info;
		std::array<std::vector<BulletData>, 2> m_append_buffer;
		btr::BufferMemory m_bullet_counter;
		btr::BufferMemory m_emit;
		btr::BufferMemory m_bullet_draw_indiret_info;
		btr::BufferMemory m_bullet_LL_head_gpu;

		vk::UniqueRenderPass m_render_pass;
		std::vector<vk::UniqueFramebuffer> m_framebuffer;

		std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
		std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
		std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

		std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
		std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
		std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;

		BulletInfo m_bullet_info_cpu;
		void setup(std::shared_ptr<btr::Loader>& loader);

		vk::CommandBuffer execute(std::shared_ptr<btr::Executer>& executer)
		{
			auto cmd = executer->m_cmd_pool->allocCmdOnetime(0);
			uint src_offset = sGlobal::Order().getCPUIndex() == 1 ? (m_bullet.getBufferInfo().range / sizeof(BulletData) / 2) : 0;
			uint dst_offset = sGlobal::Order().getCPUIndex() == 0 ? (m_bullet.getBufferInfo().range / sizeof(BulletData) / 2) : 0;
			auto& data = m_append_buffer[sGlobal::Order().getGPUIndex()];

			{
				// transfer
				std::vector<vk::BufferMemoryBarrier> to_transfer = {
					m_bullet_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
					m_bullet_LL_head_gpu.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

				cmd.updateBuffer<vk::DrawIndirectCommand>(m_bullet_counter.getBufferInfo().buffer, m_bullet_counter.getBufferInfo().offset, vk::DrawIndirectCommand(4, 0, 0, 0));
				cmd.fillBuffer(m_bullet_LL_head_gpu.getBufferInfo().buffer, m_bullet_LL_head_gpu.getBufferInfo().offset, m_bullet_LL_head_gpu.getBufferInfo().range, 0xFFFFFFFF);

				std::vector<vk::BufferMemoryBarrier> to_update_barrier = {
					m_bullet_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
					m_bullet_LL_head_gpu.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead| vk::AccessFlagBits::eShaderWrite),
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
				cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), vk::ShaderStageFlagBits::eCompute, 0, block);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_UPDATE].get(), {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_UPDATE].get());
				auto groups = app::calcDipatchGroups(glm::uvec3(m_bullet_info_cpu.m_max_num / 2, 1, 1), glm::uvec3(1024, 1, 1));
				cmd.dispatch(groups.x, groups.y, groups.z);

			}
			// エミット
			{
				if (!data.empty())
				{
					std::vector<vk::BufferMemoryBarrier> to_emit_barrier =
					{
						m_bullet.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite),
						m_bullet_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_emit_barrier, {});

					// emit
					auto to_transfer = m_emit.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite);
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

					btr::AllocatedMemory::Descriptor emit_desc;
					emit_desc.size = vector_sizeof(data);
					emit_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
					btr::BufferMemory bullet_emit = executer->m_staging_memory.allocateMemory(emit_desc);
					memcpy(bullet_emit.getMappedPtr(), data.data(), emit_desc.size);

					vk::BufferCopy copy;
					copy.dstOffset = m_emit.getBufferInfo().offset;
					copy.setSrcOffset(bullet_emit.getBufferInfo().offset);
					copy.setSize(emit_desc.size);
					cmd.copyBuffer(bullet_emit.getBufferInfo().buffer, m_emit.getBufferInfo().buffer, copy);

					auto to_read = m_emit.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

					cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_EMIT].get());
					auto groups = app::calcDipatchGroups(glm::uvec3(data.size(), 1, 1), glm::uvec3(1024, 1, 1));
					cmd.dispatch(groups.x, groups.y, groups.z);

					data.clear();
				}
			}


			auto particle_barrier = m_bullet.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, particle_barrier, {});

			auto to_draw = m_bullet_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_draw }, {});

			cmd.end();
			return cmd;
		}

		vk::CommandBuffer draw(std::shared_ptr<btr::Executer>& executer)
		{
			auto cmd = executer->m_cmd_pool->allocCmdOnetime(0);
			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_render_pass.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), executer->m_window->getClientSize<vk::Extent2D>()));
			begin_render_Info.setFramebuffer(m_framebuffer[executer->getGPUFrame()].get());
			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			auto dst_offset = sGlobal::Order().getGPUIndex() == 1 ? (m_bullet.getBufferInfo().range / sizeof(BulletData) / 2) : 0;
			cmd.pushConstants<uint>(m_pipeline_layout[PIPELINE_LAYOUT_BULLET_DRAW].get(), vk::ShaderStageFlagBits::eVertex, 0, dst_offset);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_BULLET_DRAW].get(), 0, m_descriptor_set[DESCRIPTOR_SET_UPDATE].get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_BULLET_DRAW].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_GRAPHICS_RENDER].get());
			cmd.drawIndirect(m_bullet_counter.getBufferInfo().buffer, m_bullet_counter.getBufferInfo().offset, 1, sizeof(vk::DrawIndirectCommand));

			cmd.endRenderPass();
			cmd.end();
			return cmd;
		}

	};
private:
	std::unique_ptr<Private> m_private;

public:
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

	void shoot(const std::vector<BulletData>& param)
	{
		auto& data = m_private->m_append_buffer[sGlobal::Order().getCPUIndex()];
		data.insert(data.end(), param.begin(), param.end());
	}

	vk::PipelineLayout getPipelineLayout(PipelineLayout layout)const { return m_private->m_pipeline_layout[layout].get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_private->m_descriptor_set_layout[desctiptor].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_private->m_descriptor_set[i].get(); }
	btr::BufferMemory& getLL() { return m_private->m_bullet_LL_head_gpu; }
	btr::BufferMemory& getBullet() { return m_private->m_bullet; }

};

