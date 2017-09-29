#include <applib/cLightPipeline.h>
#include <applib/cModelInstancingRender.h>

void cFowardPlusPipeline::Private::setup(const std::shared_ptr<btr::Context>& context)
{
	auto device = context->m_device;
	m_light_num = 1024;
	glm::uvec2 resolution_size(640, 480);
	glm::uvec2 tile_size(32, 32);
	glm::uvec2 tile_num(resolution_size / tile_size);
	m_light_info.m_resolution = resolution_size;
	m_light_info.m_tile_size = tile_size;
	m_light_info.m_tile_num = tile_num;
	auto tile_num_all = tile_num.x*tile_num.y;

	// ƒƒ‚ƒŠ‚ÌŠm•Û‚Æ‚©
	{
		{
			btr::UpdateBufferDescriptor desc;
			desc.device_memory = context->m_uniform_memory;
			desc.staging_memory = context->m_staging_memory;
			desc.element_num = 1;
			desc.frame_max = context->m_window->getSwapchain().getBackbufferNum();
			m_light_info_gpu.setup(desc);
		}
		{
			btr::UpdateBufferDescriptor desc;
			desc.device_memory = context->m_uniform_memory;
			desc.staging_memory = context->m_staging_memory;
			desc.element_num = 1;
			desc.frame_max = context->m_window->getSwapchain().getBackbufferNum();
			m_frustom_point.setup(desc);
		}
		{
			btr::UpdateBufferDescriptor desc;
			desc.device_memory = context->m_storage_memory;
			desc.staging_memory = context->m_staging_memory;
			desc.element_num = m_light_num;
			desc.frame_max = context->m_window->getSwapchain().getBackbufferNum();
			m_light.setup(desc);
		}
		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = sizeof(uint32_t) * tile_num_all;
			m_lightLL_head = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = sizeof(LightLL) * tile_num_all * m_light_num;
			m_lightLL = context->m_storage_memory.allocateMemory(desc);

		}
		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = sizeof(uint32_t);
			m_light_counter = context->m_storage_memory.allocateMemory(desc);

		}
	}


	// setup shader
	{
		const char* name[] = {
//			"MakeLight.comp.spv",
			"CullLight.comp.spv",
		};
		static_assert(array_length(name) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourceLibPath() + "shader\\binary\\";
		for (size_t i = 0; i < SHADER_NUM; i++) 
		{
			m_shader_module[i] = loadShaderUnique(device.getHandle(), path + name[i]);
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
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(8),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(9),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(10),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(11),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(12),
		};

		for (size_t i = 0; i < bindings.size(); i++)
		{
			vk::DescriptorSetLayoutCreateInfo descriptor_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings[i].size())
				.setPBindings(bindings[i].data());
			m_descriptor_set_layout[i] = device->createDescriptorSetLayoutUnique(descriptor_layout_info);
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
		auto descriptors = device->allocateDescriptorSetsUnique(descriptor_set_alloc_info);
		std::copy(std::make_move_iterator(descriptors.begin()), std::make_move_iterator(descriptors.end()), m_descriptor_set.begin());
	}

	// Create pipeline layout
	{
		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_LIGHT].get(),
		};
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
			.setSetLayoutCount(array_length(layouts))
			.setPSetLayouts(layouts);
		m_pipeline_layout[PIPELINE_CULL_LIGHT] = device->createPipelineLayoutUnique(pipelineLayoutInfo);

	}

	// Create pipeline
	{
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info = {
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_COMPUTE_CULL_LIGHT])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_CULL_LIGHT].get()),
		};

		auto p = device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		for (size_t i = 0; i < compute_pipeline_info.size(); i++) {
			m_pipeline[i] = std::move(p[i]);
		}

	}

	// LightCulling
	{
		std::vector<vk::DescriptorBufferInfo> uniforms =
		{
			m_light_info_gpu.getBufferInfo(),
			m_frustom_point.getBufferInfo(),
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
		device->updateDescriptorSets(desc, {});
		desc.setDescriptorType(vk::DescriptorType::eStorageBuffer);
		desc.setDescriptorCount(storages.size());
		desc.setPBufferInfo(storages.data());
		desc.setDstBinding(8);
		desc.setDstSet(m_descriptor_set[DESCRIPTOR_SET_LIGHT].get());
		device->updateDescriptorSets(desc, {});

	}
}
