#pragma once
#include "GI2DPhysics_procedure.h"
#include "GI2DPhysics.h"

GI2DPhysics_procedure::GI2DPhysics_procedure(const std::shared_ptr<GI2DPhysics>& physics_context, const std::shared_ptr<GI2DSDF>& sdf)
{

	{
		const char* name[] =
		{

			"Rigid_DrawParticle.comp.spv",

			"Rigid_MakeParticle.comp.spv",
			"Rigid_MakeCollidable.comp.spv",
			"Rigid_MakeCollidableWall.comp.spv",
			"Rigid_CollisionDetective.comp.spv",
			"Rigid_CalcPressure.comp.spv",
			"Rigid_CalcCenterMass.comp.spv",
			"Rigid_MakeTransformMatrix.comp.spv",
			"Rigid_UpdateParticleBlock.comp.spv",
			"Rigid_UpdateRigidbody.comp.spv",

		};
		static_assert(array_length(name) == Shader_Num, "not equal shader num");

		std::string path = btr::getResourceShaderPath();
		for (size_t i = 0; i < array_length(name); i++) {
			m_shader[i] = loadShaderUnique(physics_context->m_context->m_device, path + name[i]);
		}
	}

	// pipeline layout
	{
		{
			vk::DescriptorSetLayout layouts[] = {
				physics_context->getDescriptorSetLayout(GI2DPhysics::DescLayout_Data),
				physics_context->m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				physics_context->m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_SDF),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayout_Rigid] = physics_context->m_context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				physics_context->getDescriptorSetLayout(GI2DPhysics::DescLayout_Data),
				physics_context->m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				RenderTarget::s_descriptor_set_layout.get(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayout_DrawParticle] = physics_context->m_context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

		}
	}

	// pipeline
	{
		std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
		shader_info[0].setModule(m_shader[Shader_DrawParticle].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		shader_info[1].setModule(m_shader[Shader_RBMakeParticle].get());
		shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[1].setPName("main");
		shader_info[2].setModule(m_shader[Shader_MakeCollision].get());
		shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[2].setPName("main");
		shader_info[3].setModule(m_shader[Shader_MakeWallCollision].get());
		shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[3].setPName("main");
		shader_info[4].setModule(m_shader[Shader_RBCollisionDetective].get());
		shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[4].setPName("main");
		shader_info[5].setModule(m_shader[Shader_RBCalcPressure].get());
		shader_info[5].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[5].setPName("main");
		shader_info[6].setModule(m_shader[Shader_RBCalcCenterMass].get());
		shader_info[6].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[6].setPName("main");
		shader_info[7].setModule(m_shader[Shader_RBMakeTransformMatrix].get());
		shader_info[7].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[7].setPName("main");
		shader_info[8].setModule(m_shader[Shader_RBUpdateParticleBlock].get());
		shader_info[8].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[8].setPName("main");
		shader_info[9].setModule(m_shader[Shader_RBUpdateRigidbody].get());
		shader_info[9].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[9].setPName("main");
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayout_DrawParticle].get()),
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
		auto compute_pipeline = physics_context->m_context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
		m_pipeline[Pipeline_DrawParticle] = std::move(compute_pipeline[0]);
		m_pipeline[Pipeline_RBMakeParticle] = std::move(compute_pipeline[1]);
		m_pipeline[Pipeline_MakeCollision] = std::move(compute_pipeline[2]);
		m_pipeline[Pipeline_MakeWallCollision] = std::move(compute_pipeline[3]);
		m_pipeline[Pipeline_RBCollisionDetective] = std::move(compute_pipeline[4]);
		m_pipeline[Pipeline_RBCalcPressure] = std::move(compute_pipeline[5]);
		m_pipeline[Pipeline_RBCalcCenterMass] = std::move(compute_pipeline[6]);
		m_pipeline[Pipeline_RBMakeTransformMatrix] = std::move(compute_pipeline[7]);
		m_pipeline[Pipeline_RBUpdateParticleBlock] = std::move(compute_pipeline[8]);
		m_pipeline[Pipeline_RBUpdateRigidbody] = std::move(compute_pipeline[9]);
	}

}
void GI2DPhysics_procedure::execute(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPhysics>& physics_context, const std::shared_ptr<GI2DSDF>& sdf)
{
	DebugLabel _label(cmd, physics_context->m_context->m_dispach, __FUNCTION__);

	physics_context->execute(cmd);

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, physics_context->getDescriptorSet(GI2DPhysics::DescLayout_Data), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, physics_context->m_gi2d_context->getDescriptorSet(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 2, sdf->getDescriptorSet(), {});

	_label.insert("make particle");
	{
		vk::BufferMemoryBarrier to_read[] = {
			physics_context->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			physics_context->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			physics_context->b_update_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
			physics_context->b_pb_update_list.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),

		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader|vk::PipelineStageFlagBits::eDrawIndirect, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBMakeParticle].get());

		cmd.dispatchIndirect(physics_context->b_update_counter.getInfo().buffer, physics_context->b_update_counter.getInfo().offset + physics_context->m_world.cpu_index * sizeof(uvec4) * 2 + sizeof(uvec4));
	}

	_label.insert("make collision");
	{
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeWallCollision].get());

			auto num = app::calcDipatchGroups(uvec3(physics_context->m_gi2d_context->RenderSize, 1), uvec3(8, 8, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		{
			vk::BufferMemoryBarrier to_read[] = {
				physics_context->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				physics_context->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				physics_context->b_collidable_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				physics_context->b_collidable.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeCollision].get());

			cmd.dispatchIndirect(physics_context->b_update_counter.getInfo().buffer, physics_context->b_update_counter.getInfo().offset + physics_context->m_world.cpu_index * sizeof(uvec4) * 2 + sizeof(uvec4));
		}
	}

	_label.insert("collision detective");
	{

		vk::BufferMemoryBarrier to_read[] = {
			physics_context->b_collidable_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			physics_context->b_collidable.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBCollisionDetective].get());
		cmd.dispatchIndirect(physics_context->b_update_counter.getInfo().buffer, physics_context->b_update_counter.getInfo().offset + physics_context->m_world.cpu_index *sizeof(uvec4)*2 + sizeof(uvec4));
	}

	_label.insert("calc pressure");
	{
		vk::BufferMemoryBarrier to_read[] = {
			physics_context->b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			physics_context->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBCalcPressure].get());
		cmd.dispatchIndirect(physics_context->b_update_counter.getInfo().buffer, physics_context->b_update_counter.getInfo().offset + physics_context->m_world.cpu_index * sizeof(uvec4) * 2 + sizeof(uvec4));

	}

	_label.insert("calc center_of_mass");
	{
		vk::BufferMemoryBarrier to_read[] = {
			physics_context->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			physics_context->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBCalcCenterMass].get());

		cmd.dispatchIndirect(physics_context->b_update_counter.getInfo().buffer, physics_context->b_update_counter.getInfo().offset + physics_context->m_world.cpu_index * sizeof(uvec4) * 2);
	}

	_label.insert("calc transform matrix");
	{
		vk::BufferMemoryBarrier to_read[] = {
			physics_context->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			physics_context->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBMakeTransformMatrix].get());

		cmd.dispatchIndirect(physics_context->b_update_counter.getInfo().buffer, physics_context->b_update_counter.getInfo().offset + physics_context->m_world.cpu_index * sizeof(uvec4) * 2 + sizeof(uvec4));
	}

	_label.insert("update particle_block");
	{
		vk::BufferMemoryBarrier to_read[] = {
			physics_context->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			physics_context->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBUpdateParticleBlock].get());

		cmd.dispatchIndirect(physics_context->b_update_counter.getInfo().buffer, physics_context->b_update_counter.getInfo().offset + physics_context->m_world.cpu_index * sizeof(uvec4) * 2 + sizeof(uvec4));
	}

	_label.insert("update rigidbody");
	{
		vk::BufferMemoryBarrier to_read[] = {
			physics_context->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			physics_context->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RBUpdateRigidbody].get());

		cmd.dispatchIndirect(physics_context->b_update_counter.getInfo().buffer, physics_context->b_update_counter.getInfo().offset + physics_context->m_world.cpu_index * sizeof(uvec4) * 2);
	}

	_label.insert("clear");
	{
		vk::BufferMemoryBarrier to_write[] = {
			physics_context->b_update_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite|vk::AccessFlagBits::eIndirectCommandRead, vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader|vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eTransfer, {},
			0, nullptr, array_length(to_write), to_write, 0, nullptr);

		cmd.updateBuffer<uvec4>(physics_context->b_update_counter.getInfo().buffer, physics_context->b_update_counter.getInfo().offset + physics_context->m_world.cpu_index * sizeof(uvec4) * 2, uvec4(0, 1, 1, 0));
		cmd.updateBuffer<uvec4>(physics_context->b_update_counter.getInfo().buffer, physics_context->b_update_counter.getInfo().offset + physics_context->m_world.cpu_index * sizeof(uvec4) * 2 + sizeof(uvec4), uvec4(0, 1, 1, 0));
	}


}

void GI2DPhysics_procedure::executeDrawParticle(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPhysics>& physics_context, const std::shared_ptr<RenderTarget>& render_target)
{
	{
		// render_targetに書き込む
		vk::BufferMemoryBarrier to_read[] = {
			physics_context->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			physics_context->b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			physics_context->b_update_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
			physics_context->b_pb_update_list.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		vk::ImageMemoryBarrier image_barrier;
		image_barrier.setImage(render_target->m_image);
		image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		image_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
		image_barrier.setOldLayout(vk::ImageLayout::eGeneral);
		image_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
		image_barrier.setNewLayout(vk::ImageLayout::eGeneral);

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eDrawIndirect, {},
			0, nullptr, array_length(to_read), to_read, 1, &image_barrier);
	}

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_DrawParticle].get(), 0, physics_context->getDescriptorSet(GI2DPhysics::DescLayout_Data), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_DrawParticle].get(), 1, physics_context->m_gi2d_context->getDescriptorSet(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_DrawParticle].get(), 2, render_target->m_descriptor.get(), {});

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_DrawParticle].get());

	cmd.dispatchIndirect(physics_context->b_update_counter.getInfo().buffer, physics_context->b_update_counter.getInfo().offset + physics_context->m_world.gpu_index * sizeof(uvec4) * 2 + sizeof(uvec4));

}
