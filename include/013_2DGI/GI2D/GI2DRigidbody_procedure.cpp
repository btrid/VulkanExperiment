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
			"Rigid_IntegrateAfter.comp.spv",
			"Rigid_CalcConstraint.comp.spv",
			"Rigid_SolveConstraint.comp.spv",
			"Rigid_SolveConstraint.comp.spv",

			"Rigid_MakeParticle.comp.spv",
			"Rigid_MakeFluid.comp.spv",
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
		shader_info[4].setModule(m_shader[Shader_IntegrateAfter].get());
		shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[4].setPName("main");
		shader_info[5].setModule(m_shader[Shader_CalcConstraint].get());
		shader_info[5].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[5].setPName("main");
		shader_info[6].setModule(m_shader[Shader_SolveConstraint].get());
		shader_info[6].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[6].setPName("main");
		shader_info[7].setModule(m_shader[Shader_ToFragment].get());
		shader_info[7].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[7].setPName("main");
		shader_info[8].setModule(m_shader[Shader_MakeParticle].get());
		shader_info[8].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[8].setPName("main");
		shader_info[9].setModule(m_shader[Shader_MakeFluid].get());
		shader_info[9].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[9].setPName("main");
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
			.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[9])
			.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
		};
		auto compute_pipeline = m_world->m_context->m_device->createComputePipelinesUnique(m_world->m_context->m_cache.get(), compute_pipeline_info);
		m_pipeline[Pipeline_CollisionDetective] = std::move(compute_pipeline[0]);
		m_pipeline[Pipeline_CollisionDetectiveBefore] = std::move(compute_pipeline[1]);
		m_pipeline[Pipeline_CalcForce] = std::move(compute_pipeline[2]);
		m_pipeline[Pipeline_Integrate] = std::move(compute_pipeline[3]);
		m_pipeline[Pipeline_IntegrateAfter] = std::move(compute_pipeline[4]);
		m_pipeline[Pipeline_CalcConstraint] = std::move(compute_pipeline[5]);
		m_pipeline[Pipeline_SolveConstraint] = std::move(compute_pipeline[6]);
		m_pipeline[Pipeline_ToFragment] = std::move(compute_pipeline[7]);
		m_pipeline[Pipeline_MakeParticle] = std::move(compute_pipeline[8]);
		m_pipeline[Pipeline_MakeFluid] = std::move(compute_pipeline[9]);
	}

}
void GI2DRigidbody_procedure::execute(vk::CommandBuffer cmd, const std::vector<const GI2DRigidbody*>& rb)
{
	{
		vk::BufferMemoryBarrier to_read[] = {
			m_world->b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			m_world->b_fluid.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, m_world->m_physics_world_desc.get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 2, m_world->m_gi2d_context->getDescriptorSet(), {});

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_CollisionDetective].get());
	for (const auto* r : rb)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, r->m_descriptor_set.get(), {});
		{
			// 衝突判定
			vk::BufferMemoryBarrier to_read[] = {
				r->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);


			auto num = app::calcDipatchGroups(uvec3(r->m_particle_num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
	}

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_CalcForce].get());
	for (const auto* r : rb)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, r->m_descriptor_set.get(), {});
		// 衝突計算
		vk::BufferMemoryBarrier to_read[] = {
			r->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			r->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);


		auto num = app::calcDipatchGroups(uvec3(r->m_particle_num, 1, 1), uvec3(1024, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Integrate].get());
	for (const auto* r : rb)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, r->m_descriptor_set.get(), {});

		vk::BufferMemoryBarrier to_read[] = {
			r->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.dispatch(1, 1, 1);
	}

	for (uint32_t iter = 0; iter < 5; iter++)
	{
		m_world->execute(cmd);
		m_world->executeMakeFluidWall(cmd);
		m_world->executeMakeFluid(cmd, rb);


		for (uint32_t i = 0; i < 16; i++)
		{
			cmd.pushConstants<uint32_t>(m_pipeline_layout[PipelineLayout_Rigid].get(), vk::ShaderStageFlagBits::eCompute, 0, i);
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_CalcConstraint].get());
			for (const auto* r : rb)
			{
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, r->m_descriptor_set.get(), {});
				// 衝突計算
				vk::BufferMemoryBarrier to_read[] = {
					r->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					r->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);


				auto num = app::calcDipatchGroups(uvec3(r->m_particle_num, 1, 1), uvec3(1024, 1, 1));
				cmd.dispatch(num.x, num.y, num.z);
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_SolveConstraint].get());
			for (const auto* r : rb)
			{
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, r->m_descriptor_set.get(), {});

				// 剛体の更新
				vk::BufferMemoryBarrier to_read[] = {
					r->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);

				cmd.dispatch(1, 1, 1);
			}

		}


		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_IntegrateAfter].get());
		for (const auto* r : rb)
		{
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, r->m_descriptor_set.get(), {});

			// 剛体の更新
			vk::BufferMemoryBarrier to_read[] = {
				r->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.dispatch(1, 1, 1);
		}
	}
}

void GI2DRigidbody_procedure::executeMakeParticle(vk::CommandBuffer cmd, const std::vector<const GI2DRigidbody*>& rb)
{

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, m_world->m_physics_world_desc.get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 2, m_world->m_gi2d_context->getDescriptorSet(), {});

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeParticle].get());
	for (const auto* r : rb)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, r->m_descriptor_set.get(), {});
		{
			// 衝突判定
			vk::BufferMemoryBarrier to_read[] = {
				r->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);


			auto num = app::calcDipatchGroups(uvec3(r->m_particle_num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
	}

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeFluid].get());
	for (const auto* r : rb)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, r->m_descriptor_set.get(), {});
		{
			// 衝突判定
			vk::BufferMemoryBarrier to_read[] = {
				r->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);


			auto num = app::calcDipatchGroups(uvec3(r->m_particle_num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
	}
}

void GI2DRigidbody_procedure::executeToFragment(vk::CommandBuffer cmd, const std::vector<const GI2DRigidbody*>& rb)
{
	{
		// fragment_dataに書き込む
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
