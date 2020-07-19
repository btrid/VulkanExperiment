#pragma once
#include <013_2DGI/Crowd/CrowdContext.h>
#include <applib/sSystem.h>
#include <013_2DGI/GI2D/GI2DContext.h>


struct Crowd_Procedure
{
	enum Shader
	{
		Shader_UnitUpdate,
		Shader_MakeLinkList,
		Shader_MakeDensity,
		Shader_DrawDensity,

		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Crowd,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_UnitUpdate,
		Pipeline_MakeLinkList,
		Pipeline_MakeDensity,
		Pipeline_DrawDensity,

		Pipeline_Num,
	};
	Crowd_Procedure(const std::shared_ptr<CrowdContext>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;

		auto cmd = context->m_context->m_cmd_pool->allocCmdTempolary(0);

		{
			const char* name[] =
			{
				"Crowd_UnitUpdate.comp.spv",
				"Crowd_MakeLinkList.comp.spv",
				"Crowd_MakeDensityMap.comp.spv",
				"Crowd_DrawDensityMap.comp.spv",

			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader_module[i] = loadShaderUnique(context->m_context->m_device, path + name[i]);
			}
		}

		// pipeline layout
		{

			{
				vk::DescriptorSetLayout layouts[] = {
					context->getDescriptorSetLayout(),
					sSystem::Order().getSystemDescriptorLayout(),
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(std::size(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_Crowd] = context->m_context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

			}
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 4> shader_info;
			shader_info[0].setModule(m_shader_module[Shader_UnitUpdate].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader_module[Shader_MakeLinkList].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader_module[Shader_MakeDensity].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			shader_info[3].setModule(m_shader_module[Shader_DrawDensity].get());
			shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[3].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[3])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
			};
			auto compute_pipeline = context->m_context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
			m_pipeline[Pipeline_UnitUpdate] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_MakeLinkList] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_MakeDensity] = std::move(compute_pipeline[2]);
			m_pipeline[Pipeline_DrawDensity] = std::move(compute_pipeline[3]);
		}
	}

	void executeUpdateUnit(vk::CommandBuffer cmd)
	{
		// カウンターのclear
		{
			cmd.updateBuffer<uvec4>(m_context->b_unit_counter.getInfo().buffer, m_context->b_unit_counter.getInfo().offset, uvec4(0));
			vk::BufferMemoryBarrier to_read[] = {
				m_context->b_unit_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite),
				m_context->b_unit_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer| vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, {}, array_length(to_read), to_read, 0, {});
		}

		// 更新
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_UnitUpdate].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 0, { m_context->getDescriptorSet() }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 1, { sSystem::Order().getSystemDescriptorSet() }, { 0 });
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 2, { m_gi2d_context->getDescriptorSet() }, {});
			cmd.dispatch(1, 1, 1);
		}

	}
	void executeMakeLinkList(vk::CommandBuffer cmd)
	{
		// カウンターのclear
		{
			cmd.fillBuffer(m_context->b_unit_link_head.getInfo().buffer, m_context->b_unit_link_head.getInfo().offset, m_context->b_unit_link_head.getInfo().range, -1);
			vk::BufferMemoryBarrier to_read[] = {
				m_context->b_unit_link_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, {}, array_length(to_read), to_read, 0, {});
		}

		// 更新
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeLinkList].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 0, { m_context->getDescriptorSet() }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 1, { sSystem::Order().getSystemDescriptorSet() }, { 0 });
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 2, { m_gi2d_context->getDescriptorSet() }, {});
			cmd.dispatch(1, 1, 1);
		}

	}

	std::shared_ptr<CrowdContext> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniqueShaderModule, Shader_Num> m_shader_module;


};