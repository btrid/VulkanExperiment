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

			"RigidDebug_DrawCollisionMap.comp.spv",

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
		shader_info[10].setModule(m_shader[ShaderDebug_DrawCollisionHeatMap].get());
		shader_info[10].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[10].setPName("main");
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
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[10])
			.setLayout(m_pipeline_layout[PipelineLayout_DrawParticle].get()),
		};
		m_pipeline[Pipeline_DrawParticle] = physics_context->m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
		m_pipeline[Pipeline_RBMakeParticle] = physics_context->m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
		m_pipeline[Pipeline_MakeCollision] = physics_context->m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[2]).value;
		m_pipeline[Pipeline_MakeWallCollision] = physics_context->m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[3]).value;
		m_pipeline[Pipeline_RBCollisionDetective] = physics_context->m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[4]).value;
		m_pipeline[Pipeline_RBCalcPressure] = physics_context->m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[5]).value;
		m_pipeline[Pipeline_RBCalcCenterMass] = physics_context->m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[6]).value;
		m_pipeline[Pipeline_RBMakeTransformMatrix] = physics_context->m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[7]).value;
		m_pipeline[Pipeline_RBUpdateParticleBlock] = physics_context->m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[8]).value;
		m_pipeline[Pipeline_RBUpdateRigidbody] = physics_context->m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[9]).value;
		m_pipeline[Pipeline_DrawCollisionHeatMap] = physics_context->m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[10]).value;
	}

}
void GI2DPhysics_procedure::execute(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPhysics>& physics_context, const std::shared_ptr<GI2DSDF>& sdf)
{
	DebugLabel _label(cmd, __FUNCTION__);

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

			auto num = app::calcDipatchGroups(uvec3(physics_context->m_gi2d_context->m_desc.Resolution, 1), uvec3(8, 8, 1));
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
		// render_target‚É‘‚«ž‚Þ
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


void GI2DPhysics_procedure::executeDebugDrawCollisionHeatMap(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPhysics>& physics_context, const std::shared_ptr<RenderTarget>& render_target)
{
	DebugLabel _label(cmd, __FUNCTION__);

	{
		// render_target‚É‘‚«ž‚Þ
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

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_DrawCollisionHeatMap].get());

	auto num = app::calcDipatchGroups(uvec3(physics_context->m_gi2d_context->m_desc.Resolution.x, physics_context->m_gi2d_context->m_desc.Resolution.y, 1), uvec3(32, 32, 1));
	cmd.dispatch(num.x, num.y, num.z);

}

#include <applib/AppPipeline.h>
#include <013_2DGI/GI2D/GI2DDebug.h>
#include <013_2DGI/GI2D/GI2DMakeHierarchy.h>
int gi2d_physics::rigidbody()
{
	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.Resolution = uvec2(1024, 1024);
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<GI2DSDF> gi2d_sdf_context = std::make_shared<GI2DSDF>(gi2d_context);
	std::shared_ptr<GI2DPhysics> gi2d_physics_context = std::make_shared<GI2DPhysics>(context, gi2d_context);
	std::shared_ptr<GI2DPhysicsDebug> gi2d_physics_debug = std::make_shared<GI2DPhysicsDebug>(gi2d_physics_context, app.m_window->getFrontBuffer());

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DPhysics_procedure gi2d_physics_proc(gi2d_physics_context, gi2d_sdf_context);
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	gi2d_debug.executeMakeFragment(cmd);
	//	gi2d_physics_context->executeMakeVoronoi(cmd);
	//	gi2d_physics_context->executeMakeVoronoiPath(cmd);
	app.setup();

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_model_update,
				cmd_render_clear,
				cmd_crowd,
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{

			}

			{
				//cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			// crowd
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				cmd.end();
				cmds[cmd_crowd] = cmd;
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				gi2d_context->execute(cmd);

				if (context->m_window->getInput().m_keyboard.isOn('A'))
				{
					for (int y = 0; y < 16; y++) {
						for (int x = 0; x < 16; x++) {
							GI2DRB_MakeParam param;
							param.aabb = uvec4(220 + x * 32, 620 - y * 16, 8, 8);
							param.is_fluid = false;
							param.is_usercontrol = false;
							gi2d_physics_context->make(cmd, param);
						}
					}
				}

				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf_context);
				//				gi2d_make_hierarchy.executeRenderSDF(cmd, gi2d_sdf_context, app.m_window->getFrontBuffer());

				gi2d_debug.executeDrawFragment(cmd, app.m_window->getFrontBuffer());
				gi2d_physics_proc.execute(cmd, gi2d_physics_context, gi2d_sdf_context);
				gi2d_physics_proc.executeDrawParticle(cmd, gi2d_physics_context, app.m_window->getFrontBuffer());
				//				gi2d_physics_proc.executeDebugDrawCollisionHeatMap(cmd, gi2d_physics_context, app.m_window->getFrontBuffer());

				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
			context->m_device.waitIdle();
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}
