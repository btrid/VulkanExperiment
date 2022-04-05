#include <applib/sAppImGui.h>
#include <applib/sSystem.h>
#include <applib/App.h>

sAppImGui::sAppImGui(const std::shared_ptr<btr::Context>& context)
{
	m_context = context;
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	{
		m_imgui_context = ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

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
			image_info.extent = vk::Extent3D(width, height, 1);
			m_font_image = context->m_device.createImageUnique(image_info);

			vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_font_image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			m_font_image_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
			context->m_device.bindImageMemory(m_font_image.get(), m_font_image_memory.get(), 0);


			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;

			{
				// staging_bufferからimageへコピー
				btr::BufferMemoryDescriptor staging_desc;
				staging_desc.size = byte_per_pixel*width*height;
				staging_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging_buffer = context->m_staging_memory.allocateMemory(staging_desc);
				memcpy_s(staging_buffer.getMappedPtr<unsigned char>(), staging_buffer.getInfo().range, pixels, staging_desc.size);

				vk::BufferImageCopy copy;
				copy.bufferOffset = staging_buffer.getInfo().offset;
				copy.imageExtent = vk::Extent3D(width, height, 1);
				copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
				copy.imageSubresource.baseArrayLayer = 0;
				copy.imageSubresource.layerCount = 1;
				copy.imageSubresource.mipLevel = 0;

				vk::ImageMemoryBarrier to_copy_barrier;
				to_copy_barrier.image = m_font_image.get();
				to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_copy_barrier.subresourceRange = subresourceRange;

				vk::ImageMemoryBarrier to_shader_read_barrier;
				to_shader_read_barrier.image = m_font_image.get();
				to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_shader_read_barrier.subresourceRange = subresourceRange;

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
				cmd.copyBufferToImage(staging_buffer.getInfo().buffer, m_font_image.get(), vk::ImageLayout::eTransferDstOptimal, 1, &copy);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });

			}

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e2D;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eR;
			view_info.components.b = vk::ComponentSwizzle::eR;
			view_info.components.a = vk::ComponentSwizzle::eR;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = m_font_image.get();
			view_info.subresourceRange = subresourceRange;
			m_font_image_view = context->m_device.createImageViewUnique(view_info);

			vk::SamplerCreateInfo sampler_info;
			sampler_info.magFilter = vk::Filter::eLinear;
			sampler_info.minFilter = vk::Filter::eLinear;
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
			m_font_sampler = context->m_device.createSamplerUnique(sampler_info);

		}
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
			.setImageView(m_font_image_view.get())
			.setSampler(m_font_sampler.get())
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		vk::WriteDescriptorSet write_desc[] =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(array_length(images))
			.setPImageInfo(images)
			.setDstBinding(0)
			.setDstSet(m_descriptor_set.get()),
		};
		context->m_device.updateDescriptorSets(array_length(write_desc), write_desc, 0, nullptr);

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
			m_shader_module[i] = loadShaderUnique(context->m_device, path + shader_info[i].name);
		}
	}

	// setup pipeline_layout
	{
		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_set_layout.get(),
			sSystem::Order().getSystemDescriptorLayout(),
		};
		vk::PushConstantRange constants[] = {
			vk::PushConstantRange().setOffset(0).setSize(4).setStageFlags(vk::ShaderStageFlagBits::eFragment),
		};
		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		// 		pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
		// 		pipeline_layout_info.setPPushConstantRanges(constants);
		m_pipeline_layout[PIPELINE_LAYOUT_RENDER] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
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
			.setFormat(vk::Format::eR16G16B16A16Sfloat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eLoad)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
		};
		vk::RenderPassCreateInfo renderpass_info;
		renderpass_info.setAttachmentCount(array_length(attach_description));
		renderpass_info.setPAttachments(attach_description);
		renderpass_info.setSubpassCount(1);
		renderpass_info.setPSubpasses(&subpass);

		m_render_pass = context->m_device.createRenderPassUnique(renderpass_info);
	}

	{
		//		vk::ImageView view[] = {
			//		render_target->m_view
		//		};

		vk::FramebufferCreateInfo framebuffer_info;
		framebuffer_info.setFlags(vk::FramebufferCreateFlagBits::eImageless);
		framebuffer_info.setRenderPass(m_render_pass.get());
		framebuffer_info.setAttachmentCount(1);
		//		framebuffer_info.setPAttachments(view);
		framebuffer_info.setWidth(1024);
		framebuffer_info.setHeight(1024);
		framebuffer_info.setLayers(1);

		vk::Format format[] = {vk::Format::eR16G16B16A16Sfloat};

		vk::FramebufferAttachmentImageInfo framebuffer_image_info;
		framebuffer_image_info.setUsage(vk::ImageUsageFlagBits::eColorAttachment);
		framebuffer_image_info.setLayerCount(1);
		framebuffer_image_info.setViewFormatCount(array_length(format));
		framebuffer_image_info.setPViewFormats(format);
		framebuffer_image_info.setWidth(1024);
		framebuffer_image_info.setHeight(1024);
		vk::FramebufferAttachmentsCreateInfo framebuffer_attach_info;
		framebuffer_attach_info.setAttachmentImageInfoCount(1);
		framebuffer_attach_info.setPAttachmentImageInfos(&framebuffer_image_info);
		framebuffer_info.setPNext(&framebuffer_attach_info);

		m_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
	}

	// setup pipeline
	{
		// assembly
		vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
		{
			vk::PipelineInputAssemblyStateCreateInfo()
			.setPrimitiveRestartEnable(VK_FALSE)
			.setTopology(vk::PrimitiveTopology::eTriangleList),
		};

		// viewport
		vk::PipelineViewportStateCreateInfo viewport_info;
		viewport_info.setViewportCount(1);
		viewport_info.setScissorCount(1);

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
			vk::VertexInputAttributeDescription().setLocation(2).setOffset(16).setBinding(0).setFormat(vk::Format::eR8G8B8A8Unorm),
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
			.setModule(m_shader_module[sAppImGui::SHADER_FRAG_RENDER].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eFragment),
		};

		vk::PipelineDynamicStateCreateInfo dynamic_info;
		vk::DynamicState dynamic_state[] = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor,
		};
		dynamic_info.setDynamicStateCount(array_length(dynamic_state));
		dynamic_info.setPDynamicStates(dynamic_state);

		std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
		{
			vk::GraphicsPipelineCreateInfo()
			.setStageCount(array_length(shader_info))
			.setPStages(shader_info)
			.setPVertexInputState(&vertex_input_info)
			.setPInputAssemblyState(&assembly_info[0])
			.setPViewportState(&viewport_info)
			.setPRasterizationState(&rasterization_info)
			.setPMultisampleState(&sample_info)
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_RENDER].get())
			.setRenderPass(m_render_pass.get())
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info)
			.setPDynamicState(&dynamic_info),
		};
 		auto pipelines = context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
 		m_pipeline = std::move(pipelines.value[0]);

	}

}

void sAppImGui::Render(vk::CommandBuffer& cmd)
{

	ImGui::SetCurrentContext(m_imgui_context);
	for (uint32_t i = 0; i < app::g_app_instance->m_window_list.size(); i++)
	{
		auto& window = app::g_app_instance->m_window_list[i];

		auto& io = ImGui::GetIO();
		io.DisplaySize.x = window->getFrontBuffer()->m_info.extent.width;
		io.DisplaySize.y = window->getFrontBuffer()->m_info.extent.height;
		{
			auto& mouse = window->getInput().m_mouse;
			io.MousePos = ImVec2(mouse.xy.x, mouse.xy.y);
			io.MouseDown[0] = mouse.isHold(cMouse::BUTTON_LEFT);
			io.MouseDown[1] = mouse.isHold(cMouse::BUTTON_RIGHT);
			io.MouseDown[2] = mouse.isHold(cMouse::BUTTON_MIDDLE);
			io.MouseWheel = mouse.getWheel();
		}
		{
			auto& keyboard = window->getInput().m_keyboard;
			auto is_shift = keyboard.isHold(VK_SHIFT);
			io.KeyShift = is_shift;
			io.KeyCtrl = keyboard.isHold(VK_CONTROL);
			io.KeyAlt = keyboard.isHold(VK_MENU);
			io.KeyMap[ImGuiKey_Tab] = VK_TAB;
			io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
			io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
			io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
			io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
			io.KeyMap[ImGuiKey_PageUp] = VK_NAVIGATION_UP;
			io.KeyMap[ImGuiKey_PageDown] = VK_NAVIGATION_DOWN;
			io.KeyMap[ImGuiKey_Home] = VK_HOME;
			io.KeyMap[ImGuiKey_End] = VK_END;
			io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
			io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
			io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
			io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
			io.KeyMap[ImGuiKey_A] = 256 + 'A';
			io.KeyMap[ImGuiKey_C] = 256 + 'C';
 			io.KeyMap[ImGuiKey_V] = 256 + 'V';
 			io.KeyMap[ImGuiKey_X] = 256 + 'X';
 			io.KeyMap[ImGuiKey_Y] = 256 + 'Y';
 			io.KeyMap[ImGuiKey_Z] = 256 + 'Z';

			for (uint32_t i = 0; i < 256; i++)
			{
				io.KeysDown[i] = keyboard.isOn(i);
			}
			io.KeysDown[256 + 'A'] = keyboard.isHold(VK_CONTROL) && keyboard.isOn('A');
			io.KeysDown[256 + 'X'] = keyboard.isHold(VK_CONTROL) && keyboard.isOn('C');
			io.KeysDown[256 + 'V'] = keyboard.isHold(VK_CONTROL) && keyboard.isOn('V');
			io.KeysDown[256 + 'X'] = keyboard.isHold(VK_CONTROL) && keyboard.isOn('X');
			io.KeysDown[256 + 'Y'] = keyboard.isHold(VK_CONTROL) && keyboard.isOn('Y');
			io.KeysDown[256 + 'Z'] = keyboard.isHold(VK_CONTROL) && keyboard.isOn('Z');
			for (uint32_t i = 0; i < keyboard.m_char_count; i++)
			{
				io.AddInputCharacter(keyboard.m_char[i]);
			}

		}
		ImGui::NewFrame();
		{
			ImGui::PushID(window.get());
			auto cmds = window->getImgui()->getImguiCmd();
			for (auto& cmd : cmds)
			{
				cmd();
			}
			cmds.clear();
			ImGui::PopID();
		}
		ImGui::Render();
		ImDrawData* draw_data = ImGui::GetDrawData();

		{
			std::array<vk::ImageMemoryBarrier, 1> image_barrier;
			image_barrier[0].setImage(window->getFrontBuffer()->m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
//			image_barrier[0].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
	//		image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
		}

		vk::RenderPassBeginInfo begin_render_info;
		begin_render_info.setFramebuffer(window->getImgui()->m_framebuffer.get());
		begin_render_info.setRenderPass(window->getImgui()->m_render_pass.get());
//		begin_render_info.setFramebuffer(m_framebuffer.get());
//		begin_render_info.setRenderPass(m_render_pass.get());
		begin_render_info.setRenderArea(vk::Rect2D({}, vk::Extent2D(window->getFrontBuffer()->m_info.extent.width, window->getFrontBuffer()->m_info.extent.height)));
		cmd.beginRenderPass(begin_render_info, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_RENDER].get(), 0, { m_descriptor_set.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_RENDER].get(), 1, { sSystem::Order().getSystemDescriptorSet() }, { i * sSystem::Order().getSystemDescriptorStride() });

		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			auto* cmd_list = draw_data->CmdLists[n];
			auto v_num = cmd_list->VtxBuffer.size();
			auto i_num = cmd_list->IdxBuffer.size();

			auto vertex = m_context->m_staging_memory.allocateMemory<ImDrawVert>(v_num, true);
			auto index = m_context->m_staging_memory.allocateMemory<ImDrawIdx>(i_num, true);

			memcpy(vertex.getMappedPtr(), cmd_list->VtxBuffer.Data, sizeof(ImDrawVert)*v_num);
			memcpy(index.getMappedPtr(), cmd_list->IdxBuffer.Data, sizeof(ImDrawIdx)*i_num);

			cmd.bindVertexBuffers(0, { vertex.getInfo().buffer }, { vertex.getInfo().offset });
			auto i_offset = 0u;
			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback)
				{
					pcmd->UserCallback(cmd_list, pcmd);
				}
				else
				{
					vk::Viewport viewport(0, 0, 1024, 1024);
					cmd.setViewport(0, 1, &viewport);
					vk::Rect2D sissor(vk::Offset2D(pcmd->ClipRect.x, pcmd->ClipRect.y), vk::Extent2D(pcmd->ClipRect.z - pcmd->ClipRect.x, pcmd->ClipRect.w - pcmd->ClipRect.y));
					cmd.setScissor(0, 1, &sissor);

					cmd.bindIndexBuffer(index.getInfo().buffer, index.getInfo().offset + i_offset * sizeof(ImDrawIdx), sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
					cmd.drawIndexed(pcmd->ElemCount, 1, 0, 0, 0);
				}
				i_offset += pcmd->ElemCount;
			}
		}

		cmd.endRenderPass();
	}
}
