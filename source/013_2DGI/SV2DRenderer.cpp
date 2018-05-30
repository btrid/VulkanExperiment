#include <013_2DGI/SV2D/SV2DRenderer.h>
#include <applib/GraphicsResource.h>

namespace sv2d
{

SV2DRenderer::SV2DRenderer(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target, const std::shared_ptr<SV2DContext>& sv2d_context)
{
	m_context = context;
	m_sv2d_context = sv2d_context;
	m_render_target = render_target;

	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	{
		{
			auto& tex = m_output;
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
			image_info.extent = { 512, 512, 1 };
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
			vk::ImageMemoryBarrier to_init[1];
			{
				to_init[0].image = m_output.m_image.get();
				to_init[0].subresourceRange = m_output.m_subresource_range;
				to_init[0].oldLayout = vk::ImageLayout::eUndefined;
				to_init[0].newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_init[0].dstAccessMask = vk::AccessFlagBits::eShaderRead;
			}
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(),
				0, nullptr, 0, nullptr, array_length(to_init), to_init);
		}

		{
			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
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
			vk::DescriptorImageInfo samplers[] = {
				vk::DescriptorImageInfo()
				.setImageView(m_output.m_image_view.get())
				.setSampler(sGraphicsResource::Order().getSampler(sGraphicsResource::BASIC_SAMPLER_LINER))
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
			};

			vk::WriteDescriptorSet write[] = {
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(array_length(samplers))
				.setPImageInfo(samplers)
				.setDstBinding(0)
				.setDstArrayElement(0)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

	}

	// shader 
	{
		const char* name[] =
		{
			"LightCulling.comp.spv",
			"MakeShadowVolume.comp.spv",
			"DrawShadowVolume.vert.spv",
			"DrawShadowVolume.frag.spv",
			"BlitShadowVolume.vert.spv",
			"BlitShadowVolume.frag.spv",
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
			sv2d_context->getDescriptorSetLayout(),
		};
		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayoutMakeShadowVolume] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		m_pipeline_layout[PipelineLayoutDrawShadowVolume] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);


		{
			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setOffset(0).setSize(8).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayoutLightCulling] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
//			m_pipeline_layout[PipelineLayoutPhotonMapping] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		vk::PushConstantRange constants[] = {
			vk::PushConstantRange().setOffset(0).setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
		};
		pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
		pipeline_layout_info.setPPushConstantRanges(constants);
	}
	{
		vk::DescriptorSetLayout layouts[] = {
			sv2d_context->getDescriptorSetLayout(),
			m_descriptor_set_layout.get(),
		};
		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayoutBlitShadowVolume] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}

	// compute pipeline
	{
		std::array<vk::PipelineShaderStageCreateInfo, 2> shader_info;
		shader_info[0].setModule(m_shader[ShaderLightCulling].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		shader_info[1].setModule(m_shader[ShaderMakeShadowVolume].get());
		shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[1].setPName("main");
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayoutLightCulling].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[1])
			.setLayout(m_pipeline_layout[PipelineLayoutMakeShadowVolume].get()),
		};
		auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[PipelineLightCulling] = std::move(compute_pipeline[0]);
		m_pipeline[PipelineMakeShadowVolume] = std::move(compute_pipeline[1]);
	}

	{
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
				.setFormat(m_output.m_image_info.format)
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

			m_output_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
		}
		{
			vk::ImageView view[] = {
				m_output.m_image_view.get(),
			};
			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_output_render_pass.get());
			framebuffer_info.setAttachmentCount(array_length(view));
			framebuffer_info.setPAttachments(view);
			framebuffer_info.setWidth(m_output.m_image_info.extent.width);
			framebuffer_info.setHeight(m_output.m_image_info.extent.height);
			framebuffer_info.setLayers(1);

			m_output_framebuffer = context->m_device->createFramebufferUnique(framebuffer_info);
		}

		{
			vk::PipelineShaderStageCreateInfo shader_info[] =
			{
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[ShaderDrawShadowVolumeVS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[ShaderDrawShadowVolumeFS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)m_output.m_image_info.extent.width, (float)m_output.m_image_info.extent.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(m_output.m_image_info.extent.width, m_output.m_image_info.extent.height));
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

			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setSrcColorBlendFactor(vk::BlendFactor::eOne)
				.setDstColorBlendFactor(vk::BlendFactor::eDstAlpha)
				.setColorBlendOp(vk::BlendOp::eAdd)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount((uint32_t)blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vk::VertexInputBindingDescription vertex_desc[] =
			{
				vk::VertexInputBindingDescription()
				.setBinding(0)
				.setStride(8)
				.setInputRate(vk::VertexInputRate::eVertex)
			};
			vk::VertexInputAttributeDescription vertex_attr[] = {
				vk::VertexInputAttributeDescription().setBinding(0).setLocation(0).setFormat(vk::Format::eR32G32Sfloat).setOffset(0)
			};
			vertex_input_info.setVertexBindingDescriptionCount(array_length(vertex_desc));
			vertex_input_info.setPVertexBindingDescriptions(vertex_desc);
			vertex_input_info.setVertexAttributeDescriptionCount(array_length(vertex_attr));
			vertex_input_info.setPVertexAttributeDescriptions(vertex_attr);

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
				.setLayout(m_pipeline_layout[PipelineLayoutDrawShadowVolume].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto graphics_pipeline = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[PipelineDrawShadowVolume] = std::move(graphics_pipeline[0]);

		}
	}

	{
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
				.setModule(m_shader[ShaderDrawShadowVolumeVS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[ShaderDrawShadowVolumeFS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

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
			depth_stencil_info.setDepthTestEnable(VK_FALSE);
			depth_stencil_info.setDepthWriteEnable(VK_FALSE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setSrcColorBlendFactor(vk::BlendFactor::eOne)
				.setDstColorBlendFactor(vk::BlendFactor::eDstAlpha)
				.setColorBlendOp(vk::BlendOp::eAdd)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount((uint32_t)blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vk::VertexInputBindingDescription vertex_desc[] =
			{
				vk::VertexInputBindingDescription()
				.setBinding(0)
				.setStride(8)
				.setInputRate(vk::VertexInputRate::eVertex)
			};
			vk::VertexInputAttributeDescription vertex_attr[] = {
				vk::VertexInputAttributeDescription().setBinding(0).setLocation(0).setFormat(vk::Format::eR32G32Sfloat).setOffset(0)
			};
			vertex_input_info.setVertexBindingDescriptionCount(array_length(vertex_desc));
			vertex_input_info.setPVertexBindingDescriptions(vertex_desc);
			vertex_input_info.setVertexAttributeDescriptionCount(array_length(vertex_attr));
			vertex_input_info.setPVertexAttributeDescriptions(vertex_attr);

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
				.setLayout(m_pipeline_layout[PipelineLayoutBlitShadowVolume].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto graphics_pipeline = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[PipelineBlitShadowVolume] = std::move(graphics_pipeline[0]);

		}

	}
}


void SV2DRenderer::execute(vk::CommandBuffer cmd)
{
	// 準備
	{
		cmd.updateBuffer<vk::DrawIndirectCommand>(m_sv2d_context->b_shadow_volume_counter.getInfo().buffer, m_sv2d_context->b_shadow_volume_counter.getInfo().offset, vk::DrawIndirectCommand{ 0, 1, 0, 0 });
	}

	// クリア
	{
		vk::ImageMemoryBarrier to_clear[] = {
			vk::ImageMemoryBarrier()
			.setImage(m_output.m_image.get())
			.setSubresourceRange(m_output.m_subresource_range)
			.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
			.setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
			.setNewLayout(vk::ImageLayout::eTransferDstOptimal)
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer, {},
			0, nullptr, 0, nullptr, array_length(to_clear), to_clear);

		cmd.clearColorImage(m_output.m_image.get(), vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(), { m_output.m_subresource_range });

	}

	// make shadow volume
	{
		vk::BufferMemoryBarrier to_write[] = {
			m_sv2d_context->b_emission_buffer.makeMemoryBarrier(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderRead),
			m_sv2d_context->b_emission_tile_linklist_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			m_sv2d_context->b_emission_tile_linkhead.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderWrite),
			m_sv2d_context->b_emission_tile_linklist.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderWrite),
			m_sv2d_context->b_shadow_volume_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			m_sv2d_context->b_shadow_volume.makeMemoryBarrier(vk::AccessFlagBits::eVertexAttributeRead, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_write), to_write, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakeShadowVolume].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakeShadowVolume].get(), 0, m_sv2d_context->getDescriptorSet(), {});
		cmd.dispatch(m_sv2d_context->RenderWidth /32, m_sv2d_context->RenderHeight/32, 1);
	}

	// draw shadow volume
	{
		{
			vk::BufferMemoryBarrier to_read[] = {
				m_sv2d_context->b_shadow_volume_counter.makeMemoryBarrier(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eIndirectCommandRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);
		}

		vk::RenderPassBeginInfo render_begin_info;
		render_begin_info.setRenderPass(m_output_render_pass.get());
		render_begin_info.setFramebuffer(m_output_framebuffer.get());
		render_begin_info.setRenderArea(vk::Rect2D({}, { (uint32_t)m_output.m_image_info.extent.width, (uint32_t)m_output.m_image_info.extent.height }));

		cmd.beginRenderPass(render_begin_info, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PipelineDrawShadowVolume].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutDrawShadowVolume].get(), 0, m_sv2d_context->m_descriptor_set.get(), {});
		cmd.bindVertexBuffers(0, { m_sv2d_context->b_shadow_volume.getInfo().buffer }, { m_sv2d_context->b_shadow_volume.getInfo().offset });
		cmd.drawIndirect(m_sv2d_context->b_shadow_volume_counter.getInfo().buffer, m_sv2d_context->b_shadow_volume_counter.getInfo().offset, 1, sizeof(vk::DrawIndirectCommand));
		cmd.endRenderPass();
	}

	// render_targetにコピー
	{
		// バリア
		{
			vk::ImageMemoryBarrier to_read[] = {
				vk::ImageMemoryBarrier()
				.setImage(m_output.m_image.get())
				.setSubresourceRange(m_output.m_subresource_range)
				.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader, {},
				0, nullptr, 0, nullptr, array_length(to_read), to_read);

		}
		vk::RenderPassBeginInfo render_begin_info;
		render_begin_info.setRenderPass(m_render_pass.get());
		render_begin_info.setFramebuffer(m_framebuffer.get());
		render_begin_info.setRenderArea(vk::Rect2D({}, { (uint32_t)m_sv2d_context->RenderWidth, (uint32_t)m_sv2d_context->RenderHeight }));

		cmd.beginRenderPass(render_begin_info, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PipelineBlitShadowVolume].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutBlitShadowVolume].get(), 0, m_sv2d_context->m_descriptor_set.get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutBlitShadowVolume].get(), 1, m_descriptor_set.get(), {});
		cmd.draw(3, 1, 0, 0);
		cmd.endRenderPass();

	}
}


}
