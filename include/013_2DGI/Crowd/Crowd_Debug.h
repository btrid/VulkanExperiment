#pragma once
#include <013_2DGI/Crowd/CrowdContext.h>
#include <applib/AppModel/AppModel.h>
#include <applib/sSystem.h>

struct Crowd_Debug
{
	Crowd_Debug(const std::shared_ptr<CrowdContext>& crowd_context)
	{
		m_crowd_context = crowd_context;

		// shader
		{
			m_shader_module = loadShaderUnique(crowd_context->m_context->m_device.get(), btr::getResourceShaderPath() + "Crowd_CrowdDebug.comp.spv");
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				crowd_context->getDescriptorSetLayout(),
			};
			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eCompute).setSize(8).setOffset(0),

			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
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
			m_pipeline = crowd_context->m_context->m_device.createComputePipelineUnique(crowd_context->m_vk::PipelineCache(), compute_pipeline_info);
		}


		{

		}
	}

	void execute(vk::CommandBuffer cmd)
	{
		static vec2 s_pos0 = vec2(1020.f, 1020.f);
		static vec2 s_pos1 = vec2(200.f, 200.f);
		float move = 3.f;
		if (m_crowd_context->m_context->m_window->getInput().m_keyboard.isHold(VK_SPACE))
		{
			s_pos1.x += m_crowd_context->m_context->m_window->getInput().m_keyboard.isHold(VK_RIGHT) * move;
			s_pos1.x -= m_crowd_context->m_context->m_window->getInput().m_keyboard.isHold(VK_LEFT) * move;
			s_pos1.y -= m_crowd_context->m_context->m_window->getInput().m_keyboard.isHold(VK_UP) * move;
			s_pos1.y += m_crowd_context->m_context->m_window->getInput().m_keyboard.isHold(VK_DOWN) * move;
		}
		else 
		{
			s_pos0.x += m_crowd_context->m_context->m_window->getInput().m_keyboard.isHold(VK_RIGHT) * move;
			s_pos0.x -= m_crowd_context->m_context->m_window->getInput().m_keyboard.isHold(VK_LEFT) * move;
			s_pos0.y -= m_crowd_context->m_context->m_window->getInput().m_keyboard.isHold(VK_UP) * move;
			s_pos0.y += m_crowd_context->m_context->m_window->getInput().m_keyboard.isHold(VK_DOWN) * move;
		}


		vk::BufferMemoryBarrier to_write[] = {
			m_crowd_context->b_crowd.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
		}; 
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer,
			{}, 0, {}, array_length(to_write), to_write, 0, {});

		vk::DescriptorSet descriptors[] =
		{
			m_crowd_context->getDescriptorSet(),
		};

		std::array<CrowdContext::CrowdData, 2> data;
		data[0].target = s_pos0;
		data[1].target = s_pos1;
		cmd.updateBuffer<decltype(data)>(m_crowd_context->b_crowd.getInfo().buffer, m_crowd_context->b_crowd.getInfo().offset, data);
// 		uint32_t offset[array_length(descriptors)] = {};
// 		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline.get());
// 		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout.get(), 0, array_length(descriptors), descriptors, array_length(offset), offset);
// 		cmd.pushConstants<vec2>(m_pipeline_layout.get(), vk::ShaderStageFlagBits::eCompute, 0, s_pos0);
// 		cmd.dispatch(1, 1, 1);
// 
		vk::BufferMemoryBarrier to_read[] = {
			m_crowd_context->b_crowd.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader,
			{}, 0, {}, array_length(to_read), to_read, 0, {});

	}

	std::shared_ptr<CrowdContext> m_crowd_context;

	vk::UniqueShaderModule m_shader_module;
	vk::UniquePipelineLayout m_pipeline_layout;
	vk::UniquePipeline m_pipeline;

};