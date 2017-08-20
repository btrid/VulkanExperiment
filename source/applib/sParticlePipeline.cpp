#include <003_particle/sParticlePipeline.h>
#include <applib/sCameraManager.h>

void sParticlePipeline::Private::setup(std::shared_ptr<btr::Loader>& loader)
{

	m_particle_info_cpu.m_particle_max_num = 8192/2;
	m_particle_info_cpu.m_emitter_max_num = 1024;
	m_particle_info_cpu.m_generate_cmd_max_num = 256;

	{
		{
			btr::BufferMemory::Descriptor data_desc;
			data_desc.size = sizeof(ParticleData) * m_particle_info_cpu.m_particle_max_num*2;
			m_particle = loader->m_storage_memory.allocateMemory(data_desc);
			std::vector<ParticleData> p(m_particle_info_cpu.m_particle_max_num*2);
			loader->m_cmd.fillBuffer(m_particle.getBufferInfo().buffer, m_particle.getBufferInfo().offset, m_particle.getBufferInfo().range, 0u);
		}
		{
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(vk::DrawIndirectCommand);
			m_particle_counter = loader->m_storage_memory.allocateMemory(desc);
			loader->m_cmd.updateBuffer<vk::DrawIndirectCommand>(m_particle_counter.getBufferInfo().buffer, m_particle_counter.getBufferInfo().offset, vk::DrawIndirectCommand(4, 0, 0, 0));
			auto count_barrier = m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { count_barrier }, {});
		}
		{
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(ParticleUpdateParameter);
			m_particle_update_param = loader->m_storage_memory.allocateMemory(desc);

			ParticleUpdateParameter updater;
			updater.m_rotate_add = vec4(0.f);
			updater.m_rotate_mul = vec4(1.f);
			updater.m_scale_add = vec4(0.f);
			updater.m_scale_mul = vec4(1.f);
			updater.m_velocity_add = vec4(0.f, -2.f, 0.f, 0.f);
			updater.m_velocity_mul = vec4(0.999f, 1.f, 0.999f, 0.f);
			loader->m_cmd.updateBuffer<ParticleUpdateParameter>(m_particle_update_param.getBufferInfo().buffer, m_particle_update_param.getBufferInfo().offset, updater);
			auto b = m_particle_update_param.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { b }, {});
			
		}
		{
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(ParticleGenerateCommand) * m_particle_info_cpu.m_emitter_max_num;
			m_particle_generate_cmd = loader->m_storage_memory.allocateMemory(desc);
			std::vector<ParticleGenerateCommand> p(m_particle_info_cpu.m_emitter_max_num);
			loader->m_cmd.fillBuffer(m_particle_generate_cmd.getBufferInfo().buffer, m_particle_generate_cmd.getBufferInfo().offset, m_particle_generate_cmd.getBufferInfo().range, 0u);
		}
		{
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(glm::uvec3);
			m_generate_cmd_counter = loader->m_storage_memory.allocateMemory(desc);
			loader->m_cmd.updateBuffer<glm::uvec3>(m_generate_cmd_counter.getBufferInfo().buffer, m_generate_cmd_counter.getBufferInfo().offset, glm::uvec3(0, 1, 1));

			m_particle_emitter_counter = loader->m_storage_memory.allocateMemory(desc);
			loader->m_cmd.updateBuffer<glm::uvec3>(m_particle_emitter_counter.getBufferInfo().buffer, m_particle_emitter_counter.getBufferInfo().offset, glm::uvec3(0, 1, 1));
		}


		{
			m_particle_info = loader->m_uniform_memory.allocateMemory(sizeof(ParticleInfo));
			loader->m_cmd.updateBuffer<ParticleInfo>(m_particle_info.getBufferInfo().buffer, m_particle_info.getBufferInfo().offset, { m_particle_info_cpu });
			auto barrier = m_particle_info.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { barrier }, {});
		}

		{
			btr::BufferMemory::Descriptor data_desc;
			data_desc.size = sizeof(EmitterData) * m_particle_info_cpu.m_emitter_max_num;
			m_particle_emitter = loader->m_storage_memory.allocateMemory(data_desc);
			std::vector<EmitterData> p(m_particle_info_cpu.m_emitter_max_num);
			loader->m_cmd.fillBuffer(m_particle_emitter.getBufferInfo().buffer, m_particle_emitter.getBufferInfo().offset, m_particle_emitter.getBufferInfo().range, 0u);
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
			{ "ParticleGenerateCPU.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ "ParticleGenerate.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ "ParticleRender.vert.spv", vk::ShaderStageFlagBits::eVertex },
			{ "ParticleRender.frag.spv", vk::ShaderStageFlagBits::eFragment },
		};
		static_assert(array_length(shader_desc) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourceAppPath() + "shader\\binary\\";
		for (size_t i = 0; i < SHADER_NUM; i++)
		{
			m_shader_info[i].setModule(loadShader(loader->m_device.getHandle(), path + shader_desc[i].name));
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
			m_descriptor_set_layout[i] = loader->m_device->createDescriptorSetLayout(descriptor_set_layout_info);
		}

		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = loader->m_descriptor_pool;
		alloc_info.descriptorSetCount = m_descriptor_set_layout.size();
		alloc_info.pSetLayouts = m_descriptor_set_layout.data();
		auto descriptor_set = loader->m_device->allocateDescriptorSets(alloc_info);
		m_descriptor_set[DESCRIPTOR_SET_PARTICLE] = descriptor_set[DESCRIPTOR_SET_PARTICLE];
	}
	{
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_PARTICLE],
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
			m_pipeline_layout[PIPELINE_LAYOUT_UPDATE] = loader->m_device->createPipelineLayout(pipeline_layout_info);
		}
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_PARTICLE],
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
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW] = loader->m_device->createPipelineLayout(pipeline_layout_info);
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
				m_generate_cmd_counter.getBufferInfo(),
				m_particle_update_param.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_LAYOUT_PARTICLE]),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(1)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_LAYOUT_PARTICLE]),
			};
			loader->m_device->updateDescriptorSets(write_desc, {});
		}
	}
	{
		// Create pipeline
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_UPDATE])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE]),
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_GENERATE])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE]),
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_GENERATE_TRANSFAR_CPU])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE]),
		};
		auto pipelines = loader->m_device->createComputePipelines(loader->m_cache, compute_pipeline_info);
		m_pipeline[PIPELINE_UPDATE] = pipelines[0];
		m_pipeline[PIPELINE_GENERATE] = pipelines[1];
		m_pipeline[PIPELINE_CMD_TRANSFAR] = pipelines[2];

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
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW])
				.setRenderPass(loader->m_render_pass)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = loader->m_device->createGraphicsPipelines(loader->m_cache, graphics_pipeline_info);
			m_pipeline[PIPELINE_DRAW] = pipelines[0];
		}
	}
}

void sParticlePipeline::Private::draw(vk::CommandBuffer cmd)
{
	// 			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline[1]);
	// 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_FLOOR_DRAW], 0, m_descriptor_set[DESCRIPTOR_DRAW_CAMERA], {});
	// 			cmd.bindVertexBuffers(0, { m_maze_geometry.m_resource->m_vertex.getBufferInfo().buffer }, { m_maze_geometry.m_resource->m_vertex.getBufferInfo().offset });
	// 			cmd.bindIndexBuffer(m_maze_geometry.m_resource->m_index.getBufferInfo().buffer, m_maze_geometry.m_resource->m_index.getBufferInfo().offset, m_maze_geometry.m_resource->m_index_type);
	// 			cmd.drawIndexedIndirect(m_maze_geometry.m_resource->m_indirect.getBufferInfo().buffer, m_maze_geometry.m_resource->m_indirect.getBufferInfo().offset, 1, sizeof(vk::DrawIndexedIndirectCommand));

	struct DrawConstantBlock
	{
		uint m_src_offset;
	};
	DrawConstantBlock draw_constant;
	uint dst_offset = sGlobal::Order().getCPUIndex() == 0 ? m_particle_info_cpu.m_particle_max_num : 0;
	draw_constant.m_src_offset = dst_offset;
	cmd.pushConstants<DrawConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_DRAW], vk::ShaderStageFlagBits::eVertex, 0, draw_constant);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW]);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW], 0, m_descriptor_set[DESCRIPTOR_SET_PARTICLE], {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW], 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
	cmd.drawIndirect(m_particle_counter.getBufferInfo().buffer, m_particle_counter.getBufferInfo().offset, 1, sizeof(vk::DrawIndirectCommand));
}
