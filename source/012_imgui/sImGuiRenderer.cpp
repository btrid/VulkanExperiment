#include <012_imgui/sImGuiRenderer.h>

#pragma comment(lib, "imgui.lib")


sImGuiRenderer::sImGuiRenderer(const std::shared_ptr<btr::Context>& context)
{
	m_context = context;
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	m_render_pass = std::make_shared<RenderBackbufferModule>(context);
	{
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = context->m_window->getClientSize<ImVec2>();
		io.FontGlobalScale = 1.f;
		io.RenderDrawListsFn = nullptr;  // Setup a render function, or set to NULL and call GetDrawData() after Render() to access the render data.

		unsigned char* pixels;
		int width, height, byte_per_pixel;
		io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height, &byte_per_pixel);
		{
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = vk::Format::eR8Unorm;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = vk::Extent3D( width, height, 1 );
			m_image = context->m_device->createImageUnique(image_info);

			vk::MemoryRequirements memory_request = context->m_device->getImageMemoryRequirements(m_image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(context->m_gpu.getHandle(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			m_image_memory = context->m_device->allocateMemoryUnique(memory_alloc_info);
			context->m_device->bindImageMemory(m_image.get(), m_image_memory.get(), 0);


			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;

			{
				// staging_buffer‚©‚çimage‚ÖƒRƒs[
				btr::BufferMemoryDescriptor staging_desc;
				staging_desc.size = byte_per_pixel*width*height;
				staging_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging_buffer = context->m_staging_memory.allocateMemory(staging_desc);
				memcpy_s(staging_buffer.getMappedPtr<unsigned char>(), staging_buffer.getInfo().range, pixels, staging_desc.size);

				vk::BufferImageCopy copy;
				copy.bufferOffset = staging_buffer.getBufferInfo().offset;
				copy.imageExtent = vk::Extent3D( width, height, 1 );
				copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
				copy.imageSubresource.baseArrayLayer = 0;
				copy.imageSubresource.layerCount = 1;
				copy.imageSubresource.mipLevel = 0;

				vk::ImageMemoryBarrier to_copy_barrier;
				to_copy_barrier.image = m_image.get();
				to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_copy_barrier.subresourceRange = subresourceRange;

				vk::ImageMemoryBarrier to_shader_read_barrier;
				to_shader_read_barrier.image = m_image.get();
				to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_shader_read_barrier.subresourceRange = subresourceRange;

				cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
				cmd->copyBufferToImage(staging_buffer.getBufferInfo().buffer, m_image.get(), vk::ImageLayout::eTransferDstOptimal, { copy });
				cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });

			}

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e2D;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eR;
			view_info.components.b = vk::ComponentSwizzle::eR;
			view_info.components.a = vk::ComponentSwizzle::eR;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = m_image.get();
			view_info.subresourceRange = subresourceRange;
			m_image_view.emplace_back(context->m_device->createImageViewUnique(view_info));

			vk::SamplerCreateInfo sampler_info;
			sampler_info.magFilter = vk::Filter::eNearest;
			sampler_info.minFilter = vk::Filter::eNearest;
			sampler_info.mipmapMode = vk::SamplerMipmapMode::eNearest;
			sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
			sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
			sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
			sampler_info.mipLodBias = 0.0f;
			sampler_info.compareOp = vk::CompareOp::eNever;
			sampler_info.minLod = 0.0f;
			sampler_info.maxLod = 0.f;
			sampler_info.maxAnisotropy = 1.0;
			sampler_info.anisotropyEnable = VK_FALSE;
			sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
			m_sampler = context->m_device->createSamplerUnique(sampler_info);

		}
	}
	{
		btr::BufferMemoryDescriptorEx<ImDrawVert> v_desc;
		v_desc.element_num = 4096;
		m_vertex = context->m_staging_memory.allocateMemory(v_desc);
		btr::BufferMemoryDescriptorEx<ImDrawIdx> i_desc;
		i_desc.element_num = 4096;
		m_index = context->m_staging_memory.allocateMemory(i_desc);
		m_vertex_offset = 0;
		m_index_offset = 0;
	}
	// descriptor
	{
		auto stage = vk::ShaderStageFlagBits::eFragment;
		std::vector<vk::DescriptorSetLayoutBinding> binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setBinding(0),
		};
		m_descriptor_set_layout = btr::Descriptor::createDescriptorSetLayout(context, binding);
		m_descriptor_set = btr::Descriptor::allocateDescriptorSet(context, context->m_descriptor_pool.get(), m_descriptor_set_layout.get());

		vk::DescriptorImageInfo images[] = {
			vk::DescriptorImageInfo()
			.setImageView(m_image_view[0].get())
			.setSampler(m_sampler.get())
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		auto write_desc =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(array_length(images))
			.setPImageInfo(images)
			.setDstBinding(0)
			.setDstSet(m_descriptor_set.get()),
		};
		context->m_device->updateDescriptorSets(write_desc.size(), write_desc.begin(), 0, {});

	}

	// setup shader
	{
		struct
		{
			const char* name;
		}shader_info[] =
		{
			{ "ImGuiRender.vert.spv", },
			{ "ImGuiRender.frag.spv", },
		};
		static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourceLibPath() + "shader\\binary\\";
		for (size_t i = 0; i < SHADER_NUM; i++) {
			m_shader_module[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_info[i].name);
		}
	}

	// setup pipeline_layout
	{
		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_set_layout.get(),
		};
		vk::PushConstantRange constants[] = {
			vk::PushConstantRange().setOffset(0).setSize(4).setStageFlags(vk::ShaderStageFlagBits::eFragment),
		};
		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
// 		pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
// 		pipeline_layout_info.setPPushConstantRanges(constants);
		m_pipeline_layout[PIPELINE_LAYOUT_RENDER] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}

	// setup pipeline
	vk::Extent3D size;
	size.setWidth(640);
	size.setHeight(480);
	size.setDepth(1);

	{
		// assembly
		vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
		{
			vk::PipelineInputAssemblyStateCreateInfo()
			.setPrimitiveRestartEnable(VK_FALSE)
			.setTopology(vk::PrimitiveTopology::eTriangleList),
		};

		// viewport
		vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
		std::vector<vk::Rect2D> scissor = {
			vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height))
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
		depth_stencil_info.setDepthTestEnable(VK_FALSE);

		std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
			vk::PipelineColorBlendAttachmentState()
			.setBlendEnable(VK_TRUE)
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
		blend_info.setAttachmentCount(blend_state.size());
		blend_info.setPAttachments(blend_state.data());

		vk::VertexInputAttributeDescription attr[] =
		{
			vk::VertexInputAttributeDescription().setLocation(0).setOffset(0).setBinding(0).setFormat(vk::Format::eR32G32Sfloat),
			vk::VertexInputAttributeDescription().setLocation(1).setOffset(8).setBinding(0).setFormat(vk::Format::eR32G32Sfloat),
			vk::VertexInputAttributeDescription().setLocation(2).setOffset(16).setBinding(0).setFormat(vk::Format::eR32Uint),
		};
		vk::VertexInputBindingDescription binding[] =
		{
			vk::VertexInputBindingDescription().setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(20)
		};
		vk::PipelineVertexInputStateCreateInfo vertex_input_info;
		vertex_input_info.setVertexAttributeDescriptionCount(array_length(attr));
		vertex_input_info.setPVertexAttributeDescriptions(attr);
		vertex_input_info.setVertexBindingDescriptionCount(array_length(binding));
		vertex_input_info.setPVertexBindingDescriptions(binding);

		vk::PipelineShaderStageCreateInfo shader_info[] = {
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader_module[SHADER_VERT_RENDER].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eVertex),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader_module[SHADER_FRAG_RENDER].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eFragment),
		};

		std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
		{
			vk::GraphicsPipelineCreateInfo()
			.setStageCount(array_length(shader_info))
			.setPStages(shader_info)
			.setPVertexInputState(&vertex_input_info)
			.setPInputAssemblyState(&assembly_info[0])
			.setPViewportState(&viewportInfo)
			.setPRasterizationState(&rasterization_info)
			.setPMultisampleState(&sample_info)
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_RENDER].get())
			.setRenderPass(m_render_pass->getRenderPass())
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info),
		};
		auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
		m_pipeline[PIPELINE_RENDER] = std::move(pipelines[0]);

	}

}

vk::CommandBuffer sImGuiRenderer::Render()
{
	auto cmd = m_context->m_cmd_pool->allocCmdOnetime(0);

	auto& io = ImGui::GetIO();
	{
		auto& mouse = m_context->m_window->getInput().m_mouse;
		io.MousePos = ImVec2(mouse.xy.x, mouse.xy.y);
		io.MouseDown[0] = mouse.isHold(cMouse::BUTTON_LEFT);
		io.MouseDown[1] = mouse.isHold(cMouse::BUTTON_RIGHT);
		io.MouseDown[2] = mouse.isHold(cMouse::BUTTON_MIDDLE);
		io.MouseWheel = mouse.getWheel();
	}
	{
		auto& keyboard = m_context->m_window->getInput().m_keyboard;
		io.KeyShift = keyboard.isHold(VK_SHIFT);
		io.KeyCtrl = keyboard.isHold(VK_CONTROL);
		io.KeyAlt = keyboard.isHold(vk_alt);
		for (uint32_t i = 0; i < 256; i++)
		{
			io.KeysDown[i] = keyboard.isHold(i);
		}
	}
	ImGui::NewFrame();

	ImGui::ShowTestWindow();

	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();

	vk::RenderPassBeginInfo begin_render_info;
	begin_render_info.setFramebuffer(m_render_pass->getFramebuffer(m_context->getGPUFrame()));
	begin_render_info.setRenderPass(m_render_pass->getRenderPass());
	begin_render_info.setRenderArea(vk::Rect2D({}, m_render_pass->getResolution()));
	cmd.beginRenderPass(begin_render_info, vk::SubpassContents::eInline);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_RENDER].get());

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		auto* cmd_list = draw_data->CmdLists[n];
		assert(cmd_list->VtxBuffer.size() < m_vertex.getDescriptor().element_num);
		auto v_begin = m_vertex_offset;
		{
			auto v_num = cmd_list->VtxBuffer.size();
			auto v_end = v_begin + v_num;
			if (v_end >= m_vertex.getDescriptor().element_num)
			{
				m_vertex_offset = 0;
				v_begin = 0;
				v_end = v_begin + v_num;
			}
			memcpy(m_vertex.getMappedPtr(v_begin), cmd_list->VtxBuffer.Data, sizeof(ImDrawVert)*v_num);
			m_vertex_offset += v_num;
		}
		auto i_begin = m_index_offset;
		{
			auto i_num = cmd_list->IdxBuffer.size();
			auto i_end = i_begin + i_num;
			if (i_end >= m_index.getDescriptor().element_num)
			{
				m_index_offset = 0;
				i_begin = 0;
				i_end = i_begin + i_num;
			}
			memcpy(m_index.getMappedPtr(i_begin), cmd_list->IdxBuffer.Data, sizeof(ImDrawIdx)*i_num);
			m_index_offset += i_num;
		}

		cmd.bindVertexBuffers(0, { m_vertex.getInfo().buffer }, { m_vertex.getInfo().offset+v_begin*sizeof(ImDrawVert) });
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				// Render 'pcmd->ElemCount/3' texture triangles
//				MyEngineBindTexture(pcmd->TextureId);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_RENDER].get(), 0, { m_descriptor_set.get() }, {});
//				MyEngineScissor((int)pcmd->ClipRect.x, (int)pcmd->ClipRect.y, (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
				cmd.bindIndexBuffer(m_index.getInfo().buffer, m_index.getInfo().offset + i_begin * sizeof(ImDrawIdx), sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
				cmd.drawIndexed(pcmd->ElemCount, 1, 0, 0, 0);
			}
			i_begin += pcmd->ElemCount;
		}
	}

	cmd.endRenderPass();
	cmd.end();
	return cmd;
}
