
#include <applib/cModelPipeline.h>
#include <applib/cModelRender.h>
#include <applib/cModelRenderPrivate.h>
#include <applib/sCameraManager.h>
#include <btrlib/Define.h>
#include <btrlib/Shape.h>
#include <btrlib/cModel.h>

void cModelPipeline::setup(std::shared_ptr<btr::Context>& loader)
{
	auto& device = loader->m_device;

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
			.setAttachmentCount((uint32_t)attach_description.size())
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

	// setup shader
	{
		struct
		{
			const char* name;
			vk::ShaderStageFlagBits stage;
		}shader_info[] =
		{
			{ "ModelRender.vert.spv",vk::ShaderStageFlagBits::eVertex },
			{ "ModelRender.frag.spv",vk::ShaderStageFlagBits::eFragment },
		};
		static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourceLibPath() + "shader\\binary\\";
		for (size_t i = 0; i < SHADER_NUM; i++) {
			m_shader_list[i] = std::move(loadShaderUnique(device.getHandle(), path + shader_info[i].name));
			m_stage_info[i].setStage(shader_info[i].stage);
			m_stage_info[i].setModule(m_shader_list[i].get());
			m_stage_info[i].setPName("main");
		}
	}


	// Create compute pipeline
	std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
	bindings[DESCRIPTOR_SET_LAYOUT_MODEL] = 
	{
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(0),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(1),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(2),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(3),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(4),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorCount(DESCRIPTOR_TEXTURE_NUM)
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setBinding(5),
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
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA)
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
				descriptor_pool_size.emplace_back(buffer.descriptorType, buffer.descriptorCount * 10);
			}
		}
		vk::DescriptorPoolCreateInfo descriptor_pool_info;
		descriptor_pool_info.maxSets = 20;
		descriptor_pool_info.poolSizeCount = descriptor_pool_size.size();
		descriptor_pool_info.pPoolSizes = descriptor_pool_size.data();
		m_model_descriptor_pool = device->createDescriptorPoolUnique(descriptor_pool_info);
	}


	vk::Extent3D size;
	size.setWidth(loader->m_window->getClientSize().x);
	size.setHeight(loader->m_window->getClientSize().y);
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

			auto vertex_input_binding = cModel::GetVertexInputBinding();
			auto vertex_input_attribute = cModel::GetVertexInputAttribute();
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
			m_render_pipeline = std::move(device->createGraphicsPipelinesUnique(loader->m_cache.get(), graphics_pipeline_info)[0]);
		}

	}
}

void cModelPipeline::addModel(std::shared_ptr<btr::Context>& executer, const std::shared_ptr<cModelRender>& model)
{
	m_model.emplace_back(model);
	m_model.back()->getPrivate()->setup(executer, *this);
}

vk::CommandBuffer cModelPipeline::draw(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	// draw
	for (auto& render : m_model)
	{
		render->getPrivate()->execute(context, cmd);
	}

	vk::RenderPassBeginInfo begin_render_Info;
	begin_render_Info.setRenderPass(m_render_pass.get());
	begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(640, 480)));
	begin_render_Info.setFramebuffer(m_framebuffer[context->getGPUFrame()].get());
	cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eSecondaryCommandBuffers);

	for (auto& render : m_model)
	{
		render->getPrivate()->draw(context, cmd);
	}
	cmd.endRenderPass();

	cmd.end();
	return cmd;
}
