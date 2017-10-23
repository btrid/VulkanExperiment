#include <999_game/sLightSystem.h>
#include <999_game/sScene.h>
#include <999_game/sBoid.h>
#include <999_game/sBulletSystem.h>
#include <applib/sSystem.h>
#include <applib/sParticlePipeline.h>

void sLightSystem::setup(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	m_tile_info_cpu.m_resolusion = uvec2(640, 480);
	m_tile_info_cpu.m_tile_num = uvec2(32);
	m_tile_info_cpu.m_tile_index_map_max = 256;
	m_tile_info_cpu.m_tile_buffer_max_num = m_tile_info_cpu.m_tile_index_map_max * m_tile_info_cpu.m_tile_num.x*m_tile_info_cpu.m_tile_num.y;
	{
		uint32_t num = 8192;
		{
			btr::BufferMemoryDescriptorEx<LightInfo> desc;
			desc.element_num = 1;
			m_light_info = context->m_uniform_memory.allocateMemory(desc);

			auto staging = context->m_staging_memory.allocateMemory(desc);
			staging.getMappedPtr()->m_max_num = num;
			vk::BufferCopy copy;
			copy.setSize(staging.getBufferInfo().range);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			copy.setDstOffset(m_light_info.getBufferInfo().offset);

			cmd->copyBuffer(staging.getBufferInfo().buffer, m_light_info.getBufferInfo().buffer, copy);
		}
		{
			btr::BufferMemoryDescriptorEx<TileInfo> desc;
			desc.element_num = 1;
			m_tile_info = context->m_uniform_memory.allocateMemory(desc);

			auto staging = context->m_staging_memory.allocateMemory(desc);
			*staging.getMappedPtr() = m_tile_info_cpu;
			vk::BufferCopy copy;
			copy.setSize(staging.getBufferInfo().range);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			copy.setDstOffset(m_tile_info.getBufferInfo().offset);

			cmd->copyBuffer(staging.getBufferInfo().buffer, m_tile_info.getBufferInfo().buffer, copy);
		}
		{
			btr::BufferMemoryDescriptorEx<LightData> desc;
			desc.element_num = num;
			m_light_data = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<uint32_t> desc;
			desc.element_num = 1;
			m_light_data_counter = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<uint32_t> desc;
			desc.element_num = m_tile_info_cpu.m_tile_num.x*m_tile_info_cpu.m_tile_num.y;
			m_tile_data_counter = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<uint32_t> desc;
			desc.element_num = m_tile_info_cpu.m_tile_buffer_max_num;
			m_tile_data_map = context->m_storage_memory.allocateMemory(desc);
		}

	}
	{
		struct ShaderDesc {
			const char* name;
			vk::ShaderStageFlagBits stage;
		} shader_desc[] =
		{
			{ "LightCollectParticle.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ "LightCollectBullet.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ "LightTileCulling.comp.spv", vk::ShaderStageFlagBits::eCompute },
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
		auto use_stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
		bindings[DESCRIPTOR_SET_LAYOUT_LIGHT] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(use_stage)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(use_stage)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(use_stage)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(use_stage)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(use_stage)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(4),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(use_stage)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(5),
		};

		for (size_t i = 0; i < DESCRIPTOR_SET_LAYOUT_NUM; i++)
		{
			vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings[i].size())
				.setPBindings(bindings[i].data());
			m_descriptor_set_layout[i] = context->m_device->createDescriptorSetLayoutUnique(descriptor_set_layout_info);
		}

		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_LIGHT].get(),
		};
		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = context->m_descriptor_pool.get();
		alloc_info.descriptorSetCount = array_length(layouts);
		alloc_info.pSetLayouts = layouts;
		auto descriptor_set = context->m_device->allocateDescriptorSetsUnique(alloc_info);
		std::copy(std::make_move_iterator(descriptor_set.begin()), std::make_move_iterator(descriptor_set.end()), m_descriptor_set.begin());

		{

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_light_info.getBufferInfo(),
				m_tile_info.getBufferInfo(),
			};
			std::vector<vk::DescriptorBufferInfo> storages = {
				m_light_data.getBufferInfo(),
				m_light_data_counter.getBufferInfo(),
				m_tile_data_counter.getBufferInfo(),
				m_tile_data_map.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount((uint32_t)uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_LIGHT].get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(2)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_LIGHT].get()),
			};
			context->m_device->updateDescriptorSets(write_desc, {});
		}	
	}

	{
		{
			std::vector<vk::DescriptorSetLayout> layouts =
			{
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_LIGHT].get(),
				sParticlePipeline::Order().getDescriptorSetLayout(sParticlePipeline::DESCRIPTOR_SET_LAYOUT_PARTICLE),
				sSystem::Order().getSystemDescriptor().getLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_PARTICLE_COLLECT] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

		}
		{
			std::vector<vk::DescriptorSetLayout> layouts =
			{
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_LIGHT].get(),
				sBulletSystem::Order().getDescriptorSetLayout(sBulletSystem::DESCRIPTOR_SET_LAYOUT_UPDATE),
				sSystem::Order().getSystemDescriptor().getLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_BULLET_COLLECT] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

		}
		{
			std::vector<vk::DescriptorSetLayout> layouts =
			{
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_LIGHT].get(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_TILE_CULLING] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

		}
	}

	{
		// Create pipeline
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_PARTICLE_COLLECT])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_PARTICLE_COLLECT].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_BULLET_COLLECT])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_BULLET_COLLECT].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_TILE_CULLING])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_TILE_CULLING].get()),
		};
		auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[PIPELINE_PARTICLE_COLLECT] = std::move(compute_pipeline[0]);
		m_pipeline[PIPELINE_BULLET_COLLECT] = std::move(compute_pipeline[1]);
		m_pipeline[PIPELINE_TILE_CULLING] = std::move(compute_pipeline[2]);

	}
}

vk::CommandBuffer sLightSystem::execute(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
	{
		{
			auto to_transfer = m_light_data_counter.makeMemoryBarrier();
			to_transfer.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			auto to_transfer2 = m_tile_data_counter.makeMemoryBarrier();
			to_transfer2.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_transfer2.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { to_transfer, to_transfer2 }, {});

			cmd.fillBuffer(m_light_data_counter.getBufferInfo().buffer, m_light_data_counter.getBufferInfo().offset, m_light_data_counter.getBufferInfo().range, 0u);
			cmd.fillBuffer(m_tile_data_counter.getBufferInfo().buffer, m_tile_data_counter.getBufferInfo().offset, m_tile_data_counter.getBufferInfo().range, 0u);

			auto to_write = m_light_data_counter.makeMemoryBarrier();
			to_write.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_write.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_write, {});
		}

	}
	// collect
	{
		auto to_read = sBulletSystem::Order().getBullet().makeMemoryBarrierEx();
		to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
		to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

		// update
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_BULLET_COLLECT].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_BULLET_COLLECT].get(), 0, m_descriptor_set[DESCRIPTOR_SET_LAYOUT_LIGHT].get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_BULLET_COLLECT].get(), 1, sBulletSystem::Order().getDescriptorSet(sBulletSystem::DESCRIPTOR_SET_UPDATE), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_BULLET_COLLECT].get(), 2, sSystem::Order().getSystemDescriptor().getSet(), {});

		cmd.dispatch(1, 1, 1);
	}

	// culling
	{

		{
			auto to_read = m_light_data.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}

		// update
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_TILE_CULLING].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_TILE_CULLING].get(), 0, m_descriptor_set[DESCRIPTOR_SET_LAYOUT_LIGHT].get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_TILE_CULLING].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});

		cmd.dispatch(32, 32, 1);

		{
			auto to_read = m_tile_data_counter.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}
	}


	cmd.end();
	return cmd;

}
