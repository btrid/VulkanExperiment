#include <999_game/sBulletSystem.h>
#include <applib/sSystem.h>

void sBulletSystem::setup(std::shared_ptr<btr::Context>& context)
{

	m_bullet_info_cpu.m_max_num = 8192/2;
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	{
		// レンダーパス
		m_render_pass = app::g_app_instance->m_window->getRenderBackbufferPass();
	}

	{
		{
			btr::BufferMemoryDescriptorEx<BulletData> data_desc;
			data_desc.element_num = m_bullet_info_cpu.m_max_num * 2;
			m_bullet = context->m_storage_memory.allocateMemory(data_desc);

			data_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			BulletData bd;
			bd.m_life = -1.f;
			std::vector<BulletData> p(m_bullet_info_cpu.m_max_num * 2, bd);
			auto staging = context->m_staging_memory.allocateMemory(data_desc);
			memcpy_s(staging.getMappedPtr(), vector_sizeof(p), p.data(), vector_sizeof(p));
			vk::BufferCopy copy_info;
			copy_info.setSize(vector_sizeof(p));
			copy_info.setSrcOffset(staging.getInfo().offset);
			copy_info.setDstOffset(m_bullet.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, m_bullet.getInfo().buffer, copy_info);

			auto to_compute = m_bullet.makeMemoryBarrier();
			to_compute.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_compute.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_compute }, {});
		}
		{
			btr::BufferMemoryDescriptorEx<BulletData> desc;
			desc.element_num = 1024;
			m_bullet_emit = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<uint32_t> desc;
			desc.element_num = 1;
			m_bullet_emit_count = context->m_storage_memory.allocateMemory(desc);
		}

		{
			btr::BufferMemoryDescriptorEx<vk::DrawIndirectCommand> desc;
			desc.element_num = 1;
			m_bullet_counter = context->m_storage_memory.allocateMemory(desc);
			cmd.updateBuffer<vk::DrawIndirectCommand>(m_bullet_counter.getInfo().buffer, m_bullet_counter.getInfo().offset, vk::DrawIndirectCommand(4, 0, 0, 0));
			auto count_barrier = m_bullet_counter.makeMemoryBarrier();
			count_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			count_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, count_barrier, {});
		}

		{
			btr::BufferMemoryDescriptorEx<BulletInfo> desc;
			desc.element_num = 1;
			m_bullet_info = context->m_uniform_memory.allocateMemory(desc);
			cmd.updateBuffer<BulletInfo>(m_bullet_info.getInfo().buffer, m_bullet_info.getInfo().offset, { m_bullet_info_cpu });
			auto barrier = m_bullet_info.makeMemoryBarrier();
			barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { barrier }, {});
		}

		{
			auto& map_desc = sScene::Order().m_map_info_cpu.m_descriptor[0];
			btr::BufferMemoryDescriptorEx<uint32_t> desc;
			desc.element_num = map_desc.m_cell_num.x* map_desc.m_cell_num.y;
			m_bullet_LL_head_gpu = context->m_storage_memory.allocateMemory(desc);
			cmd.fillBuffer(m_bullet_LL_head_gpu.getInfo().buffer, m_bullet_LL_head_gpu.getInfo().offset, m_bullet_LL_head_gpu.getInfo().range, 0xFFFFFFFF);
		}

	}

	{
		struct ShaderDesc {
			const char* name;
			vk::ShaderStageFlagBits stage;
		} shader_desc[] =
		{
			{"BulletUpdate.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{"BulletEmit.comp.spv",	 vk::ShaderStageFlagBits::eCompute },
			{"BulletRender.vert.spv", vk::ShaderStageFlagBits::eVertex },
			{"BulletRender.frag.spv", vk::ShaderStageFlagBits::eFragment },
		};
		static_assert(array_length(shader_desc) == SHADER_NUM, "not equal shader num");
		std::string path = btr::getResourceAppPath() + "shader\\binary\\";
		for (uint32_t i = 0; i < SHADER_NUM; i++)
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
		bindings[DESCRIPTOR_SET_LAYOUT_UPDATE] =
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
		};

		for (size_t i = 0; i < bindings.size(); i++)
		{
			vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings[i].size())
				.setPBindings(bindings[i].data());
			m_descriptor_set_layout[i] = context->m_device->createDescriptorSetLayoutUnique(descriptor_set_layout_info);
		}

		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_UPDATE].get(),
		};
		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = context->m_descriptor_pool.get();
		alloc_info.descriptorSetCount = array_length(layouts);
		alloc_info.pSetLayouts = layouts;
		auto descriptor_set = context->m_device->allocateDescriptorSetsUnique(alloc_info);
		std::copy(std::make_move_iterator(descriptor_set.begin()), std::make_move_iterator(descriptor_set.end()), m_descriptor_set.begin());
	}
	{

		std::vector<vk::DescriptorBufferInfo> uniforms = {
			m_bullet_info.getInfo(),
		};
		std::vector<vk::DescriptorBufferInfo> storages = {
			m_bullet.getInfo(),
			m_bullet_counter.getInfo(),
			m_bullet_emit.getInfo(),
			m_bullet_LL_head_gpu.getInfo(),
			m_bullet_emit_count.getInfo(),
		};
		std::vector<vk::WriteDescriptorSet> write_desc =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount((uint32_t)uniforms.size())
			.setPBufferInfo(uniforms.data())
			.setDstBinding(0)
			.setDstSet(m_descriptor_set[DESCRIPTOR_SET_UPDATE].get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(storages.size())
			.setPBufferInfo(storages.data())
			.setDstBinding(1)
			.setDstSet(m_descriptor_set[DESCRIPTOR_SET_UPDATE].get()),
		};
		context->m_device->updateDescriptorSets(write_desc, {});
	}

	// pipeline layout
	{
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_UPDATE].get(),
				sScene::Order().getDescriptorSetLayout(sScene::DESCRIPTOR_SET_LAYOUT_MAP),
				sSystem::Order().getSystemDescriptorLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_UPDATE] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_UPDATE].get(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				sSystem::Order().getSystemDescriptorLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}


	}
	{
		// Create pipeline
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_COMPUTE_UPDATE])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_COMPUTE_EMIT])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get()),
		};
		auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[PIPELINE_COMPUTE_UPDATE] = std::move(compute_pipeline[0]);
		m_pipeline[PIPELINE_COMPUTE_EMIT] = std::move(compute_pipeline[1]);

		// pipeline
		{
			std::vector<vk::PipelineShaderStageCreateInfo> shader_info =
			{
				m_shader_info[SHADER_VERTEX_PARTICLE],
				m_shader_info[SHADER_FRAGMENT_PARTICLE],
			};

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleStrip),
			};

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)m_render_pass->getResolution().width, (float)m_render_pass->getResolution().height, 0.f, 1.f);
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), m_render_pass->getResolution())
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

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount((uint32_t)shader_info.size())
				.setPStages(shader_info.data())
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
			auto graphics_pipeline = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[PIPELINE_GRAPHICS_RENDER] = std::move(graphics_pipeline[0]);

		}
	}

}

vk::CommandBuffer sBulletSystem::execute(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	// エミット
	{
		auto data = m_data[sGlobal::Order().getRenderIndex()].get();
		if (!data.empty())
		{
			{
				auto to_write = m_bullet_emit_count.makeMemoryBarrier();
				to_write.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				to_write.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_write, {});

				cmd.updateBuffer<uint32_t>(m_bullet_emit_count.getInfo().buffer, m_bullet_emit_count.getInfo().offset, uint32_t(data.size()));

				auto to_read = m_bullet_emit_count.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
			}
			{
				auto to_read = m_bullet_counter.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
				to_read.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_read, {});
			}

			auto to_transfer = m_bullet_emit.makeMemoryBarrier();
			to_transfer.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

			btr::BufferMemoryDescriptor desc;
			desc.size = vector_sizeof(data);
			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			memcpy(staging.getMappedPtr(), data.data(), desc.size);

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setSrcOffset(staging.getInfo().offset);
			copy.setDstOffset(m_bullet_emit.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, m_bullet_emit.getInfo().buffer, copy);

			auto to_read = m_bullet_emit.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_EMIT].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_UPDATE].get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 2, sSystem::Order().getSystemDescriptorSet(), {0});

			auto groups = app::calcDipatchGroups(glm::uvec3(data.size(), 1, 1), glm::uvec3(1024, 1, 1));
			cmd.dispatch(groups.x, groups.y, groups.z);
		}
	}

	{
		// 初期化
		auto to_clear = m_bullet_counter.makeMemoryBarrier();
		to_clear.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
		to_clear.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_clear, {});

		cmd.fillBuffer(m_bullet_LL_head_gpu.getInfo().buffer, m_bullet_LL_head_gpu.getInfo().offset, m_bullet_LL_head_gpu.getInfo().range, 0xFFFFFFFF);
		cmd.updateBuffer<vk::DrawIndirectCommand>(m_bullet_counter.getInfo().buffer, m_bullet_counter.getInfo().offset, vk::DrawIndirectCommand(4, 0, 0, 0));

		auto to_read = m_bullet_counter.makeMemoryBarrier();
		to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
		to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_read }, {});
	}
	{
		auto to_read = m_bullet.makeMemoryBarrier();
		to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
		to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_read }, {});

		// update
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_UPDATE].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_UPDATE].get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get(), 2, sSystem::Order().getSystemDescriptorSet(), {0});

		auto groups = app::calcDipatchGroups(glm::uvec3(m_bullet_info_cpu.m_max_num, 1, 1), glm::uvec3(1024, 1, 1));
		cmd.dispatch(groups.x, groups.y, groups.z);

	}

	cmd.end();
	return cmd;
}

vk::CommandBuffer sBulletSystem::draw(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	{
		auto to_draw = m_bullet_counter.makeMemoryBarrier();
		to_draw.setSrcAccessMask(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite);
		to_draw.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { to_draw }, {});
	}

	vk::RenderPassBeginInfo begin_render_Info;
	begin_render_Info.setRenderPass(m_render_pass->getRenderPass());
	begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_render_pass->getResolution()));
	begin_render_Info.setFramebuffer(m_render_pass->getFramebuffer(context->getGPUFrame()));
	cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_GRAPHICS_RENDER].get());
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 0, m_descriptor_set[DESCRIPTOR_SET_UPDATE].get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 2, sSystem::Order().getSystemDescriptorSet(), {0});

	cmd.drawIndirect(m_bullet_counter.getInfo().buffer, m_bullet_counter.getInfo().offset, 1, sizeof(vk::DrawIndirectCommand));

	cmd.endRenderPass();
	cmd.end();
	return cmd;
}
