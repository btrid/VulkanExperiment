#include <003_particle/Collision.h>
#include <003_particle/GameDefine.h>
#include <003_particle/sBoid.h>

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
				sBoid::Order().getDescriptorSetLayout(sBoid::DESCRIPTOR_SOLDIER_UPDATE),
				m_descriptor_set_layout[DESCRIPTOR_UPDATE],
				sScene::Order().m_descriptor_set_layout[sScene::DESCRIPTOR_LAYOUT_MAP],
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

}
