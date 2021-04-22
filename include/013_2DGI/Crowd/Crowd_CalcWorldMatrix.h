#pragma once
#include <013_2DGI/Crowd/CrowdContext.h>
#include <applib/AppModel/AppModel.h>
#include <applib/sSystem.h>

struct Crowd_CalcWorldMatrix
{
	Crowd_CalcWorldMatrix(const std::shared_ptr<CrowdContext>& crowd_context, const std::shared_ptr<AppModelContext>& appmodel_context)
	{
		m_crowd_context = crowd_context;
		m_appmodel_context = appmodel_context;

		// shader
		{
			m_shader_module = loadShaderUnique(crowd_context->m_context->m_device, btr::getResourceShaderPath() + "Crowd_CalcWorldMatrix.comp.spv");
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				crowd_context->getDescriptorSetLayout(),
				appmodel_context->getLayout(AppModelContext::DescriptorLayout_Update),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout = crowd_context->m_context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			vk::PipelineShaderStageCreateInfo shader_info;
			shader_info.setModule(m_shader_module.get());
			shader_info.setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info.setPName("main");
			vk::ComputePipelineCreateInfo compute_pipeline_info;
			compute_pipeline_info.setStage(shader_info);
			compute_pipeline_info.setLayout(m_pipeline_layout.get());
			m_pipeline = crowd_context->m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info).value;
		}


		{

		}
	}

	void execute(vk::CommandBuffer cmd, const std::shared_ptr<AppModel>& model)
	{
		vk::BufferMemoryBarrier to_write[] = {
			m_crowd_context->b_unit_pos.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			m_crowd_context->b_unit_move.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			model->b_world.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
			{}, 0, {}, array_length(to_write), to_write, 0, {});

		vk::DescriptorSet descriptors[] =
		{
			m_crowd_context->getDescriptorSet(),
			model->getDescriptorSet(AppModel::DescriptorSet_Update),
		};
		
		uint32_t offset[array_length(descriptors)] = {};
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline.get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout.get(), 0, array_length(descriptors), descriptors, array_length(offset), offset);
		cmd.dispatch(1, 1, 1);

		vk::BufferMemoryBarrier to_read[] = {
			model->b_world.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
			{}, 0, {}, array_length(to_read), to_read, 0, {});

	}
	std::shared_ptr<CrowdContext> m_crowd_context;
	std::shared_ptr<AppModelContext> m_appmodel_context;

	vk::UniqueShaderModule m_shader_module;
	vk::UniquePipelineLayout m_pipeline_layout;
	vk::UniquePipeline m_pipeline;

};