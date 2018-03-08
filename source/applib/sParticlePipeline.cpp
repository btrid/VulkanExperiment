#include <applib/sParticlePipeline.h>
#include <applib/sCameraManager.h>
#include <applib/sSystem.h>
#include <btrlib/cWindow.h>

void sParticlePipeline::setup(std::shared_ptr<btr::Context>& context)
{
	m_particle_info_cpu.m_particle_max_num = 8192/2;
	m_particle_info_cpu.m_emitter_max_num = 1024;
	m_particle_info_cpu.m_generate_cmd_max_num = 256;

	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	{
		{
			btr::BufferMemoryDescriptorEx<ParticleData> desc;
			desc.element_num = m_particle_info_cpu.m_particle_max_num*2;
			m_particle = context->m_storage_memory.allocateMemory(desc);
			cmd.fillBuffer(m_particle.getInfo().buffer, m_particle.getInfo().offset, m_particle.getInfo().range, 0u);
		}
		{
			btr::BufferMemoryDescriptorEx<vk::DrawIndirectCommand> desc;
			desc.element_num = 1;
			m_particle_counter = context->m_storage_memory.allocateMemory(desc);
			cmd.updateBuffer<vk::DrawIndirectCommand>(m_particle_counter.getInfo().buffer, m_particle_counter.getInfo().offset, vk::DrawIndirectCommand(4, 0, 0, 0));
		}
		{
			btr::BufferMemoryDescriptorEx<ParticleUpdateParameter> desc;
			desc.element_num = 1;
			m_particle_update_param = context->m_storage_memory.allocateMemory(desc);

			ParticleUpdateParameter updater;
			updater.m_rotate_add = vec4(0.f);
			updater.m_rotate_mul = vec4(1.f);
			updater.m_scale_add = vec4(0.f);
			updater.m_scale_mul = vec4(1.f);
			updater.m_velocity_add = vec4(0.f, -2.f, 0.f, 0.f);
			updater.m_velocity_mul = vec4(0.999f, 1.f, 0.999f, 0.f);
			cmd.updateBuffer<ParticleUpdateParameter>(m_particle_update_param.getInfo().buffer, m_particle_update_param.getInfo().offset, updater);
			
		}
		{
			btr::BufferMemoryDescriptorEx<ParticleGenerateCommand> desc;
			desc.element_num = m_particle_info_cpu.m_emitter_max_num;
			m_particle_generate_cmd = context->m_storage_memory.allocateMemory(desc);
			cmd.fillBuffer(m_particle_generate_cmd.getInfo().buffer, m_particle_generate_cmd.getInfo().offset, m_particle_generate_cmd.getInfo().range, 0u);
		}
		{
			btr::BufferMemoryDescriptorEx<glm::uvec3> desc;
			desc.element_num = 1;
			m_particle_generate_cmd_counter = context->m_storage_memory.allocateMemory(desc);
			cmd.updateBuffer<glm::uvec3>(m_particle_generate_cmd_counter.getInfo().buffer, m_particle_generate_cmd_counter.getInfo().offset, glm::uvec3(0, 1, 1));

			m_particle_emitter_counter = context->m_storage_memory.allocateMemory(desc);
			cmd.updateBuffer<glm::uvec3>(m_particle_emitter_counter.getInfo().buffer, m_particle_emitter_counter.getInfo().offset, glm::uvec3(0, 1, 1));
		}


		{
			btr::BufferMemoryDescriptorEx<ParticleInfo> desc;
			desc.element_num = 1;
			m_particle_info = context->m_uniform_memory.allocateMemory(desc);
			cmd.updateBuffer<ParticleInfo>(m_particle_info.getInfo().buffer, m_particle_info.getInfo().offset, { m_particle_info_cpu });
		}

		{
			btr::BufferMemoryDescriptorEx<EmitterData> desc;
			desc.element_num = m_particle_info_cpu.m_emitter_max_num;
			m_particle_emitter = context->m_storage_memory.allocateMemory(desc);
			cmd.fillBuffer(m_particle_emitter.getInfo().buffer, m_particle_emitter.getInfo().offset, m_particle_emitter.getInfo().range, 0u);
		}

	}

	{
		struct ShaderDesc {
			const char* name;
			vk::ShaderStageFlagBits stage;
		} shader_desc[] =
		{
			{ "ParticleUpdate.comp.spv", vk::ShaderStageFlagBits::eCompute },
//			{ "ParticleEmit.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ "ParticleGenerateDebug.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ "ParticleGenerate.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ "ParticleRender.vert.spv", vk::ShaderStageFlagBits::eVertex },
			{ "ParticleRender.frag.spv", vk::ShaderStageFlagBits::eFragment },
		};
		static_assert(array_length(shader_desc) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourceLibPath() + "shader\\binary\\";
		for (size_t i = 0; i < SHADER_NUM; i++)
		{
			m_shader_module[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_desc[i].name);
		}
	}

	{
		// descriptor set layout
		std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
		bindings[DESCRIPTOR_SET_LAYOUT_PARTICLE] =
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
				sSystem::Order().getSystemDescriptorLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_UPDATE] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_PARTICLE].get(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				sSystem::Order().getSystemDescriptorLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}


		{

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_particle_info.getInfo(),
			};
			std::vector<vk::DescriptorBufferInfo> storages = {
				m_particle.getInfo(),
				m_particle_counter.getInfo(),
				m_particle_emitter.getInfo(),
				m_particle_emitter_counter.getInfo(),
				m_particle_generate_cmd.getInfo(),
				m_particle_generate_cmd_counter.getInfo(),
				m_particle_update_param.getInfo(),
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
		{
			//		m_render_pass = std::make_shared<RenderBackbufferModule>(context);
			m_render_pass = context->m_window->getRenderBackbufferPass();
		}

		vk::PipelineShaderStageCreateInfo shader_info[] =
		{
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader_module[SHADER_UPDATE].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eCompute),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader_module[SHADER_GENERATE].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eCompute),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader_module[SHADER_GENERATE_TRANSFAR_DEBUG].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eCompute),
		};
		// Create pipeline
		vk::ComputePipelineCreateInfo compute_pipeline_info[] =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[1])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[2])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get()),
		};
		m_pipeline[PIPELINE_UPDATE] = context->m_device->createComputePipelineUnique(context->m_cache.get(), compute_pipeline_info[0]);
		m_pipeline[PIPELINE_GENERATE] = context->m_device->createComputePipelineUnique(context->m_cache.get(), compute_pipeline_info[1]);
		m_pipeline[PIPELINE_GENERATE_DEBUG] = context->m_device->createComputePipelineUnique(context->m_cache.get(), compute_pipeline_info[2]);

		vk::Extent2D size = m_render_pass->getResolution();
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
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::PipelineShaderStageCreateInfo shader_info[] = 
			{
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader_module[SHADER_DRAW_VERTEX].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader_module[SHADER_DRAW_FRAGMENT].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment)
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
				.setRenderPass(m_render_pass->getRenderPass())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[PIPELINE_DRAW] = std::move(pipelines[0]);
		}
	}
}

vk::CommandBuffer sParticlePipeline::execute(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	// debug generate command
	static int a;
	a++;
	if (a == 100)
	{
		a = 0;
		{
			vk::BufferMemoryBarrier to_read = m_particle_generate_cmd_counter.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_GENERATE_DEBUG].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_PARTICLE].get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 1, sSystem::Order().getSystemDescriptorSet(), {});
		cmd.dispatch(1, 1, 1);
	}

	// generate particle
	{
		{
			// 描画待ち
			vk::BufferMemoryBarrier to_read = m_particle_counter.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}
		{
			vk::BufferMemoryBarrier to_read = m_particle_generate_cmd.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
			vk::BufferMemoryBarrier to_read2 = m_particle_generate_cmd_counter.makeMemoryBarrier();
			to_read2.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_read2.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eAllCommands, {}, {}, to_read2, {});
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_GENERATE].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_PARTICLE].get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 1, sSystem::Order().getSystemDescriptorSet(), {0});
		cmd.dispatchIndirect(m_particle_generate_cmd_counter.getInfo().buffer, m_particle_generate_cmd_counter.getInfo().offset);
	}
	{
		// パーティクル数初期化
		vk::BufferMemoryBarrier to_transfer = m_particle_counter.makeMemoryBarrier();
		to_transfer.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
		to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

		cmd.updateBuffer<vk::DrawIndirectCommand>(m_particle_counter.getInfo().buffer, m_particle_counter.getInfo().offset, vk::DrawIndirectCommand(4, 0, 0, 0));
		cmd.updateBuffer<uvec3>(m_particle_generate_cmd_counter.getInfo().buffer, m_particle_generate_cmd_counter.getInfo().offset, uvec3(0, 1, 1));
	}

	// update
	{
		vk::BufferMemoryBarrier to_read = m_particle_counter.makeMemoryBarrier();
		to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
		to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_UPDATE].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_PARTICLE].get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 1, sSystem::Order().getSystemDescriptorSet(), {0});
		auto groups = app::calcDipatchGroups(glm::uvec3(m_particle_info_cpu.m_particle_max_num, 1, 1), glm::uvec3(1024, 1, 1));
		cmd.dispatch(groups.x, groups.y, groups.z);
	}

	cmd.end();
	return cmd;
}

vk::CommandBuffer sParticlePipeline::draw(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	auto to_draw = m_particle_counter.makeMemoryBarrier();
	to_draw.setSrcAccessMask(vk::AccessFlagBits::eShaderRead| vk::AccessFlagBits::eShaderWrite);
	to_draw.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { to_draw }, {});

	vk::RenderPassBeginInfo begin_render_Info;
	begin_render_Info.setRenderPass(m_render_pass->getRenderPass());
	begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), context->m_window->getClientSize<vk::Extent2D>()));
	begin_render_Info.setFramebuffer(m_render_pass->getFramebuffer(context->getGPUFrame()));
	cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW].get());
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 0, m_descriptor_set[DESCRIPTOR_SET_PARTICLE].get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 2, sSystem::Order().getSystemDescriptorSet(), {0});
	cmd.drawIndirect(m_particle_counter.getInfo().buffer, m_particle_counter.getInfo().offset, 1, sizeof(vk::DrawIndirectCommand));

	cmd.endRenderPass();
	cmd.end();
	return cmd;
}

