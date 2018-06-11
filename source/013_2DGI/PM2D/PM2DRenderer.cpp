#include <013_2DGI/PM2D/PM2DRenderer.h>
#include <applib/GraphicsResource.h>

namespace pm2d
{

PM2DRenderer::PM2DRenderer(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target, const std::shared_ptr<PM2DContext>& pm2d_context)
{
	m_context = context;
	m_pm2d_context = pm2d_context;
	m_render_target = render_target;

	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	{
		for (int i = 0; i < m_pm2d_context->BounceNum; i++)
		{
			auto& tex = m_color_tex[i];
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = vk::Format::eR16G16B16A16Sfloat;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 2;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
//			image_info.extent = { render_target->m_resolution.width >> i, render_target->m_resolution.height >> i, 1 };
			image_info.extent = { (uint32_t)pm2d_context->RenderWidth >> i, (uint32_t)pm2d_context->RenderHeight >> i, 1 };
			image_info.flags = vk::ImageCreateFlagBits::eMutableFormat;

			tex.m_image = context->m_device->createImageUnique(image_info);
			tex.m_image_info = image_info;

			vk::MemoryRequirements memory_request = context->m_device->getImageMemoryRequirements(tex.m_image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(context->m_gpu.getHandle(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			tex.m_memory = context->m_device->allocateMemoryUnique(memory_alloc_info);
			context->m_device->bindImageMemory(tex.m_image.get(), tex.m_memory.get(), 0);

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;
			tex.m_subresource_range = subresourceRange;

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e2D;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eG;
			view_info.components.b = vk::ComponentSwizzle::eB;
			view_info.components.a = vk::ComponentSwizzle::eA;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = vk::Format::eR16G16B16A16Sfloat;
			view_info.image = tex.m_image.get();
			view_info.subresourceRange = subresourceRange;
			tex.m_image_view = context->m_device->createImageViewUnique(view_info);

		}

		{
//			std::vector<vk::ImageMemoryBarrier> to_init(m_pm2d_context->BounceNum);
			vk::ImageMemoryBarrier to_init[PM2DContext::_BounceNum];
			for (int i = 0; i < array_length(to_init); i++)
			{
				to_init[i].image = m_color_tex[i].m_image.get();
				to_init[i].subresourceRange = m_color_tex[i].m_subresource_range;
				to_init[i].oldLayout = vk::ImageLayout::eUndefined;
				to_init[i].newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_init[i].dstAccessMask = vk::AccessFlagBits::eShaderRead;
			}
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(),
				0, nullptr, 0, nullptr, array_length(to_init), to_init);
		}
	}

	{
		{
			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(PM2DContext::_BounceNum)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(PM2DContext::_BounceNum)
				.setBinding(1),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(PM2DContext::_BounceNum)
				.setBinding(2),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

		}
		{
			vk::DescriptorImageInfo output_images[] = {
				vk::DescriptorImageInfo().setImageView(m_color_tex[0].m_image_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
				vk::DescriptorImageInfo().setImageView(m_color_tex[1].m_image_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
				vk::DescriptorImageInfo().setImageView(m_color_tex[2].m_image_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
				vk::DescriptorImageInfo().setImageView(m_color_tex[3].m_image_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
			};
			vk::DescriptorImageInfo samplers[] = {
				vk::DescriptorImageInfo()
				.setImageView(m_color_tex[0].m_image_view.get())
				.setSampler(sGraphicsResource::Order().getSampler(sGraphicsResource::BASIC_SAMPLER_LINER))
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
				vk::DescriptorImageInfo()
				.setImageView(m_color_tex[1].m_image_view.get())
				.setSampler(sGraphicsResource::Order().getSampler(sGraphicsResource::BASIC_SAMPLER_LINER))
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
				vk::DescriptorImageInfo()
				.setImageView(m_color_tex[2].m_image_view.get())
				.setSampler(sGraphicsResource::Order().getSampler(sGraphicsResource::BASIC_SAMPLER_LINER))
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
				vk::DescriptorImageInfo()
				.setImageView(m_color_tex[3].m_image_view.get())
				.setSampler(sGraphicsResource::Order().getSampler(sGraphicsResource::BASIC_SAMPLER_LINER))
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
			};

			vk::WriteDescriptorSet write[] = {
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(array_length(output_images))
				.setPImageInfo(output_images)
				.setDstBinding(0)
				.setDstArrayElement(0)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(array_length(samplers))
				.setPImageInfo(samplers)
				.setDstBinding(1)
				.setDstArrayElement(0)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(array_length(output_images))
				.setPImageInfo(output_images)
				.setDstBinding(2)
				.setDstArrayElement(0)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

	}

	{
		const char* name[] =
		{
			"LightCulling.comp.spv",
			"PhotonMapping.comp.spv",
			"PhotonCollect.comp.spv",
			"PMRendering.vert.spv",
			"PMRendering.frag.spv",
			"DebugFragmentMap.comp.spv",
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
			pm2d_context->getDescriptorSetLayout(),
			m_descriptor_set_layout.get(),
		};
		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayoutRendering] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		m_pipeline_layout[PipelineLayoutPhotonCollect] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		m_pipeline_layout[PipelineLayoutDebugRenderFragmentMap] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);


		{
			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setOffset(0).setSize(8).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayoutLightCulling] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			m_pipeline_layout[PipelineLayoutPhotonMapping] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		vk::PushConstantRange constants[] = {
			vk::PushConstantRange().setOffset(0).setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
		};
		pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
		pipeline_layout_info.setPPushConstantRanges(constants);
	}

	// pipeline
	{
		std::array<vk::PipelineShaderStageCreateInfo, 4> shader_info;
		shader_info[0].setModule(m_shader[ShaderLightCulling].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		shader_info[1].setModule(m_shader[ShaderPhotonMapping].get());
		shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[1].setPName("main");
		shader_info[2].setModule(m_shader[ShaderPhotonCollect].get());
		shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[2].setPName("main");
		shader_info[3].setModule(m_shader[ShaderDebugRenderFragmentMap].get());
		shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[3].setPName("main");
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayoutLightCulling].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[1])
			.setLayout(m_pipeline_layout[PipelineLayoutPhotonMapping].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[2])
			.setLayout(m_pipeline_layout[PipelineLayoutPhotonCollect].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[3])
			.setLayout(m_pipeline_layout[PipelineLayoutDebugRenderFragmentMap].get()),
		};
		auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[PipelineLayoutLightCulling] = std::move(compute_pipeline[0]);
		m_pipeline[PipelinePhotonMapping] = std::move(compute_pipeline[1]);
		m_pipeline[PipelinePhotonCollect] = std::move(compute_pipeline[2]);
		m_pipeline[PipelineDebugRenderFragmentMap] = std::move(compute_pipeline[3]);
	}

	// レンダーパス
	{
		vk::AttachmentReference color_ref[] = {
			vk::AttachmentReference()
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setAttachment(0)
		};
		// sub pass
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
			.setFormat(render_target->m_info.format)
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

		m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
	}
	{
		vk::ImageView view[] = {
			render_target->m_view,
		};
		vk::FramebufferCreateInfo framebuffer_info;
		framebuffer_info.setRenderPass(m_render_pass.get());
		framebuffer_info.setAttachmentCount(array_length(view));
		framebuffer_info.setPAttachments(view);
		framebuffer_info.setWidth(render_target->m_info.extent.width);
		framebuffer_info.setHeight(render_target->m_info.extent.height);
		framebuffer_info.setLayers(1);

		m_framebuffer = context->m_device->createFramebufferUnique(framebuffer_info);
	}

	{
		vk::PipelineShaderStageCreateInfo shader_info[] =
		{
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[ShaderRenderingVS].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eVertex),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[ShaderRenderingFS].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eFragment),
		};
	
		// assembly
		vk::PipelineInputAssemblyStateCreateInfo assembly_info;
		assembly_info.setPrimitiveRestartEnable(VK_FALSE);
		assembly_info.setTopology(vk::PrimitiveTopology::eTriangleStrip);

		// viewport
		vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)render_target->m_resolution.width, (float)render_target->m_resolution.height, 0.f, 1.f);
		vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), render_target->m_resolution);
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
		depth_stencil_info.setDepthTestEnable(VK_TRUE);
		depth_stencil_info.setDepthWriteEnable(VK_FALSE);
		depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
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
			.setStageCount(array_length(shader_info))
			.setPStages(shader_info)
			.setPVertexInputState(&vertex_input_info)
			.setPInputAssemblyState(&assembly_info)
			.setPViewportState(&viewportInfo)
			.setPRasterizationState(&rasterization_info)
			.setPMultisampleState(&sample_info)
			.setLayout(m_pipeline_layout[PipelineLayoutRendering].get())
			.setRenderPass(m_render_pass.get())
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info),
		};
		auto graphics_pipeline = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
		m_pipeline[PipelineRendering] = std::move(graphics_pipeline[0]);

	}
}


void PM2DRenderer::execute(vk::CommandBuffer cmd)
{

// 	// clear いらない？
// 	{
// 		vk::ImageMemoryBarrier to_clear[PM2DContext::_BounceNum];
// 		for (int i = 0; i < m_color_tex.max_size(); i++)
// 		{
// 			to_clear[i].setImage(m_color_tex[i].m_image.get());
// 			to_clear[i].setSubresourceRange(m_color_tex[i].m_subresource_range);
// 			to_clear[i].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
// 			to_clear[i].setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
// 			to_clear[i].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
// 			to_clear[i].setNewLayout(vk::ImageLayout::eTransferDstOptimal);
// 		}
// 		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {},
// 			0, nullptr, 0, nullptr, array_length(to_clear), to_clear);
// 
// 		vk::ClearColorValue clear_value({});
// 		cmd.clearColorImage(m_color_tex[0].m_image.get(), vk::ImageLayout::eTransferDstOptimal, clear_value, m_color_tex[0].m_subresource_range);
// 		cmd.clearColorImage(m_color_tex[1].m_image.get(), vk::ImageLayout::eTransferDstOptimal, clear_value, m_color_tex[1].m_subresource_range);
// 		cmd.clearColorImage(m_color_tex[2].m_image.get(), vk::ImageLayout::eTransferDstOptimal, clear_value, m_color_tex[2].m_subresource_range);
// 		cmd.clearColorImage(m_color_tex[3].m_image.get(), vk::ImageLayout::eTransferDstOptimal, clear_value, m_color_tex[3].m_subresource_range);
// 	}
// 	// クリア待ち
// 	{
// 		vk::ImageMemoryBarrier to_write[PM2DContext::_BounceNum];
// 		for (int i = 0; i < m_color_tex.max_size(); i++)
// 		{
// 			to_write[i].setImage(m_color_tex[i].m_image.get());
// 			to_write[i].setSubresourceRange(m_color_tex[i].m_subresource_range);
// 			to_write[i].setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
// 			to_write[i].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
// 			to_write[i].setOldLayout(vk::ImageLayout::eTransferDstOptimal);
// 			to_write[i].setNewLayout(vk::ImageLayout::eGeneral);
// 		}
// 		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
// 			0, nullptr, 0, nullptr, array_length(to_write), to_write);
// 	}

//#define debug_render_fragment_map
#if defined(debug_render_fragment_map)
	DebugRnederFragmentMap(cmd);
//	cmd.end();
//	return cmd;
	return;
#endif
	for (int i = 0; i < 1; i++)
	{
		static int s_frame;
		s_frame++;
		ivec2 constant_param[] = {
			ivec2(0, s_frame),
			ivec2(2, -1),
		};
		// light culling
		{
			{
				vk::BufferMemoryBarrier to_read[] = {
					m_pm2d_context->b_emission_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);
				

			}
			// clear emission link
			{
				vk::BufferMemoryBarrier to_clear[] = {
					m_pm2d_context->b_emission_tile_linklist_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {},
					0, nullptr, array_length(to_clear), to_clear, 0, nullptr);

				cmd.fillBuffer(m_pm2d_context->b_emission_tile_linklist_counter.getInfo().buffer, m_pm2d_context->b_emission_tile_linklist_counter.getInfo().offset, m_pm2d_context->b_emission_tile_linklist_counter.getInfo().range, 0u);

			}

			vk::BufferMemoryBarrier to_write[] = {
				m_pm2d_context->b_emission_buffer.makeMemoryBarrier(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderRead),
				m_pm2d_context->b_emission_tile_linklist_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				m_pm2d_context->b_emission_tile_linkhead.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderWrite),
				m_pm2d_context->b_emission_tile_linklist.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineLayoutLightCulling].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutLightCulling].get(), 0, m_pm2d_context->getDescriptorSet(), {});
			cmd.pushConstants<ivec2>(m_pipeline_layout[PipelineLayoutLightCulling].get(), vk::ShaderStageFlagBits::eCompute, 0, constant_param[i]);
			cmd.dispatch(m_pm2d_context->m_pm2d_info.m_emission_tile_num.x, m_pm2d_context->m_pm2d_info.m_emission_tile_num.y, 1);
		}
		// photonmapping
		{
			vk::BufferMemoryBarrier to_read[] = {
				m_pm2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_pm2d_context->b_emission_tile_linklist_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_pm2d_context->b_emission_tile_linkhead.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_pm2d_context->b_emission_tile_linklist.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelinePhotonMapping].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutPhotonMapping].get(), 0, m_pm2d_context->getDescriptorSet(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutPhotonMapping].get(), 1, m_descriptor_set.get(), {});

			cmd.pushConstants<ivec2>(m_pipeline_layout[PipelineLayoutPhotonMapping].get(), vk::ShaderStageFlagBits::eCompute, 0, constant_param[i]);
#define march_64
#if defined(march_64)
			cmd.dispatch((m_pm2d_context->m_pm2d_info.m_emission_tile_num.x / 8) >> constant_param[i].x, (m_pm2d_context->m_pm2d_info.m_emission_tile_num.y / 8) >> constant_param[i].x, 1);
#else
			cmd.dispatch(m_pm2d_context->m_pm2d_info.m_emission_tile_num.x >> constant_param[i].x, m_pm2d_context->m_pm2d_info.m_emission_tile_num.y >> constant_param[i].x, 1);
#endif
		}
	}

	// photon collect
	{
		vk::BufferMemoryBarrier to_read[] = {
			m_pm2d_context->b_emission_reached.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelinePhotonCollect].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutPhotonCollect].get(), 0, m_pm2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutPhotonCollect].get(), 1, m_descriptor_set.get(), {});

		cmd.dispatch(m_pm2d_context->RenderWidth / 32, m_pm2d_context->RenderHeight / 32, 1);
	}

	// render_targetに書く
	{
		{
			vk::ImageMemoryBarrier to_read[PM2DContext::_BounceNum];
			for (int i = 0; i < m_color_tex.max_size(); i++)
			{
				to_read[i].setImage(m_color_tex[i].m_image.get());
				to_read[i].setSubresourceRange(m_color_tex[i].m_subresource_range);
				to_read[i].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				to_read[i].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				to_read[i].setOldLayout(vk::ImageLayout::eGeneral);
				to_read[i].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
			}
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
				{}, 0, nullptr, 0, nullptr, array_length(to_read), to_read);
		}

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_render_pass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_render_target->m_resolution));
		begin_render_Info.setFramebuffer(m_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PipelineRendering].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutRendering].get(), 0, m_pm2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutRendering].get(), 1, m_descriptor_set.get(), {});
		cmd.draw(3, 1, 0, 0);

		cmd.endRenderPass();
	}
}

void PM2DRenderer::DebugRnederFragmentMap(vk::CommandBuffer &cmd)
{
	// fragment_hierarchyのテスト
	{
		vk::BufferMemoryBarrier to_read[] = {
			m_pm2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			m_pm2d_context->b_emission_tile_linklist_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			m_pm2d_context->b_emission_tile_linkhead.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			m_pm2d_context->b_emission_tile_linklist.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineDebugRenderFragmentMap].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutDebugRenderFragmentMap].get(), 0, m_pm2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutDebugRenderFragmentMap].get(), 1, m_descriptor_set.get(), {});
		cmd.dispatch(m_pm2d_context->m_pm2d_info.m_resolution.x / 32, m_pm2d_context->m_pm2d_info.m_resolution.y / 32, 1);
	}

	// render_targetに書く
	{
		{
			vk::ImageMemoryBarrier to_write[PM2DContext::_BounceNum];
			for (int i = 0; i < m_color_tex.max_size(); i++)
			{
				to_write[i].setImage(m_color_tex[i].m_image.get());
				to_write[i].setSubresourceRange(m_color_tex[i].m_subresource_range);
				to_write[i].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				to_write[i].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				to_write[i].setOldLayout(vk::ImageLayout::eGeneral);
				to_write[i].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
			}
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
				{}, 0, nullptr, 0, nullptr, array_length(to_write), to_write);

		}

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_render_pass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_render_target->m_resolution));
		begin_render_Info.setFramebuffer(m_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PipelineRendering].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutRendering].get(), 0, m_pm2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutRendering].get(), 1, m_descriptor_set.get(), {});
		cmd.draw(3, 1, 0, 0);

		cmd.endRenderPass();
	}
}

}
