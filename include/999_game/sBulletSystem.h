#pragma once

#include <vector>
#include <array>
#include <memory>
#include <btrlib/Singleton.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cCamera.h>
#include <btrlib/Context.h>

#include <applib/App.h>
#include <applib/sSystem.h>
#include <applib/Utility.h>
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

	BulletData()
	{
		m_ll_next = -1;
	}
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
		PIPELINE_LAYOUT_DRAW,
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

	btr::BufferMemory m_bullet;
	btr::BufferMemory m_bullet_info;
	std::array<AppendBuffer<BulletData, 1024>, 2> m_data;
	btr::BufferMemory m_bullet_counter;
	btr::BufferMemory m_bullet_emit;
	btr::BufferMemory m_bullet_emit_count;
	btr::BufferMemory m_bullet_draw_indiret_info;
	btr::BufferMemory m_bullet_LL_head_gpu;

	std::shared_ptr<RenderPassModule> m_render_pass;

	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;

	BulletInfo m_bullet_info_cpu;
	void setup(std::shared_ptr<btr::Context>& context);

	vk::CommandBuffer execute(std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

		{
			// transfer
			auto to_transfer = m_bullet_counter.makeMemoryBarrierEx();
			to_transfer.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

			cmd.fillBuffer(m_bullet_LL_head_gpu.getBufferInfo().buffer, m_bullet_LL_head_gpu.getBufferInfo().offset, m_bullet_LL_head_gpu.getBufferInfo().range, 0xFFFFFFFF);
			cmd.updateBuffer<vk::DrawIndirectCommand>(m_bullet_counter.getBufferInfo().buffer, m_bullet_counter.getBufferInfo().offset, vk::DrawIndirectCommand(4, 0, 0, 0));
		}
		{
			auto to_read2 = m_bullet_counter.makeMemoryBarrierEx();
			to_read2.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read2.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_read2 }, {});

			// update
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_UPDATE].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_UPDATE].get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 2, sSystem::Order().getSystemDescriptor().getSet(), {});

			auto groups = app::calcDipatchGroups(glm::uvec3(m_bullet_info_cpu.m_max_num, 1, 1), glm::uvec3(1024, 1, 1));
			cmd.dispatch(groups.x, groups.y, groups.z);

		}
		// エミット
		{
			auto data = m_data[sGlobal::Order().getGPUIndex()].get();
			if (!data.empty())
			{
				auto to_emit = m_bullet_counter.makeMemoryBarrierEx();
				to_emit.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				to_emit.setDstAccessMask(vk::AccessFlagBits::eShaderRead| vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_emit, {});

				btr::AllocatedMemory::Descriptor emit_desc;
				emit_desc.size = vector_sizeof(data);
				emit_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(emit_desc);
				memcpy(staging.getMappedPtr(), data.data(), emit_desc.size);

				cmd.updateBuffer<uint32_t>(m_bullet_emit_count.getBufferInfo().buffer, m_bullet_emit_count.getBufferInfo().offset, uint32_t(data.size()));

				vk::BufferCopy copy;
				copy.setSize(emit_desc.size);
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(m_bullet_emit.getBufferInfo().offset);
				cmd.copyBuffer(staging.getBufferInfo().buffer, m_bullet_emit.getBufferInfo().buffer, copy);

				auto to_read = m_bullet_emit.makeMemoryBarrierEx();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_EMIT].get());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_UPDATE].get(), {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 2, sSystem::Order().getSystemDescriptor().getSet(), {});

				auto groups = app::calcDipatchGroups(glm::uvec3(data.size(), 1, 1), glm::uvec3(1024, 1, 1));
				cmd.dispatch(groups.x, groups.y, groups.z);
			}
		}

		cmd.end();
		return cmd;
	}

	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

		auto to_draw = m_bullet_counter.makeMemoryBarrierEx();
		to_draw.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
		to_draw.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { to_draw }, {});

		to_draw = m_bullet.makeMemoryBarrierEx();
		to_draw.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
		to_draw.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader, {}, {}, { to_draw }, {});

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_render_pass->getRenderPass());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), context->m_window->getClientSize<vk::Extent2D>()));
		begin_render_Info.setFramebuffer(m_render_pass->getFramebuffer(context->getGPUFrame()));
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_GRAPHICS_RENDER].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 0, m_descriptor_set[DESCRIPTOR_SET_UPDATE].get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 2, sSystem::Order().getSystemDescriptor().getSet(), {});

		cmd.drawIndirect(m_bullet_counter.getBufferInfo().buffer, m_bullet_counter.getBufferInfo().offset, 1, sizeof(vk::DrawIndirectCommand));

		cmd.endRenderPass();
		cmd.end();
		return cmd;
	}

public:

	void shoot(const std::vector<BulletData>& param)
	{
		auto& data = m_data[sGlobal::Order().getCPUIndex()];
		data.push(param.data(), param.size());
	}

	vk::PipelineLayout getPipelineLayout(PipelineLayout layout)const { return m_pipeline_layout[layout].get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_descriptor_set_layout[desctiptor].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_descriptor_set[i].get(); }
	btr::BufferMemory& getLL() { return m_bullet_LL_head_gpu; }
	btr::BufferMemory& getBullet() { return m_bullet; }

};

