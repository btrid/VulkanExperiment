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
	}

	{
// 		{
// 			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
// 			vk::DescriptorSetLayoutBinding binding[] = {
// 				vk::DescriptorSetLayoutBinding()
// 				.setStageFlags(stage)
// 				.setDescriptorType(vk::DescriptorType::eStorageImage)
// 				.setDescriptorCount(PM2DContext::_BounceNum)
// 				.setBinding(0),
// 				vk::DescriptorSetLayoutBinding()
// 				.setStageFlags(stage)
// 				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
// 				.setDescriptorCount(PM2DContext::_BounceNum)
// 				.setBinding(1),
// 				vk::DescriptorSetLayoutBinding()
// 				.setStageFlags(stage)
// 				.setDescriptorType(vk::DescriptorType::eStorageImage)
// 				.setDescriptorCount(PM2DContext::_BounceNum)
// 				.setBinding(2),
// 			};
// 			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
// 			desc_layout_info.setBindingCount(array_length(binding));
// 			desc_layout_info.setPBindings(binding);
// 			m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
// 		}
// 		{
// 			vk::DescriptorSetLayout layouts[] = {
// 				m_descriptor_set_layout.get(),
// 			};
// 			vk::DescriptorSetAllocateInfo desc_info;
// 			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
// 			desc_info.setDescriptorSetCount(array_length(layouts));
// 			desc_info.setPSetLayouts(layouts);
// 			m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);
// 
// 		}

	}

	{
		const char* name[] =
		{
			"LightCulling.comp.spv",
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
// 		m_pipeline_layout[PipelineLayoutRendering] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
// 		m_pipeline_layout[PipelineLayoutPhotonCollect] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
// 		m_pipeline_layout[PipelineLayoutDebugRenderFragmentMap] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);


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

	// pipeline
	{
		std::array<vk::PipelineShaderStageCreateInfo, 1> shader_info;
		shader_info[0].setModule(m_shader[ShaderLightCulling].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayoutLightCulling].get()),
		};
		auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[PipelineLayoutLightCulling] = std::move(compute_pipeline[0]);
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
// 			.setModule(m_shader[ShaderRenderingVS].get())
 			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eVertex),
			vk::PipelineShaderStageCreateInfo()
// 			.setModule(m_shader[ShaderRenderingFS].get())
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

// 		std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
// 		{
// 			vk::GraphicsPipelineCreateInfo()
// 			.setStageCount(array_length(shader_info))
// 			.setPStages(shader_info)
// 			.setPVertexInputState(&vertex_input_info)
// 			.setPInputAssemblyState(&assembly_info)
// 			.setPViewportState(&viewportInfo)
// 			.setPRasterizationState(&rasterization_info)
// 			.setPMultisampleState(&sample_info)
// 			.setLayout(m_pipeline_layout[PipelineLayoutRendering].get())
// 			.setRenderPass(m_render_pass.get())
// 			.setPDepthStencilState(&depth_stencil_info)
// 			.setPColorBlendState(&blend_info),
// 		};
// 		auto graphics_pipeline = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
// 		m_pipeline[PipelineRendering] = std::move(graphics_pipeline[0]);

	}
}


void SV2DRenderer::execute(vk::CommandBuffer cmd)
{
	for (int i = 0; i < 1; i++)
	{
		ivec2 constant_param[] = {

			ivec2(0, 2),
			ivec2(2, -1),
		};
		// light culling
		{
			// clear emission link
			{
				vk::BufferMemoryBarrier to_clear[] = {
					m_sv2d_context->b_emission_tile_linklist_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
					m_sv2d_context->b_emission_tile_linkhead.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {},
					0, nullptr, array_length(to_clear), to_clear, 0, nullptr);

				cmd.fillBuffer(m_sv2d_context->b_emission_tile_linklist_counter.getInfo().buffer, m_sv2d_context->b_emission_tile_linklist_counter.getInfo().offset, m_sv2d_context->b_emission_tile_linklist_counter.getInfo().range, 0u);
				cmd.fillBuffer(m_sv2d_context->b_emission_tile_linkhead.getInfo().buffer, m_sv2d_context->b_emission_tile_linkhead.getInfo().offset, m_sv2d_context->b_emission_tile_linkhead.getInfo().range, -1);

			}

			vk::BufferMemoryBarrier to_write[] = {
				m_sv2d_context->b_emission_buffer.makeMemoryBarrier(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderRead),
				m_sv2d_context->b_emission_tile_linklist_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				m_sv2d_context->b_emission_tile_linkhead.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderWrite),
				m_sv2d_context->b_emission_tile_linklist.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineLayoutLightCulling].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutLightCulling].get(), 0, m_sv2d_context->getDescriptorSet(), {});
			cmd.pushConstants<ivec2>(m_pipeline_layout[PipelineLayoutLightCulling].get(), vk::ShaderStageFlagBits::eCompute, 0, constant_param[i]);
			cmd.dispatch(m_sv2d_context->m_sv2d_info.m_emission_tile_num.x, m_sv2d_context->m_sv2d_info.m_emission_tile_num.y, 1);
		}

	}

}


}
