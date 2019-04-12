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

			"Rigid_ToFragment.comp.spv",

			"Rigid_MakeParticle.comp.spv",
			"Rigid_MakeFluid.comp.spv",
			"Rigid_ConstraintSolve.comp.spv",
			"Rigid_CalcCenterMass.comp.spv",
			"Rigid_ApqAccum.comp.spv",
			"Rigid_ApqCalc.comp.spv",


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
			m_world->m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
		};
		vk::PushConstantRange ranges[] = {
			vk::PushConstantRange().setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
		};
		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		pipeline_layout_info.setPushConstantRangeCount(std::size(ranges));
		pipeline_layout_info.setPPushConstantRanges(ranges);
		m_pipeline_layout[PipelineLayout_Rigid] = m_world->m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}

	// pipeline
	{
		std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
		shader_info[0].setModule(m_shader[Shader_ToFragment].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		shader_info[1].setModule(m_shader[Shader_RBMakeParticle].get());
		shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[1].setPName("main");
		shader_info[2].setModule(m_shader[Shader_RBMakeFluid].get());
		shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[2].setPName("main");
		shader_info[3].setModule(m_shader[Shader_RBConstraintSolve].get());
		shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[3].setPName("main");
		shader_info[4].setModule(m_shader[Shader_RBCalcCenterMass].get());
		shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[4].setPName("main");
		shader_info[5].setModule(m_shader[Shader_RBApqAccum].get());
		shader_info[5].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[5].setPName("main");
		shader_info[6].setModule(m_shader[Shader_RBApqCalc].get());
		shader_info[6].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[6].setPName("main");
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
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[5])
			.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[6])
			.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
		};
		auto compute_pipeline = m_world->m_context->m_device->createComputePipelinesUnique(m_world->m_context->m_cache.get(), compute_pipeline_info);
		m_pipeline[Pipeline_ToFragment] = std::move(compute_pipeline[0]);
		m_pipeline[Pipeline_RBMakeParticle] = std::move(compute_pipeline[1]);
		m_pipeline[Pipeline_RBMakeFluid] = std::move(compute_pipeline[2]);
		m_pipeline[Pipeline_RBConstraintSolve] = std::move(compute_pipeline[3]);
		m_pipeline[Pipeline_RBCalcCenterMass] = std::move(compute_pipeline[4]);
		m_pipeline[Pipeline_RBApqAccum] = std::move(compute_pipeline[5]);
		m_pipeline[Pipeline_RBApqCalc] = std::move(compute_pipeline[6]);
	}

}
void GI2DRigidbody_procedure::executeSM(vk::CommandBuffer cmd, const std::shared_ptr<PhysicsWorld>& world)
{
	m_world->execute(cmd);

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, m_world->m_physics_world_desc.get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, m_world->m_gi2d_context->getDescriptorSet(), {});
	{
		vk::BufferMemoryBarrier to_read[] = {
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBMakeParticle].get());

		auto num = app::calcDipatchGroups(uvec3(world->m_particle_id, 1, 1), uvec3(1, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

	{
		{

			executeMakeFluid(cmd, world);

		}
		{
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, m_world->m_physics_world_desc.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, m_world->m_gi2d_context->getDescriptorSet(), {});

			vk::BufferMemoryBarrier to_read[] = {
				world->b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				world->b_fluid.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBConstraintSolve].get());

			auto num = app::calcDipatchGroups(uvec3(world->m_particle_id, 1, 1), uvec3(1, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
	}


	{
		vk::BufferMemoryBarrier to_read[] = {
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBCalcCenterMass].get());

		auto num = app::calcDipatchGroups(uvec3(world->m_rigidbody_id, 1, 1), uvec3(1, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}
	{
		vk::BufferMemoryBarrier to_read[] = {
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBApqAccum].get());

		auto num = app::calcDipatchGroups(uvec3(world->m_particle_id, 1, 1), uvec3(1, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}
	{
		vk::BufferMemoryBarrier to_read[] = {
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBApqCalc].get());

		auto num = app::calcDipatchGroups(uvec3(world->m_rigidbody_id, 1, 1), uvec3(1, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

}
void GI2DRigidbody_procedure::executeMakeFluid(vk::CommandBuffer cmd, const std::shared_ptr<PhysicsWorld>& world)
{

	m_world->execute(cmd);
	m_world->executeMakeFluidWall(cmd);

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, m_world->m_physics_world_desc.get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, m_world->m_gi2d_context->getDescriptorSet(), {});

	{
		// 衝突判定用
		vk::BufferMemoryBarrier to_read[] = {
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			world->b_fluid.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBMakeFluid].get());

		auto num = app::calcDipatchGroups(uvec3(world->m_particle_id, 1, 1), uvec3(1, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

}

void GI2DRigidbody_procedure::executeToFragment(vk::CommandBuffer cmd, const std::shared_ptr<PhysicsWorld>& world)
{
	{
		// fragment_dataに書き込む
		vk::BufferMemoryBarrier to_read[] = {
			world->m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, world->m_physics_world_desc.get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, world->m_gi2d_context->getDescriptorSet(), {});

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_ToFragment].get());

	auto num = app::calcDipatchGroups(uvec3(world->m_particle_id, 1, 1), uvec3(1, 1, 1));
	cmd.dispatch(num.x, num.y, num.z);
}
