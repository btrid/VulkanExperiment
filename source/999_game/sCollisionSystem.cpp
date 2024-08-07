#include <999_game/sCollisionSystem.h>
#include <999_game/sScene.h>
#include <999_game/sBoid.h>
#include <999_game/sBulletSystem.h>
#include <applib/sSystem.h>

void sCollisionSystem::setup(std::shared_ptr<btr::Context>& context)
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
			m_shader_module[i] = loadShaderUnique(context->m_device, path + shader_desc[i].name);
			m_shader_info[i].setModule(m_shader_module[i].get());
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
			m_descriptor_set_layout[i] = context->m_device.createDescriptorSetLayoutUnique(descriptor_set_layout_info);
		}

		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_set_layout[DESCRIPTOR_LAYOUT_COLLISION_TEST].get(),
		};
		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = context->m_descriptor_pool.get();
		alloc_info.descriptorSetCount = array_length(layouts);
		alloc_info.pSetLayouts = layouts;
		auto descriptor_set = context->m_device.allocateDescriptorSetsUnique(alloc_info);
		std::copy(std::make_move_iterator(descriptor_set.begin()), std::make_move_iterator(descriptor_set.end()), m_descriptor_set.begin());

	}

	{
		{
			std::vector<vk::DescriptorSetLayout> layouts = 
			{
				sBoid::Order().getDescriptorSetLayout(sBoid::DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE),
				sBulletSystem::Order().getDescriptorSetLayout(sBulletSystem::DESCRIPTOR_SET_LAYOUT_UPDATE),
				sScene::Order().getDescriptorSetLayout(sScene::DESCRIPTOR_SET_LAYOUT_MAP),
				sSystem::Order().getSystemDescriptorLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_COLLISION] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

		}
	}

	{
		// Create pipeline
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(m_shader_info[SHADER_COMPUTE_COLLISION])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COLLISION].get()),
		};
		auto compute_pipeline = context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
		m_pipeline[PIPELINE_LAYOUT_COLLISION] = std::move(compute_pipeline[0]);

	}
}

vk::CommandBuffer sCollisionSystem::execute(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
	{
		std::vector<vk::BufferMemoryBarrier> to_read = {
			sBoid::Order().getLL().makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			sBoid::Order().getSoldier().makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead| vk::AccessFlagBits::eShaderWrite),
			sBulletSystem::Order().getBullet().makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

		// update
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_COLLISION].get(), 0, sBoid::Order().getDescriptorSet(sBoid::DESCRIPTOR_SET_SOLDIER_UPDATE), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_COLLISION].get(), 1, sBulletSystem::Order().getDescriptorSet(sBulletSystem::DESCRIPTOR_SET_UPDATE), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_COLLISION].get(), 2, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_COLLISION].get(), 3, sSystem::Order().getSystemDescriptorSet(), {0});

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COLLISION_TEST].get());
		cmd.dispatch(1, 1, 1);

		std::vector<vk::BufferMemoryBarrier> to = {
			sBoid::Order().getSoldier().makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			sBulletSystem::Order().getBullet().makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to, {});
	}
	cmd.end();
	return cmd;

}
