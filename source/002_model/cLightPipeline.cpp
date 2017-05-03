#include <002_model/cLightPipeline.h>
#include <002_model/cModelRender.h>

void cFowardPlusPipeline::Private::setup(cModelRenderer& renderer)
{
	m_device = renderer.m_device;
	m_light_num = 1024;

	glm::uvec2 resolution_size(640, 480);
	glm::uvec2 tile_size(32, 32);
	glm::uvec2 tile_num(resolution_size / tile_size);
	m_light_info.m_resolution = resolution_size;
	m_light_info.m_tile_size = tile_size;
	m_light_info.m_tile_num = tile_num;
	auto tile_num_all = tile_num.x*tile_num.y;
	m_storage_memory = renderer.m_storage_memory;
	if (!m_storage_memory.isValid())
	{
		vk::DeviceSize size = sizeof(LightParam) * m_light_num;
		size += sizeof(uint32_t) * tile_num_all;
		size += sizeof(glm::uvec2) * tile_num_all * m_light_num;
		size += tile_num.x * tile_num.y * sizeof(Frustom2);
		size += sizeof(uint32_t) * 4;
		m_storage_memory.setup(m_device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, size);
	}

	m_uniform_memory.setup(m_device, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, 1024);


	// �������̊m�ۂƂ�
	{
		m_light = m_storage_memory.allocateMemory(sizeof(LightParam) * m_light_num);
		m_lightLL_head = m_storage_memory.allocateMemory(sizeof(uint32_t) * tile_num_all);
		m_lightLL = m_storage_memory.allocateMemory(sizeof(glm::uvec2) * tile_num_all * m_light_num);
		m_light_counter = m_storage_memory.allocateMemory(sizeof(uint32_t));
		m_tiled_frustom = m_storage_memory.allocateMemory(tile_num.x * tile_num.y * sizeof(Frustom2));

		vk::DeviceSize alloc_size = m_light.getSize()*sGlobal::FRAME_MAX;
		alloc_size += m_lightLL_head.getSize();
		alloc_size += m_tiled_frustom.getSize() * sGlobal::FRAME_MAX;
		m_staging_memory.setup(m_device, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached, alloc_size);
	}

	m_light_cpu.setup(m_staging_memory, m_light.getSize()*sGlobal::FRAME_MAX);
	m_light_info_gpu.setup(m_uniform_memory, m_staging_memory);
	m_frustom_point.setup(m_uniform_memory, m_staging_memory);

	// setup shader
	{
		const char* name[] = {
			"MakeFrustom.comp.spv",
//			"MakeLight.comp.spv",
			"CullLight.comp.spv",
		};
		static_assert(array_length(name) == COMPUTE_NUM, "not equal shader num");

		std::string path = btr::getResourcePath() + "shader\\binary\\";
		for (size_t i = 0; i < COMPUTE_NUM; i++) {
			m_shader_info[i].setModule(loadShader(m_device.getHandle(), path + name[i]));
			m_shader_info[i].setStage(vk::ShaderStageFlagBits::eCompute);
			m_shader_info[i].setPName("main");
		}
	}

	{
		// Create compute pipeline
		std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings;
		// MakeFrustom
		std::vector<vk::DescriptorSetLayoutBinding> binding =
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
		};
		bindings.push_back(binding);
		binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
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
		bindings.push_back(binding);

		for (size_t i = 0; i < bindings.size(); i++)
		{
			vk::DescriptorSetLayoutCreateInfo descriptor_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings[i].size())
				.setPBindings(bindings[i].data());
			auto descriptor_set_layout = m_device->createDescriptorSetLayout(descriptor_layout_info);

			vk::PipelineLayoutCreateInfo pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
				.setSetLayoutCount(1)
				.setPSetLayouts(&descriptor_set_layout);
			m_pipeline_layout[i] = m_device->createPipelineLayout(pipelineLayoutInfo);
			m_descriptor_set_layout[i] = descriptor_set_layout;
		}

		// DescriptorPool
		{
			std::vector<vk::DescriptorPoolSize> descriptor_pool_size;
			for (auto& binding : bindings)
			{
				for (auto& buffer : binding)
				{
					descriptor_pool_size.emplace_back(buffer.descriptorType, buffer.descriptorCount);
				}
			}
			vk::DescriptorPoolCreateInfo descriptor_pool_info;
			descriptor_pool_info.maxSets = bindings.size();
			descriptor_pool_info.poolSizeCount = descriptor_pool_size.size();
			descriptor_pool_info.pPoolSizes = descriptor_pool_size.data();

			m_descriptor_pool = m_device->createDescriptorPool(descriptor_pool_info);
		}


		// pipeline cache
		{
			vk::PipelineCacheCreateInfo cacheInfo = vk::PipelineCacheCreateInfo();
			m_cache = m_device->createPipelineCache(cacheInfo);
		}
		// Create pipeline
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info = {
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[0])
			.setLayout(m_pipeline_layout[0]),
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[1])
			.setLayout(m_pipeline_layout[1]),
		};

		auto p = m_device->createComputePipelines(m_cache, compute_pipeline_info);
		for (size_t i = 0; i < compute_pipeline_info.size(); i++) {
			m_pipeline[i] = p[i];
		}
	}
	vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
	descriptor_set_alloc_info.setDescriptorPool(m_descriptor_pool);
	descriptor_set_alloc_info.setDescriptorSetCount(m_descriptor_set_layout.size());
	descriptor_set_alloc_info.setPSetLayouts(m_descriptor_set_layout.data());
	m_compute_descriptor_set = m_device->allocateDescriptorSets(descriptor_set_alloc_info);

	// MakeFrustom
	{
		std::vector<vk::DescriptorBufferInfo> uniforms =
		{
			m_light_info_gpu.getBufferInfo(),
			m_frustom_point.getBufferInfo(),
		};
		std::vector<vk::DescriptorBufferInfo> storages =
		{
			m_tiled_frustom.getBufferInfo(),
		};

		vk::WriteDescriptorSet desc;
		desc.setDescriptorType(vk::DescriptorType::eUniformBuffer);
		desc.setDescriptorCount(uniforms.size());
		desc.setPBufferInfo(uniforms.data());
		desc.setDstBinding(0);
		desc.setDstSet(m_compute_descriptor_set[COMPUTE_MAKE_FRUSTOM]);
		m_device->updateDescriptorSets(desc, {});
		desc.setDescriptorType(vk::DescriptorType::eStorageBuffer);
		desc.setDescriptorCount(storages.size());
		desc.setPBufferInfo(storages.data());
		desc.setDstBinding(8);
		desc.setDstSet(m_compute_descriptor_set[COMPUTE_MAKE_FRUSTOM]);
		m_device->updateDescriptorSets(desc, {});
	}
	// LightCulling
	{
		std::vector<vk::DescriptorBufferInfo> uniforms =
		{
			m_light_info_gpu.getBufferInfo(),
		};
		std::vector<vk::DescriptorBufferInfo> storages =
		{
			m_tiled_frustom.getBufferInfo(),
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
		desc.setDstSet(m_compute_descriptor_set[COMPUTE_CULL_LIGHT]);
		m_device->updateDescriptorSets(desc, {});
		desc.setDescriptorType(vk::DescriptorType::eStorageBuffer);
		desc.setDescriptorCount(storages.size());
		desc.setPBufferInfo(storages.data());
		desc.setDstBinding(8);
		desc.setDstSet(m_compute_descriptor_set[COMPUTE_CULL_LIGHT]);
		m_device->updateDescriptorSets(desc, {});

	}
}
