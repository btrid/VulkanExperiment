#include <013_2DGI/GI2D/GI2DDebug.h>
#include <btrlib/cWindow.h>

namespace gi2d
{
	
GI2DDebug::GI2DDebug(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
{
	m_context = context;
	m_gi2d_context = gi2d_context;
	std::vector<GI2DContext::Fragment> map_data(gi2d_context->RenderWidth*gi2d_context->RenderHeight);
	{
		struct Fragment
		{
			ivec4 rect;
			vec4 color;
		};
		std::vector<Fragment> rect;
		rect.emplace_back(Fragment{ ivec4{ 200, 400, 10, 20 }, vec4{0.0f,0.0f,0.6f,1.f} });
		rect.emplace_back(Fragment{ ivec4{ 300, 40, 8, 10 }, vec4{ 0.6f,0.0f,0.f,1.f } });
		rect.emplace_back(Fragment{ ivec4{ 270, 150, 4, 10 }, vec4{ 0.f,0.6f,0.f,1.f } });
		rect.emplace_back(Fragment{ ivec4{ 50, 200, 12, 12 }, vec4{ 0.2f,0.2f,0.2f,1.f } });
#if 0
		rect.emplace_back(Fragment{ ivec4{ 200, 150, 100, 100}, vec4{ 1.f,0.f,0.f,0.f } });
		rect.emplace_back(Fragment{ ivec4{ 80, 50, 500, 20 }, vec4{ 1.f,0.f,0.f,0.f } });
#else
		for (int i = 0; i < 100; i++) {
			rect.emplace_back(Fragment{ ivec4{ std::rand() % 512, std::rand() % 512, std::rand() % 22+5, std::rand() % 22+5 }, vec4{ 0.1f,0.1f,0.1f,0.f } });
		}

#endif
		for (size_t y = 0; y < gi2d_context->RenderHeight; y++)
		{
			for (size_t x = 0; x < gi2d_context->RenderWidth; x++)
			{
				auto& m = map_data[x + y * gi2d_context->RenderWidth];
				m.albedo = vec4(0.f);
				for (auto& r : rect)
				{
					if (x >= r.rect.x && y >= r.rect.y && x <= r.rect.x + r.rect.z && y <= r.rect.y + r.rect.w) {
						m.albedo = r.color;
						break;
					}
				}
			}

		}
	}


	btr::BufferMemoryDescriptorEx<GI2DContext::Fragment> desc;
	desc.element_num = map_data.size();
	m_map_data = m_context->m_storage_memory.allocateMemory(desc);

	desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
	auto staging = m_context->m_staging_memory.allocateMemory(desc);
	memcpy_s(staging.getMappedPtr(), vector_sizeof(map_data), map_data.data(), vector_sizeof(map_data));

	vk::BufferCopy copy;
	copy.setSrcOffset(staging.getInfo().offset);
	copy.setDstOffset(m_map_data.getInfo().offset);
	copy.setSize(m_map_data.getInfo().range);

	auto cmd = m_context->m_cmd_pool->allocCmdTempolary(0);
	cmd.copyBuffer(staging.getInfo().buffer, m_map_data.getInfo().buffer, copy);

	{
		const char* name[] =
		{
			"GI2DDebugLight.comp.spv",
		};
		static_assert(array_length(name) == array_length(m_shader), "not equal shader num");

		std::string path = btr::getResourceShaderPath();
		for (size_t i = 0; i < array_length(name); i++) {
			m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
		}

	}

	// pipeline layout
	{
		vk::DescriptorSetLayout layouts[] = {
			m_gi2d_context->getDescriptorSetLayout(),
		};
		vk::PushConstantRange constants[] = {
			vk::PushConstantRange().setOffset(0).setSize(sizeof(GI2DLightData)).setStageFlags(vk::ShaderStageFlagBits::eCompute),
		};
		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
		pipeline_layout_info.setPPushConstantRanges(constants);
		m_pipeline_layout[PipelineLayoutPointLight] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}


	{
		vk::PipelineShaderStageCreateInfo shader_info[] =
		{
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[ShaderPointLight].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eCompute),
		};

		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayoutPointLight].get()),
		};
		auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[PipelineLayoutPointLight] = std::move(compute_pipeline[0]);
	}

}

void GI2DDebug::execute(vk::CommandBuffer cmd)
{
	vk::BufferMemoryBarrier to_write[] =
	{
		m_gi2d_context->b_fragment_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, 0, nullptr, array_length(to_write), to_write, 0, nullptr);

	vk::BufferCopy copy;
	copy.setSrcOffset(m_map_data.getInfo().offset);
	copy.setDstOffset(m_gi2d_context->b_fragment_buffer.getInfo().offset);
	copy.setSize(m_gi2d_context->b_fragment_buffer.getInfo().range);

	cmd.copyBuffer(m_map_data.getInfo().buffer, m_gi2d_context->b_fragment_buffer.getInfo().buffer, copy);

	// light‚Ì‚Å‚Î‚Á‚®
//	if(0)
	{
		static vec2 light_pos = vec2(200.f);
		static float light_dir = 0.f;
		static int level = 0;
		float move = 1.f;
		light_pos.x += m_context->m_window->getInput().m_keyboard.isHold(VK_RIGHT) * move;
		light_pos.x -= m_context->m_window->getInput().m_keyboard.isHold(VK_LEFT) * move;
		light_pos.y -= m_context->m_window->getInput().m_keyboard.isHold(VK_UP) * move;
		light_pos.y += m_context->m_window->getInput().m_keyboard.isHold(VK_DOWN) * move;
		light_dir += 0.02f;

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineLayoutPointLight].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutPointLight].get(), 0, m_gi2d_context->getDescriptorSet(), {});
		cmd.pushConstants<GI2DLightData>(m_pipeline_layout[PipelineLayoutPointLight].get(), vk::ShaderStageFlagBits::eCompute, 0, GI2DLightData{ light_pos, light_dir, -1.4f, vec4(2500.f, 0.f, 2500.f, 0.f), level });
		cmd.dispatch(1, 1, 1);

// 		{
// 
//  			cmd.pushConstants<GI2DLightData>(m_pipeline_layout[PipelineLayoutPointLight].get(), vk::ShaderStageFlagBits::eCompute, 0, GI2DLightData{ vec2(430.f, 170.f), 0.f, -1.4f, vec4(2500.f, 0.f, 2500.f, 0.f), level });
//  			cmd.dispatch(1, 1, 1);
//  			cmd.pushConstants<GI2DLightData>(m_pipeline_layout[PipelineLayoutPointLight].get(), vk::ShaderStageFlagBits::eCompute, 0, GI2DLightData{ vec2(60.f, 400.f), 0.f, -1.4f, vec4(2500.f, 0.f, 2500.f, 0.f), level });
//  			cmd.dispatch(1, 1, 1);
//  			cmd.pushConstants<GI2DLightData>(m_pipeline_layout[PipelineLayoutPointLight].get(), vk::ShaderStageFlagBits::eCompute, 0, GI2DLightData{ vec2(80.f, 200.f), 0.f, -1.4f, vec4(2500.f, 0.f, 2500.f, 0.f), level });
//  			cmd.dispatch(1, 1, 1);
// 		}
	}
	vk::BufferMemoryBarrier to_read[] =
	{
		m_gi2d_context->b_fragment_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);
}

}