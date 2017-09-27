
#include <applib/cModelInstancingPipeline.h>
#include <applib/cModelInstancingRender.h>
#include <btrlib/Define.h>
#include <btrlib/Shape.h>
#include <btrlib/cModel.h>
#include <applib/sCameraManager.h>

void cModelInstancingPipeline::setup(std::shared_ptr<btr::Loader>& loader, cModelInstancingRenderer& renderer)
{
	auto& gpu = sGlobal::Order().getGPU(0);
	auto& device = gpu.getDevice();

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
				.setFormat(loader->m_window->getSwapchain().m_depth.m_format)
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
	// setup shader
	{
		struct
		{
			const char* name;
			vk::ShaderStageFlagBits stage;
		}shader_info[] =
		{
			{ "001_Clear.comp.spv",			vk::ShaderStageFlagBits::eCompute },
			{ "002_AnimationUpdate.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ "003_MotionUpdate.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ "004_NodeTransform.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ "005_CameraCulling.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ "006_BoneTransform.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ "Render.vert.spv",vk::ShaderStageFlagBits::eVertex },
			{ "RenderFowardPlus.frag.spv",vk::ShaderStageFlagBits::eFragment },
		};
		static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourceLibPath() + "shader\\binary\\";
		for (size_t i = 0; i < SHADER_NUM; i++) {
			m_shader_list[i] = loadShaderUnique(device.getHandle(), path + shader_info[i].name);
			m_stage_info[i].setStage(shader_info[i].stage);
			m_stage_info[i].setModule(m_shader_list[i].get());
			m_stage_info[i].setPName("main");
		}
	}

	// Create compute pipeline
	std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_NUM);
	bindings[DESCRIPTOR_SET_LAYOUT_ANIMATION] = {
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(0),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(1),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(2),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(3),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(4),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(5),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(6),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(7),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(8),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setDescriptorCount(1)
		.setBinding(32),
	};
	bindings[DESCRIPTOR_SET_LAYOUT_MODEL] =
	{
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(0),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(1),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(2),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(3),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(4),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorCount(16)
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setBinding(5),
	};
	bindings[DESCRIPTOR_SET_LAYOUT_LIGHT] =
	{
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eFragment)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setBinding(0),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eFragment)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setBinding(1),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eFragment)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setBinding(2),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eFragment)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setBinding(3),
	};
	for (u32 i = 0; i < bindings.size(); i++)
	{
		vk::DescriptorSetLayoutCreateInfo descriptor_layout_info = vk::DescriptorSetLayoutCreateInfo()
			.setBindingCount(bindings[i].size())
			.setPBindings(bindings[i].data());
		m_descriptor_set_layout[i] = device->createDescriptorSetLayoutUnique(descriptor_layout_info);
	}

	{
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_MODEL].get(),
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_ANIMATION].get(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info = vk::PipelineLayoutCreateInfo()
				.setSetLayoutCount(array_length(layouts))
				.setPSetLayouts(layouts);
			m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE] = device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_MODEL].get(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_LIGHT].get(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);

			m_pipeline_layout[PIPELINE_LAYOUT_RENDER] = device->createPipelineLayoutUnique(pipeline_layout_info);
		}

	}

	// DescriptorPool
	{
		std::vector<vk::DescriptorPoolSize> descriptor_pool_size;
		for (auto& binding : bindings)
		{
			for (auto& buffer : binding)
			{
				descriptor_pool_size.emplace_back(buffer.descriptorType, buffer.descriptorCount*10);
			}
		}
		vk::DescriptorPoolCreateInfo descriptor_pool_info;
		descriptor_pool_info.maxSets = 20;
		descriptor_pool_info.poolSizeCount = descriptor_pool_size.size();
		descriptor_pool_info.pPoolSizes = descriptor_pool_size.data();

		m_descriptor_pool = device->createDescriptorPoolUnique(descriptor_pool_info);
	}


	// Create pipeline
	std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info = {
		vk::ComputePipelineCreateInfo()
		.setStage(m_stage_info[SHADER_COMPUTE_CLEAR])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE].get()),
		vk::ComputePipelineCreateInfo()
		.setStage(m_stage_info[SHADER_COMPUTE_ANIMATION_UPDATE])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE].get()),
		vk::ComputePipelineCreateInfo()
		.setStage(m_stage_info[SHADER_COMPUTE_MOTION_UPDATE])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE].get()),
		vk::ComputePipelineCreateInfo()
		.setStage(m_stage_info[SHADER_COMPUTE_NODE_TRANSFORM])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE].get()),
		vk::ComputePipelineCreateInfo()
		.setStage(m_stage_info[SHADER_COMPUTE_CULLING])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE].get()),
		vk::ComputePipelineCreateInfo()
		.setStage(m_stage_info[SHADER_COMPUTE_BONE_TRANSFORM])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE].get()),
	};

	auto p = device->createComputePipelinesUnique(loader->m_cache.get(), compute_pipeline_info);
	for (size_t i = 0; i < p.size(); i++) {
		m_pipeline[i] = std::move(p[i]);
	}

	vk::Extent3D size;
	size.setWidth(640);
	size.setHeight(480);
	size.setDepth(1);
	// pipeline
	{
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height))
			};
			vk::PipelineViewportStateCreateInfo viewport_info;
			viewport_info.setViewportCount(1);
			viewport_info.setPViewports(&viewport);
			viewport_info.setScissorCount((uint32_t)scissor.size());
			viewport_info.setPScissors(scissor.data());

			// ラスタライズ
			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eBack);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setLineWidth(1.f);
			// サンプリング
			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			// デプスステンシル
			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			// ブレンド
			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			// todo
			std::vector<vk::VertexInputBindingDescription> vertex_input_binding =
			{
				vk::VertexInputBindingDescription()
				.setBinding(0)
				.setInputRate(vk::VertexInputRate::eVertex)
				.setStride(sizeof(cModel::Vertex))
			};

			std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute =
			{
				// pos
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(0)
				.setFormat(vk::Format::eR32G32B32Sfloat)
				.setOffset(0),
				// normal
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(1)
				.setFormat(vk::Format::eR32G32B32Sfloat)
				.setOffset(12),
				// texcoord
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(2)
				.setFormat(vk::Format::eR8G8B8A8Snorm)
				.setOffset(24),
				// boneID
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(3)
				.setFormat(vk::Format::eR8G8B8A8Uint)
				.setOffset(28),
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(4)
				.setFormat(vk::Format::eR8G8B8A8Unorm)
				.setOffset(32),
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexBindingDescriptionCount((uint32_t)vertex_input_binding.size());
			vertex_input_info.setPVertexBindingDescriptions(vertex_input_binding.data());
			vertex_input_info.setVertexAttributeDescriptionCount((uint32_t)vertex_input_attribute.size());
			vertex_input_info.setPVertexAttributeDescriptions(vertex_input_attribute.data());

			std::array<vk::PipelineShaderStageCreateInfo, 2> stage_info =
			{
				m_stage_info[SHADER_RENDER_VERT],
				m_stage_info[SHADER_RENDER_FRAG],
			};
			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount((uint32_t)stage_info.size())
				.setPStages(stage_info.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewport_info)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_RENDER].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_graphics_pipeline = std::move(device->createGraphicsPipelinesUnique(loader->m_cache.get(), graphics_pipeline_info)[0]);
		}

	}

	{
		// ライトのDescriptorの設定
		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_set_layout[cModelInstancingPipeline::DESCRIPTOR_SET_LAYOUT_LIGHT].get(),
		};
		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = loader->m_descriptor_pool.get();
		alloc_info.descriptorSetCount = array_length(layouts);
		alloc_info.pSetLayouts = layouts;
		m_descriptor_set_light = std::move(device->allocateDescriptorSetsUnique(alloc_info)[0]);

		std::vector<vk::DescriptorBufferInfo> uniformBufferInfo = {
			renderer.getLight().getLightInfoBufferInfo(),
		};
		std::vector<vk::DescriptorBufferInfo> storageBufferInfo = {
			renderer.getLight().getLightLLHeadBufferInfo(),
			renderer.getLight().getLightLLBufferInfo(),
			renderer.getLight().getLightBufferInfo(),
		};
		std::vector<vk::WriteDescriptorSet> write_descriptor_set =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(uniformBufferInfo.size())
			.setPBufferInfo(uniformBufferInfo.data())
			.setDstBinding(0)
			.setDstSet(m_descriptor_set_light.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(storageBufferInfo.size())
			.setPBufferInfo(storageBufferInfo.data())
			.setDstBinding(1)
			.setDstSet(m_descriptor_set_light.get()),
		};
		device->updateDescriptorSets(write_descriptor_set, {});
	}
}
