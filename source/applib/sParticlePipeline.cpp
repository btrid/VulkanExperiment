#include <applib/sParticlePipeline.h>
#include <applib/sCameraManager.h>

void sParticlePipeline::Private::setup(std::shared_ptr<btr::Context>& context)
{
	m_particle_info_cpu.m_particle_max_num = 8192/2;
	m_particle_info_cpu.m_emitter_max_num = 1024;
	m_particle_info_cpu.m_generate_cmd_max_num = 256;

	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
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
				.setFormat(context->m_window->getSwapchain().m_surface_format.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
				vk::AttachmentDescription()
				.setFormat(vk::Format::eD32Sfloat)
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

	}
	{
		{
			btr::AllocatedMemory::Descriptor data_desc;
			data_desc.size = sizeof(ParticleData) * m_particle_info_cpu.m_particle_max_num*2;
			m_particle = context->m_storage_memory.allocateMemory(data_desc);
			std::vector<ParticleData> p(m_particle_info_cpu.m_particle_max_num*2);
			cmd->fillBuffer(m_particle.getBufferInfo().buffer, m_particle.getBufferInfo().offset, m_particle.getBufferInfo().range, 0u);
		}
		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = sizeof(vk::DrawIndirectCommand);
			m_particle_counter = context->m_storage_memory.allocateMemory(desc);
			cmd->updateBuffer<vk::DrawIndirectCommand>(m_particle_counter.getBufferInfo().buffer, m_particle_counter.getBufferInfo().offset, vk::DrawIndirectCommand(4, 0, 0, 0));
			auto count_barrier = m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { count_barrier }, {});
		}
		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = sizeof(ParticleUpdateParameter);
			m_particle_update_param = context->m_storage_memory.allocateMemory(desc);

			ParticleUpdateParameter updater;
			updater.m_rotate_add = vec4(0.f);
			updater.m_rotate_mul = vec4(1.f);
			updater.m_scale_add = vec4(0.f);
			updater.m_scale_mul = vec4(1.f);
			updater.m_velocity_add = vec4(0.f, -2.f, 0.f, 0.f);
			updater.m_velocity_mul = vec4(0.999f, 1.f, 0.999f, 0.f);
			cmd->updateBuffer<ParticleUpdateParameter>(m_particle_update_param.getBufferInfo().buffer, m_particle_update_param.getBufferInfo().offset, updater);
			auto b = m_particle_update_param.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { b }, {});
			
		}
		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = sizeof(ParticleGenerateCommand) * m_particle_info_cpu.m_emitter_max_num;
			m_particle_generate_cmd = context->m_storage_memory.allocateMemory(desc);
			std::vector<ParticleGenerateCommand> p(m_particle_info_cpu.m_emitter_max_num);
			cmd->fillBuffer(m_particle_generate_cmd.getBufferInfo().buffer, m_particle_generate_cmd.getBufferInfo().offset, m_particle_generate_cmd.getBufferInfo().range, 0u);
		}
		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = sizeof(glm::uvec3);
			m_particle_generate_cmd_counter = context->m_storage_memory.allocateMemory(desc);
			cmd->updateBuffer<glm::uvec3>(m_particle_generate_cmd_counter.getBufferInfo().buffer, m_particle_generate_cmd_counter.getBufferInfo().offset, glm::uvec3(0, 1, 1));

			m_particle_emitter_counter = context->m_storage_memory.allocateMemory(desc);
			cmd->updateBuffer<glm::uvec3>(m_particle_emitter_counter.getBufferInfo().buffer, m_particle_emitter_counter.getBufferInfo().offset, glm::uvec3(0, 1, 1));
		}


		{
			m_particle_info = context->m_uniform_memory.allocateMemory(sizeof(ParticleInfo));
			cmd->updateBuffer<ParticleInfo>(m_particle_info.getBufferInfo().buffer, m_particle_info.getBufferInfo().offset, { m_particle_info_cpu });
			auto barrier = m_particle_info.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { barrier }, {});
		}

		{
			btr::AllocatedMemory::Descriptor data_desc;
			data_desc.size = sizeof(EmitterData) * m_particle_info_cpu.m_emitter_max_num;
			m_particle_emitter = context->m_storage_memory.allocateMemory(data_desc);
			std::vector<EmitterData> p(m_particle_info_cpu.m_emitter_max_num);
			cmd->fillBuffer(m_particle_emitter.getBufferInfo().buffer, m_particle_emitter.getBufferInfo().offset, m_particle_emitter.getBufferInfo().range, 0u);
		}

	}

	{
		struct ShaderDesc {
			const char* name;
			vk::ShaderStageFlagBits stage;
		} shader_desc[] =
		{
			{ "ParticleUpdate.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ "ParticleEmit.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ "ParticleGenerateDebug.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ "ParticleGenerate.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ "ParticleRender.vert.spv", vk::ShaderStageFlagBits::eVertex },
			{ "ParticleRender.frag.spv", vk::ShaderStageFlagBits::eFragment },
		};
		static_assert(array_length(shader_desc) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourceAppPath() + "shader\\binary\\";
		for (size_t i = 0; i < SHADER_NUM; i++)
		{
			m_shader_module[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_desc[i].name);
			m_shader_info[i].setModule(m_shader_module[i].get());
			m_shader_info[i].setStage(shader_desc[i].stage);
			m_shader_info[i].setPName("main");
		}
	}

	{
		// descriptor set layout
		std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
		bindings[DESCRIPTOR_SET_PARTICLE] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(4),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(5),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(6),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(7),
		};
		for (size_t i = 0; i < DESCRIPTOR_SET_LAYOUT_NUM; i++)
		{
			vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings[i].size())
				.setPBindings(bindings[i].data());
			m_descriptor_set_layout[i] = context->m_device->createDescriptorSetLayoutUnique(descriptor_set_layout_info);
		}

		vk::DescriptorSetLayout layouts[] = { 
			m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_PARTICLE].get() 
		};
		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = context->m_descriptor_pool.get();
		alloc_info.descriptorSetCount = array_length(layouts);
		alloc_info.pSetLayouts = layouts;
		auto descriptor_set = context->m_device->allocateDescriptorSetsUnique(alloc_info);
		m_descriptor_set[DESCRIPTOR_SET_PARTICLE] = std::move(descriptor_set[0]);
	}
	{
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_PARTICLE].get(),
			};
			std::vector<vk::PushConstantRange> push_constants = {
				vk::PushConstantRange()
				.setStageFlags(vk::ShaderStageFlagBits::eCompute)
				.setSize(8),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			pipeline_layout_info.setPushConstantRangeCount(push_constants.size());
			pipeline_layout_info.setPPushConstantRanges(push_constants.data());
			m_pipeline_layout[PIPELINE_LAYOUT_UPDATE] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_PARTICLE].get(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
			};
			std::vector<vk::PushConstantRange> push_constants = {
				vk::PushConstantRange()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex)
				.setSize(4),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			pipeline_layout_info.setPushConstantRangeCount(push_constants.size());
			pipeline_layout_info.setPPushConstantRanges(push_constants.data());
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}


		{

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_particle_info.getBufferInfo(),
			};
			std::vector<vk::DescriptorBufferInfo> storages = {
				m_particle.getBufferInfo(),
				m_particle_counter.getBufferInfo(),
				m_particle_emitter.getBufferInfo(),
				m_particle_emitter_counter.getBufferInfo(),
				m_particle_generate_cmd.getBufferInfo(),
				m_particle_generate_cmd_counter.getBufferInfo(),
				m_particle_update_param.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_LAYOUT_PARTICLE].get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(1)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_LAYOUT_PARTICLE].get()),
			};
			context->m_device->updateDescriptorSets(write_desc, {});
		}
	}
	{
		// Create pipeline
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_UPDATE])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_GENERATE])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_GENERATE_TRANSFAR_DEBUG])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get()),
		};
		auto pipelines = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[PIPELINE_UPDATE] = std::move(pipelines[0]);
		m_pipeline[PIPELINE_GENERATE] = std::move(pipelines[1]);
		m_pipeline[PIPELINE_GENERATE_DEBUG] = std::move(pipelines[2]);

		vk::Extent3D size;
		size.setWidth(640);
		size.setHeight(480);
		size.setDepth(1);
		// pipeline
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleStrip),
			};

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height))
			};
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount((uint32_t)scissor.size());
			viewportInfo.setPScissors(scissor.data());

			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);

			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
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
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::PipelineShaderStageCreateInfo shader_info[] = {
				m_shader_info[SHADER_DRAW_VERTEX],
				m_shader_info[SHADER_DRAW_FRAGMENT],
			};

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info))
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[PIPELINE_DRAW] = std::move(pipelines[0]);
		}
	}
}

vk::CommandBuffer sParticlePipeline::Private::execute(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	struct UpdateConstantBlock
	{
		float m_deltatime;
		uint m_double_buffer_dst_index;
	};
	UpdateConstantBlock constant;
	constant.m_deltatime = sGlobal::Order().getDeltaTime();
	constant.m_double_buffer_dst_index = sGlobal::Order().getCPUIndex();


	// debug
	static int a;
	a++;
	if (a == 100)
	{
		a = 0;
		{
			vk::BufferMemoryBarrier to_read = m_particle_generate_cmd_counter.makeMemoryBarrierEx();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_GENERATE_DEBUG].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_PARTICLE].get(), {});
		cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), vk::ShaderStageFlagBits::eCompute, 0, constant);
		cmd.dispatch(1, 1, 1);
	}

	// generate particle
	{
		{
			// 描画待ち
			vk::BufferMemoryBarrier to_read = m_particle_counter.makeMemoryBarrierEx();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}
		{
			vk::BufferMemoryBarrier to_read = m_particle_generate_cmd.makeMemoryBarrierEx();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
			vk::BufferMemoryBarrier to_read2 = m_particle_generate_cmd_counter.makeMemoryBarrierEx();
			to_read2.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_read2.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, to_read2, {});
		}


		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_GENERATE].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_PARTICLE].get(), {});
		cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), vk::ShaderStageFlagBits::eCompute, 0, constant);
		cmd.dispatchIndirect(m_particle_generate_cmd_counter.getBufferInfo().buffer, m_particle_generate_cmd_counter.getBufferInfo().offset);
	}
	{
		// パーティクル数初期化
		vk::BufferMemoryBarrier to_transfer = m_particle_counter.makeMemoryBarrierEx();
		to_transfer.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
		to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

		cmd.updateBuffer<vk::DrawIndirectCommand>(m_particle_counter.getBufferInfo().buffer, m_particle_counter.getBufferInfo().offset, vk::DrawIndirectCommand(4, 0, 0, 0));
		cmd.updateBuffer<glm::uvec3>(m_particle_generate_cmd_counter.getBufferInfo().buffer, m_particle_generate_cmd_counter.getBufferInfo().offset, glm::uvec3(0, 1, 1));

	}

	// update
	{
		vk::BufferMemoryBarrier to_read = m_particle_counter.makeMemoryBarrierEx();
		to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
		to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_UPDATE].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_PARTICLE].get(), {});
		constant.m_double_buffer_dst_index = sGlobal::Order().getGPUIndex();
		cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), vk::ShaderStageFlagBits::eCompute, 0, constant);
		auto groups = app::calcDipatchGroups(glm::uvec3(m_particle_info_cpu.m_particle_max_num, 1, 1), glm::uvec3(1024, 1, 1));
		cmd.dispatch(groups.x, groups.y, groups.z);
	}

	cmd.end();
	return cmd;
}

vk::CommandBuffer sParticlePipeline::Private::draw(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	auto to_draw = m_particle_counter.makeMemoryBarrierEx();
	to_draw.setSrcAccessMask(vk::AccessFlagBits::eShaderRead| vk::AccessFlagBits::eShaderWrite);
	to_draw.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { to_draw }, {});

	vk::RenderPassBeginInfo begin_render_Info;
	begin_render_Info.setRenderPass(m_render_pass.get());
	begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), context->m_window->getClientSize<vk::Extent2D>()));
	begin_render_Info.setFramebuffer(m_framebuffer[context->getGPUFrame()].get());
	cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

	struct DrawConstantBlock
	{
		uint m_src_offset;
	};
	DrawConstantBlock draw_constant;
	uint dst_offset = sGlobal::Order().getCPUIndex() == 0 ? m_particle_info_cpu.m_particle_max_num : 0;
	draw_constant.m_src_offset = dst_offset;
	cmd.pushConstants<DrawConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), vk::ShaderStageFlagBits::eVertex, 0, draw_constant);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW].get());
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 0, m_descriptor_set[DESCRIPTOR_SET_PARTICLE].get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
	cmd.drawIndirect(m_particle_counter.getBufferInfo().buffer, m_particle_counter.getBufferInfo().offset, 1, sizeof(vk::DrawIndirectCommand));

	cmd.endRenderPass();
	cmd.end();
	return cmd;
}

