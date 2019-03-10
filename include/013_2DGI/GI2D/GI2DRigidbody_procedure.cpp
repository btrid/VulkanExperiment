#pragma once
#include "GI2DRigidbody_procedure.h"
#include "GI2DPhysicsWorld.h"
#include "GI2DRigidbody.h"

GI2DRigidbody_procedure::GI2DRigidbody_procedure(const std::shared_ptr<PhysicsWorld>& world)
{
	m_world = world;
	{
		const char* name[] =
		{
			"Rigid_CollisionDetective.comp.spv",
			"Rigid_CollisionDetectiveBefore.comp.spv",
			"Rigid_CalcForce.comp.spv",
			"Rigid_Integrate.comp.spv",
			"Rigid_ToFragment.comp.spv",
		};
		static_assert(array_length(name) == Shader_Num, "not equal shader num");

		std::string path = btr::getResourceShaderPath();
		for (size_t i = 0; i < array_length(name); i++) {
			m_shader[i] = loadShaderUnique(m_world->m_context->m_device.getHandle(), path + name[i]);
		}
	}

	// pipeline layout
	{
		vk::DescriptorSetLayout layouts[] = {
			m_world->m_physics_world_desc_layout.get(),
			m_world->m_rigitbody_desc_layout.get(),
			m_world->m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
		};

		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayout_Rigid] = m_world->m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}

	// pipeline
	{
		std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
		shader_info[0].setModule(m_shader[Shader_CollisionDetective].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		shader_info[1].setModule(m_shader[Shader_CollisionDetectiveBefore].get());
		shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[1].setPName("main");
		shader_info[2].setModule(m_shader[Shader_CalcForce].get());
		shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[2].setPName("main");
		shader_info[3].setModule(m_shader[Shader_Integrate].get());
		shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[3].setPName("main");
		shader_info[4].setModule(m_shader[Shader_ToFragment].get());
		shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[4].setPName("main");
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[1])
			.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[2])
			.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[3])
			.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[4])
			.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
		};
		auto compute_pipeline = m_world->m_context->m_device->createComputePipelinesUnique(m_world->m_context->m_cache.get(), compute_pipeline_info);
		m_pipeline[Pipeline_CollisionDetective] = std::move(compute_pipeline[0]);
		m_pipeline[Pipeline_CollisionDetectiveBefore] = std::move(compute_pipeline[1]);
		m_pipeline[Pipeline_CalcForce] = std::move(compute_pipeline[2]);
		m_pipeline[Pipeline_Integrate] = std::move(compute_pipeline[3]);
		m_pipeline[Pipeline_ToFragment] = std::move(compute_pipeline[4]);
	}

}
void GI2DRigidbody_procedure::execute(vk::CommandBuffer cmd, const GI2DRigidbody* rb)
{

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, m_world->m_physics_world_desc.get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, rb->m_descriptor_set.get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 2, m_world->m_gi2d_context->getDescriptorSet(), {});

	{
		// Õ“Ë”»’è
		vk::BufferMemoryBarrier to_read[] = {
			rb->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),

		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_CollisionDetective].get());

		auto num = app::calcDipatchGroups(uvec3(rb->m_particle_num, 1, 1), uvec3(1024, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);

	}
	{
		// Õ“ËŒvŽZ
		vk::BufferMemoryBarrier to_read[] = {
			rb->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			rb->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_CalcForce].get());

		auto num = app::calcDipatchGroups(uvec3(rb->m_particle_num, 1, 1), uvec3(1024, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);

	}

	{
		// „‘Ì‚ÌXV
		vk::BufferMemoryBarrier to_read[] = {
			rb->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Integrate].get());
		cmd.dispatch(1, 1, 1);
	}
}

void GI2DRigidbody_procedure::executeToFragment(vk::CommandBuffer cmd, const std::vector<GI2DRigidbody*>& rb)
{
	{
		// fragment_data‚É‘‚«ž‚Þ
		vk::BufferMemoryBarrier to_read[] = {
			m_world->m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, m_world->m_physics_world_desc.get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 2, m_world->m_gi2d_context->getDescriptorSet(), {});

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_ToFragment].get());
	for (const auto* r : rb)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, r->m_descriptor_set.get(), {});

		auto num = app::calcDipatchGroups(uvec3(r->m_particle_num, 1, 1), uvec3(1024, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

}
