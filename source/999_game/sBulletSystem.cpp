#include <999_game/sBulletSystem.h>

void sBulletSystem::Private::setup(std::shared_ptr<btr::Loader>& loader)
{

	m_bullet_info_cpu.m_max_num = 8192;

	{
		// レンダーパス
		{
			// sub pass
			std::vector<vk::AttachmentReference> color_ref =
			{
				vk::AttachmentReference()
				.setAttachment(0)
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
			};
			vk::AttachmentReference depth_ref;
			depth_ref.setAttachment(1);
			depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount((uint32_t)color_ref.size());
			subpass.setPColorAttachments(color_ref.data());
			subpass.setPDepthStencilAttachment(&depth_ref);

			std::vector<vk::AttachmentDescription> attach_description = {
				// color1
				vk::AttachmentDescription()
				.setFormat(loader->m_window->getSwapchain().m_surface_format.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
				vk::AttachmentDescription()
				.setFormat(vk::Format::eD32Sfloat)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
				.setAttachmentCount(attach_description.size())
				.setPAttachments(attach_description.data())
				.setSubpassCount(1)
				.setPSubpasses(&subpass);

			m_render_pass = loader->m_device->createRenderPassUnique(renderpass_info);
		}

		m_framebuffer.resize(loader->m_window->getSwapchain().getBackbufferNum());
		{
			std::array<vk::ImageView, 2> view;

			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass.get());
			framebuffer_info.setAttachmentCount((uint32_t)view.size());
			framebuffer_info.setPAttachments(view.data());
			framebuffer_info.setWidth(loader->m_window->getClientSize().x);
			framebuffer_info.setHeight(loader->m_window->getClientSize().y);
			framebuffer_info.setLayers(1);

			for (size_t i = 0; i < m_framebuffer.size(); i++) {
				view[0] = loader->m_window->getSwapchain().m_backbuffer[i].m_view;
				view[1] = loader->m_window->getSwapchain().m_depth.m_view;
				m_framebuffer[i] = loader->m_device->createFramebufferUnique(framebuffer_info);
			}
		}

	}

	{
		{
			btr::BufferMemory::Descriptor data_desc;
			data_desc.size = sizeof(BulletData) * m_bullet_info_cpu.m_max_num;
			m_bullet = loader->m_storage_memory.allocateMemory(data_desc);
			std::vector<BulletData> p(m_bullet_info_cpu.m_max_num);
			loader->m_cmd.fillBuffer(m_bullet.getBufferInfo().buffer, m_bullet.getBufferInfo().offset, m_bullet.getBufferInfo().range, 0u);

			data_desc.size = sizeof(BulletData) * 1024;
			m_emit = loader->m_storage_memory.allocateMemory(data_desc);
		}

		btr::BufferMemory::Descriptor desc;
		desc.size = sizeof(vk::DrawIndirectCommand);
		m_bullet_counter = loader->m_storage_memory.allocateMemory(desc);
		loader->m_cmd.updateBuffer<vk::DrawIndirectCommand>(m_bullet_counter.getBufferInfo().buffer, m_bullet_counter.getBufferInfo().offset, vk::DrawIndirectCommand(4, 0, 0, 0));
		auto count_barrier = m_bullet_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
		loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { count_barrier }, {});

		m_bullet_info = loader->m_uniform_memory.allocateMemory(sizeof(BulletInfo));
		loader->m_cmd.updateBuffer<BulletInfo>(m_bullet_info.getBufferInfo().buffer, m_bullet_info.getBufferInfo().offset, { m_bullet_info_cpu });
		auto barrier = m_bullet_info.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
		loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { barrier }, {});

		auto& map_desc = sScene::Order().m_map_info_cpu.m_descriptor[0];
		desc.size = sizeof(uint32_t) * map_desc.m_cell_num.x* map_desc.m_cell_num.y;
		m_bullet_LL_head_gpu = loader->m_storage_memory.allocateMemory(desc);
		loader->m_cmd.fillBuffer(m_bullet_LL_head_gpu.getBufferInfo().buffer, m_bullet_LL_head_gpu.getBufferInfo().offset, m_bullet_LL_head_gpu.getBufferInfo().range, 0xFFFFFFFF);

	}

	{
		struct ShaderDesc {
			const char* name;
			vk::ShaderStageFlagBits stage;
		} shader_desc[] =
		{
			{"BulletUpdate.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{"BulletEmit.comp.spv",	 vk::ShaderStageFlagBits::eCompute },
			{"BulletRender.vert.spv", vk::ShaderStageFlagBits::eVertex },
			{"BulletRender.frag.spv", vk::ShaderStageFlagBits::eFragment },
		};
		static_assert(array_length(shader_desc) == SHADER_NUM, "not equal shader num");
		std::string path = btr::getResourceAppPath() + "shader\\binary\\";
		for (uint32_t i = 0; i < SHADER_NUM; i++)
		{
			m_shader_module[i] = loadShaderUnique(loader->m_device.getHandle(), path + shader_desc[i].name);
			m_shader_info[i].setModule(m_shader_module[i].get());
			m_shader_info[i].setStage(shader_desc[i].stage);
			m_shader_info[i].setPName("main");
		}
	}

	{
		// descriptor set layout
		std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
		bindings[DESCRIPTOR_SET_LAYOUT_UPDATE] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(4),
		};

		for (size_t i = 0; i < bindings.size(); i++)
		{
			vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings[i].size())
				.setPBindings(bindings[i].data());
			m_descriptor_set_layout[i] = loader->m_device->createDescriptorSetLayoutUnique(descriptor_set_layout_info);
		}

		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = loader->m_descriptor_pool.get();
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_UPDATE].get();
		auto descriptor_set = loader->m_device->allocateDescriptorSetsUnique(alloc_info);
		std::copy(std::make_move_iterator(descriptor_set.begin()), std::make_move_iterator(descriptor_set.end()), m_descriptor_set.begin());
	}
	{
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_UPDATE].get(),
				sScene::Order().getDescriptorSetLayout(sScene::DESCRIPTOR_SET_LAYOUT_MAP),
			};
			std::vector<vk::PushConstantRange> push_constants = {
				vk::PushConstantRange()
				.setStageFlags(vk::ShaderStageFlagBits::eCompute)
				.setSize(16),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			pipeline_layout_info.setPushConstantRangeCount(push_constants.size());
			pipeline_layout_info.setPPushConstantRanges(push_constants.data());
			m_pipeline_layout[PIPELINE_LAYOUT_UPDATE] = loader->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_UPDATE].get(),
				sScene::Order().getDescriptorSetLayout(sScene::DESCRIPTOR_SET_LAYOUT_CAMERA),
			};
			std::vector<vk::PushConstantRange> push_constants = {
				vk::PushConstantRange()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex)
				.setSize(4),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			pipeline_layout_info.setPushConstantRangeCount(push_constants.size());
			pipeline_layout_info.setPPushConstantRanges(push_constants.data());
			m_pipeline_layout[PIPELINE_LAYOUT_BULLET_DRAW] = loader->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		{

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_bullet_info.getBufferInfo(),
			};
			std::vector<vk::DescriptorBufferInfo> storages = {
				m_bullet.getBufferInfo(),
				m_bullet_counter.getBufferInfo(),
				m_emit.getBufferInfo(),
				m_bullet_LL_head_gpu.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount((uint32_t)uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_UPDATE].get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(1)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_UPDATE].get()),
			};
			loader->m_device->updateDescriptorSets(write_desc, {});
		}

	}
	{
		// Create pipeline
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_COMPUTE_UPDATE])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_COMPUTE_EMIT])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get()),
		};
		auto compute_pipeline = loader->m_device->createComputePipelinesUnique(loader->m_cache.get(), compute_pipeline_info);
		m_pipeline[PIPELINE_COMPUTE_UPDATE] = std::move(compute_pipeline[0]);
		m_pipeline[PIPELINE_COMPUTE_EMIT] = std::move(compute_pipeline[1]);

		// pipeline
		{
			std::vector<vk::PipelineShaderStageCreateInfo> shader_info =
			{
				m_shader_info[SHADER_VERTEX_PARTICLE],
				m_shader_info[SHADER_FRAGMENT_PARTICLE],
			};

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleStrip),
			};

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)loader->m_window->getClientSize().x, (float)loader->m_window->getClientSize().y, 0.f, 1.f);
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), loader->m_window->getClientSize<vk::Extent2D>())
			};
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount((uint32_t)scissor.size());
			viewportInfo.setPScissors(scissor.data());

			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);

			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount((uint32_t)blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount((uint32_t)shader_info.size())
				.setPStages(shader_info.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_BULLET_DRAW].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto graphics_pipeline = loader->m_device->createGraphicsPipelinesUnique(loader->m_cache.get(), graphics_pipeline_info);
			m_pipeline[PIPELINE_GRAPHICS_RENDER] = std::move(graphics_pipeline[0]);

		}
	}

}
