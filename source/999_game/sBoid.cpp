#include <999_game/sBoid.h>
#include <applib/sSystem.h>
#include <applib/AppModel/AppModel.h>
#include <999_game/sScene.h>

void sBoid::setup(std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target)
{
	m_render_target = render_target;
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	// レンダーパス
	{
		// sub pass
		vk::AttachmentReference color_ref[] =
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
		subpass.setColorAttachmentCount(array_length(color_ref));
		subpass.setPColorAttachments(color_ref);
		subpass.setPDepthStencilAttachment(&depth_ref);

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
			// depth
			vk::AttachmentDescription()
			.setFormat(render_target->m_depth_info.format)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eLoad)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
		};
		vk::RenderPassCreateInfo renderpass_info;
		renderpass_info.setAttachmentCount(array_length(attach_description));
		renderpass_info.setPAttachments(attach_description);
		renderpass_info.setSubpassCount(1);
		renderpass_info.setPSubpasses(&subpass);

		m_render_pass = context->m_device.createRenderPassUnique(renderpass_info);
	}

	{
		vk::ImageView view[] = {
			render_target->m_view,
			render_target->m_depth_view,
		};
		vk::FramebufferCreateInfo framebuffer_info;
		framebuffer_info.setRenderPass(m_render_pass.get());
		framebuffer_info.setAttachmentCount(array_length(view));
		framebuffer_info.setPAttachments(view);
		framebuffer_info.setWidth(render_target->m_info.extent.width);
		framebuffer_info.setHeight(render_target->m_info.extent.height);
		framebuffer_info.setLayers(1);

		m_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
	}

	{
		// emit test
		auto* buf = m_emit_data_cpu[sGlobal::Order().getWorkerIndex()].reserve(70);
		for (uint32_t i = 0; i < 70; i++)
		{
			buf[i].m_pos = glm::vec4(212.f + std::rand() % 80 / 3.f, 0.f, 162.f + std::rand() % 80 / 3.f, 1.f);
			buf[i].m_vel = glm::vec4(glm::normalize(glm::vec3(std::rand() % 50 - 25, 0.f, std::rand() % 50 - 25 + 0.5f)), 10.5f);
			buf[i].m_life = std::rand() % 30 + 10.f;
			buf[i].m_soldier_type = 0;
			buf[i].m_brain_index = 0;
			buf[i].m_ll_next = -1;
			buf[i].m_astar_target = (buf[i].m_pos + glm::normalize(buf[i].m_vel)).xz;
			buf[i].m_inertia = glm::vec4(0.f, 0.f, 0.f, 1.f);
			buf[i].m_map_index = sScene::Order().calcMapIndex(buf[i].m_pos);
		}

	}

	{
		// memory alloc
		{
			btr::BufferMemoryDescriptorEx<BoidInfo> desc;
			desc.element_num = 1;
			m_boid_info_gpu = context->m_uniform_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			*staging.getMappedPtr() = m_boid_info;
			vk::BufferCopy copy;
			copy.setSize(m_boid_info_gpu.getInfo().range);
			copy.setSrcOffset(staging.getInfo().offset);
			copy.setDstOffset(m_boid_info_gpu.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, m_boid_info_gpu.getInfo().buffer, copy);
		}
		{
			btr::BufferMemoryDescriptorEx<SoldierInfo> desc;
			desc.element_num = m_boid_info.m_soldier_info_max;
			m_soldier_info_gpu = context->m_uniform_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			auto* info = staging.getMappedPtr();
			info[0].m_turn_speed = glm::radians(45.f);
			info[0].m_move_speed = 8.f;

			vk::BufferCopy copy;
			copy.setSize(m_soldier_info_gpu.getInfo().range);
			copy.setSrcOffset(staging.getInfo().offset);
			copy.setDstOffset(m_soldier_info_gpu.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, m_soldier_info_gpu.getInfo().buffer, copy);
		}
		{
			btr::BufferMemoryDescriptor desc;
			desc.size = sizeof(BrainData) * m_boid_info.m_brain_max;
			auto brain_gpu = context->m_storage_memory.allocateMemory(desc);

			cmd.fillBuffer(brain_gpu.getInfo().buffer, brain_gpu.getInfo().offset, brain_gpu.getInfo().range, 0);

			m_brain_gpu.setup(brain_gpu);
		}

		{
			btr::BufferMemoryDescriptor desc;
			desc.size = sizeof(SoldierData) * m_boid_info.m_soldier_max*2;
			auto soldier_gpu = context->m_storage_memory.allocateMemory(desc);

			cmd.fillBuffer(soldier_gpu.getInfo().buffer, soldier_gpu.getInfo().offset, soldier_gpu.getInfo().range, 0);

			m_soldier_gpu.setup(soldier_gpu);
		}

		{
			btr::BufferMemoryDescriptorEx<SoldierData> desc;
			desc.element_num = 1024;
			m_soldier_emit_data = context->m_storage_memory.allocateMemory(desc);
		}

		{
			btr::BufferMemoryDescriptor desc;
			auto& map_desc = sScene::Order().m_map_info_cpu.m_descriptor[0];
			desc.size = sizeof(uint32_t) * map_desc.m_cell_num.x* map_desc.m_cell_num.y*2;
			m_soldier_LL_head_gpu.setup(context->m_storage_memory.allocateMemory(desc));
			cmd.fillBuffer(m_soldier_LL_head_gpu.getOrg().buffer, m_soldier_LL_head_gpu.getOrg().offset, m_soldier_LL_head_gpu.getOrg().range, 0xFFFFFFFF);

			// transfer
			vk::BufferMemoryBarrier ll_compute;
			ll_compute.buffer = m_soldier_LL_head_gpu.getOrg().buffer;
			ll_compute.offset = m_soldier_LL_head_gpu.getOrg().offset;
			ll_compute.size = m_soldier_LL_head_gpu.getOrg().range;
			ll_compute.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite ;
			std::vector<vk::BufferMemoryBarrier> to_compute = {
				ll_compute,
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_compute, {});
		}
		{
			btr::BufferMemoryDescriptorEx<vk::DrawIndirectCommand> desc;
			desc.element_num = 256;
			m_soldier_draw_indiret_gpu = context->m_vertex_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			auto* info = staging.getMappedPtr();
			for (int i = 0; i < 256; i++)
			{
				info[i].firstInstance = 0;
				info[i].firstVertex = 0;
				info[i].instanceCount = 1;
				info[i].vertexCount = 0;
			}

			vk::BufferCopy copy;
			copy.setSize(m_soldier_draw_indiret_gpu.getInfo().range);
			copy.setSrcOffset(staging.getInfo().offset);
			copy.setDstOffset(m_soldier_draw_indiret_gpu.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, m_soldier_draw_indiret_gpu.getInfo().buffer, copy);
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
//			image_info.flags = vk::ImageCreateFlagBits::e2DArrayCompatibleKHR;
			auto image = context->m_device.createImageUnique(image_info);

			vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			auto image_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
			context->m_device.bindImageMemory(image.get(), image_memory.get(), 0);

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
			auto image_view = context->m_device.createImageViewUnique(view_info);

			{

				vk::ImageMemoryBarrier to_transfer;
				to_transfer.image = image.get();
				to_transfer.oldLayout = vk::ImageLayout::eUndefined;
				to_transfer.newLayout = vk::ImageLayout::eTransferDstOptimal;
				to_transfer.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_transfer.subresourceRange = subresourceRange;

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_transfer });
			}

			{
				vk::ImageSubresourceLayers l;
				l.setAspectMask(vk::ImageAspectFlagBits::eColor);
				l.setBaseArrayLayer(0);
				l.setLayerCount(1);
				l.setMipLevel(0);
				btr::BufferMemoryDescriptor desc;
				desc.size = vector_sizeof(solver);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), solver.data(), desc.size);
				vk::BufferImageCopy copy;
				copy.setBufferOffset(staging.getInfo().offset);
				copy.setImageSubresource(l);
				copy.setImageExtent(image_info.extent);
				cmd.copyBufferToImage(staging.getInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, copy);
			}

			{
				vk::ImageMemoryBarrier to_shader_read;
//				to_shader_read.dstQueueFamilyIndex = context->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_shader_read.image = image.get();
				to_shader_read.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				to_shader_read.newLayout = vk::ImageLayout::eGeneral;
				to_shader_read.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_shader_read.subresourceRange = subresourceRange;

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_shader_read });
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
			m_shader_module[i] = loadShaderUnique(context->m_device, path + shader_info[i].name);
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
				m_descriptor_set_layout[i] = context->m_device.createDescriptorSetLayoutUnique(descriptor_set_layout_info);
			}

			vk::DescriptorSetLayout layouts[] = { m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE].get() };
			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.descriptorPool = context->m_descriptor_pool.get();
			alloc_info.descriptorSetCount = array_length(layouts);
			alloc_info.pSetLayouts = layouts;
			auto descriptor_set = context->m_device.allocateDescriptorSetsUnique(alloc_info);
			m_descriptor_set[DESCRIPTOR_SET_SOLDIER_UPDATE] = std::move(descriptor_set[0]);

			// update
			{

				std::vector<vk::DescriptorBufferInfo> uniforms = {
					m_boid_info_gpu.getInfo(),
					m_soldier_info_gpu.getInfo(),
				};
				std::vector<vk::DescriptorBufferInfo> storages = {
					m_brain_gpu.getOrg(),
					m_soldier_gpu.getOrg(),
					m_soldier_draw_indiret_gpu.getInfo(),
					m_soldier_LL_head_gpu.getOrg(),
					m_soldier_emit_data.getInfo(),
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
				context->m_device.updateDescriptorSets(write_desc, {});
			}
		}

		{
			{
				std::vector<vk::DescriptorSetLayout> layouts = {
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE].get(),
					sScene::Order().getDescriptorSetLayout(sScene::DESCRIPTOR_SET_LAYOUT_MAP),
					sSystem::Order().getSystemDescriptorLayout(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(layouts.size());
				pipeline_layout_info.setPSetLayouts(layouts.data());
				m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}
			{
				std::vector<vk::DescriptorSetLayout> layouts = {
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE].get(),
					sSystem::Order().getSystemDescriptorLayout(),
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
				m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}
			{
				std::vector<vk::DescriptorSetLayout> layouts = {
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
					sSystem::Order().getSystemDescriptorLayout(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(layouts.size());
				pipeline_layout_info.setPSetLayouts(layouts.data());
				m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}
		}

	}

	std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
	{
		vk::ComputePipelineCreateInfo()
		.setStage(
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader_module[SHADER_COMPUTE_UPDATE_SOLDIER].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eCompute))
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE].get()),
		vk::ComputePipelineCreateInfo()
		.setStage(
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader_module[SHADER_COMPUTE_EMIT_SOLDIER].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eCompute))
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT].get()),
	};
	auto compute_pipeline = context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
	m_pipeline[PIPELINE_COMPUTE_SOLDIER_UPDATE] = std::move(compute_pipeline[0]);
	m_pipeline[PIPELINE_COMPUTE_SOLDIER_EMIT] = std::move(compute_pipeline[1]);

	vk::Extent2D size = render_target->m_resolution;
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
		depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
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

		vk::PipelineShaderStageCreateInfo shader_info[] =
		{
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader_module[SHADER_VERTEX_DRAW_SOLDIER].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eVertex),
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader_module[SHADER_FRAGMENT_DRAW_SOLDIER].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eFragment),
		};
		std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
		{
			vk::GraphicsPipelineCreateInfo()
			.setStageCount(array_length(shader_info))
			.setPStages(shader_info)
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
		auto graphics_pipeline = context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
		m_pipeline[PIPELINE_GRAPHICS_SOLDIER_DRAW] = std::move(graphics_pipeline[0]);
	}
}
vk::CommandBuffer sBoid::execute(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	{
		{
			// transfer
			vk::BufferMemoryBarrier to_transfer = m_soldier_draw_indiret_gpu.makeMemoryBarrier();
			to_transfer.srcAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
			to_transfer.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

			cmd.updateBuffer<vk::DrawIndirectCommand>(m_soldier_draw_indiret_gpu.getInfo().buffer, m_soldier_draw_indiret_gpu.getInfo().offset, vk::DrawIndirectCommand(0, 1, 0, 0));
			cmd.fillBuffer(m_soldier_LL_head_gpu.getInfo(sGlobal::Order().getRenderIndex()).buffer, m_soldier_LL_head_gpu.getInfo(sGlobal::Order().getRenderIndex()).offset, m_soldier_LL_head_gpu.getInfo(sGlobal::Order().getRenderIndex()).range, 0xFFFFFFFF);

			vk::BufferMemoryBarrier to_read = m_soldier_draw_indiret_gpu.makeMemoryBarrier();
			to_read.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_read.dstAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}

		// update
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_SOLDIER_UPDATE].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE].get(), 0, getDescriptorSet(DESCRIPTOR_SET_SOLDIER_UPDATE), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE].get(), 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_UPDATE].get(), 2, sSystem::Order().getSystemDescriptorSet(), {0});

			auto groups = app::calcDipatchGroups(glm::uvec3(m_boid_info.m_soldier_max, 1, 1), glm::uvec3(1024, 1, 1));
			cmd.dispatch(groups.x, groups.y, groups.z);
		}

		// emit
		{

			if (!m_emit_data_cpu[sGlobal::Order().getRenderIndex()].empty())
			{
				auto data = m_emit_data_cpu[sGlobal::Order().getRenderIndex()].get();
				btr::BufferMemoryDescriptor desc;
				desc.size = vector_sizeof(data);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				memcpy_s(staging.getMappedPtr(), desc.size, data.data(), desc.size);

				vk::BufferCopy copy_info;
				copy_info.setSize(desc.size);
				copy_info.setSrcOffset(staging.getInfo().offset);
				copy_info.setDstOffset(m_soldier_emit_data.getInfo().offset);
				cmd.copyBuffer(staging.getInfo().buffer, m_soldier_emit_data.getInfo().buffer, copy_info);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_SOLDIER_EMIT].get());
				cmd.pushConstants<uvec2>(m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec2(data.size(), 0));
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT].get(), 0, getDescriptorSet(DESCRIPTOR_SET_SOLDIER_UPDATE), {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_EMIT].get(), 1, sSystem::Order().getSystemDescriptorSet(), { 0 });

// 				vk::BufferMemoryBarrier to_transfer = m_soldier_draw_indiret_gpu.makeMemoryBarrierEx();
// 				to_transfer.srcAccessMask = vk::AccessFlagBits::eIndirectCommandRead;
// 				to_transfer.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
// 				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_transfer, {});

				vk::BufferMemoryBarrier to_read = m_soldier_emit_data.makeMemoryBarrier();
				to_read.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_read.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

				cmd.dispatch(1, 1, 1);
			}
		}
	}
	cmd.end();
	return cmd;
}

vk::CommandBuffer sBoid::draw(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	auto to_draw = vk::BufferMemoryBarrier();
	to_draw.buffer = m_soldier_draw_indiret_gpu.getInfo().buffer;
	to_draw.size = m_soldier_draw_indiret_gpu.getInfo().range;
	to_draw.offset = m_soldier_draw_indiret_gpu.getInfo().offset;
	to_draw.srcAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
	to_draw.dstAccessMask = vk::AccessFlagBits::eIndirectCommandRead;
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, to_draw, {});

	vk::RenderPassBeginInfo begin_render_Info;
	begin_render_Info.setRenderPass(m_render_pass.get());
	begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_render_target->m_resolution));
	begin_render_Info.setFramebuffer(m_framebuffer.get());
	cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_GRAPHICS_SOLDIER_DRAW].get());
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW].get(), 0, getDescriptorSet(DESCRIPTOR_SET_SOLDIER_UPDATE), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_SOLDIER_DRAW].get(), 2, sSystem::Order().getSystemDescriptorSet(), { 0 });

	cmd.drawIndirect(m_soldier_draw_indiret_gpu.getInfo().buffer, m_soldier_draw_indiret_gpu.getInfo().offset, 1, sizeof(vk::DrawIndirectCommand));

	cmd.endRenderPass();
	cmd.end();

	return cmd;
}
