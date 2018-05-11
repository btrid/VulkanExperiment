#include <applib/PM2D/PM2DDebug.h>
#include <btrlib/cWindow.h>

namespace pm2d
{
	
PM2DDebug::PM2DDebug(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DContext>& pm2d_context)
{
	m_context = context;
	m_pm2d_context = pm2d_context;
	std::vector<PM2DContext::Fragment> map_data(pm2d_context->RenderWidth*pm2d_context->RenderHeight);
	{
		std::vector<ivec4> rect;
#if 1
		rect.emplace_back(400, 400, 200, 400);
		rect.emplace_back(300, 200, 100, 100);
		rect.emplace_back(80, 50, 200, 30);
#else
		for (int i = 0; i < 300; i++) {
			rect.emplace_back(std::rand() % 512, std::rand() % 512, std::rand() % 30, std::rand() % 30);
		}

#endif
		for (size_t y = 0; y < pm2d_context->RenderHeight; y++)
		{
			for (size_t x = 0; x < pm2d_context->RenderWidth; x++)
			{
				auto& m = map_data[x + y * pm2d_context->RenderWidth];
				m.albedo = vec3(0.f);
				for (auto& r : rect)
				{
					if (x >= r.x && y >= r.y && x <= r.x + r.z && y <= r.y + r.w) {
						m.albedo = vec3(1.f, 0., 0.);
					}
				}
			}

		}
	}


	btr::BufferMemoryDescriptorEx<PM2DContext::Fragment> desc;
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
			"PMPointLight.comp.spv",
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
			m_pm2d_context->getDescriptorSetLayout(),
		};
		vk::PushConstantRange constants[] = {
			vk::PushConstantRange().setOffset(0).setSize(sizeof(PM2DLightData)).setStageFlags(vk::ShaderStageFlagBits::eCompute),
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

void PM2DDebug::execute(vk::CommandBuffer cmd)
{
	vk::BufferMemoryBarrier to_write[] =
	{
		m_pm2d_context->b_emission_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferWrite),
		m_pm2d_context->b_fragment_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, 0, nullptr, array_length(to_write), to_write, 0, nullptr);

	vk::BufferCopy copy;
	copy.setSrcOffset(m_map_data.getInfo().offset);
	copy.setDstOffset(m_pm2d_context->b_fragment_buffer.getInfo().offset);
	copy.setSize(m_pm2d_context->b_fragment_buffer.getInfo().range);

	cmd.copyBuffer(m_map_data.getInfo().buffer, m_pm2d_context->b_fragment_buffer.getInfo().buffer, copy);

	// light‚Ì‚Å‚Î‚Á‚®
//	if(0)
	{
		static vec2 light_pos = vec2(200.f);
		static float light_dir = 0.f;
		static int level = 2;
		float move = 1.f;
		light_pos.x += m_context->m_window->getInput().m_keyboard.isHold(VK_RIGHT) * move;
		light_pos.x -= m_context->m_window->getInput().m_keyboard.isHold(VK_LEFT) * move;
		light_pos.y -= m_context->m_window->getInput().m_keyboard.isHold(VK_UP) * move;
		light_pos.y += m_context->m_window->getInput().m_keyboard.isHold(VK_DOWN) * move;
		light_dir += 0.02f;

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineLayoutPointLight].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutPointLight].get(), 0, m_pm2d_context->getDescriptorSet(), {});
		cmd.pushConstants<PM2DLightData>(m_pipeline_layout[PipelineLayoutPointLight].get(), vk::ShaderStageFlagBits::eCompute, 0, PM2DLightData{ light_pos, light_dir, -1.4f, vec4(2500.f, 0.f, 2500.f, 0.f), level });
		cmd.dispatch(1, 1, 1);
	}
	vk::BufferMemoryBarrier to_read[] =
	{
		m_pm2d_context->b_emission_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
		m_pm2d_context->b_emission_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
		m_pm2d_context->b_fragment_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);
}

}