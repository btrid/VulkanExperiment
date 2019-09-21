#include <applib/cLightPipeline.h>
#include <applib/sCameraManager.h>

void cFowardPlusPipeline::setup(const std::shared_ptr<btr::Context>& context)
{
	auto& device = context->m_device;
	m_light_info.m_light_max_num = 1024;
	glm::uvec2 resolution_size(640, 480);
	glm::uvec2 tile_size(32, 32);
	glm::uvec2 tile_num(resolution_size / tile_size);
	m_light_info.m_resolution = resolution_size;
	m_light_info.m_tile_size = tile_size;
	m_light_info.m_tile_num = tile_num;
	auto tile_num_all = tile_num.x*tile_num.y;

	// メモリの確保とか
	{
		{
			btr::UpdateBufferDescriptor desc;
			desc.device_memory = context->m_uniform_memory;
			desc.staging_memory = context->m_staging_memory;
			desc.element_num = 1;
			desc.frame_max = context->m_window->getSwapchain()->getBackbufferNum();
			m_light_info_gpu.setup(desc);
		}
		{
			btr::UpdateBufferDescriptor desc;
			desc.device_memory = context->m_storage_memory;
			desc.staging_memory = context->m_staging_memory;
			desc.element_num = m_light_info.m_light_max_num;
			desc.frame_max = context->m_window->getSwapchain()->getBackbufferNum();
			m_light.setup(desc);
		}
		{
			btr::BufferMemoryDescriptor desc;
			desc.size = sizeof(uint32_t) * tile_num_all;
			m_lightLL_head = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptor desc;
			desc.size = sizeof(LightLL) * tile_num_all * m_light_info.m_light_max_num;
			m_lightLL = context->m_storage_memory.allocateMemory(desc);

		}
		{
			btr::BufferMemoryDescriptor desc;
			desc.size = sizeof(uint32_t);
			m_light_counter = context->m_storage_memory.allocateMemory(desc);

		}
	}


	// setup shader
	{
		const char* name[] = {
			"CullLight.comp.spv",
		};
		static_assert(array_length(name) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourceLibPath() + "shader\\binary\\";
		for (size_t i = 0; i < SHADER_NUM; i++) 
		{
			m_shader_module[i] = loadShaderUnique(device, path + name[i]);
			m_shader_info[i].setModule(m_shader_module[i].get());
			m_shader_info[i].setStage(vk::ShaderStageFlagBits::eCompute);
			m_shader_info[i].setPName("main");
		}
	}

	// descriptor set
	{
		std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
		bindings[DESCRIPTOR_SET_LAYOUT_LIGHT] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(4),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(5),
		};

		for (size_t i = 0; i < bindings.size(); i++)
		{
			vk::DescriptorSetLayoutCreateInfo descriptor_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings[i].size())
				.setPBindings(bindings[i].data());
			m_descriptor_set_layout[i] = device.createDescriptorSetLayoutUnique(descriptor_layout_info);
		}
	}

	// descriptor set
	{
		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_LIGHT].get(),
		};
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(context->m_descriptor_pool.get());
		descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
		descriptor_set_alloc_info.setPSetLayouts(layouts);
		auto descriptors = device.allocateDescriptorSetsUnique(descriptor_set_alloc_info);
		std::copy(std::make_move_iterator(descriptors.begin()), std::make_move_iterator(descriptors.end()), m_descriptor_set.begin());

		// update descriptor_set
		{
			std::vector<vk::DescriptorBufferInfo> uniforms =
			{
				m_light_info_gpu.getBufferInfo(),
			};
			std::vector<vk::DescriptorBufferInfo> storages =
			{
				m_light.getBufferInfo(),
				m_lightLL_head.getBufferInfo(),
				m_lightLL.getBufferInfo(),
				m_light_counter.getBufferInfo(),
			};

			vk::WriteDescriptorSet desc;
			desc.setDescriptorType(vk::DescriptorType::eUniformBuffer);
			desc.setDescriptorCount(uniforms.size());
			desc.setPBufferInfo(uniforms.data());
			desc.setDstBinding(0);
			desc.setDstSet(m_descriptor_set[DESCRIPTOR_SET_LIGHT].get());
			device.updateDescriptorSets(desc, {});
			desc.setDescriptorType(vk::DescriptorType::eStorageBuffer);
			desc.setDescriptorCount(storages.size());
			desc.setPBufferInfo(storages.data());
			desc.setDstBinding(1);
			desc.setDstSet(m_descriptor_set[DESCRIPTOR_SET_LIGHT].get());
			device.updateDescriptorSets(desc, {});

		}
	}

	// Create pipeline layout
	{
		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_LIGHT].get(),
			sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
		};
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
			.setSetLayoutCount(array_length(layouts))
			.setPSetLayouts(layouts);
		m_pipeline_layout[PIPELINE_CULL_LIGHT] = device.createPipelineLayoutUnique(pipelineLayoutInfo);

	}

	// Create pipeline
	{
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info = {
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_COMPUTE_CULL_LIGHT])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_CULL_LIGHT].get()),
		};

		auto p = device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
		for (size_t i = 0; i < compute_pipeline_info.size(); i++) {
			m_pipeline[i] = std::move(p[i]);
		}

	}

}

vk::CommandBuffer cFowardPlusPipeline::execute(const std::shared_ptr<btr::Context>& context)
{
	// lightの更新
	{
		// 新しいライトの追加
		std::vector<std::unique_ptr<Light>> light;
		{
			std::lock_guard<std::mutex> lock(m_light_new_mutex);
			light = std::move(m_light_list_new);
		}
		m_light_list.insert(m_light_list.end(), std::make_move_iterator(light.begin()), std::make_move_iterator(light.end()));
		// 更新と寿命の尽きたライトの削除
		m_light_list.erase(std::remove_if(m_light_list.begin(), m_light_list.end(), [](auto& p) { return !p->update(); }), m_light_list.end());
	}


	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	// cpuからの更新のバリア
	{
		auto to_copy_barrier = m_light_counter.makeMemoryBarrierEx();
		to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
		to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});
	}
	{
		uint32_t zero = 0;
		cmd.updateBuffer<uint32_t>(m_light_counter.getBufferInfo().buffer, m_light_counter.getBufferInfo().offset, { zero });
	}

	{
		// ライトのデータをdeviceにコピー
		uint32_t light_num = 0;
		{
			auto* p = m_light.mapSubBuffer(context->getGPUFrame());
			for (auto& it : m_light_list)
			{
				assert(light_num < m_light_info.m_light_max_num);
				p[light_num] = it->getParam();
				light_num++;
			}

			m_light.flushSubBuffer(light_num, 0, context->getGPUFrame());
			auto copy_info = m_light.update(context->getGPUFrame());
			cmd.copyBuffer(m_light.getStagingBufferInfo().buffer, m_light.getBufferInfo().buffer, copy_info);
		}
		{

			m_light_info.m_active_light_num = light_num;
			m_light_info_gpu.subupdate(&m_light_info, 1, 0, context->getGPUFrame());
			auto copy_info = m_light_info_gpu.update(context->getGPUFrame());
			cmd.copyBuffer(m_light_info_gpu.getStagingBufferInfo().buffer, m_light_info_gpu.getBufferInfo().buffer, copy_info);

			auto to_shader_read_barrier = m_light_info_gpu.getBufferMemory().makeMemoryBarrierEx();
			to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
		}
	}

	{

		{
			std::vector<vk::BufferMemoryBarrier> barrier = {
				m_light_counter.makeMemoryBarrierEx()
				.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, barrier, {});
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_CULL_LIGHT].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_CULL_LIGHT].get(), 0, m_descriptor_set[DESCRIPTOR_SET_LIGHT].get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_CULL_LIGHT].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
		cmd.dispatch(1, 1, 1);
		{
			std::vector<vk::BufferMemoryBarrier> barrier = {
				m_lightLL_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite),
				m_lightLL.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite)
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, barrier, {});
		}
	}

	cmd.end();
	return cmd;
}
