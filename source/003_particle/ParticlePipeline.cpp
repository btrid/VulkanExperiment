#include <003_particle/ParticlePipeline.h>

void cParticlePipeline::Private::setup(btr::Loader& loader)
{
	{
		m_maze.generate(g_scene->m_map_info_cpu.m_cell_num.x, g_scene->m_map_info_cpu.m_cell_num.y);
		auto geometry = m_maze.makeGeometry(g_scene->m_map_info_cpu.m_cell_size);
		Geometry::OptimaizeDuplicateVertexDescriptor opti_desc;
		//Geometry::OptimaizeDuplicateVertex(geometry, opti_desc);

		std::vector<vk::VertexInputAttributeDescription> vertex_attr(1);
		vertex_attr[0].setOffset(0);
		vertex_attr[0].setBinding(0);
		vertex_attr[0].setFormat(vk::Format::eR32G32B32Sfloat);
		vertex_attr[0].setLocation(0);
		std::vector<vk::VertexInputBindingDescription> vertex_binding(1);
		vertex_binding[0].setBinding(0);
		vertex_binding[0].setInputRate(vk::VertexInputRate::eVertex);
		vertex_binding[0].setStride(sizeof(glm::vec3));
		m_maze_geometry = Geometry::MakeGeometry(
			loader,
			std::get<0>(geometry).data(),
			vector_sizeof(std::get<0>(geometry)),
			std::get<1>(geometry).data(),
			vector_sizeof(std::get<1>(geometry)),
			vk::IndexType::eUint32,
			vertex_attr,
			vertex_binding
		);

		{
			auto map = m_maze.makeMapData();
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = vk::Format::eR8Uint;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = { (uint32_t)m_maze.getSizeX(), (uint32_t)m_maze.getSizeY(), 1u };
			image_info.flags = vk::ImageCreateFlagBits::eMutableFormat;
			auto image = loader.m_device->createImage(image_info);

			vk::MemoryRequirements memory_request = loader.m_device->getImageMemoryRequirements(image);
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader.m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			auto image_memory = loader.m_device->allocateMemory(memory_alloc_info);
			loader.m_device->bindImageMemory(image, image_memory, 0);


			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;
			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e2D;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eG;
			view_info.components.b = vk::ComponentSwizzle::eB;
			view_info.components.a = vk::ComponentSwizzle::eA;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = image;
			view_info.subresourceRange = subresourceRange;
			auto image_view = loader.m_device->createImageView(view_info);

			{

				vk::ImageMemoryBarrier to_transfer;
				//						to_transfer.dstQueueFamilyIndex = loader.m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_transfer.image = image;
				to_transfer.oldLayout = vk::ImageLayout::eUndefined;
				to_transfer.newLayout = vk::ImageLayout::eTransferDstOptimal;
				to_transfer.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_transfer.subresourceRange = subresourceRange;

				loader.m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, { to_transfer });
			}

			{
				vk::ImageSubresourceLayers l;
				l.setAspectMask(vk::ImageAspectFlagBits::eColor);
				l.setBaseArrayLayer(0);
				l.setLayerCount(1);
				l.setMipLevel(0);
				btr::BufferMemory::Descriptor desc;
				desc.size = memory_alloc_info.allocationSize;
				desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = loader.m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), map.data(), desc.size);
				vk::BufferImageCopy copy;
				copy.setBufferOffset(staging.getBufferInfo().offset);
				copy.setImageSubresource(l);
				copy.setImageExtent(image_info.extent);
				loader.m_cmd.copyBufferToImage(staging.getBufferInfo().buffer, image, vk::ImageLayout::eTransferDstOptimal, copy);
			}

			{

				vk::ImageMemoryBarrier to_shader_read;
				to_shader_read.dstQueueFamilyIndex = loader.m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_shader_read.image = image;
				to_shader_read.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				to_shader_read.newLayout = vk::ImageLayout::eGeneral;
				to_shader_read.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_shader_read.subresourceRange = subresourceRange;

				loader.m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_shader_read });
			}

			m_map_image = image;
			m_map_image_view = image_view;
			m_map_image_memory = image_memory;
		}


		btr::BufferMemory::Descriptor desc;
		desc.size = sizeof(MapInfo);
		m_map_info = loader.m_uniform_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = loader.m_staging_memory.allocateMemory(desc);
		*staging.getMappedPtr<MapInfo>() = g_scene->m_map_info_cpu;

		vk::BufferCopy vertex_copy;
		vertex_copy.setSize(desc.size);
		vertex_copy.setSrcOffset(staging.getBufferInfo().offset);
		vertex_copy.setDstOffset(m_map_info.getBufferInfo().offset);
		loader.m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_map_info.getBufferInfo().buffer, vertex_copy);

		auto barrier = m_map_info.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
		loader.m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { barrier }, {});
	}

	m_particle_info_cpu.m_max_num = 8192;
	m_particle_info_cpu.m_emit_max_num = 1024;

	{
		{
			btr::BufferMemory::Descriptor data_desc;
			data_desc.size = sizeof(ParticleData) * m_particle_info_cpu.m_max_num;
			m_particle = loader.m_storage_memory.allocateMemory(data_desc);
			std::vector<ParticleData> p(m_particle_info_cpu.m_max_num);
			loader.m_cmd.fillBuffer(m_particle.getBufferInfo().buffer, m_particle.getBufferInfo().offset, m_particle.getBufferInfo().range, 0u);
		}

		{
			btr::UpdateBufferDescriptor emit_desc;
			emit_desc.device_memory = loader.m_storage_memory;
			emit_desc.staging_memory = loader.m_staging_memory;
			emit_desc.frame_max = sGlobal::FRAME_MAX;
			m_particle_emit.setup(emit_desc);
		}

		btr::BufferMemory::Descriptor desc;
		desc.size = sizeof(vk::DrawIndirectCommand);
		m_particle_counter = loader.m_storage_memory.allocateMemory(desc);
		loader.m_cmd.updateBuffer<vk::DrawIndirectCommand>(m_particle_counter.getBufferInfo().buffer, m_particle_counter.getBufferInfo().offset, vk::DrawIndirectCommand(1, 1, 0, 0));
		auto count_barrier = m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
		loader.m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { count_barrier }, {});

		m_particle_info = loader.m_uniform_memory.allocateMemory(sizeof(ParticleInfo));
		loader.m_cmd.updateBuffer<ParticleInfo>(m_particle_info.getBufferInfo().buffer, m_particle_info.getBufferInfo().offset, { m_particle_info_cpu });
		auto barrier = m_particle_info.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
		loader.m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { barrier }, {});

	}

	{
		const char* name[] = {
			"ParticleUpdate.comp.spv",
			"ParticleEmit.comp.spv",
		};
		static_assert(array_length(name) == COMPUTE_NUM, "not equal shader num");

		std::string path = btr::getResourceAppPath() + "shader\\binary\\";
		for (size_t i = 0; i < COMPUTE_NUM; i++)
		{
			m_compute_shader_info[i].setModule(loadShader(loader.m_device.getHandle(), path + name[i]));
			m_compute_shader_info[i].setStage(vk::ShaderStageFlagBits::eCompute);
			m_compute_shader_info[i].setPName("main");
		}
	}

	{
		struct ShaderDesc {
			const char* name;
			vk::ShaderStageFlagBits stage;
		} shader_desc[] =
		{
			{"Render.vert.spv", vk::ShaderStageFlagBits::eVertex },
			{"Render.frag.spv", vk::ShaderStageFlagBits::eFragment },
			{ "FloorRender.vert.spv", vk::ShaderStageFlagBits::eVertex },
			{ "FloorRender.geom.spv", vk::ShaderStageFlagBits::eGeometry },
			{"FloorRender.frag.spv", vk::ShaderStageFlagBits::eFragment },
		};
		static_assert(array_length(shader_desc) == GRAPHICS_SHADER_NUM, "not equal shader num");
		std::string path = btr::getResourceAppPath() + "shader\\binary\\";
		for (uint32_t i = 0; i < GRAPHICS_SHADER_NUM; i++)
		{
			m_graphics_shader_info[i].setModule(loadShader(loader.m_device.getHandle(), path + shader_desc[i].name));
			m_graphics_shader_info[i].setStage(shader_desc[i].stage);
			m_graphics_shader_info[i].setPName("main");
		}
	}

	{
		// descriptor set layout
		std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_NUM);
		bindings[DESCRIPTOR_UPDATE] =
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
		};

		bindings[DESCRIPTOR_MAP_INFO] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(1),
		};

		bindings[DESCRIPTOR_EMIT] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(0),
		};

		bindings[DESCRIPTOR_DRAW_CAMERA] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(0),
		};

		for (size_t i = 0; i < DESCRIPTOR_NUM; i++)
		{
			vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings[i].size())
				.setPBindings(bindings[i].data());
			m_descriptor_set_layout[i] = loader.m_device->createDescriptorSetLayout(descriptor_set_layout_info);
		}

		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = loader.m_descriptor_pool;
		alloc_info.descriptorSetCount = m_descriptor_set_layout.size();
		alloc_info.pSetLayouts = m_descriptor_set_layout.data();
		auto descriptor_set = loader.m_device->allocateDescriptorSets(alloc_info);
		std::copy(descriptor_set.begin(), descriptor_set.end(), m_descriptor_set.begin());
	}
	{
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_UPDATE],
				m_descriptor_set_layout[DESCRIPTOR_MAP_INFO],
			};
			std::vector<vk::PushConstantRange> push_constants = {
				vk::PushConstantRange()
				.setStageFlags(vk::ShaderStageFlagBits::eCompute)
				.setSize(12),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			pipeline_layout_info.setPushConstantRangeCount(push_constants.size());
			pipeline_layout_info.setPPushConstantRanges(push_constants.data());
			m_pipeline_layout[PIPELINE_LAYOUT_UPDATE] = loader.m_device->createPipelineLayout(pipeline_layout_info);
		}
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_UPDATE],
				m_descriptor_set_layout[DESCRIPTOR_EMIT],
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
			m_pipeline_layout[COMPUTE_PIPELINE_LAYOUT_EMIT] = loader.m_device->createPipelineLayout(pipeline_layout_info);
		}
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_UPDATE],
				m_descriptor_set_layout[DESCRIPTOR_DRAW_CAMERA],
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
			m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_PARTICLE_DRAW] = loader.m_device->createPipelineLayout(pipeline_layout_info);
		}

		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_DRAW_CAMERA],
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_FLOOR_DRAW] = loader.m_device->createPipelineLayout(pipeline_layout_info);
		}

		{

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_particle_info.getBufferInfo(),
			};
			std::vector<vk::DescriptorBufferInfo> storages = {
				m_particle.getBufferInfo(),
				m_particle_counter.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_UPDATE]),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(1)
				.setDstSet(m_descriptor_set[DESCRIPTOR_UPDATE]),
			};
			loader.m_device->updateDescriptorSets(write_desc, {});
		}
		{

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_map_info.getBufferInfo(),
			};
			std::vector<vk::DescriptorImageInfo> images = {
				vk::DescriptorImageInfo()
				.setImageLayout(vk::ImageLayout::eGeneral)
				.setImageView(m_map_image_view)
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(images.size())
				.setPImageInfo(images.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_MAP_INFO]),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(1)
				.setDstSet(m_descriptor_set[DESCRIPTOR_MAP_INFO]),
			};
			loader.m_device->updateDescriptorSets(write_desc, {});
		}
		{
			std::vector<vk::DescriptorBufferInfo> storages = {
				m_particle_emit.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_EMIT]),
			};
			loader.m_device->updateDescriptorSets(write_desc, {});
		}
		{
			std::vector<vk::DescriptorBufferInfo> uniforms = {
				g_scene->m_camera.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_DRAW_CAMERA]),
			};
			loader.m_device->updateDescriptorSets(write_desc, {});
		}
	}
	{
		// Create pipeline
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(m_compute_shader_info[0])
			.setLayout(m_pipeline_layout[0]),
			vk::ComputePipelineCreateInfo()
			.setStage(m_compute_shader_info[1])
			.setLayout(m_pipeline_layout[1]),
		};
		m_compute_pipeline = loader.m_device->createComputePipelines(loader.m_cache, compute_pipeline_info);

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
				.setTopology(vk::PrimitiveTopology::ePointList),
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList),
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

			vk::PipelineVertexInputStateCreateInfo vertex_input_info[2];
			std::vector<vk::VertexInputBindingDescription> vertex_input_binding =
			{
				vk::VertexInputBindingDescription()
				.setBinding(0)
				.setInputRate(vk::VertexInputRate::eVertex)
				.setStride(sizeof(glm::vec3))
			};

			std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute =
			{
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(0)
				.setFormat(vk::Format::eR32G32B32Sfloat)
				.setOffset(0),
			};

			vertex_input_info[1].setVertexBindingDescriptionCount(vertex_input_binding.size());
			vertex_input_info[1].setPVertexBindingDescriptions(vertex_input_binding.data());
			vertex_input_info[1].setVertexAttributeDescriptionCount(vertex_input_attribute.size());
			vertex_input_info[1].setPVertexAttributeDescriptions(vertex_input_attribute.data());

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(2)
				.setPStages(m_graphics_shader_info.data())
				.setPVertexInputState(&vertex_input_info[0])
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_PARTICLE_DRAW])
				.setRenderPass(loader.m_render_pass)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(3)
				.setPStages(m_graphics_shader_info.data() + 2)
				.setPVertexInputState(&vertex_input_info[1])
				.setPInputAssemblyState(&assembly_info[1])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_FLOOR_DRAW])
				.setRenderPass(loader.m_render_pass)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_graphics_pipeline = loader.m_device->createGraphicsPipelines(loader.m_cache, graphics_pipeline_info);

		}
	}
}

