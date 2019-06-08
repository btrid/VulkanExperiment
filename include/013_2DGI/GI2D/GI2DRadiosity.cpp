#pragma once
#include <memory>
#include <013_2DGI/GI2D/GI2DRadiosity.h>

GI2DRadiosity::GI2DRadiosity(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context, const std::shared_ptr<RenderTarget>& render_target)
{
	m_context = context;
	m_gi2d_context = gi2d_context;
	m_render_target = render_target;

	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	{
		GI2DRadiosityInfo info;
		info.ray_num_max = Ray_All_Num;
		info.ray_frame_max = Ray_Frame_Num;
		info.frame_max = Frame;
		u_radiosity_info = m_context->m_uniform_memory.allocateMemory<GI2DRadiosityInfo>({1,{} });
		cmd.updateBuffer<GI2DRadiosityInfo>(u_radiosity_info.getInfo().buffer, u_radiosity_info.getInfo().offset, info);
		vk::BufferMemoryBarrier to_read[] = {
			u_radiosity_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);


		uint32_t size = m_gi2d_context->RenderWidth * m_gi2d_context->RenderHeight;
		b_radiance = m_context->m_storage_memory.allocateMemory<uint32_t>({ size * 4,{} });
		b_segment_counter = m_context->m_storage_memory.allocateMemory<ivec4>({ 1,{} });
		b_ray = m_context->m_storage_memory.allocateMemory<D2Ray>({ Ray_All_Num,{} });
		b_segment = m_context->m_storage_memory.allocateMemory<D2Segment>({ 1,{} });
		b_segment_ex = m_context->m_storage_memory.allocateMemory<u16vec4>({ Segment_Num,{} });
		b_vertex_array_counter = m_context->m_storage_memory.allocateMemory<vk::DrawIndirectCommand>({ 1,{} });
		b_vertex_array_index = m_context->m_storage_memory.allocateMemory<uint>({ size,{} });
		b_vertex_array = m_context->m_storage_memory.allocateMemory<u16vec2>({ Ray_Frame_Num* 1024,{} });
	}

	{

		{
			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, stage),
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
				u_radiosity_info.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] = {
				b_radiance.getInfo(),
				b_ray.getInfo(),
				b_segment.getInfo(),
				b_segment_counter.getInfo(),
				b_segment_ex.getInfo(),
				b_vertex_array_counter.getInfo(),
				b_vertex_array_index.getInfo(),
				b_vertex_array.getInfo(),
			};

			vk::WriteDescriptorSet write[] = 
			{
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
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

	}

	{
		const char* name[] =
		{
			"Radiosity.comp.spv",
			"Radiosity_Render.vert.spv",
			"Radiosity_Render.frag.spv",

			"Radiosity_RayGenerate.comp.spv",
			"Radiosity_MakeVertex.comp.spv",
			"Radiosity_RayMarch.comp.spv",
			"Radiosity_RayHit.comp.spv",
			"Radiosity_RayBounce.comp.spv",

			"Radiosity_Radiate.vert.spv",
			"Radiosity_Radiate.geom.spv",
			"Radiosity_Radiate.frag.spv",

		};
		static_assert(array_length(name) == Shader_Num, "not equal shader num");

		std::string path = btr::getResourceShaderPath();
		for (size_t i = 0; i < array_length(name); i++) {
			m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
		}
	}

	// pipeline layout
	{
		vk::DescriptorSetLayout layouts[] = {
			gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
			m_descriptor_set_layout.get(),
		};

		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayout_Radiosity] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}

	// pipeline
	{
		std::array<vk::PipelineShaderStageCreateInfo, 10> shader_info;
		shader_info[0].setModule(m_shader[Shader_Radiosity].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		shader_info[1].setModule(m_shader[Shader_RayGenerate].get());
		shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[1].setPName("main");
		shader_info[2].setModule(m_shader[Shader_MakeHitpoint].get());
		shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[2].setPName("main");
		shader_info[3].setModule(m_shader[Shader_RayMarch].get());
		shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[3].setPName("main");
		shader_info[4].setModule(m_shader[Shader_RayHit].get());
		shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[4].setPName("main");
		shader_info[5].setModule(m_shader[Shader_RayBounce].get());
		shader_info[5].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[5].setPName("main");
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[1])
			.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[2])
			.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[3])
			.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[4])
			.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[5])
			.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get()),
		};
		auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[Pipeline_Radiosity] = std::move(compute_pipeline[0]);
		m_pipeline[Pipeline_RayGenerate] = std::move(compute_pipeline[1]);
		m_pipeline[Pipeline_MakeHitpoint] = std::move(compute_pipeline[2]);
		m_pipeline[Pipeline_RayMarch] = std::move(compute_pipeline[3]);
		m_pipeline[Pipeline_RayHit] = std::move(compute_pipeline[4]);
		m_pipeline[Pipeline_RayBounce] = std::move(compute_pipeline[5]);
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
		subpass.setColorAttachmentCount(std::size(color_ref));
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
		renderpass_info.setAttachmentCount(std::size(attach_description));
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
		framebuffer_info.setAttachmentCount(std::size(view));
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
			.setModule(m_shader[Shader_RenderingVS].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eVertex),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[Shader_RenderingFS].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eFragment),
		};
		vk::PipelineShaderStageCreateInfo shader_info2[] =
		{
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[Shader_RadiateVS].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eVertex),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[Shader_RadiateGS].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eGeometry),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[Shader_RadiateFS].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eFragment),
		};

		// assembly
		vk::PipelineInputAssemblyStateCreateInfo assembly_info;
		assembly_info.setPrimitiveRestartEnable(VK_FALSE);
		assembly_info.setTopology(vk::PrimitiveTopology::eTriangleStrip);
		vk::PipelineInputAssemblyStateCreateInfo assembly_info2;
		assembly_info2.setPrimitiveRestartEnable(VK_FALSE);
		assembly_info2.setTopology(vk::PrimitiveTopology::ePointList);

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

		vk::PipelineColorBlendAttachmentState blend_state;
		blend_state.setBlendEnable(VK_FALSE);
		blend_state.setColorWriteMask(vk::ColorComponentFlagBits::eR
			| vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB
			| vk::ColorComponentFlagBits::eA);
		vk::PipelineColorBlendStateCreateInfo blend_info;
		blend_info.setAttachmentCount(1);
		blend_info.setPAttachments(&blend_state);

		vk::PipelineColorBlendAttachmentState blend_state2;
		blend_state2.setBlendEnable(VK_TRUE);
		blend_state2.setColorBlendOp(vk::BlendOp::eAdd);
		blend_state2.setSrcColorBlendFactor(vk::BlendFactor::eOne);
		blend_state2.setDstColorBlendFactor(vk::BlendFactor::eOne);
		blend_state2.setColorWriteMask(vk::ColorComponentFlagBits::eR
			| vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB
			| vk::ColorComponentFlagBits::eA);
		vk::PipelineColorBlendStateCreateInfo blend_info2;
		blend_info2.setAttachmentCount(1);
		blend_info2.setPAttachments(&blend_state2);

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
			.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get())
			.setRenderPass(m_render_pass.get())
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info),
			vk::GraphicsPipelineCreateInfo()
			.setStageCount(array_length(shader_info2))
			.setPStages(shader_info2)
			.setPVertexInputState(&vertex_input_info)
			.setPInputAssemblyState(&assembly_info2)
			.setPViewportState(&viewportInfo)
			.setPRasterizationState(&rasterization_info)
			.setPMultisampleState(&sample_info)
			.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get())
			.setRenderPass(m_render_pass.get())
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info2),
		};
		auto graphics_pipeline = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
		m_pipeline[Pipeline_Output] = std::move(graphics_pipeline[0]);
		m_pipeline[Pipeline_Radiosity2] = std::move(graphics_pipeline[1]);

	}

}
void GI2DRadiosity::executeGenerateRay(const vk::CommandBuffer& cmd)
{
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RayGenerate].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 0, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 1, m_descriptor_set.get(), {});
		auto num = app::calcDipatchGroups(uvec3(1024, Ray_Direction_Num, Frame), uvec3(128, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

	{
		vk::BufferMemoryBarrier to_read[] = {
			b_ray.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}


}
void GI2DRadiosity::executeRadiosity(const vk::CommandBuffer& cmd)
{
	DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

	// データクリア
	{
		vk::BufferMemoryBarrier to_write[] = {
			b_radiance.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			b_segment_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			b_vertex_array_counter.makeMemoryBarrier(vk::AccessFlagBits::eIndirectCommandRead, vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect|vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, array_length(to_write), to_write, 0, nullptr);

		cmd.updateBuffer<ivec4>(b_segment_counter.getInfo().buffer, b_segment_counter.getInfo().offset, ivec4(0, 1, 1, 0));
		int range = b_radiance.getInfo().range / Frame;
		cmd.fillBuffer(b_radiance.getInfo().buffer, b_radiance.getInfo().offset + range * m_gi2d_context->m_gi2d_scene.m_frame, range, 0);
		cmd.updateBuffer<vk::DrawIndirectCommand>(b_vertex_array_counter.getInfo().buffer, b_vertex_array_counter.getInfo().offset, { 1, 0, 0, 0});

	}

	// レイのヒットポイント生成
	_label.insert("GI2DRadiosity::executeMakeVertex");
	{
		vk::BufferMemoryBarrier to_read[] = {
			b_vertex_array_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			b_vertex_array.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer| vk::PipelineStageFlagBits::eGeometryShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeHitpoint].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 0, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 1, m_descriptor_set.get(), {});
		auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth, m_gi2d_context->RenderHeight, 1), uvec3(32, 32, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

	// レイの範囲の生成
	_label.insert("GI2DRadiosity::executeRayMarch");
	{
		vk::BufferMemoryBarrier to_read[] = {
			m_gi2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			m_gi2d_context->b_light.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			b_vertex_array_index.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			b_vertex_array.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			b_segment_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RayMarch].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 0, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 1, m_descriptor_set.get(), {});
		auto num = app::calcDipatchGroups(uvec3(Ray_Frame_Num, 1, 1), uvec3(128, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

	{
		vk::BufferMemoryBarrier to_read[] =
		{
			b_segment_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eIndirectCommandRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect| vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

	}

	// bounce
	{

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 0, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 1, m_descriptor_set.get(), {});
		for (int i = 0; i<0; i++)
		{

			_label.insert("GI2DRadiosity::executeHit");
			{
				vk::BufferMemoryBarrier to_read[] = 
				{
					b_segment.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
					m_gi2d_context->b_light.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);


				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RayHit].get());
				cmd.dispatchIndirect(b_segment_counter.getInfo().buffer, b_segment_counter.getInfo().offset);
			}

			_label.insert("GI2DRadiosity::executeBounce");
			{
				vk::BufferMemoryBarrier to_read[] = 
				{
					b_segment.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite),
					m_gi2d_context->b_light.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RayBounce].get());
				cmd.dispatchIndirect(b_segment_counter.getInfo().buffer, b_segment_counter.getInfo().offset);
			}

		}

	}
	
	// radiance
	_label.insert("GI2DRadiosity::executeCollectRadiate");
// 	{
// 
// 		vk::BufferMemoryBarrier to_read[] = {
// 			b_radiance.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
// 			b_segment.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
// 		};
// 		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, std::size(to_read), to_read, 0, nullptr);
// 
// 		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Radiosity].get());
// 		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 0, m_gi2d_context->getDescriptorSet(), {});
// 		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 1, m_descriptor_set.get(), {});
// 		cmd.dispatchIndirect(b_segment_counter.getInfo().buffer, b_segment_counter.getInfo().offset);
// 	}
}


void GI2DRadiosity::executeRendering(const vk::CommandBuffer& cmd)
{

	DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

	// render_targetに書く
	{
		vk::BufferMemoryBarrier to_read[] = {
			m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			b_radiance.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			b_vertex_array.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			b_vertex_array_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
		};

		vk::ImageMemoryBarrier image_barrier;
		image_barrier.setImage(m_render_target->m_image);
		image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		image_barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		image_barrier.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect| vk::PipelineStageFlagBits::eGeometryShader | vk::PipelineStageFlagBits::eFragmentShader| vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, { array_size(to_read), to_read }, {image_barrier});

	}

	vk::RenderPassBeginInfo begin_render_Info;
	begin_render_Info.setRenderPass(m_render_pass.get());
	begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_render_target->m_resolution));
	begin_render_Info.setFramebuffer(m_framebuffer.get());
	cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_Radiosity].get(), 0, m_gi2d_context->getDescriptorSet(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_Radiosity].get(), 1, m_descriptor_set.get(), {});
// 	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Output].get());
// 	cmd.draw(3, 1, 0, 0);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Radiosity2].get());
	cmd.drawIndirect(b_vertex_array_counter.getInfo().buffer, b_vertex_array_counter.getInfo().offset, 1, sizeof(vk::DrawIndirectCommand));

	cmd.endRenderPass();
}
