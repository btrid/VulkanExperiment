#include <013_2DGI/PM2DRenderer2.h>
#include <applib/GraphicsResource.h>
namespace pm2d_2
{

PM2DRenderer::PM2DRenderer(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target)
{
	m_context = context;
	m_render_target = render_target;

	RenderWidth = m_render_target->m_resolution.width;
	RenderHeight = m_render_target->m_resolution.height;
	RenderDepth = 1;
	FragmentBufferSize = RenderWidth * RenderHeight*RenderDepth;

	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	Info info;
	{
		btr::BufferMemoryDescriptorEx<Info> desc;
		desc.element_num = 1;
		m_fragment_info = context->m_uniform_memory.allocateMemory(desc);

		info.m_resolution = ivec2(RenderWidth, RenderHeight);
		info.m_emission_buffer_size[0] = RenderWidth * RenderHeight;
		info.m_emission_buffer_offset[0] = 0;
		for (int i = 1; i < BounceNum; i++)
		{
			info.m_emission_buffer_size[i] = info.m_emission_buffer_size[i-1]/4;
			info.m_emission_buffer_offset[i] = info.m_emission_buffer_offset[i-1]+info.m_emission_buffer_size[i-1];
		}
		cmd.updateBuffer<Info>(m_fragment_info.getInfo().buffer, m_fragment_info.getInfo().offset, info);
	}
	{
		btr::BufferMemoryDescriptorEx<Fragment> desc;
		desc.element_num = FragmentBufferSize;
		m_fragment_buffer = context->m_storage_memory.allocateMemory(desc);
	}
	{
		btr::BufferMemoryDescriptorEx<int64_t> desc;
		desc.element_num = RenderWidth * RenderHeight / 8 / 8;
		m_fragment_hierarchy = context->m_storage_memory.allocateMemory(desc);
	}
	{
		btr::BufferMemoryDescriptorEx<ivec4> desc;
		desc.element_num = BounceNum;
		m_emission_counter = context->m_storage_memory.allocateMemory(desc);
	}
	{
		btr::BufferMemoryDescriptorEx<Emission> desc;
		desc.element_num = info.m_emission_buffer_offset[BounceNum-1]+info.m_emission_buffer_size[BounceNum-1];
		m_emission_buffer = context->m_storage_memory.allocateMemory(desc);
	}
	{
		btr::BufferMemoryDescriptorEx<int32_t> desc;
		desc.element_num = info.m_emission_buffer_offset[BounceNum-1]+info.m_emission_buffer_size[BounceNum-1];
		m_emission_list = context->m_storage_memory.allocateMemory(desc);
	}
	{
		btr::BufferMemoryDescriptorEx<int32_t> desc;
		desc.element_num = info.m_emission_buffer_offset[BounceNum-1] + info.m_emission_buffer_size[BounceNum-1];
		m_emission_map = context->m_storage_memory.allocateMemory(desc);
	}

	{
		btr::BufferMemoryDescriptorEx<vec4> desc;
		desc.element_num = FragmentBufferSize;
		m_color = context->m_storage_memory.allocateMemory(desc);

	}

	{
		for (int i = 0; i < m_color_tex.max_size(); i++)
		{
			auto& tex = m_color_tex[i];
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = vk::Format::eR32Uint;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 2;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = { render_target->m_resolution.width>>i, render_target->m_resolution.height>>i, 1 };
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
			subresourceRange.layerCount = 2;
			subresourceRange.levelCount = 1;
			tex.m_subresource_range = subresourceRange;

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e2DArray;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eG;
			view_info.components.b = vk::ComponentSwizzle::eB;
			view_info.components.a = vk::ComponentSwizzle::eA;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = vk::Format::eR16G16Unorm;
			view_info.image = tex.m_image.get();
			view_info.subresourceRange = subresourceRange;
			tex.m_image_view = context->m_device->createImageViewUnique(view_info);

		}

		{
			vk::ImageMemoryBarrier to_init[BounceNum];
			for (int i = 0; i < m_color_tex.max_size(); i++)
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
		auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
		vk::DescriptorSetLayoutBinding binding[] = {
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(10),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(11),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(12),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(13),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(20),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setDescriptorCount(BounceNum)
			.setBinding(21),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(BounceNum)
			.setBinding(30),
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

		vk::DescriptorBufferInfo uniforms[] = {
			m_fragment_info.getInfo(),
		};
		vk::DescriptorBufferInfo storages[] = {
			m_fragment_buffer.getInfo(),
			m_fragment_hierarchy.getInfo(),
		};
		vk::DescriptorBufferInfo emission_storages[] = {
			m_emission_counter.getInfo(),
			m_emission_buffer.getInfo(),
			m_emission_list.getInfo(),
			m_emission_map.getInfo(),
		};
		vk::DescriptorBufferInfo output_storages[] = {
			m_color.getInfo(),
		};

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
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(array_length(uniforms))
			.setPBufferInfo(uniforms)
			.setDstBinding(0)
			.setDstSet(m_descriptor_set.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(storages))
			.setPBufferInfo(storages)
			.setDstBinding(1)
			.setDstSet(m_descriptor_set.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(emission_storages))
			.setPBufferInfo(emission_storages)
			.setDstBinding(10)
			.setDstSet(m_descriptor_set.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(output_storages))
			.setPBufferInfo(output_storages)
			.setDstBinding(20)
			.setDstSet(m_descriptor_set.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setDescriptorCount(array_length(output_images))
			.setPImageInfo(output_images)
			.setDstBinding(21)
			.setDstArrayElement(0)
			.setDstSet(m_descriptor_set.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(array_length(samplers))
			.setPImageInfo(samplers)
			.setDstBinding(30)
			.setDstArrayElement(0)
			.setDstSet(m_descriptor_set.get()),
		};
		context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
	}

	{
		const char* name[] =
		{
			"MakeFragmentHierarchy2.comp.spv",
			"LightCulling2.comp.spv",
			"PhotonMapping2.comp.spv",
			"PMRendering2.vert.spv",
			"PMRendering2.frag.spv",
		};
		static_assert(array_length(name) == array_length(m_shader), "not equal shader num");

		std::string path = btr::getResourceAppPath() + "shader\\binary\\";
		for (size_t i = 0; i < array_length(name); i++) {
			m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
		}

	}

	// pipeline layout
	{
		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_set_layout.get()
		};
		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		m_pipeline_layout[PipelineLayoutLightCulling] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		m_pipeline_layout[PipelineLayoutRendering] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);


		vk::PushConstantRange constants[] = {
			vk::PushConstantRange().setOffset(0).setSize(8).setStageFlags(vk::ShaderStageFlagBits::eCompute),
//			vk::PushConstantRange().setOffset(4).setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
		};
		pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
		pipeline_layout_info.setPPushConstantRanges(constants);
		m_pipeline_layout[PipelineLayoutPhotonMapping] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}

	// pipeline
	{
		vk::PipelineShaderStageCreateInfo shader_info[3];
		shader_info[0].setModule(m_shader[ShaderMakeFragmentHierarchy].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		shader_info[1].setModule(m_shader[ShaderLightCulling].get());
		shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[1].setPName("main");
		shader_info[2].setModule(m_shader[ShaderPhotonMapping].get());
		shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[2].setPName("main");

		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[1])
			.setLayout(m_pipeline_layout[PipelineLayoutLightCulling].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[2])
			.setLayout(m_pipeline_layout[PipelineLayoutPhotonMapping].get()),
		};
		auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[PipelineMakeFragmentHierarchy] = std::move(compute_pipeline[0]);
		m_pipeline[PipelineLayoutLightCulling] = std::move(compute_pipeline[1]);
		m_pipeline[PipelinePhotonMapping] = std::move(compute_pipeline[2]);
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


vk::CommandBuffer PM2DRenderer::execute(const std::vector<PM2DPipeline*>& pipeline)
{
	auto cmd = m_context->m_cmd_pool->allocCmdOnetime(0);
	// pre
	{
		{
			vk::BufferMemoryBarrier to_clear = m_color.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { to_clear }, {});
		}
		{
			vk::BufferMemoryBarrier to_write[] = {
				m_emission_list.makeMemoryBarrier(vk::AccessFlags(), vk::AccessFlagBits::eTransferWrite),
				m_emission_map.makeMemoryBarrier(vk::AccessFlags(), vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);
		}

		// clear
		union {
			uint32_t u;
			float f;
		}u2f;
		u2f.f = 0.f;
		cmd.fillBuffer(m_color.getInfo().buffer, m_color.getInfo().offset, m_color.getInfo().range, u2f.u);
		cmd.fillBuffer(m_fragment_buffer.getInfo().buffer, m_fragment_buffer.getInfo().offset, m_fragment_buffer.getInfo().range, 0u);
		ivec4 emissive[] = { {0,16,1,0},{ 0,1,1,0 },{ 0,1,1,0 },{ 1,1,1,0 } };
		static_assert(array_length(emissive) == BounceNum, "");
		cmd.updateBuffer(m_emission_counter.getInfo().buffer, m_emission_counter.getInfo().offset, sizeof(emissive), emissive);
		cmd.fillBuffer(m_emission_list.getInfo().buffer, m_emission_list.getInfo().offset, m_emission_list.getInfo().range, -1);
		cmd.fillBuffer(m_emission_map.getInfo().buffer, m_emission_map.getInfo().offset, m_emission_map.getInfo().range, -1);

		{

			vk::ImageMemoryBarrier to_clear[BounceNum];
			for (int i = 0; i < m_color_tex.max_size(); i++)
			{
				to_clear[i].setImage(m_color_tex[i].m_image.get());
				to_clear[i].setSubresourceRange(m_color_tex[i].m_subresource_range);
				to_clear[i].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				to_clear[i].setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_clear[i].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				to_clear[i].setNewLayout(vk::ImageLayout::eTransferDstOptimal);
			}
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {},
				0, nullptr, 0, nullptr, array_length(to_clear), to_clear);

			vk::ClearColorValue clear_value({});
			cmd.clearColorImage(m_color_tex[0].m_image.get(), vk::ImageLayout::eTransferDstOptimal, clear_value, m_color_tex[0].m_subresource_range);
			cmd.clearColorImage(m_color_tex[1].m_image.get(), vk::ImageLayout::eTransferDstOptimal, clear_value, m_color_tex[1].m_subresource_range);

		}

		{
			vk::BufferMemoryBarrier to_read[] = {
				m_emission_list.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite),
				m_emission_map.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

		}
	}
	// exe
	for (auto& p : pipeline)
	{
		p->execute(cmd);
	}

	// post
	{
		auto to_write = m_fragment_hierarchy.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_write }, {});

		// make hierarchy
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakeFragmentHierarchy].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy].get(), 0, m_descriptor_set.get(), {});
		cmd.dispatch(RenderWidth / 8, RenderHeight / 8, 1);
	}
	{
		auto to_write = m_emission_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eMemoryWrite);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_write }, {});

		// light culling
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineLayoutLightCulling].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutLightCulling].get(), 0, m_descriptor_set.get(), {});
		cmd.dispatch(20, 20, 1);

	}

	// photonmapping
	{
		{
			vk::BufferMemoryBarrier to_read[] = {
				m_fragment_hierarchy.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);
		}
		{
			vk::BufferMemoryBarrier to_read[] = {
				m_emission_list.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_emission_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

		}

		{
			vk::BufferMemoryBarrier to_read[] = {
				m_emission_counter.makeMemoryBarrier(vk::AccessFlags(), vk::AccessFlagBits::eIndirectCommandRead),
			};
			to_read[0].setOffset(m_emission_counter.getInfo().offset);
			to_read[0].setSize(sizeof(ivec4));
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);
		}

		vk::ImageMemoryBarrier to_write[BounceNum];
		for (int i = 0; i < m_color_tex.max_size(); i++)
		{
			to_write[i].setImage(m_color_tex[i].m_image.get());
			to_write[i].setSubresourceRange(m_color_tex[i].m_subresource_range);
			to_write[i].setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_write[i].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
			to_write[i].setOldLayout(vk::ImageLayout::eTransferDstOptimal);
			to_write[i].setNewLayout(vk::ImageLayout::eGeneral);
		}
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, 0, nullptr, array_length(to_write), to_write);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelinePhotonMapping].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutPhotonMapping].get(), 0, m_descriptor_set.get(), {});
		cmd.pushConstants<ivec2>(m_pipeline_layout[PipelineLayoutPhotonMapping].get(), vk::ShaderStageFlagBits::eCompute, 0, ivec2(1, 0));
		cmd.dispatchIndirect(m_emission_counter.getInfo().buffer, m_emission_counter.getInfo().offset);
	}

	// bounce 1
//	if(0)
	{
		{
			vk::BufferMemoryBarrier to_read[] = {
				m_emission_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
			};
			to_read[0].setOffset(m_emission_counter.getInfo().offset + sizeof(ivec4));
			to_read[0].setSize(sizeof(ivec4));
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);
		}
		{
			vk::BufferMemoryBarrier to_read[] = {
				m_emission_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelinePhotonMapping].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutPhotonMapping].get(), 0, m_descriptor_set.get(), {});
		cmd.pushConstants<ivec2>(m_pipeline_layout[PipelineLayoutPhotonMapping].get(), vk::ShaderStageFlagBits::eCompute, 0, ivec2(0, 1));
 		cmd.dispatchIndirect(m_emission_counter.getInfo().buffer, m_emission_counter.getInfo().offset + sizeof(ivec4));
	}

	{
		vk::BufferMemoryBarrier to_render[] = {
			m_color.makeMemoryBarrier(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderRead),
		};
		vk::ImageMemoryBarrier to_write[BounceNum];
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
			{}, 0, nullptr, array_length(to_render), to_render, array_length(to_write), to_write);

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_render_pass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_render_target->m_resolution));
		begin_render_Info.setFramebuffer(m_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		// render_targetに書く
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PipelineRendering].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutRendering].get(), 0, m_descriptor_set.get(), {});
		cmd.draw(3, 1, 0, 0);

		cmd.endRenderPass();
	}

	cmd.end();
	return cmd;
}

DebugPM2D::DebugPM2D(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DRenderer>& renderer)
{
	m_context = context;
	m_renderer = renderer;

	// リソースセットアップ
	{
		std::vector<PM2DRenderer::Fragment> map_data(renderer->RenderWidth*renderer->RenderHeight);
		{
			std::vector<ivec4> rect;
			rect.emplace_back(400, 400, 200, 400);
			rect.emplace_back(300, 200, 100, 100);
			rect.emplace_back(80, 50, 300, 30);

			for (size_t y = 0; y < renderer->RenderHeight; y++)
			{
				for (size_t x = 0; x < renderer->RenderWidth; x++)
				{
					auto& m = map_data[x + y * renderer->RenderWidth];
					m.albedo = vec3(0.f);
					for (auto& r : rect)
					{
						if (x >= r.x && y >= r.y && x <= r.x + r.z && y <= r.y + r.w) {
							m.albedo = vec3(1.f);
						}
					}
				}

			}
		}

		btr::BufferMemoryDescriptorEx<PM2DRenderer::Fragment> desc;
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
	}

	{
		const char* name[] =
		{
			"PMPointLight.vert.spv",
			"PMPointLight.frag.spv",
		};
		static_assert(array_length(name) == array_length(m_shader), "not equal shader num");

		std::string path = btr::getResourceAppPath() + "shader\\binary\\";
		for (size_t i = 0; i < array_length(name); i++) {
			m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
		}

	}

	// pipeline layout
	{
		vk::DescriptorSetLayout layouts[] = {
			renderer->m_descriptor_set_layout.get(),
		};
		vk::PushConstantRange constants[] = {
			vk::PushConstantRange().setOffset(0).setSize(8).setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment),
		};
		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
		pipeline_layout_info.setPPushConstantRanges(constants);
		m_pipeline_layout[PipelineLayoutPointLight] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}

	// レンダーパス
	{
		// sub pass
		vk::SubpassDescription subpass;
		subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
		subpass.setInputAttachmentCount(0);
		subpass.setPInputAttachments(nullptr);
		subpass.setColorAttachmentCount(0);
		subpass.setPColorAttachments(nullptr);

		vk::RenderPassCreateInfo renderpass_info;
		renderpass_info.setSubpassCount(1);
		renderpass_info.setPSubpasses(&subpass);

		m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
	}
	{
		vk::FramebufferCreateInfo framebuffer_info;
		framebuffer_info.setRenderPass(m_render_pass.get());
		framebuffer_info.setAttachmentCount(0);
		framebuffer_info.setPAttachments(nullptr);
		framebuffer_info.setWidth(renderer->m_render_target->m_info.extent.width);
		framebuffer_info.setHeight(renderer->m_render_target->m_info.extent.height);
		framebuffer_info.setLayers(1);

		m_framebuffer = context->m_device->createFramebufferUnique(framebuffer_info);
	}

	{
		vk::PipelineShaderStageCreateInfo shader_info[] =
		{
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[ShaderPointLightVS].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eVertex),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[ShaderPointLightFS].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eFragment),
		};

		// assembly
		vk::PipelineInputAssemblyStateCreateInfo assembly_info;
		assembly_info.setPrimitiveRestartEnable(VK_FALSE);
		assembly_info.setTopology(vk::PrimitiveTopology::ePointList);

		// viewport
		vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)renderer->m_render_target->m_resolution.width, (float)renderer->m_render_target->m_resolution.height, 0.f, 1.f);
		vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), renderer->m_render_target->m_resolution);
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

		vk::PipelineColorBlendStateCreateInfo blend_info;
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
			.setLayout(m_pipeline_layout[PipelineLayoutPointLight].get())
			.setRenderPass(m_render_pass.get())
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info),
		};
		auto graphics_pipeline = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
		m_pipeline[PipelinePointLight] = std::move(graphics_pipeline[0]);

	}

}

void DebugPM2D::execute(vk::CommandBuffer cmd)
{
	vk::BufferMemoryBarrier to_write[] =
	{
		m_renderer->m_emission_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferWrite),
		m_renderer->m_fragment_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, 0, nullptr, array_length(to_write), to_write, 0, nullptr);


	vk::BufferCopy copy;
	copy.setSrcOffset(m_map_data.getInfo().offset);
	copy.setDstOffset(m_renderer->m_fragment_buffer.getInfo().offset);
	copy.setSize(m_renderer->m_fragment_buffer.getInfo().range);

	cmd.copyBuffer(m_map_data.getInfo().buffer, m_renderer->m_fragment_buffer.getInfo().buffer, copy);

	{
		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_render_pass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_renderer->m_render_target->m_resolution));
		begin_render_Info.setFramebuffer(m_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PipelineLayoutPointLight].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutPointLight].get(), 0, m_renderer->m_descriptor_set.get(), {});
		cmd.pushConstants<vec2>(m_pipeline_layout[PipelineLayoutPointLight].get(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, vec2(100.f, 400.f));
		cmd.draw(1, 1, 0, 0);
// 		cmd.pushConstants<vec2>(m_pipeline_layout[PipelineLayoutPointLight].get(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, vec2(500.f, 120.f));
// 		cmd.draw(1, 1, 0, 0);

		cmd.endRenderPass();
	}
	vk::BufferMemoryBarrier to_read[] =
	{
		m_renderer->m_emission_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
		m_renderer->m_emission_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
		m_renderer->m_fragment_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

}

}