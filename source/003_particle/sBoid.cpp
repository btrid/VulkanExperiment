#include <003_particle/sBoid.h>
#include <003_particle/ParticlePipeline.h>
#include <003_particle/sScene.h>

void sBoid::setup(std::shared_ptr<btr::Loader>& loader)
{
	{
		m_boid_info.m_brain_max = 256;
		m_boid_info.m_soldier_max = 8192;
		m_boid_info.m_soldier_info_max = 16;
		// memory alloc
		{
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(BoidInfo);
			m_boid_info_gpu = loader->m_uniform_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = loader->m_staging_memory.allocateMemory(desc);
			*staging.getMappedPtr<BoidInfo>() = m_boid_info;
			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			copy.setDstOffset(m_boid_info_gpu.getBufferInfo().offset);
			loader->m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_boid_info_gpu.getBufferInfo().buffer, copy);
		}
		{
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(SoldierInfo) * m_boid_info.m_soldier_info_max;
			m_soldier_info_gpu = loader->m_uniform_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = loader->m_staging_memory.allocateMemory(desc);
			auto* info = staging.getMappedPtr<SoldierInfo>();
			info[0].m_turn_speed = glm::radians(45.f);
			info[0].m_move_speed = 8.f;

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			copy.setDstOffset(m_soldier_info_gpu.getBufferInfo().offset);
			loader->m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_soldier_info_gpu.getBufferInfo().buffer, copy);
		}
		{
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(BrainData) * m_boid_info.m_brain_max;
			auto brain_gpu = loader->m_storage_memory.allocateMemory(desc);

			loader->m_cmd.fillBuffer(brain_gpu.getBufferInfo().buffer, brain_gpu.getBufferInfo().offset, brain_gpu.getBufferInfo().range, 0);

			m_brain_gpu.setup(brain_gpu);
		}

		{
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(SoldierData) * m_boid_info.m_soldier_max;
			auto soldier_gpu = loader->m_storage_memory.allocateMemory(desc);

			loader->m_cmd.fillBuffer(soldier_gpu.getBufferInfo().buffer, soldier_gpu.getBufferInfo().offset, soldier_gpu.getBufferInfo().range, 0);

			m_soldier_gpu.setup(soldier_gpu);
		}

		{
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(uint32_t) * sScene::Order().m_maze.getSizeX()* sScene::Order().m_maze.getSizeY()*2;
			m_soldier_LL_head_gpu.setup(loader->m_storage_memory.allocateMemory(desc));
			loader->m_cmd.fillBuffer(m_soldier_LL_head_gpu.getOrg().buffer, m_soldier_LL_head_gpu.getOrg().offset, m_soldier_LL_head_gpu.getOrg().range, 0xFFFFFFFF);

			// transfer
			vk::BufferMemoryBarrier ll_compute;
			ll_compute.buffer = m_soldier_LL_head_gpu.getOrg().buffer;
			ll_compute.offset = m_soldier_LL_head_gpu.getOrg().offset;
			ll_compute.size = m_soldier_LL_head_gpu.getOrg().range;
			ll_compute.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite ;
			std::vector<vk::BufferMemoryBarrier> to_compute = {
				ll_compute,
			};
			loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_compute, {});
		}
		{
			btr::UpdateBufferDescriptor emit_desc;
			emit_desc.device_memory = loader->m_storage_memory;
			emit_desc.staging_memory = loader->m_staging_memory;
			emit_desc.frame_max = sGlobal::FRAME_MAX;
			m_soldier_emit_gpu.setup(emit_desc);
		}
		{
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(vk::DrawIndirectCommand) * 256;
			m_soldier_draw_indiret_gpu = loader->m_vertex_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = loader->m_staging_memory.allocateMemory(desc);
			auto* info = staging.getMappedPtr<vk::DrawIndirectCommand>();
			for (int i = 0; i < 256; i++)
			{
				info[i].firstInstance = 0;
				info[i].firstVertex = 0;
				info[i].instanceCount = 1;
				info[i].vertexCount = 0;
			}

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			copy.setDstOffset(m_soldier_draw_indiret_gpu.getBufferInfo().offset);
			loader->m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_soldier_draw_indiret_gpu.getBufferInfo().buffer, copy);
		}

		{
			auto* maze = &sScene::Order().m_maze;
			std::vector<uint32_t> solver = maze->solveEx(4, 4);
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = vk::Format::eR32Uint;
			image_info.mipLevels = 1;
			image_info.arrayLayers = m_boid_info.m_soldier_info_max;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = { (uint32_t)maze->getSizeX(), (uint32_t)maze->getSizeY(), 1 };
			image_info.flags = vk::ImageCreateFlagBits::e2DArrayCompatibleKHR;
			auto image = loader->m_device->createImage(image_info);

			vk::MemoryRequirements memory_request = loader->m_device->getImageMemoryRequirements(image);
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			auto image_memory = loader->m_device->allocateMemory(memory_alloc_info);
			loader->m_device->bindImageMemory(image, image_memory, 0);

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = m_boid_info.m_soldier_info_max;;
			subresourceRange.levelCount = 1;
			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e2DArray;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eG;
			view_info.components.b = vk::ComponentSwizzle::eB;
			view_info.components.a = vk::ComponentSwizzle::eA;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = image;
			view_info.subresourceRange = subresourceRange;
			auto image_view = loader->m_device->createImageView(view_info);

			{

				vk::ImageMemoryBarrier to_transfer;
				to_transfer.image = image;
				to_transfer.oldLayout = vk::ImageLayout::eUndefined;
				to_transfer.newLayout = vk::ImageLayout::eTransferDstOptimal;
				to_transfer.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_transfer.subresourceRange = subresourceRange;

				loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, { to_transfer });
			}

			{
				vk::ImageSubresourceLayers l;
				l.setAspectMask(vk::ImageAspectFlagBits::eColor);
				l.setBaseArrayLayer(0);
				l.setLayerCount(1);
				l.setMipLevel(0);
				btr::BufferMemory::Descriptor desc;
				desc.size = vector_sizeof(solver);
				desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = loader->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), solver.data(), desc.size);
				vk::BufferImageCopy copy;
				copy.setBufferOffset(staging.getBufferInfo().offset);
				copy.setImageSubresource(l);
				copy.setImageExtent(image_info.extent);
				loader->m_cmd.copyBufferToImage(staging.getBufferInfo().buffer, image, vk::ImageLayout::eTransferDstOptimal, copy);
			}

			{
				vk::ImageMemoryBarrier to_shader_read;
				to_shader_read.dstQueueFamilyIndex = loader->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_shader_read.image = image;
				to_shader_read.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				to_shader_read.newLayout = vk::ImageLayout::eGeneral;
				to_shader_read.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_shader_read.subresourceRange = subresourceRange;

				loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_shader_read });
			}

			m_astar_image = image;
			m_astar_image_view = image_view;
			m_astar_image_memory = image_memory;
		}
	}


	// shader create
	{
		struct
		{
			const char* name;
			vk::ShaderStageFlagBits stage;
		}shader_info[] =
		{
			{ "BoidSoldierUpdate.comp.spv",  vk::ShaderStageFlagBits::eCompute },
			{ "BoidSoldierEmit.comp.spv",  vk::ShaderStageFlagBits::eCompute },
			{ "BoidSoldierRender.vert.spv",  vk::ShaderStageFlagBits::eVertex },
			{ "BoidSoldierRender.frag.spv",  vk::ShaderStageFlagBits::eFragment },
		};
		static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourceAppPath() + "shader\\binary\\";
		for (size_t i = 0; i < SHADER_NUM; i++)
		{
			m_shader_info[i].setModule(loadShader(loader->m_device.getHandle(), path + shader_info[i].name));
			m_shader_info[i].setStage(shader_info[i].stage);
			m_shader_info[i].setPName("main");
		}
	}

	{
		// descriptor
		{
			std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
			bindings[DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE] =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
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
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setBinding(7),
			};

			for (size_t i = 0; i < bindings.size(); i++)
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
			std::copy(descriptor_set.begin(), descriptor_set.end(), m_descriptor_set.begin());
		}

		{
			{
				std::vector<vk::DescriptorSetLayout> layouts = {
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE],
					sScene::Order().getDescriptorSetLayout(sScene::DESCRIPTOR_SET_LAYOUT_MAP),
				};
				std::vector<vk::PushConstantRange> push_constants = {
					vk::PushConstantRange()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setSize(16),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(layouts.size());
				pipeline_layout_info.setPSetLayouts(layouts.data());
				pipeline_layout_info.setPushConstantRangeCount(push_constants.size());
				pipeline_layout_info.setPPushConstantRanges(push_constants.data());
				m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE] = loader->m_device->createPipelineLayout(pipeline_layout_info);
			}
			{
				std::vector<vk::DescriptorSetLayout> layouts = {
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE],
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
				m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT] = loader->m_device->createPipelineLayout(pipeline_layout_info);
			}
			{
				std::vector<vk::DescriptorSetLayout> layouts = {
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE],
					sScene::Order().getDescriptorSetLayout(sScene::DESCRIPTOR_SET_LAYOUT_CAMERA),
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
				m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW] = loader->m_device->createPipelineLayout(pipeline_layout_info);
			}
		}

		// update
		{

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_boid_info_gpu.getBufferInfo(),
				m_soldier_info_gpu.getBufferInfo(),
			};
			std::vector<vk::DescriptorBufferInfo> storages = {
				m_brain_gpu.getOrg(),
				m_soldier_gpu.getOrg(),
				m_soldier_draw_indiret_gpu.getBufferInfo(),
				m_soldier_LL_head_gpu.getOrg(),
				m_soldier_emit_gpu.getBufferInfo(),
			};
			std::vector<vk::DescriptorImageInfo> images = {
				vk::DescriptorImageInfo()
				.setImageLayout(vk::ImageLayout::eGeneral)
				.setImageView(m_astar_image_view)
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(getDescriptorSet(DESCRIPTOR_SET_SOLDIER_UPDATE)),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(2)
				.setDstSet(getDescriptorSet(DESCRIPTOR_SET_SOLDIER_UPDATE)),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(images.size())
				.setPImageInfo(images.data())
				.setDstBinding(7)
				.setDstSet(getDescriptorSet(DESCRIPTOR_SET_SOLDIER_UPDATE)),
			};
			loader->m_device->updateDescriptorSets(write_desc, {});
		}
	}

	// Create pipeline
	std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
	{
		vk::ComputePipelineCreateInfo()
		.setStage(m_shader_info[SHADER_COMPUTE_UPDATE_SOLDIER])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE]),
		vk::ComputePipelineCreateInfo()
		.setStage(m_shader_info[SHADER_COMPUTE_EMIT_SOLDIER])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT]),
	};
	auto compute_pipeline = loader->m_device->createComputePipelines(loader->m_cache, compute_pipeline_info);
	m_pipeline[PIPELINE_COMPUTE_SOLDIER_UPDATE] = compute_pipeline[0];
	m_pipeline[PIPELINE_COMPUTE_SOLDIER_EMIT] = compute_pipeline[1];

	vk::Extent3D size;
	size.setWidth(640);
	size.setHeight(480);
	size.setDepth(1);
	// pipeline
	{
		// assembly
		vk::PipelineInputAssemblyStateCreateInfo assembly_info;
		assembly_info.setPrimitiveRestartEnable(VK_FALSE);
		assembly_info.setTopology(vk::PrimitiveTopology::ePointList);

		// viewport
		vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
		vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height));
		vk::PipelineViewportStateCreateInfo viewport_info;
		viewport_info.setViewportCount(1);
		viewport_info.setPViewports(&viewport);
		viewport_info.setScissorCount(1);
		viewport_info.setPScissors(&scissor);

		// ラスタライズ
		vk::PipelineRasterizationStateCreateInfo rasterization_info;
		rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
		rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
		rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
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

		vk::PipelineVertexInputStateCreateInfo vertex_input_info;

		std::array<vk::PipelineShaderStageCreateInfo, 2> shader_info
		{
			m_shader_info[SHADER_VERTEX_DRAW_SOLDIER],
			m_shader_info[SHADER_FRAGMENT_DRAW_SOLDIER],
		};
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
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW])
			.setRenderPass(loader->m_render_pass)
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info),
		};
		auto graphics_pipeline = loader->m_device->createGraphicsPipelines(loader->m_cache, graphics_pipeline_info);
		m_pipeline[PIPELINE_GRAPHICS_SOLDIER_DRAW] = graphics_pipeline[0];
	}
}

void sBoid::execute(std::shared_ptr<btr::Executer>& executer)
{

	auto cmd = executer->m_cmd;
	{
		// transfer
		vk::BufferMemoryBarrier ll_to_transfer;
		ll_to_transfer.buffer = m_soldier_LL_head_gpu.getOrg().buffer;
		ll_to_transfer.offset = m_soldier_LL_head_gpu.getOrg().offset;
		ll_to_transfer.size = m_soldier_LL_head_gpu.getOrg().range;
		ll_to_transfer.srcAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
		ll_to_transfer.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		std::vector<vk::BufferMemoryBarrier> to_transfer = {
			ll_to_transfer,
			m_soldier_draw_indiret_gpu.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

		cmd.updateBuffer<vk::DrawIndirectCommand>(m_soldier_draw_indiret_gpu.getBufferInfo().buffer, m_soldier_draw_indiret_gpu.getBufferInfo().offset, vk::DrawIndirectCommand(0, 1, 0, 0));
		cmd.fillBuffer(m_soldier_LL_head_gpu.getDst().buffer, m_soldier_LL_head_gpu.getDst().offset, m_soldier_LL_head_gpu.getDst().range, 0xFFFFFFFF);

		vk::BufferMemoryBarrier ll_to_read = ll_to_transfer;
		ll_to_read.buffer = m_soldier_LL_head_gpu.getOrg().buffer;
		ll_to_read.offset = m_soldier_LL_head_gpu.getOrg().offset;
		ll_to_read.size = m_soldier_LL_head_gpu.getOrg().range;
		ll_to_read.srcAccessMask = ll_to_transfer.dstAccessMask;
		ll_to_read.dstAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
		std::vector<vk::BufferMemoryBarrier> to_update_barrier = {
			ll_to_read,
			m_soldier_draw_indiret_gpu.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_update_barrier, {});

	}


	{
		auto src_info = m_soldier_gpu.getSrc();
		auto dst_info = m_soldier_gpu.getDst();
		// update
		struct UpdateConstantBlock
		{
			float m_deltatime;
			uint m_src_offset;
			uint m_dst_offset;
			uint m_double_buffer_index;
		};
		UpdateConstantBlock block;
		block.m_deltatime = sGlobal::Order().getDeltaTime();
		block.m_src_offset = m_soldier_gpu.getSrcOffset() / sizeof(SoldierData);
		block.m_dst_offset = m_soldier_gpu.getDstOffset() / sizeof(SoldierData);
		block.m_double_buffer_index = m_soldier_LL_head_gpu.getDstIndex();
		cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE], vk::ShaderStageFlagBits::eCompute, 0, block);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_SOLDIER_UPDATE]);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE], 0, getDescriptorSet(DESCRIPTOR_SET_SOLDIER_UPDATE), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE], 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});
		auto groups = app::calcDipatchGroups(glm::uvec3(m_boid_info.m_soldier_max / 2, 1, 1), glm::uvec3(1024, 1, 1));
		cmd.dispatch(groups.x, groups.y, groups.z);
	}


	{
		static int count;
		count++;
		count %= 400;
		if (count == 1)
		{
			auto soldier_barrier = vk::BufferMemoryBarrier();
 			soldier_barrier.buffer = m_soldier_gpu.getDst().buffer;
 			soldier_barrier.size = m_soldier_gpu.getDst().range;
 			soldier_barrier.offset = m_soldier_gpu.getDst().offset;
			soldier_barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
			soldier_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead| vk::AccessFlagBits::eShaderWrite;

			std::vector<vk::BufferMemoryBarrier> to_emit_barrier =
			{
				soldier_barrier,
				m_soldier_draw_indiret_gpu.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_emit_barrier, {});

			// emit
			std::array<SoldierData, 70> data;
			for (auto& p : data)
			{
				p.m_pos = glm::vec4(212.f + std::rand() % 80 / 3.f, 0.f, 162.f + std::rand() % 80 / 3.f, 1.f);
				p.m_vel = glm::vec4(glm::normalize(glm::vec3(std::rand() % 50 - 25, 0.f, std::rand() % 50 - 25 + 0.5f)), 10.5f);
				p.m_life = std::rand() % 30 + 10.f;
				p.m_soldier_type = 0;
				p.m_brain_index = 0;
				p.m_ll_next = -1;
				p.m_astar_target = (p.m_pos + p.m_vel).xz;
				p.m_inertia = glm::vec4(0.f, 0.f, 0.f, 1.f);
				p.m_map_index = sScene::Order().calcMapIndex(p.m_pos);
			}
			m_soldier_emit_gpu.subupdate(data.data(), vector_sizeof(data), 0);

			auto to_transfer = m_soldier_emit_gpu.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});
			m_soldier_emit_gpu.update(cmd);
			auto to_read = m_soldier_emit_gpu.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_SOLDIER_EMIT]);
			struct EmitConstantBlock
			{
				uint m_emit_num;
				uint m_offset;
			};
			EmitConstantBlock block;
			block.m_emit_num = data.size();
			block.m_offset = m_soldier_gpu.getDstOffset() / sizeof(SoldierData);
			cmd.pushConstants<EmitConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT], vk::ShaderStageFlagBits::eCompute, 0, block);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT], 0, getDescriptorSet(DESCRIPTOR_SET_SOLDIER_UPDATE), {});
			auto groups = app::calcDipatchGroups(glm::uvec3(block.m_emit_num, 1, 1), glm::uvec3(1024, 1, 1));
			cmd.dispatch(groups.x, groups.y, groups.z);

		}

		auto soldier_draw_barrier = vk::BufferMemoryBarrier();
		soldier_draw_barrier.buffer = m_soldier_gpu.getOrg().buffer;
		soldier_draw_barrier.size = m_soldier_gpu.getOrg().range;
		soldier_draw_barrier.offset = m_soldier_gpu.getOrg().offset;
		soldier_draw_barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite| vk::AccessFlagBits::eShaderRead;
		soldier_draw_barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, soldier_draw_barrier, {});

		auto to_draw = m_soldier_draw_indiret_gpu.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_draw }, {});

	}

	m_brain_gpu.swap();
	m_soldier_gpu.swap();
	m_soldier_LL_head_gpu.swap();
}

void sBoid::draw(vk::CommandBuffer cmd)
{
	struct DrawConstantBlock
	{
		uint m_src_offset;
	};
	DrawConstantBlock block;
	block.m_src_offset = m_soldier_gpu.getDstOffset() / sizeof(SoldierData);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_GRAPHICS_SOLDIER_DRAW]);
	cmd.pushConstants<DrawConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW], vk::ShaderStageFlagBits::eVertex, 0, block);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW], 0, getDescriptorSet(DESCRIPTOR_SET_SOLDIER_UPDATE), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW], 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_CAMERA), {});
	cmd.drawIndirect(m_soldier_draw_indiret_gpu.getBufferInfo().buffer, m_soldier_draw_indiret_gpu.getBufferInfo().offset, 1, sizeof(vk::DrawIndirectCommand));


}
