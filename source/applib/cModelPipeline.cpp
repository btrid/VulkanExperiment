
#include <applib/cModelPipeline.h>
#include <applib/sCameraManager.h>
#include <btrlib/Define.h>
#include <btrlib/Shape.h>
#include <applib/DrawHelper.h>

void cModelPipeline::setup(std::shared_ptr<btr::Context>& context)
{
	auto& device = context->m_device;

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
			.setFormat(context->m_window->getSwapchain().m_surface_format.format)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eLoad)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
			vk::AttachmentDescription()
			.setFormat(context->m_window->getSwapchain().m_depth.m_format)
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

		m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
	}

	m_framebuffer.resize(context->m_window->getSwapchain().getBackbufferNum());
	{
		std::array<vk::ImageView, 2> view;

		vk::FramebufferCreateInfo framebuffer_info;
		framebuffer_info.setRenderPass(m_render_pass.get());
		framebuffer_info.setAttachmentCount((uint32_t)view.size());
		framebuffer_info.setPAttachments(view.data());
		framebuffer_info.setWidth(context->m_window->getClientSize().x);
		framebuffer_info.setHeight(context->m_window->getClientSize().y);
		framebuffer_info.setLayers(1);

		for (size_t i = 0; i < m_framebuffer.size(); i++) {
			view[0] = context->m_window->getSwapchain().m_backbuffer[i].m_view;
			view[1] = context->m_window->getSwapchain().m_depth.m_view;
			m_framebuffer[i] = context->m_device->createFramebufferUnique(framebuffer_info);
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
	size.setWidth(context->m_window->getClientSize().x);
	size.setHeight(context->m_window->getClientSize().y);
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
			m_render_pipeline = std::move(device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info)[0]);
		}
	}
}

vk::CommandBuffer cModelPipeline::draw(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	// draw
	for (auto& render : m_model)
	{
		render->m_animation->execute(context, cmd);
	}

	vk::RenderPassBeginInfo begin_render_Info;
	begin_render_Info.setRenderPass(m_render_pass.get());
	begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(640, 480)));
	begin_render_Info.setFramebuffer(m_framebuffer[context->getGPUFrame()].get());
	cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eSecondaryCommandBuffers);

	for (auto& render : m_model)
	{
		render->m_render->draw(context, cmd);
	}
	cmd.endRenderPass();

	cmd.end();
	return cmd;
}
std::shared_ptr<Model> cModelPipeline::createRender(std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource)
{
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	auto model = std::make_shared<Model>();
	model->m_material = std::make_shared<DefaultMaterialModule>(context, resource);
	model->m_animation = std::make_shared<DefaultAnimationModule>(context, resource);
	auto render = std::make_shared<ModelRender>();


	{
		auto& device = context->m_device;
		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_descriptor_set_layout[cModelPipeline::DESCRIPTOR_SET_LAYOUT_MODEL].get()
			};

			vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
			descriptor_set_alloc_info.setDescriptorPool(m_model_descriptor_pool.get());
			descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
			descriptor_set_alloc_info.setPSetLayouts(layouts);
			render->m_descriptor_set_model = std::move(device->allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);

			std::vector<vk::DescriptorBufferInfo> storages = {
				model->m_animation->getBoneBuffer().getBufferInfo(),
				model->m_material->getMaterialIndexBuffer().getBufferInfo(),
				model->m_material->getMaterialBuffer().getBufferInfo(),
			};

			std::vector<vk::DescriptorImageInfo> color_images(cModelPipeline::DESCRIPTOR_TEXTURE_NUM, vk::DescriptorImageInfo(DrawHelper::Order().getWhiteTexture().m_sampler.get(), DrawHelper::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
			for (size_t i = 0; i < resource->m_mesh.size(); i++)
			{
				auto& material = resource->m_material[resource->m_mesh[i].m_material_index];
				color_images[i] = vk::DescriptorImageInfo(material.mDiffuseTex.getSampler(), material.mDiffuseTex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
			}
			std::vector<vk::WriteDescriptorSet> drawWriteDescriptorSets =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount((uint32_t)storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(2)
				.setDstSet(render->m_descriptor_set_model.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount((uint32_t)color_images.size())
				.setPImageInfo(color_images.data())
				.setDstBinding(5)
				.setDstSet(render->m_descriptor_set_model.get()),
			};
			device->updateDescriptorSets(drawWriteDescriptorSets, {});
		}
	}


	{
		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandBufferCount = sGlobal::FRAME_MAX;
		cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
		cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
		render->m_draw_cmd = context->m_device->allocateCommandBuffersUnique(cmd_buffer_info);

		for (size_t i = 0; i < render->m_draw_cmd.size(); i++)
		{
			auto& cmd = render->m_draw_cmd[i].get();
			vk::CommandBufferBeginInfo begin_info;
			begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue);
			vk::CommandBufferInheritanceInfo inheritance_info;
			inheritance_info.setFramebuffer(m_framebuffer[i].get());
			inheritance_info.setRenderPass(m_render_pass.get());
			begin_info.pInheritanceInfo = &inheritance_info;

			cmd.begin(begin_info);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_render_pipeline.get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[cModelPipeline::PIPELINE_LAYOUT_RENDER].get(), 0, render->m_descriptor_set_model.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[cModelPipeline::PIPELINE_LAYOUT_RENDER].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
			cmd.bindVertexBuffers(0, { resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().buffer }, { resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().offset });
			cmd.bindIndexBuffer(resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().buffer, resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().offset, resource->m_mesh_resource.mIndexType);
			cmd.drawIndexedIndirect(resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().buffer, resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().offset, resource->m_mesh_resource.mIndirectCount, sizeof(cModel::Mesh));

			cmd.end();
		}
	}
	model->m_render = std::move(render);
	m_model.push_back(model);
	return model;

}
