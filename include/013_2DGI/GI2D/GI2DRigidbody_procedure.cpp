#pragma once
#include "GI2DRigidbody_procedure.h"
#include "GI2DPhysicsWorld.h"
#include "GI2DRigidbody.h"

GI2DRigidbody_procedure::GI2DRigidbody_procedure(const std::shared_ptr<PhysicsWorld>& world, const std::shared_ptr<GI2DSDF>& sdf)
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
			"Rigid_UpdateParticleBlock.comp.spv",
			"Rigid_ApqCalc.comp.spv",

			"Rigid_ToFluidWall2.comp.spv"

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
			m_world->getDescriptorSetLayout(PhysicsWorld::DescLayout_Data),
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
	{
		vk::DescriptorSetLayout layouts[] = {
			m_world->getDescriptorSetLayout(PhysicsWorld::DescLayout_Data),
			m_world->m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
			m_world->m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_SDF),
		};
		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayout_MakeWallCollision] = m_world->m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
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
		shader_info[6].setModule(m_shader[Shader_RBUpdateParticleBlock].get());
		shader_info[6].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[6].setPName("main");
		shader_info[7].setModule(m_shader[Shader_RBApqCalc].get());
		shader_info[7].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[7].setPName("main");
		shader_info[8].setModule(m_shader[Shader_MakeWallCollision].get());
		shader_info[8].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[8].setPName("main");
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
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[7])
			.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[8])
			.setLayout(m_pipeline_layout[PipelineLayout_MakeWallCollision].get()),
		};
		auto compute_pipeline = m_world->m_context->m_device->createComputePipelinesUnique(m_world->m_context->m_cache.get(), compute_pipeline_info);
		m_pipeline[Pipeline_ToFragment] = std::move(compute_pipeline[0]);
		m_pipeline[Pipeline_RBMakeParticle] = std::move(compute_pipeline[1]);
		m_pipeline[Pipeline_RBMakeFluid] = std::move(compute_pipeline[2]);
		m_pipeline[Pipeline_RBConstraintSolve] = std::move(compute_pipeline[3]);
		m_pipeline[Pipeline_RBCalcCenterMass] = std::move(compute_pipeline[4]);
		m_pipeline[Pipeline_RBApqAccum] = std::move(compute_pipeline[5]);
		m_pipeline[Pipeline_RBUpdateParticleBlock] = std::move(compute_pipeline[6]);
		m_pipeline[Pipeline_RBApqCalc] = std::move(compute_pipeline[7]);
		m_pipeline[Pipeline_MakeWallCollision] = std::move(compute_pipeline[8]);
	}

}
void GI2DRigidbody_procedure::execute(vk::CommandBuffer cmd, const std::shared_ptr<PhysicsWorld>& world, const std::shared_ptr<GI2DSDF>& sdf)
{
	m_world->execute(cmd);

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, m_world->getDescriptorSet(PhysicsWorld::DescLayout_Data), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, m_world->m_gi2d_context->getDescriptorSet(), {});
	{
		vk::BufferMemoryBarrier to_read[] = {
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_update_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader|vk::PipelineStageFlagBits::eDrawIndirect, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBMakeParticle].get());

		cmd.dispatchIndirect(world->b_update_counter.getInfo().buffer, world->b_update_counter.getInfo().offset + world->m_world.cpu_index * sizeof(uvec4) * 2 + sizeof(uvec4));
	}

	// collision
	{
		{

//			m_world->execute(cmd);

			_executeMakeFluidWall(cmd, world, sdf);
			_executeMakeFluidParticle(cmd, world);
		}
		{
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, m_world->getDescriptorSet(PhysicsWorld::DescLayout_Data), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, m_world->m_gi2d_context->getDescriptorSet(), {});

			vk::BufferMemoryBarrier to_read[] = {
				world->b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				world->b_fluid.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBConstraintSolve].get());

			cmd.dispatchIndirect(world->b_update_counter.getInfo().buffer, world->b_update_counter.getInfo().offset + world->m_world.cpu_index *sizeof(uvec4)*2 + sizeof(uvec4));
		}
	}

	// calc center_of_mass
	{
		vk::BufferMemoryBarrier to_read[] = {
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBCalcCenterMass].get());

		cmd.dispatchIndirect(world->b_update_counter.getInfo().buffer, world->b_update_counter.getInfo().offset + world->m_world.cpu_index * sizeof(uvec4) * 2);
	}

	// calc Apq
	{
		vk::BufferMemoryBarrier to_read[] = {
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBApqAccum].get());

		cmd.dispatchIndirect(world->b_update_counter.getInfo().buffer, world->b_update_counter.getInfo().offset + world->m_world.cpu_index * sizeof(uvec4) * 2 + sizeof(uvec4));
	}
	// update particle_block
	{
		vk::BufferMemoryBarrier to_read[] = {
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBUpdateParticleBlock].get());

		cmd.dispatchIndirect(world->b_update_counter.getInfo().buffer, world->b_update_counter.getInfo().offset + world->m_world.cpu_index * sizeof(uvec4) * 2 + sizeof(uvec4));
	}

	// update rigidbody
	{
		vk::BufferMemoryBarrier to_read[] = {
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBApqCalc].get());

		cmd.dispatchIndirect(world->b_update_counter.getInfo().buffer, world->b_update_counter.getInfo().offset + world->m_world.cpu_index * sizeof(uvec4) * 2);
	}

	{
		vk::BufferMemoryBarrier to_write[] = {
			world->b_update_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite|vk::AccessFlagBits::eIndirectCommandRead, vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader|vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eTransfer, {},
			0, nullptr, array_length(to_write), to_write, 0, nullptr);

		cmd.updateBuffer<uvec4>(world->b_update_counter.getInfo().buffer, world->b_update_counter.getInfo().offset + world->m_world.cpu_index * sizeof(uvec4) * 2, uvec4(0, 1, 1, 0));
		cmd.updateBuffer<uvec4>(world->b_update_counter.getInfo().buffer, world->b_update_counter.getInfo().offset + world->m_world.cpu_index * sizeof(uvec4) * 2 + sizeof(uvec4), uvec4(0, 1, 1, 0));

	}


}
void GI2DRigidbody_procedure::executeMakeFluid(vk::CommandBuffer cmd, const std::shared_ptr<PhysicsWorld>& world)
{

	m_world->execute(cmd);
	m_world->executeMakeFluidWall(cmd);

	_executeMakeFluidParticle(cmd, world);

}

void GI2DRigidbody_procedure::_executeMakeFluidParticle(vk::CommandBuffer &cmd, const std::shared_ptr<PhysicsWorld>& world)
{
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, m_world->getDescriptorSet(PhysicsWorld::DescLayout_Data), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, m_world->m_gi2d_context->getDescriptorSet(), {});

	{
		// Õ“Ë”»’è—p
		vk::BufferMemoryBarrier to_read[] = {
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			world->b_fluid.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBMakeFluid].get());

		cmd.dispatchIndirect(world->b_update_counter.getInfo().buffer, world->b_update_counter.getInfo().offset + world->m_world.cpu_index * sizeof(uvec4) * 2 + sizeof(uvec4));
	}
}
void GI2DRigidbody_procedure::_executeMakeFluidWall(vk::CommandBuffer &cmd, const std::shared_ptr<PhysicsWorld>& world, const std::shared_ptr<GI2DSDF>& sdf)
{
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeWallCollision].get(), 0, m_world->getDescriptorSet(PhysicsWorld::DescLayout_Data), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeWallCollision].get(), 1, m_world->m_gi2d_context->getDescriptorSet(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeWallCollision].get(), 2, sdf->getDescriptorSet(), {});

	{
		// Õ“Ë”»’è—p
// 		vk::BufferMemoryBarrier to_read[] = {
// 			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
// 			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
// 			world->b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
// 			world->b_fluid.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
// 		};
// 		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
// 			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeWallCollision].get());

		auto num = app::calcDipatchGroups(uvec3(world->m_gi2d_context->RenderSize, 1), uvec3(8, 8, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

}

void GI2DRigidbody_procedure::executeToFragment(vk::CommandBuffer cmd, const std::shared_ptr<PhysicsWorld>& world)
{
	{
		// fragment_data‚É‘‚«ž‚Þ
		vk::BufferMemoryBarrier to_read[] = {
			world->m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			world->b_update_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader| vk::PipelineStageFlagBits::eDrawIndirect, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, m_world->getDescriptorSet(PhysicsWorld::DescLayout_Data), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, world->m_gi2d_context->getDescriptorSet(), {});

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_ToFragment].get());

//	auto num = app::calcDipatchGroups(uvec3(world->m_particle_id, 1, 1), uvec3(1, 1, 1));
//	cmd.dispatch(num.x, num.y, num.z);

	cmd.dispatchIndirect(world->b_update_counter.getInfo().buffer, world->b_update_counter.getInfo().offset + world->m_world.gpu_index * sizeof(uvec4) * 2 + sizeof(uvec4));

}
