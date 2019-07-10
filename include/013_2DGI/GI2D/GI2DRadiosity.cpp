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
		uint32_t size = m_gi2d_context->RenderWidth * m_gi2d_context->RenderHeight;
		u_radiosity_info = m_context->m_uniform_memory.allocateMemory<GI2DRadiosityInfo>({ 1,{} });
		b_vertex_counter = m_context->m_storage_memory.allocateMemory<VertexCounter>({ Frame_Num,{} });
		b_vertex_index = m_context->m_storage_memory.allocateMemory<uint>({ Frame_Num*size,{} });
		b_vertex = m_context->m_storage_memory.allocateMemory<RadiosityVertex>({ Vertex_Num*Frame_Num,{} });
		b_edge = m_context->m_storage_memory.allocateMemory<uint64_t>({ size / 64,{} });


		m_info._unused = 0;
		m_info.vertex_max = Vertex_Num;
		m_info.frame_max = Frame_Num;
		m_info.frame = 0;
		cmd.updateBuffer<GI2DRadiosityInfo>(u_radiosity_info.getInfo().buffer, u_radiosity_info.getInfo().offset, m_info);
		vk::BufferMemoryBarrier to_read[] = {
			u_radiosity_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);


		cmd.fillBuffer(b_vertex_counter.getInfo().buffer, b_vertex_counter.getInfo().offset, b_vertex_counter.getInfo().range, 0);
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
				b_vertex_counter.getInfo(),
				b_vertex_index.getInfo(),
				b_vertex.getInfo(),
				b_edge.getInfo(),
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
			"Radiosity_MakeVertex.comp.spv",
			"Radiosity_RayMarch.comp.spv",
			"Radiosity_RayBounce.comp.spv",

			"Radiosity_Render.vert.spv",
			"Radiosity_Render.geom.spv",
			"Radiosity_Render.frag.spv",

		};
		static_assert(array_length(name) == Shader_Num, "not equal shader num");

		std::string path = btr::getResourceShaderPath();
		for (size_t i = 0; i < array_length(name); i++) {
			m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
		}
	}

	// pipeline layout
	{
		{
			vk::DescriptorSetLayout layouts[] = {
				gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				m_descriptor_set_layout.get(),
			};
			vk::PushConstantRange ranges[] = {
				vk::PushConstantRange().setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
			pipeline_layout_info.setPPushConstantRanges(ranges);
			m_pipeline_layout[PipelineLayout_Radiosity] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

		}

		{
			vk::DescriptorSetLayout layouts[] = {
				gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				m_descriptor_set_layout.get(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayout_Rendering] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

	}

	// pipeline
	{
		std::array<vk::PipelineShaderStageCreateInfo, 10> shader_info;
		shader_info[0].setModule(m_shader[Shader_MakeHitpoint].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		shader_info[1].setModule(m_shader[Shader_RayMarch].get());
		shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[1].setPName("main");
		shader_info[2].setModule(m_shader[Shader_RayBounce].get());
		shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[2].setPName("main");
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
		};
		auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[Pipeline_MakeHitpoint] = std::move(compute_pipeline[0]);
		m_pipeline[Pipeline_RayMarch] = std::move(compute_pipeline[1]);
		m_pipeline[Pipeline_RayBounce] = std::move(compute_pipeline[2]);
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

		vk::AttachmentDescription attach_desc[] =
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
		renderpass_info.setAttachmentCount(array_length(attach_desc));
		renderpass_info.setPAttachments(attach_desc);
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
			.setModule(m_shader[Shader_RenderVS].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eVertex),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[Shader_RenderGS].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eGeometry),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[Shader_RenderFS].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eFragment),
		};

		// assembly
		vk::PipelineInputAssemblyStateCreateInfo assembly_info;
		assembly_info.setPrimitiveRestartEnable(VK_FALSE);
		assembly_info.setTopology(vk::PrimitiveTopology::ePointList);

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
		blend_state.setBlendEnable(VK_TRUE);
		blend_state.setColorBlendOp(vk::BlendOp::eAdd);
		blend_state.setSrcColorBlendFactor(vk::BlendFactor::eOne);
		blend_state.setDstColorBlendFactor(vk::BlendFactor::eOne);
		blend_state.setAlphaBlendOp(vk::BlendOp::eAdd);
		blend_state.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
		blend_state.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
		blend_state.setColorWriteMask(vk::ColorComponentFlagBits::eR
			| vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB
			/*| vk::ColorComponentFlagBits::eA*/);
		vk::PipelineColorBlendStateCreateInfo blend_info;
		blend_info.setAttachmentCount(1);
		blend_info.setPAttachments(&blend_state);

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
			.setLayout(m_pipeline_layout[PipelineLayout_Rendering].get())
			.setRenderPass(m_render_pass.get())
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info),
		};
		auto graphics_pipeline = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
		m_pipeline[Pipeline_Render] = std::move(graphics_pipeline[0]);
	}

}
void GI2DRadiosity::executeRadiosity(const vk::CommandBuffer& cmd)
{
	DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

	{
		vk::BufferMemoryBarrier to_write[] = {
			u_radiosity_info.makeMemoryBarrier(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer,{},
			0, nullptr, array_length(to_write), to_write, 0, nullptr);

		m_info.frame = (m_info.frame + 1) % m_info.frame_max;
		cmd.updateBuffer<GI2DRadiosityInfo>(u_radiosity_info.getInfo().buffer, u_radiosity_info.getInfo().offset, m_info);

		vk::BufferMemoryBarrier to_read[] = {
			u_radiosity_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}




	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 0, m_gi2d_context->getDescriptorSet(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 1, m_descriptor_set.get(), {});

	// データクリア
	{
		vk::BufferMemoryBarrier to_write[] = {
			b_edge.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			b_vertex_counter.makeMemoryBarrier(vk::AccessFlagBits::eIndirectCommandRead, vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect|vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, array_length(to_write), to_write, 0, nullptr);

		cmd.updateBuffer<VertexCounter>(b_vertex_counter.getInfo().buffer, b_vertex_counter.getInfo().offset + sizeof(VertexCounter)*m_info.frame, VertexCounter{ { 1, 0, 0, 0 }, {0, 1, 1, 0} });
		cmd.fillBuffer(b_edge.getInfo().buffer, b_edge.getInfo().offset, b_edge.getInfo().range, 0);

	}

	// レイのヒットポイント生成
	_label.insert("GI2DRadiosity::executeMakeVertex");
	{
		vk::BufferMemoryBarrier to_read[] = {
			b_vertex_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			b_edge.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			b_vertex.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),

		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer| vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eGeometryShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeHitpoint].get());
		auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth, m_gi2d_context->RenderHeight, 1), uvec3(32, 32, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

	// レイの範囲の生成
	_label.insert("GI2DRadiosity::executeRayMarch");
	{
		vk::BufferMemoryBarrier to_read[] = {
			m_gi2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			b_vertex_index.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			b_vertex.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			b_edge.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RayMarch].get());

		cmd.pushConstants<float>(m_pipeline_layout[PipelineLayout_Radiosity].get(), vk::ShaderStageFlagBits::eCompute, 0, 0.25f);

		auto num = app::calcDipatchGroups(uvec3(2048, Dir_Num, 1), uvec3(128, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

	// bounce
	{
		vk::BufferMemoryBarrier to_read[] = {
			b_vertex_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

		for (int i = 0; i<Bounce_Num; i++)
		{
			_label.insert("GI2DRadiosity::executeBounce");
			{
				vk::BufferMemoryBarrier to_read[] = 
				{
					b_vertex.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);
				cmd.pushConstants<int>(m_pipeline_layout[PipelineLayout_Radiosity].get(), vk::ShaderStageFlagBits::eCompute, 0, i);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RayBounce].get());
				cmd.dispatchIndirect(b_vertex_counter.getInfo().buffer, b_vertex_counter.getInfo().offset + sizeof(VertexCounter)*m_info.frame + offsetof(VertexCounter, bounce_cmd));

			}

		}

	}

}


void GI2DRadiosity::executeRendering(const vk::CommandBuffer& cmd)
{

	DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

	// render_targetに書く
	{
		vk::BufferMemoryBarrier to_read[] = {
			b_vertex.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};

		vk::ImageMemoryBarrier image_barrier;
		image_barrier.setImage(m_render_target->m_image);
		image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		image_barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		image_barrier.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eGeometryShader | vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, { array_size(to_read), to_read }, {image_barrier});
	}

	vk::RenderPassBeginInfo begin_render_Info;
	begin_render_Info.setRenderPass(m_render_pass.get());
	begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_render_target->m_resolution));
	begin_render_Info.setFramebuffer(m_framebuffer.get());
	cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_Rendering].get(), 0, m_gi2d_context->getDescriptorSet(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_Rendering].get(), 1, m_descriptor_set.get(), {});
  
  	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Render].get());
 	cmd.drawIndirect(b_vertex_counter.getInfo().buffer, b_vertex_counter.getInfo().offset, Frame_Num, sizeof(VertexCounter));

	cmd.endRenderPass();
	
}

// testcode
#define map_reso 1024
#define dir_reso_bit (7)
#define dir_reso (128)
ivec2 cell_origin = (ivec2(8) << dir_reso_bit);
bool intersection(ivec2 pos, ivec2 dir, int& n, int& f)
{
	ivec4 aabb = ivec4(0, 0, map_reso, map_reso)*dir_reso;
	pos *= dir_reso;
	ivec4 t = ((aabb - pos.xyxy)/dir.xyxy());
	t += ivec4(notEqual((aabb - pos.xyxy) % dir.xyxy(), ivec4(0)));

	ivec2 tmin = glm::min(t.xy(), t.zw());
	ivec2 tmax = glm::max(t.xy(), t.zw());

	n = glm::max(tmin.x, tmin.y);
	f = glm::min(tmax.x, tmax.y);

	return glm::min(tmax.x, tmax.y) > glm::max(glm::max(tmin.x, tmin.y), 0);
}


vec2 rotateEx(float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return vec2(c, s);
}

void calcDirEx(float angle, vec2& dir, vec2& inv_dir)
{
	dir = rotateEx(angle);
	dir.x = abs(dir.x) < FLT_EPSILON ? (dir.x >= 0. ? 0.0001 : -0.0001) : dir.x;
	dir.y = abs(dir.y) < FLT_EPSILON ? (dir.y >= 0. ? 0.0001 : -0.0001) : dir.y;
	inv_dir = 1.f / dir;
	dir = dir * glm::min(abs(inv_dir.x), abs(inv_dir.y));
	inv_dir = 1.f / dir;
}
#define HALF_PI glm::radians(90.)
#define TWO_PI glm::radians(360.)
#define PI glm::radians(180.)
#define QUARTER_PI glm::radians(45.)

void test()
{
	std::vector<int> map(map_reso * map_reso);

	int angle_num = 32;

	for (int angle_index = 0; angle_index < angle_num; angle_index++)
	{
		uint area = angle_index / (angle_num);
		float a = HALF_PI / (angle_num);
		float angle = glm::fma(a, float(angle_index), a*0.5f);

		vec2 dir, inv_dir;
		calcDirEx(angle, dir, inv_dir);

		auto i_dir = abs(ivec2(round(dir * dir_reso)));
		auto i_inv_dir = abs(ivec2(round(inv_dir * dir_reso)));
		printf("dir[%3d]=[%4d,%4d]\n", angle_index, i_dir.x, i_dir.y);

		for (int x = 0; x < map_reso * 2; x++)
		{
			ivec2 origin = ivec2(0);
			if (i_dir.x>= i_dir.y)
			{
				origin.y += (map_reso-1) - int((ceil(i_dir.y / (float)i_dir.x))) * x;
			}
			else
			{
 				origin.x += (map_reso - 1) - int((ceil(i_dir.x / (float)i_dir.y))) * x;
			}

			int begin, end;
			if (!intersection(origin, i_dir, begin, end))
			{
				break;
			}

			for (int i = begin; i < end;)
			{
				ivec2 t1 = i * i_dir.xy() + origin * dir_reso;
				ivec2 mi = t1 >> dir_reso_bit;

				map[mi.x + mi.y*map_reso] += 1;
				ivec2 next = ((mi >> 3) + 1) << 3;
				ivec2 tp = next - mi;

				i++;

// 				ivec2 space = cell_origin - ((i * i_dir.xy + origin* dir_reso) % cell_origin);
// 				ivec2 tp = space / i_dir;
// 				int skip = (int(glm::min(tp.x, tp.y))) + 1;
// 				i += skip;
			}

		}

		for (int y = 0; y < map_reso; y++)
		{
			for (int x = 0; x < map_reso; x++)
			{
				assert(map[x + y * map_reso] == angle_index+1);
			}
		}
	}
}
