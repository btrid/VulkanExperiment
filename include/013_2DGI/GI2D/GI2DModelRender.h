#pragma once

#include <013_2DGI/GI2D/GI2DContext.h>
#include <applib/AppModel/AppModel.h>

struct GI2DModelRender
{
	enum Shader
	{
		ShaderRenderingVS,
		ShaderRenderingFS,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutRendering,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineRendering,
		PipelineNum,
	};

	GI2DModelRender(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<AppModelContext>& appmodel_context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;
		m_appmodel_context = appmodel_context;
		// レンダーパス
		{
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_render_pass = context->m_device.createRenderPassUnique(renderpass_info);

			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass.get());
			framebuffer_info.setWidth(gi2d_context->m_desc.Resolution.x);
			framebuffer_info.setHeight(gi2d_context->m_desc.Resolution.y);
			framebuffer_info.setLayers(1);

			m_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);

		}
		{
			const char* name[] =
			{
				"GI2DModel_MakeFragment.vert.spv",
				"GI2DModel_MakeFragment.frag.spv",
			};
			static_assert(array_length(name) == ShaderNum, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device, path + name[i]);
			}
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				appmodel_context->getLayout(AppModelContext::DescriptorLayout_Model),
				appmodel_context->getLayout(AppModelContext::DescriptorLayout_Render),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayoutRendering] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
		}


		// pipeline
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList);

			vk::Extent3D size = gi2d_context->m_desc.Resolution.x;
			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)gi2d_context->m_desc.Resolution.x, (float)gi2d_context->m_desc.Resolution.y, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(gi2d_context->m_desc.Resolution.x, gi2d_context->m_desc.Resolution.y));

			vk::PipelineViewportStateCreateInfo viewport_info;
			viewport_info.setViewportCount(1);
			viewport_info.setPViewports(&viewport);
			viewport_info.setScissorCount(1);
			viewport_info.setPScissors(&scissor);

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
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			// ブレンド
			vk::PipelineColorBlendStateCreateInfo blend_info;

			// vertexinput
// 			auto vertex_input_binding = cModel::GetVertexInputBinding();
// 			auto vertex_input_attribute = cModel::GetVertexInputAttribute();
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
// 			vertex_input_info.setVertexBindingDescriptionCount((uint32_t)vertex_input_binding.size());
// 			vertex_input_info.setPVertexBindingDescriptions(vertex_input_binding.data());
// 			vertex_input_info.setVertexAttributeDescriptionCount((uint32_t)vertex_input_attribute.size());
// 			vertex_input_info.setPVertexAttributeDescriptions(vertex_input_attribute.data());

			std::array<vk::PipelineShaderStageCreateInfo, 2> shader_info;
			shader_info[0].setModule(m_shader[ShaderRenderingVS].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eVertex);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[ShaderRenderingFS].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eFragment);
			shader_info[1].setPName("main");

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount((uint32_t)shader_info.size())
				.setPStages(shader_info.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewport_info)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PipelineLayoutRendering].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_pipeline[PipelineRendering] = context->m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info[0]).value;
		}

	}
	vk::UniqueCommandBuffer createCmd(const std::shared_ptr<AppModel>& render)
	{
		// recode draw command
		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandBufferCount = 1;
		cmd_buffer_info.commandPool = m_context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
		cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
		auto cmd = std::move(m_context->m_device.allocateCommandBuffersUnique(cmd_buffer_info)[0]);
		{

			vk::CommandBufferBeginInfo begin_info;
			begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue);
			vk::CommandBufferInheritanceInfo inheritance_info;
			inheritance_info.setFramebuffer(m_framebuffer.get());
			inheritance_info.setRenderPass(m_render_pass.get());
			begin_info.pInheritanceInfo = &inheritance_info;

			cmd->begin(begin_info);

			cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PipelineLayoutRendering].get());
			std::vector<vk::DescriptorSet> sets = {
				m_gi2d_context->getDescriptorSet(),
				render->getDescriptorSet(AppModel::DescriptorSet_Model),
				render->getDescriptorSet(AppModel::DescriptorSet_Render),
			};
			cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutRendering].get(), 0, sets, {});

			render->m_render.draw(cmd.get());

			cmd->end();
		}
		return cmd;

	}

	void dispatchCmd(vk::CommandBuffer cmd, std::vector<vk::CommandBuffer>& cmds)
	{

		vk::DebugMarkerMarkerInfoEXT marker;
		marker.setPMarkerName("PM_Make_Fragment");

		vk::RenderPassBeginInfo render_begin_info;
		render_begin_info.setRenderPass(m_render_pass.get());
		render_begin_info.setFramebuffer(m_framebuffer.get());
		render_begin_info.setRenderArea(vk::Rect2D({}, { (uint32_t)m_gi2d_context->m_desc.Resolution.x, (uint32_t)m_gi2d_context->m_desc.Resolution.y}));

		cmd.beginRenderPass(render_begin_info, vk::SubpassContents::eSecondaryCommandBuffers);
		cmd.executeCommands(cmds.size(), cmds.data());
		cmd.endRenderPass();

	}

	void execute(vk::CommandBuffer cmd)
	{

	}
	std::array<vk::UniqueShaderModule, ShaderNum> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;
	std::shared_ptr<AppModelContext> m_appmodel_context;
};
