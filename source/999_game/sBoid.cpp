#include <999_game/sBoid.h>
#include <999_game/ParticlePipeline.h>
#include <999_game/sScene.h>

void sBoid::setup(std::shared_ptr<btr::Loader>& loader)
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
			.setFormat(loader->m_window->getSwapchain().m_surface_format.format)
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

		m_render_pass = loader->m_device->createRenderPassUnique(renderpass_info);
	}

	m_framebuffer.resize(loader->m_window->getSwapchain().getBackbufferNum());
	{
		std::array<vk::ImageView, 2> view;

		vk::FramebufferCreateInfo framebuffer_info;
		framebuffer_info.setRenderPass(m_render_pass.get());
		framebuffer_info.setAttachmentCount((uint32_t)view.size());
		framebuffer_info.setPAttachments(view.data());
		framebuffer_info.setWidth(loader->m_window->getClientSize().x);
		framebuffer_info.setHeight(loader->m_window->getClientSize().y);
		framebuffer_info.setLayers(1);

		for (size_t i = 0; i < m_framebuffer.size(); i++) {
			view[0] = loader->m_window->getSwapchain().m_backbuffer[i].m_view;
			view[1] = loader->m_window->getSwapchain().m_depth.m_view;
			m_framebuffer[i] = loader->m_device->createFramebufferUnique(framebuffer_info);
		}
	}

	{
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
			auto& map_desc = sScene::Order().m_map_info_cpu.m_descriptor[0];
			desc.size = sizeof(uint32_t) * map_desc.m_cell_num.x* map_desc.m_cell_num.y*2;
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
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(SoldierData)*m_boid_info.m_soldier_emit_max;
			m_soldier_emit_gpu = loader->m_storage_memory.allocateMemory(desc);
		}
		{
			m_emit_transfer.resize(loader->m_window->getSwapchain().getBackbufferNum());
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(SoldierData) * 1024;
			for (auto& data : m_emit_transfer)
			{
				data.m_is_emit = false;
				data.m_buffer = loader->m_staging_memory.allocateMemory(desc);
			}
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
				p.m_astar_target = (p.m_pos + glm::normalize(p.m_vel)).xz;
				p.m_inertia = glm::vec4(0.f, 0.f, 0.f, 1.f);
				p.m_map_index = sScene::Order().calcMapIndex(p.m_pos);
			}
			memcpy_s(m_emit_transfer[2].m_buffer.getMappedPtr(), vector_sizeof(data), data.data(), vector_sizeof(data));
			m_emit_transfer[2].m_is_emit = true;
			m_emit_transfer[2].m_emit_num = 70;

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
			auto image = loader->m_device->createImageUnique(image_info);

			vk::MemoryRequirements memory_request = loader->m_device->getImageMemoryRequirements(image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			auto image_memory = loader->m_device->allocateMemoryUnique(memory_alloc_info);
			loader->m_device->bindImageMemory(image.get(), image_memory.get(), 0);

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
			view_info.image = image.get();
			view_info.subresourceRange = subresourceRange;
			auto image_view = loader->m_device->createImageViewUnique(view_info);

			{

				vk::ImageMemoryBarrier to_transfer;
				to_transfer.image = image.get();
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
				loader->m_cmd.copyBufferToImage(staging.getBufferInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, copy);
			}

			{
				vk::ImageMemoryBarrier to_shader_read;
				to_shader_read.dstQueueFamilyIndex = loader->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_shader_read.image = image.get();
				to_shader_read.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				to_shader_read.newLayout = vk::ImageLayout::eGeneral;
				to_shader_read.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_shader_read.subresourceRange = subresourceRange;

				loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_shader_read });
			}

			m_astar_image = std::move(image);
			m_astar_image_view = std::move(image_view);
			m_astar_image_memory = std::move(image_memory);
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
			m_shader_module[i] = loadShaderUnique(loader->m_device.getHandle(), path + shader_info[i].name);
			m_shader_info[i].setModule(m_shader_module[i].get());
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
				m_descriptor_set_layout[i] = loader->m_device->createDescriptorSetLayoutUnique(descriptor_set_layout_info);
			}

			vk::DescriptorSetLayout layouts[] = { m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE].get() };
			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.descriptorPool = loader->m_descriptor_pool;
			alloc_info.descriptorSetCount = array_length(layouts);
			alloc_info.pSetLayouts = layouts;
			auto descriptor_set = loader->m_device->allocateDescriptorSetsUnique(alloc_info);
			m_descriptor_set[DESCRIPTOR_SET_SOLDIER_UPDATE] = std::move(descriptor_set[0]);
		}

		{
			{
				std::vector<vk::DescriptorSetLayout> layouts = {
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE].get(),
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
				m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE] = loader->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			}
			{
				std::vector<vk::DescriptorSetLayout> layouts = {
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE].get(),
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
				m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT] = loader->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			}
			{
				std::vector<vk::DescriptorSetLayout> layouts = {
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE].get(),
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
				m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW] = loader->m_device->createPipelineLayoutUnique(pipeline_layout_info);
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
				.setImageView(m_astar_image_view.get())
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
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE].get()),
		vk::ComputePipelineCreateInfo()
		.setStage(m_shader_info[SHADER_COMPUTE_EMIT_SOLDIER])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT].get()),
	};
	auto compute_pipeline = loader->m_device->createComputePipelinesUnique(loader->m_cache, compute_pipeline_info);
	m_pipeline[PIPELINE_COMPUTE_SOLDIER_UPDATE] = std::move(compute_pipeline[0]);
	m_pipeline[PIPELINE_COMPUTE_SOLDIER_EMIT] = std::move(compute_pipeline[1]);

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
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW].get())
			.setRenderPass(m_render_pass.get())
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info),
		};
		auto graphics_pipeline = loader->m_device->createGraphicsPipelinesUnique(loader->m_cache, graphics_pipeline_info);
		m_pipeline[PIPELINE_GRAPHICS_SOLDIER_DRAW] = std::move(graphics_pipeline[0]);
	}

	// record cmd
	{
		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandBufferCount = sGlobal::FRAME_MAX;
		cmd_buffer_info.commandPool = sThreadLocal::Order().getCmdPool(sGlobal::CMD_POOL_TYPE_COMPILED, 0);
		cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;

		m_update_cmd = sGlobal::Order().getGPU(0).getDevice()->allocateCommandBuffersUnique(cmd_buffer_info);
		for (size_t i = 0; i < m_update_cmd.size(); i++)
		{
			vk::CommandBufferBeginInfo begin_info;
			begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
			vk::CommandBufferInheritanceInfo inheritance_info;
			begin_info.setPInheritanceInfo(&inheritance_info);

			auto& cmd = m_update_cmd[i].get();
			cmd.begin(begin_info);
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
				// update
				struct UpdateConstantBlock
				{
					float m_deltatime;
					uint m_double_buffer_index;
				};
				UpdateConstantBlock block;
				block.m_deltatime = sGlobal::Order().getDeltaTime();
				block.m_double_buffer_index = m_soldier_LL_head_gpu.getDstIndex();
				cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE].get(), vk::ShaderStageFlagBits::eCompute, 0, block);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_SOLDIER_UPDATE].get());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE].get(), 0, getDescriptorSet(DESCRIPTOR_SET_SOLDIER_UPDATE), {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE].get(), 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});
				auto groups = app::calcDipatchGroups(glm::uvec3(m_boid_info.m_soldier_max / 2, 1, 1), glm::uvec3(1024, 1, 1));
				cmd.dispatch(groups.x, groups.y, groups.z);
			}
			cmd.end();
		}



		m_emit_cpu_cmd = sGlobal::Order().getGPU(0).getDevice()->allocateCommandBuffersUnique(cmd_buffer_info);
		for (size_t i = 0; i < m_emit_cpu_cmd.size(); i++)
		{
			vk::CommandBufferBeginInfo begin_info;
			begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
			vk::CommandBufferInheritanceInfo inheritance_info;
			begin_info.setPInheritanceInfo(&inheritance_info);

			auto& cmd = m_emit_cpu_cmd[i].get();
			cmd.begin(begin_info);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_SOLDIER_EMIT].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT].get(), 0, getDescriptorSet(DESCRIPTOR_SET_SOLDIER_UPDATE), {});
			auto groups = app::calcDipatchGroups(glm::uvec3(1, 1, 1), glm::uvec3(1024, 1, 1));
			cmd.dispatch(groups.x, groups.y, groups.z);

			cmd.end();
		}

		m_draw_cmd = sGlobal::Order().getGPU(0).getDevice()->allocateCommandBuffersUnique(cmd_buffer_info);
		for (size_t i = 0; i < m_draw_cmd.size(); i++)
		{
			vk::CommandBufferBeginInfo begin_info;
			begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse|vk::CommandBufferUsageFlagBits::eRenderPassContinue);
			vk::CommandBufferInheritanceInfo inheritance_info;
			inheritance_info.setFramebuffer(m_framebuffer[i].get());
			inheritance_info.setRenderPass(m_render_pass.get());
			begin_info.setPInheritanceInfo(&inheritance_info);

			auto& cmd = m_draw_cmd[i].get();
			cmd.begin(begin_info);

// 			auto to_draw = vk::BufferMemoryBarrier();
// 			to_draw.buffer = m_soldier_draw_indiret_gpu.getBufferInfo().buffer;
// 			to_draw.size = m_soldier_draw_indiret_gpu.getBufferInfo().range;
// 			to_draw.offset = m_soldier_draw_indiret_gpu.getBufferInfo().offset;
// 			to_draw.srcAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
// 			to_draw.dstAccessMask = vk::AccessFlagBits::eShaderRead;
// 			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, /*vk::DependencyFlagBits::eByRegion*/{}, {}, to_draw, {});

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_GRAPHICS_SOLDIER_DRAW].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW].get(), 0, getDescriptorSet(DESCRIPTOR_SET_SOLDIER_UPDATE), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW].get(), 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_CAMERA), {});
			cmd.drawIndirect(m_soldier_draw_indiret_gpu.getBufferInfo().buffer, m_soldier_draw_indiret_gpu.getBufferInfo().offset, 1, sizeof(vk::DrawIndirectCommand));

			cmd.end();
		}
	}
}

vk::CommandBuffer sBoid::execute(std::shared_ptr<btr::Executer>& executer)
{
	auto& gpu = sGlobal::Order().getGPU(0);
	auto& device = gpu.getDevice();
	auto cmd = sThreadLocal::Order().getCmdOnetime(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));
	{
		// update
		struct UpdateConstantBlock
		{
			float m_deltatime;
			uint m_double_buffer_index;
		};
		UpdateConstantBlock block;
		block.m_deltatime = sGlobal::Order().getDeltaTime();
		block.m_double_buffer_index = sGlobal::Order().getGPUIndex();
		cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE].get(), vk::ShaderStageFlagBits::eCompute, 0, block);

		cmd.executeCommands(m_update_cmd[executer->getGPUFrame()].get());

	}
	{
		auto& data = m_emit_transfer[executer->getGPUFrame()];

		if (data.m_is_emit)
		{
			data.m_is_emit = false;
			struct EmitConstantBlock
			{
				uint m_emit_num;
				uint m_double_buffer_index;
			};
			EmitConstantBlock block;
			block.m_emit_num = data.m_emit_num;
			block.m_double_buffer_index = sGlobal::Order().getGPUIndex();
			cmd.pushConstants<EmitConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT].get(), vk::ShaderStageFlagBits::eCompute, 0, block);
			cmd.executeCommands(m_emit_cpu_cmd[executer->getGPUFrame()].get());
		}
	}
	cmd.end();
	return cmd;
}

vk::CommandBuffer sBoid::draw(std::shared_ptr<btr::Executer>& executer)
{
	auto& gpu = sGlobal::Order().getGPU(0);
	auto& device = gpu.getDevice();
	auto cmd = sThreadLocal::Order().getCmdOnetime(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));

	auto to_draw = vk::BufferMemoryBarrier();
	to_draw.buffer = m_soldier_draw_indiret_gpu.getBufferInfo().buffer;
	to_draw.size = m_soldier_draw_indiret_gpu.getBufferInfo().range;
	to_draw.offset = m_soldier_draw_indiret_gpu.getBufferInfo().offset;
	to_draw.srcAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
	to_draw.dstAccessMask = vk::AccessFlagBits::eShaderRead;
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, /*vk::DependencyFlagBits::eByRegion*/{}, {}, to_draw, {});

	struct DrawConstantBlock
	{
		uint m_double_buffer_index;
	};
	DrawConstantBlock block;
	block.m_double_buffer_index = sGlobal::Order().getGPUIndex();
	cmd.pushConstants<DrawConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW].get(), vk::ShaderStageFlagBits::eVertex, 0, block);

	vk::RenderPassBeginInfo begin_render_Info;
	begin_render_Info.setRenderPass(m_render_pass.get());
	begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), executer->m_window->getClientSize<vk::Extent2D>()));
	begin_render_Info.setFramebuffer(m_framebuffer[sGlobal::Order().getCurrentFrame()].get());
	cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eSecondaryCommandBuffers);

	cmd.executeCommands(m_draw_cmd[executer->getGPUFrame()].get());

	cmd.endRenderPass();
	cmd.end();

	return cmd;
}
