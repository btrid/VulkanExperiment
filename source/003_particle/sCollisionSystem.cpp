#include <003_particle/sCollisionSystem.h>
#include <003_particle/sScene.h>
#include <003_particle/sBoid.h>
#include <003_particle/sBulletSystem.h>

void sCollisionSystem::setup(std::shared_ptr<btr::Loader>& loader)
{
	{
		struct ShaderDesc {
			const char* name;
			vk::ShaderStageFlagBits stage;
		} shader_desc[] =
		{
			{ "CollisionTest.comp.spv", vk::ShaderStageFlagBits::eCompute },
		};
		static_assert(array_length(shader_desc) == SHADER_NUM, "not equal shader num");
		std::string path = btr::getResourceAppPath() + "shader\\binary\\";
		for (uint32_t i = 0; i < SHADER_NUM; i++)
		{
			m_shader_info[i].setModule(loadShader(loader->m_device.getHandle(), path + shader_desc[i].name));
			m_shader_info[i].setStage(shader_desc[i].stage);
			m_shader_info[i].setPName("main");
		}
	}

	{
		// descriptor set layout
		std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_NUM);
		bindings[DESCRIPTOR_COLLISION_TEST] =
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
		};

		for (size_t i = 0; i < DESCRIPTOR_NUM; i++)
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
			std::vector<vk::DescriptorSetLayout> layouts = 
			{
				sBoid::Order().getDescriptorSetLayout(sBoid::DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE),
				sBulletSystem::Order().getDescriptorSetLayout(sBulletSystem::DESCRIPTOR_SET_LAYOUT_UPDATE),
				sScene::Order().getDescriptorSetLayout(sScene::DESCRIPTOR_SET_LAYOUT_MAP),
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
			m_pipeline_layout[PIPELINE_LAYOUT_COLLISION] = loader->m_device->createPipelineLayout(pipeline_layout_info);

		}
	}

	{
		// Create pipeline
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_COMPUTE_COLLISION])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COLLISION]),
		};
		auto compute_pipeline = loader->m_device->createComputePipelines(loader->m_cache, compute_pipeline_info);
		m_pipeline[PIPELINE_LAYOUT_COLLISION] = compute_pipeline[0];

	}
}

void sCollisionSystem::execute(std::shared_ptr<btr::Executer>& executer)
{
	{
		std::vector<vk::BufferMemoryBarrier> to_read = {
			sBoid::Order().getLL().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
			sBulletSystem::Order().getLL().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
			sBoid::Order().getSoldier().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead| vk::AccessFlagBits::eShaderWrite),
			sBulletSystem::Order().getBullet().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		executer->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

		// update
		struct UpdateConstantBlock
		{
			float m_deltatime;
			uint m_double_buffer_index;
		};
		UpdateConstantBlock block;
		block.m_deltatime = sGlobal::Order().getDeltaTime();
		block.m_double_buffer_index = sGlobal::Order().getGPUIndex();
		executer->m_cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[PIPELINE_LAYOUT_COLLISION], vk::ShaderStageFlagBits::eCompute, 0, block);
		executer->m_cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_COLLISION], 0, sBoid::Order().getDescriptorSet(sBoid::DESCRIPTOR_SET_SOLDIER_UPDATE), {});
		executer->m_cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_COLLISION], 1, sBulletSystem::Order().getDescriptorSet(sBulletSystem::DESCRIPTOR_SET_UPDATE), {});
		executer->m_cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_COLLISION], 2, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});

		executer->m_cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COLLISION_TEST]);
		executer->m_cmd.dispatch(1, 1, 1);


		std::vector<vk::BufferMemoryBarrier> to = {
			sBoid::Order().getSoldier().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
			sBulletSystem::Order().getBullet().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
		};
		executer->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to, {});
	}

}
