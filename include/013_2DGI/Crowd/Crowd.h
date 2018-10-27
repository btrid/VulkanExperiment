#pragma once
#include <013_2DGI/Crowd/CrowdContext.h>
#include <applib/sSystem.h>
#include <013_2DGI/GI2D/GI2DContext.h>


struct Crowd 
{
	enum Shader
	{
		Shader_UnitUpdate,
		Shader_MakeDensity,

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
		Pipeline_MakeDensity,

		Pipeline_Num,
	};
	Crowd(const std::shared_ptr<CrowdContext>& context, const std::shared_ptr<gi2d::GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;

		auto cmd = context->m_context->m_cmd_pool->allocCmdTempolary(0);

		{
			const char* name[] =
			{
				"Crowd_UnitUpdate.comp.spv",
				"Crowd_MakeDensityMap.comp.spv"
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader_module[i] = loadShaderUnique(context->m_context->m_device.getHandle(), path + name[i]);
			}
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				context->getDescriptorSetLayout(),
				sSystem::Order().getSystemDescriptorLayout(),
				gi2d_context->getDescriptorSetLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayout_Crowd] = context->m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 2> shader_info;
			shader_info[0].setModule(m_shader_module[Shader_UnitUpdate].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader_module[Shader_MakeDensity].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
			};
			auto compute_pipeline = context->m_context->m_device->createComputePipelinesUnique(context->m_context->m_cache.get(), compute_pipeline_info);
			m_pipeline[Pipeline_UnitUpdate] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_MakeDensity] = std::move(compute_pipeline[1]);
		}
	}

	void execute(vk::CommandBuffer cmd) 
	{
		// カウンターのclear
		{
			cmd.updateBuffer<uvec4>(m_context->b_unit_counter.getInfo().buffer, m_context->b_unit_counter.getInfo().offset, uvec4(0));
			vk::BufferMemoryBarrier to_read[] = {
				m_context->b_unit_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, {}, array_length(to_read), to_read, 0, {});
		}

		// 更新
		{
			vk::DescriptorSet descriptors[] =
			{
				m_context->getDescriptorSet(),
				sSystem::Order().getSystemDescriptorSet(),
				m_gi2d_context->getDescriptorSet(),
			};

			uint32_t offset[array_length(descriptors)] = {};
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_UnitUpdate].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 0, array_length(descriptors), descriptors, array_length(offset), offset);
			cmd.dispatch(1, 1, 1);
		}

	}

	void executeMakeDensity(vk::CommandBuffer cmd)
	{
		// カウンターのclear
		{
			cmd.updateBuffer<uvec4>(m_context->b_crowd_density_map.getInfo().buffer, m_context->b_crowd_density_map.getInfo().offset, uvec4(0));
			vk::BufferMemoryBarrier to_read[] = {
				m_context->b_crowd_density_map.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, {}, array_length(to_read), to_read, 0, {});
		}

		// 更新
		{
			vk::DescriptorSet descriptors[] =
			{
				m_context->getDescriptorSet(),
				sSystem::Order().getSystemDescriptorSet(),
				m_gi2d_context->getDescriptorSet(),
			};

			uint32_t offset[array_length(descriptors)] = {};
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeDensity].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 0, array_length(descriptors), descriptors, array_length(offset), offset);
			cmd.dispatch(1, 1, 1);
		}

	}

	std::shared_ptr<CrowdContext> m_context;
	std::shared_ptr<gi2d::GI2DContext> m_gi2d_context;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniqueShaderModule, Shader_Num> m_shader_module;


};