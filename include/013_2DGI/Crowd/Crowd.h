#pragma once
#include <013_2DGI/Crowd/CrowdContext.h>
#include <applib/sSystem.h>


struct Crowd 
{
	enum Shader
	{
		Shader_UnitUpdate,

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

		Pipeline_Num,
	};
	Crowd(const std::shared_ptr<CrowdContext>& context)
	{
		m_context = context;
		auto cmd = context->m_context->m_cmd_pool->allocCmdTempolary(0);

		{
			const char* name[] =
			{
				"Crowd_UnitUpdate.comp.spv",

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
			};

// 			vk::PushConstantRange constants[] = {
// 				vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eCompute).setSize(8).setOffset(0),
// 			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
// 			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
// 			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayout_Crowd] = context->m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 1> shader_info;
			shader_info[0].setModule(m_shader_module[Shader_UnitUpdate].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
			};
			auto compute_pipeline = context->m_context->m_device->createComputePipelinesUnique(context->m_context->m_cache.get(), compute_pipeline_info);
			m_pipeline[Pipeline_UnitUpdate] = std::move(compute_pipeline[0]);
		}
	}

	void execute(vk::CommandBuffer cmd) 
	{
		vk::DescriptorSet descriptors[] =
		{
			m_context->getDescriptorSet(),
			sSystem::Order().getSystemDescriptorSet(),
		};

		uint32_t offset[array_length(descriptors)] = {};
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_UnitUpdate].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 0, array_length(descriptors), descriptors, array_length(offset), offset);
		cmd.dispatch(1, 1, 1);

	}

	std::shared_ptr<CrowdContext> m_context;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniqueShaderModule, Shader_Num> m_shader_module;


};