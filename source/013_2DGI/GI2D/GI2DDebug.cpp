#include <013_2DGI/GI2D/GI2DDebug.h>

#include <btrlib/cWindow.h>

#include <013_2DGI/PathContext.h>


struct GI2DLightData
{
	vec4 m_pos;
	vec4 m_emissive;
}; 

GI2DLightData g_data[5];
GI2DContext::Fragment g_wall(vec3(0.8f, 0.2f, 0.2f), true, false);
GI2DContext::Fragment g_path(vec3(1.f), false, false);

GI2DDebug::GI2DDebug(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
{
	for (int i = 0; i < std::size(g_data); i++)
	{
		g_data[i] = GI2DLightData{ vec4(std::rand() % 950 + 40, std::rand() % 950 + 40, 0.f, 0.f), vec4(std::rand() % 80 + 20,std::rand() % 80 + 20,std::rand() % 80 + 20,100) * 0.01f };
	}

	m_context = context;
	m_gi2d_context = gi2d_context;
	std::vector<GI2DContext::Fragment> map_data(gi2d_context->RenderWidth*gi2d_context->RenderHeight, g_path);
	{

#if 0
		for (int y = 0; y < gi2d_context->RenderHeight; y++)
		{
			for (int x = 0; x < gi2d_context->RenderWidth; x++)
			{
				auto& m = map_data[x + y * gi2d_context->RenderWidth];
				{
					m = glm::simplex(vec2(x,y) / 256.f) >= abs(0.5f) ? wall : path;
				}
			}
		}
#elif 0
		ivec2 size = gi2d_context->RenderSize / 8;
// 		auto map = pathmake_maze(size.x, size.y);
// 		auto aa = size /8;
// 		for (size_t y = 0; y < aa.y; y++)
// 		{
// 			for (size_t x = 0; x < aa.x; x++)
// 			{
// 				for (int i = 0; i < 64; i++)
// 				{
// 					auto ii = x + y * (aa.x);
// 					auto type = (map[ii] & (1ull << i) != 0) ? wall : path;
// 					for (int _i = 0; _i < 64; _i++)
// 					{
// //						auto di = ii + _i;
// 						auto di = (((x * 8 + i % 8)*8+_i) + ((y * 8 + i / 8)*8+_i)/8 * gi2d_context->RenderSize.x);
// 						map_data[di] = type;
// 					}
// 				}
// 			}
// 
// 		}

// 		auto map = pathmake_maze_(size.x, size.y);
// 		for (size_t y = 0; y < gi2d_context->RenderSize.y; y++)
// 		{
// 			for (size_t x = 0; x < gi2d_context->RenderSize.x; x++)
// 			{
// 				auto ii = x/8 + y/8 * size.x;
// 				auto type = (map[ii] == 0) ? wall : path;
// 
// 				auto di = ivec2(x, y);
// 				map_data[di.x + di.y*gi2d_context->RenderSize.x] = type;
// 			}
// 
// 		}

		auto map = pathmake_noise(gi2d_context->RenderSize.x, gi2d_context->RenderSize.y);
		for (int y = 0; y < size.y; y++)
		{
			for (int x = 0; x < size.x; x++)
			{
				auto& m = map[x + y * size.x];
				for (int32_t i = 0; i < 64; i++)
				{
					ivec2 xy = ivec2(x, y)*8 + ivec2(i%8, i/8);
					map_data[xy.x + xy.y*gi2d_context->RenderSize.x] = (m & (1ull << i)) != 0 ? wall : path;
				}
			}
		}


#else
		struct Fragment
		{
			ivec4 rect;
			vec4 color;
			bool is_diffuse;
			bool is_emissive;
		};
		std::vector<Fragment> rect;
		for (int i = 0; i < 600; i++) {
//			rect.emplace_back(Fragment{ ivec4{ std::rand() % gi2d_context->RenderWidth , std::rand() % gi2d_context->RenderHeight, std::rand() % 12 + 16, std::rand() % 12 + 16 }, vec4{ 0.8f,0.2f,0.2f,0.f } });
		}	
//		rect.emplace_back(Fragment{ ivec4{ 70, 900, 900, 10, }, vec4{ 0.8f,0.2f,0.2f,0.f } });
//		rect.emplace_back(Fragment{ ivec4{ 70, 900, 828, 42, }, vec4{ 0.8f,0.2f,0.2f,0.f } });
//		rect.emplace_back(Fragment{ ivec4{ 0, 0, 1023, 1023, }, vec4{ 0.8f,0.2f,0.2f,0.f } });

		for (size_t y = 0; y < gi2d_context->RenderHeight; y++)
		{
			for (size_t x = 0; x < gi2d_context->RenderWidth; x++)
			{
				auto& m = map_data[x + y * gi2d_context->RenderWidth];

				for (auto& r : rect)
				{
//					if (x >= r.rect.x && y >= r.rect.y && x < r.rect.x + r.rect.z && y < r.rect.y + r.rect.w)
					if (x == r.rect.x || y == r.rect.y || x == r.rect.x + r.rect.z || y == r.rect.y + r.rect.w)
					{
						m = g_wall;
						break;
					}
				}

			}

		}

#endif
	}

	//ŽlŠp‚ÅˆÍ‚Þ
// 	for (int32_t y = 0; y < gi2d_context->RenderHeight; y++)
// 	{
// 		for (int32_t x = 0; x < 4; x++)
// 		{
// 			map_data[x + y * gi2d_context->RenderWidth] = g_wall;
// 			map_data[(gi2d_context->RenderWidth -1 - x) + y * gi2d_context->RenderWidth] = g_wall;
// 		}
// 	}
//  	for (int32_t x = 0; x < gi2d_context->RenderWidth; x++)
//  	{
// 		for (int32_t y = 0; y < 4; y++)
// 		{
// 			map_data[x + y * gi2d_context->RenderWidth] = g_wall;
// 			map_data[x + (gi2d_context->RenderHeight - 1 - y) * gi2d_context->RenderWidth] = g_wall;
// 		}
// 	}


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
			"GI2DDebug_MakeLight.comp.spv",
			"GI2DDebug_DrawFragmentMap.comp.spv",
			"GI2DDebug_DrawFragment.comp.spv",
			"GI2DDebug_DrawReachMap.comp.spv",
		};
		static_assert(array_length(name) == Shader_Num, "not equal shader num");

		std::string path = btr::getResourceShaderPath();
		for (size_t i = 0; i < array_length(name); i++) {
			m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
		}

	}

	// pipeline layout
	{
		{
			vk::DescriptorSetLayout layouts[] = {
				m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
			};
			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setOffset(0).setSize(sizeof(GI2DLightData)).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayout_PointLight] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				RenderTarget::s_descriptor_set_layout.get(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayout_DrawFragmentMap] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

		}

		{
			vk::DescriptorSetLayout layouts[] = {
				m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Path),
				RenderTarget::s_descriptor_set_layout.get(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayout_DrawReachMap] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

		}
	}


	{
		vk::PipelineShaderStageCreateInfo shader_info[] =
		{
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[Shader_PointLight].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eCompute),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[Shader_DrawFragmentMap].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eCompute),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[Shader_DrawFragment].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eCompute),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[Shader_DrawReachMap].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eCompute),
		};

		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayout_PointLight].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[1])
			.setLayout(m_pipeline_layout[PipelineLayout_DrawFragmentMap].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[2])
			.setLayout(m_pipeline_layout[PipelineLayout_DrawFragmentMap].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[3])
			.setLayout(m_pipeline_layout[PipelineLayout_DrawReachMap].get()),
		};
		auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[Pipeline_PointLight] = std::move(compute_pipeline[0]);
		m_pipeline[Pipeline_DrawFragmentMap] = std::move(compute_pipeline[1]);
		m_pipeline[Pipeline_DrawFragment] = std::move(compute_pipeline[2]);
		m_pipeline[Pipeline_DrawReachMap] = std::move(compute_pipeline[3]);
	}

}

void GI2DDebug::executeUpdateMap(vk::CommandBuffer cmd, const std::vector<uint64_t>& map)
{
	auto staging = m_context->m_staging_memory.allocateMemory<GI2DContext::Fragment>({ m_gi2d_context->RenderSize.x*m_gi2d_context->RenderSize.y, btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT });


	for (int y = 0; y < m_gi2d_context->RenderSize.y; y++)
	{
		for (int x = 0; x < m_gi2d_context->RenderSize.x; x++)
		{
			auto mi = ivec2(x, y) >> 3;
			auto mbit = ivec2(x, y) % 8;
			*staging.getMappedPtr(x + y * m_gi2d_context->RenderSize.x) = (map[mi.x + mi.y*(m_gi2d_context->RenderSize.x >> 3)] & (1ull << (mbit.x + mbit.y * 8))) != 0 ? g_wall : g_path;
		}
	}

	vk::BufferCopy copy;
	copy.setSrcOffset(staging.getInfo().offset);
	copy.setDstOffset(m_map_data.getInfo().offset);
	copy.setSize(m_map_data.getInfo().range);
	cmd.copyBuffer(staging.getInfo().buffer, m_map_data.getInfo().buffer, copy);
}

void GI2DDebug::executeDrawFragmentMap(vk::CommandBuffer cmd, const std::shared_ptr<RenderTarget>& render_target)
{
	vk::BufferMemoryBarrier barrier[] = {
		m_gi2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
		{}, 0, nullptr, std::size(barrier), barrier, 0, nullptr);

	vk::DescriptorSet descriptorsets[] = {
		m_gi2d_context->getDescriptorSet(),
		render_target->m_descriptor.get(),
	};
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_DrawFragmentMap].get(), 0, std::size(descriptorsets), descriptorsets, 0, nullptr);

	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_DrawFragmentMap].get());

		auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderSize.x, m_gi2d_context->RenderSize.y, 1), uvec3(32, 32, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

}

void GI2DDebug::executeDrawFragment(vk::CommandBuffer cmd, const std::shared_ptr<RenderTarget>& render_target)
{
	vk::BufferMemoryBarrier barrier[] = {
		m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
	};
	vk::ImageMemoryBarrier image_barrier;
	image_barrier.setImage(render_target->m_image);
	image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
	image_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
	image_barrier.setNewLayout(vk::ImageLayout::eGeneral);
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, { image_barrier });

	vk::DescriptorSet descriptorsets[] = {
		m_gi2d_context->getDescriptorSet(),
		render_target->m_descriptor.get(),
	};
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_DrawFragmentMap].get(), 0, std::size(descriptorsets), descriptorsets, 0, nullptr);

	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_DrawFragment].get());

		auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderSize.x, m_gi2d_context->RenderSize.y, 1), uvec3(32, 32, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

}

void GI2DDebug::executeMakeFragment(vk::CommandBuffer cmd)
{
	vk::BufferMemoryBarrier to_write[] =
	{
		m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, 0, nullptr, array_length(to_write), to_write, 0, nullptr);

	vk::BufferCopy copy;
	copy.setSrcOffset(m_map_data.getInfo().offset);
	copy.setDstOffset(m_gi2d_context->b_fragment.getInfo().offset);
	copy.setSize(m_gi2d_context->b_fragment.getInfo().range);

	cmd.copyBuffer(m_map_data.getInfo().buffer, m_gi2d_context->b_fragment.getInfo().buffer, copy);

// 	// light‚Ì‚Å‚Î‚Á‚®
// //	if(0)
	{
		vk::BufferMemoryBarrier to_read[] =
		{
			m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead| vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

		static vec4 light_pos = vec4(0.5f, 0.5f, 200.f, 200.f);
//		static vec4 light_pos = vec4(1023.5f, 0.5f, 200.f, 200.f);
		float move = 3.f;
		if (m_context->m_window->getInput().m_keyboard.isHold('A'))
		{
			move = 0.3f;
		}
		if (m_context->m_window->getInput().m_keyboard.isHold(VK_SPACE))
			{
			light_pos.z += m_context->m_window->getInput().m_keyboard.isHold(VK_RIGHT) * move;
			light_pos.z -= m_context->m_window->getInput().m_keyboard.isHold(VK_LEFT) * move;
			light_pos.w -= m_context->m_window->getInput().m_keyboard.isHold(VK_UP) * move;
			light_pos.w += m_context->m_window->getInput().m_keyboard.isHold(VK_DOWN) * move;
		}
		else
		{
			light_pos.x += m_context->m_window->getInput().m_keyboard.isHold(VK_RIGHT) * move;
			light_pos.x -= m_context->m_window->getInput().m_keyboard.isHold(VK_LEFT) * move;
			light_pos.y -= m_context->m_window->getInput().m_keyboard.isHold(VK_UP) * move;
			light_pos.y += m_context->m_window->getInput().m_keyboard.isHold(VK_DOWN) * move;
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineLayout_PointLight].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_PointLight].get(), 0, m_gi2d_context->getDescriptorSet(), {});
//  		for (int y = -1; y <= 1; y++) {
//  			for (int x = -1; x <= 1; x++) {
//  				cmd.pushConstants<GI2DLightData>(m_pipeline_layout[PipelineLayout_PointLight].get(), vk::ShaderStageFlagBits::eCompute, 0, GI2DLightData{ vec4(light_pos.x+x, light_pos.y+y, 0.f, 0.f), vec4(1.f, 1.f, 1.f, 1.f) });
//  				cmd.dispatch(1, 1, 1);
//  			}
//  		}
		cmd.pushConstants<GI2DLightData>(m_pipeline_layout[PipelineLayout_PointLight].get(), vk::ShaderStageFlagBits::eCompute, 0, GI2DLightData{ vec4(light_pos.x, light_pos.y, 0.f, 0.f), vec4(1.f, 1.f, 1.f, 1.f) });
		cmd.dispatch(1, 1, 1);

// 		cmd.pushConstants<GI2DLightData>(m_pipeline_layout[PipelineLayout_PointLight].get(), vk::ShaderStageFlagBits::eCompute, 0, GI2DLightData{ vec4(0.f, 0.f, 0.f, 0.f), vec4(1.f, 1.f, 1.f, 1.f) });
// 		cmd.dispatch(1, 1, 1);
// 		cmd.pushConstants<GI2DLightData>(m_pipeline_layout[PipelineLayout_PointLight].get(), vk::ShaderStageFlagBits::eCompute, 0, GI2DLightData{ vec4(1023.f, 0.f, 0.f, 0.f), vec4(1.f, 1.f, 1.f, 1.f) });
// 		cmd.dispatch(1, 1, 1);
// 		cmd.pushConstants<GI2DLightData>(m_pipeline_layout[PipelineLayout_PointLight].get(), vk::ShaderStageFlagBits::eCompute, 0, GI2DLightData{ vec4(0.f, 1023.f, 0.f, 0.f), vec4(1.f, 1.f, 1.f, 1.f) });
// 		cmd.dispatch(1, 1, 1);
// 		cmd.pushConstants<GI2DLightData>(m_pipeline_layout[PipelineLayout_PointLight].get(), vk::ShaderStageFlagBits::eCompute, 0, GI2DLightData{ vec4(1023.f, 1023.f, 0.f, 0.f), vec4(1.f, 1.f, 1.f, 1.f) });
// 		cmd.dispatch(1, 1, 1);

		for (int i = 0; i < std::size(g_data); i++)
		{
			cmd.pushConstants<GI2DLightData>(m_pipeline_layout[PipelineLayout_PointLight].get(), vk::ShaderStageFlagBits::eCompute, 0, g_data[i]);
//	 		cmd.dispatch(1, 1, 1);
		}
	}
}

void GI2DDebug::executeDrawReachMap(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPathContext>& gi2d_path_context, const std::shared_ptr<RenderTarget>& render_target)
{
	DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__, DebugLabel::k_color_debug);

	vk::BufferMemoryBarrier barrier[] = {
		gi2d_path_context->b_closed.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
	};
	vk::ImageMemoryBarrier image_barrier;
	image_barrier.setImage(render_target->m_image);
	image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
	image_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
	image_barrier.setNewLayout(vk::ImageLayout::eGeneral);
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, { image_barrier });


	vk::DescriptorSet descriptorsets[] = {
		m_gi2d_context->getDescriptorSet(),
		gi2d_path_context->getDescriptorSet(),
		render_target->m_descriptor.get(),
	};
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_DrawReachMap].get(), 0, array_length(descriptorsets), descriptorsets, 0, nullptr);

	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_DrawReachMap].get());

		auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderSize.x, m_gi2d_context->RenderSize.y, 1), uvec3(32, 32, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

}
