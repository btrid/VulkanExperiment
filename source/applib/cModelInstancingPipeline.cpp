
#include <applib/cModelInstancingPipeline.h>
#include <applib/cModelInstancingRender.h>
#include <btrlib/Define.h>
#include <btrlib/Shape.h>
#include <btrlib/cModel.h>
#include <applib/sCameraManager.h>

void cModelInstancingPipeline::addModel(const std::shared_ptr<btr::Context>& context, ModelInstancingRender* model)
{
	m_model.emplace_back(model);
	m_model.back()->setup(context, *this);
}

void cModelInstancingPipeline::setup(const std::shared_ptr<btr::Context>& context)
{
	auto& device = context->m_device;
	m_render_pass = std::make_shared<RenderPassModule>(context);

	m_model_descriptor = std::make_shared<ModelDescriptorModule>(context);
	m_animation_descriptor = std::make_shared<InstancingAnimationDescriptorModule>(context);

	m_light_pipeline = std::make_shared<cFowardPlusPipeline>();
	m_light_pipeline->setup(context);

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


	{
		{
			vk::DescriptorSetLayout layouts[] = {
				m_model_descriptor->getLayout(),
				m_animation_descriptor->getLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info = vk::PipelineLayoutCreateInfo()
				.setSetLayoutCount(array_length(layouts))
				.setPSetLayouts(layouts);
			m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE] = device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				m_model_descriptor->getLayout(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				m_light_pipeline->getDescriptorSetLayout(cFowardPlusPipeline::DESCRIPTOR_SET_LAYOUT_LIGHT),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);

			m_pipeline_layout[PIPELINE_LAYOUT_RENDER] = device->createPipelineLayoutUnique(pipeline_layout_info);
		}

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

	auto p = device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
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

			// vertexinput
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
				.setRenderPass(m_render_pass->getRenderPass())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_graphics_pipeline = std::move(device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info)[0]);
		}

	}
}

vk::CommandBuffer cModelInstancingPipeline::execute(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	for (auto& render : m_model)
	{
		render->execute(context, cmd);
	}
	cmd.end();
	return cmd;
}

vk::CommandBuffer cModelInstancingPipeline::draw(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	vk::RenderPassBeginInfo render_begin_info;
	render_begin_info.setRenderPass(m_render_pass->getRenderPass());
	render_begin_info.setFramebuffer(m_render_pass->getFramebuffer(context->getGPUFrame()));
	render_begin_info.setRenderArea(vk::Rect2D({}, context->m_window->getClientSize<vk::Extent2D>()));

	cmd.beginRenderPass(render_begin_info, vk::SubpassContents::eSecondaryCommandBuffers);

	// draw
	for (auto& render : m_model)
	{
		render->draw(context, cmd);
	}

	cmd.endRenderPass();
	cmd.end();
	return cmd;
}
