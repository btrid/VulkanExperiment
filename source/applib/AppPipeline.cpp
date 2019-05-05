#include <applib/AppPipeline.h>
#include <applib/GraphicsResource.h>

ClearPipeline::ClearPipeline(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target)
{
	vk::CommandBufferAllocateInfo cmd_buffer_info;
	cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
	cmd_buffer_info.commandBufferCount = 1;
	cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
	m_cmd = std::move(context->m_device->allocateCommandBuffersUnique(cmd_buffer_info)[0]);

	vk::CommandBufferBeginInfo begin_info;
	begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
	vk::CommandBufferInheritanceInfo inheritance_info;
	begin_info.setPInheritanceInfo(&inheritance_info);
	m_cmd->begin(begin_info);

	{
		// clear
		vk::ImageMemoryBarrier present_to_clear;
		present_to_clear.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
		present_to_clear.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
		present_to_clear.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		present_to_clear.setImage(render_target->m_image);
		m_cmd->pipelineBarrier(
			vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlagBits::eTransfer,
			vk::DependencyFlags(),
			nullptr, nullptr, present_to_clear);

		vk::ClearColorValue clear_color;
		clear_color.setFloat32(std::array<float, 4>{0.8f, 0.8f, 1.f, 0.f});
		m_cmd->clearColorImage(render_target->m_image, vk::ImageLayout::eTransferDstOptimal, clear_color, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

	}

	{
		// render target用
		vk::ImageMemoryBarrier clear_to_render;
		clear_to_render.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
		clear_to_render.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		clear_to_render.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
		clear_to_render.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
		clear_to_render.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		clear_to_render.setImage(render_target->m_image);
		m_cmd->pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::DependencyFlags(),
			nullptr, nullptr, clear_to_render);
	}

	{
		vk::ImageMemoryBarrier render_to_clear_depth;
		render_to_clear_depth.setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		render_to_clear_depth.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
		render_to_clear_depth.setOldLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
		render_to_clear_depth.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
		render_to_clear_depth.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
		render_to_clear_depth.setImage(render_target->m_depth_image);
		m_cmd->pipelineBarrier(
			vk::PipelineStageFlagBits::eLateFragmentTests,
			vk::PipelineStageFlagBits::eTransfer,
			vk::DependencyFlags(),
			nullptr, nullptr, render_to_clear_depth);

		vk::ClearDepthStencilValue clear_depth;
		clear_depth.setDepth(0.f);
		m_cmd->clearDepthStencilImage(render_target->m_depth_image, vk::ImageLayout::eTransferDstOptimal, clear_depth, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
	}

	{
		vk::ImageMemoryBarrier clear_to_render_depth;
		clear_to_render_depth.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
		clear_to_render_depth.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		clear_to_render_depth.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
		clear_to_render_depth.setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
		clear_to_render_depth.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
		clear_to_render_depth.setImage(render_target->m_depth_image);
		m_cmd->pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eLateFragmentTests,
			vk::DependencyFlags(),
			nullptr, nullptr, clear_to_render_depth);

	}

	m_cmd->end();

#if _DEBUG
	vk::DebugUtilsObjectNameInfoEXT name_info;
	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_cmd.get());
	name_info.objectType = vk::ObjectType::eCommandBuffer;
	name_info.pObjectName = "ClearPipeline CMD";
	context->m_device->setDebugUtilsObjectNameEXT(name_info, context->m_dispach);
#endif


}

PresentPipeline::PresentPipeline(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target, const std::shared_ptr<Swapchain>& swapchain)
{
	m_swapchain = swapchain;

	{
		auto stage = vk::ShaderStageFlagBits::eFragment;
		vk::DescriptorSetLayoutBinding binding[] = {
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setBinding(0),
		};
		vk::DescriptorSetLayoutCreateInfo desc_layout_info;
		desc_layout_info.setBindingCount(array_length(binding));
		desc_layout_info.setPBindings(binding);
		m_descriptor_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

	}
	{
		struct
		{
			const char* name;
			vk::ShaderStageFlagBits stage;
		}shader_info[] =
		{
			{ "CopyImage.vert.spv",vk::ShaderStageFlagBits::eVertex },
			{ "CopyImage.frag.spv",vk::ShaderStageFlagBits::eFragment },
		};
		static_assert(array_length(shader_info) == 2, "not equal shader num");

		std::string path = btr::getResourceLibPath() + "shader\\binary\\";
		for (size_t i = 0; i < 2; i++) {
			m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_info[i].name);
		}

	}

	{
		// viewの作成
		m_backbuffer_view.resize(swapchain->getBackbufferNum());
		for (uint32_t i = 0; i < m_backbuffer_view.size(); i++)
		{
			auto subresourceRange = vk::ImageSubresourceRange()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setBaseArrayLayer(0)
				.setLayerCount(1)
				.setBaseMipLevel(0)
				.setLevelCount(1);

			vk::ImageViewCreateInfo backbuffer_view_info;
			backbuffer_view_info.setImage(swapchain->m_backbuffer_image[i]);
			backbuffer_view_info.setFormat(swapchain->m_surface_format.format);
			backbuffer_view_info.setViewType(vk::ImageViewType::e2D);
			backbuffer_view_info.setComponents(vk::ComponentMapping{
				vk::ComponentSwizzle::eR,
				vk::ComponentSwizzle::eG,
				vk::ComponentSwizzle::eB,
				vk::ComponentSwizzle::eA,
				});
			backbuffer_view_info.setSubresourceRange(subresourceRange);
			m_backbuffer_view[i] = context->m_device->createImageViewUnique(backbuffer_view_info);
		}
	}

	// レンダーパス
	{
		// sub pass
		vk::AttachmentReference color_ref[] =
		{
			vk::AttachmentReference()
			.setAttachment(0)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
		};
		vk::SubpassDescription subpass;
		subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
		subpass.setInputAttachmentCount(0);
		subpass.setPInputAttachments(nullptr);
		subpass.setColorAttachmentCount(array_length(color_ref));
		subpass.setPColorAttachments(color_ref);

		vk::AttachmentDescription attach_description[] =
		{
			// color1
			vk::AttachmentDescription()
			.setFormat(swapchain->m_surface_format.format)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
		};
		vk::RenderPassCreateInfo renderpass_info;
		renderpass_info.setAttachmentCount(array_length(attach_description));
		renderpass_info.setPAttachments(attach_description);
		renderpass_info.setSubpassCount(1);
		renderpass_info.setPSubpasses(&subpass);

		m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
	}

	m_framebuffer.resize(swapchain->getBackbufferNum());
	for (uint32_t i = 0; i < m_framebuffer.size(); i++)
	{
		vk::ImageView view[] = {
			m_backbuffer_view[i].get(),
		};
		vk::FramebufferCreateInfo framebuffer_info;
		framebuffer_info.setRenderPass(m_render_pass.get());
		framebuffer_info.setAttachmentCount(array_length(view));
		framebuffer_info.setPAttachments(view);
		framebuffer_info.setWidth(swapchain->getSize().width);
		framebuffer_info.setHeight(swapchain->getSize().height);
		framebuffer_info.setLayers(1);

		m_framebuffer[i] = context->m_device->createFramebufferUnique(framebuffer_info);
	}

	// pipeline layout
	{
		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_layout.get()
		};
		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}

	// pipeline 
	{
		// assembly
		vk::PipelineInputAssemblyStateCreateInfo assembly_info;
		assembly_info.setPrimitiveRestartEnable(VK_FALSE);
		assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

		// viewport
		vk::Viewport viewport = vk::Viewport(0.f, 0.f, swapchain->getSize().width, swapchain->getSize().height, 0.f, 1.f);
		vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), swapchain->getSize());
		vk::PipelineViewportStateCreateInfo viewportInfo;
		viewportInfo.setViewportCount(1);
		viewportInfo.setPViewports(&viewport);
		viewportInfo.setScissorCount(1);
		viewportInfo.setPScissors(&scissor);

		vk::PipelineRasterizationStateCreateInfo rasterization_info;
		rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
		rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
		rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
		rasterization_info.setLineWidth(1.f);

		vk::PipelineMultisampleStateCreateInfo sample_info;
		sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

		vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
		depth_stencil_info.setDepthTestEnable(VK_FALSE);
		depth_stencil_info.setDepthWriteEnable(VK_FALSE);
		depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
		depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
		depth_stencil_info.setStencilTestEnable(VK_FALSE);

		vk::PipelineColorBlendAttachmentState blend_state[] = {
			vk::PipelineColorBlendAttachmentState()
			.setBlendEnable(VK_FALSE)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
			.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setAlphaBlendOp(vk::BlendOp::eAdd)
			.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setColorWriteMask(vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB
				| vk::ColorComponentFlagBits::eA)
		};

		vk::PipelineColorBlendStateCreateInfo blend_info;
		blend_info.setAttachmentCount(array_length(blend_state));
		blend_info.setPAttachments(blend_state);

		vk::PipelineVertexInputStateCreateInfo vertex_input_info;

		vk::PipelineShaderStageCreateInfo shader_info[] =
		{
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[0].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eVertex),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[1].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eFragment),
		};

		vk::PipelineDynamicStateCreateInfo dynamic_info;

		vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
		{
			vk::GraphicsPipelineCreateInfo()
			.setStageCount(array_length(shader_info))
			.setPStages(shader_info)
			.setPVertexInputState(&vertex_input_info)
			.setPInputAssemblyState(&assembly_info)
			.setPViewportState(&viewportInfo)
			.setPRasterizationState(&rasterization_info)
			.setPMultisampleState(&sample_info)
			.setLayout(m_pipeline_layout.get())
			.setRenderPass(m_render_pass.get())
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info)
		};
		auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
		m_pipeline = std::move(pipelines[0]);
	}

	{
		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_layout.get(),
		};
		vk::DescriptorSetAllocateInfo desc_info;
		desc_info.setDescriptorPool(context->m_descriptor_pool.get());
		desc_info.setDescriptorSetCount(array_length(layouts));
		desc_info.setPSetLayouts(layouts);
		m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

		vk::DescriptorImageInfo images[] = {
			vk::DescriptorImageInfo()
			.setImageView(render_target->m_view)
			.setSampler(sGraphicsResource::Order().getSampler(sGraphicsResource::BASIC_SAMPLER_LINER))
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
		};
		vk::WriteDescriptorSet write[] = {
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(array_length(images))
			.setPImageInfo(images)
			.setDstBinding(0)
			.setDstSet(m_descriptor_set.get())
		};
		context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);

	}

	{
		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
		cmd_buffer_info.commandBufferCount = sGlobal::FRAME_MAX;
		cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
		m_cmd = context->m_device->allocateCommandBuffersUnique(cmd_buffer_info);
	}


	for (int i = 0; i < m_cmd.size(); i++)
	{
		auto& cmd = m_cmd[i];

		vk::CommandBufferBeginInfo begin_info;
		begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
		vk::CommandBufferInheritanceInfo inheritance_info;
		begin_info.setPInheritanceInfo(&inheritance_info);
		cmd->begin(begin_info);

		{
			vk::ImageMemoryBarrier present_to_dstcopy;
			present_to_dstcopy.setSrcAccessMask(vk::AccessFlagBits::eMemoryRead);
			present_to_dstcopy.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			present_to_dstcopy.setOldLayout(vk::ImageLayout::ePresentSrcKHR);
			present_to_dstcopy.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			present_to_dstcopy.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			present_to_dstcopy.setImage(swapchain->m_backbuffer_image[i]);
			cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eColorAttachmentOutput, {},
				nullptr, nullptr, present_to_dstcopy);
		}

		{
			vk::RenderPassBeginInfo begin_render_info;
			begin_render_info.setFramebuffer(m_framebuffer[i].get());
			begin_render_info.setRenderPass(m_render_pass.get());
			begin_render_info.setRenderArea(vk::Rect2D({}, render_target->m_resolution));
			cmd->beginRenderPass(begin_render_info, vk::SubpassContents::eInline);

			cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
			cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 0, { m_descriptor_set.get() }, {});
			cmd->draw(3, 1, 0, 0);

			cmd->endRenderPass();
		}

		cmd->end();
	}

#if USE_DEBUG_REPORT
	char buf[256];
	vk::DebugUtilsObjectNameInfoEXT name_info;
	name_info.objectType = vk::ObjectType::eCommandBuffer;
	name_info.pObjectName = buf;
	for (int i = 0; i < m_cmd.size(); i++)
	{
		name_info.objectHandle = reinterpret_cast<uint64_t &>(m_cmd[i].get());
		sprintf_s(buf, "PresentPipeline CMD[%d]", i);
		context->m_device->setDebugUtilsObjectNameEXT(name_info, context->m_dispach);
	}
#endif
}

