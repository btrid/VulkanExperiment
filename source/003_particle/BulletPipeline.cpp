#include <003_particle/BulletPipeline.h>

void sBulletSystem::Private::setup(btr::Loader& loader)
{

	m_particle_info_cpu.m_max_num = 8192;
	m_particle_info_cpu.m_emit_max_num = 1024;

	{
		{
			btr::BufferMemory::Descriptor data_desc;
			data_desc.size = sizeof(BulletData) * m_particle_info_cpu.m_max_num;
			m_particle = loader.m_storage_memory.allocateMemory(data_desc);
			std::vector<BulletData> p(m_particle_info_cpu.m_max_num);
			loader.m_cmd.fillBuffer(m_particle.getBufferInfo().buffer, m_particle.getBufferInfo().offset, m_particle.getBufferInfo().range, 0u);
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
				Scene::Order().m_descriptor_set_layout[Scene::DESCRIPTOR_LAYOUT_MAP],
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
				Scene::Order().m_descriptor_set_layout[Scene::DESCRIPTOR_LAYOUT_CAMERA],
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
			m_pipeline_layout[PIPELINE_LAYOUT_PARTICLE_DRAW] = loader.m_device->createPipelineLayout(pipeline_layout_info);
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

	}
}

