#include <013_2DGI/SV2D/SV2DLight.h>

namespace sv2d
{
	
SV2DLight::SV2DLight(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<SV2DContext>& gi2d_context)
{
	m_context = context;
	m_sv2d_context = gi2d_context;

	{
		b_light_count = context->m_storage_memory.allocateMemory<uvec4>({ 1,{} });
		b_light_data = context->m_storage_memory.allocateMemory<SV2DLightData>({ 4096, {} });
	}
	// descriptor_set
	{
		{
			auto stage = vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(1),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo storages[] = {
				b_light_count.getInfo(),
				b_light_data.getInfo(),
			};

			vk::WriteDescriptorSet write[] = {
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(0)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

	}

	{
		const char* name[] =
		{
			"PMMakeLightList.comp.spv",
		};
		static_assert(array_length(name) == array_length(m_shader), "not equal shader num");

		std::string path = btr::getResourceShaderPath();
		for (size_t i = 0; i < array_length(name); i++) {
			m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
		}

	}

	// pipeline layout
	{
		vk::DescriptorSetLayout layouts[] = {
			m_sv2d_context->getDescriptorSetLayout(),
		};
		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayoutPointLight] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}


	{
		vk::PipelineShaderStageCreateInfo shader_info[] =
		{
			vk::PipelineShaderStageCreateInfo()
			.setModule(m_shader[ShaderPointLight].get())
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eCompute),
		};

		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayoutPointLight].get()),
		};
		auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[PipelineLayoutPointLight] = std::move(compute_pipeline[0]);
	}

}

void SV2DLight::execute(vk::CommandBuffer cmd)
{
	vk::BufferMemoryBarrier to_write[] =
	{
		m_sv2d_context->b_emission_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead|vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferWrite),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, 0, nullptr, array_length(to_write), to_write, 0, nullptr);

	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineLayoutPointLight].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutPointLight].get(), 0, m_sv2d_context->getDescriptorSet(), {});
		cmd.dispatch(1, 1, 1);

	}
	vk::BufferMemoryBarrier to_read[] =
	{
		m_sv2d_context->b_emission_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
		m_sv2d_context->b_emission_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);
}

}