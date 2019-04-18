#pragma once
#include "GI2DPhysicsWorld.h"
#include "GI2DContext.h"
#include "GI2DRigidbody.h"


PhysicsWorld::PhysicsWorld(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
{
	m_context = context;
	m_gi2d_context = gi2d_context;

	m_rigidbody_id = 0;
	m_particle_id = 0;
	{
		auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
		vk::DescriptorSetLayoutBinding binding[] = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, stage),
		};
		vk::DescriptorSetLayoutCreateInfo desc_layout_info;
		desc_layout_info.setBindingCount(array_length(binding));
		desc_layout_info.setPBindings(binding);
		m_desc_layout[DescLayout_Data] = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
	}
	{
		auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
		vk::DescriptorSetLayoutBinding binding[] = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
		};
		vk::DescriptorSetLayoutCreateInfo desc_layout_info;
		desc_layout_info.setBindingCount(array_length(binding));
		desc_layout_info.setPBindings(binding);
		m_desc_layout[DescLayout_Make] = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
	}

	{
		const char* name[] =
		{
			"Rigid_ToFluid.comp.spv",
			"Rigid_ToFluidWall.comp.spv",

			"RigidMake_Register.comp.spv",
			"RigidMake_MakeJFA.comp.spv",
			"RigidMake_MakeSDF.comp.spv",
		};
		static_assert(array_length(name) == Shader_Num, "not equal shader num");

		std::string path = btr::getResourceShaderPath();
		for (size_t i = 0; i < array_length(name); i++) {
			m_shader[i] = loadShaderUnique(m_context->m_device.getHandle(), path + name[i]);
		}
	}

	// pipeline layout
	{
		vk::DescriptorSetLayout layouts[] = {
			m_desc_layout[DescLayout_Data].get(),
			m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
		};

		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayout_ToFluid] = m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}
	{
		vk::DescriptorSetLayout layouts[] = {
			m_desc_layout[DescLayout_Data].get(),
			m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
		};

		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayout_ToFluidWall] = m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}
	{
		vk::DescriptorSetLayout layouts[] = {
			m_desc_layout[DescLayout_Data].get(),
			m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
			m_desc_layout[DescLayout_Make].get(),
		};
		vk::PushConstantRange ranges[] = {
			vk::PushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, 16),
		};

		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
		pipeline_layout_info.setPPushConstantRanges(ranges);
		m_pipeline_layout[PipelineLayout_MakeRB] = m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}

	// pipeline
	{
		std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
		shader_info[0].setModule(m_shader[Shader_ToFluid].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		shader_info[1].setModule(m_shader[Shader_ToFluidWall].get());
		shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[1].setPName("main");
		shader_info[2].setModule(m_shader[Shader_MakeRB_Register].get());
		shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[2].setPName("main");
		shader_info[3].setModule(m_shader[Shader_MakeRB_MakeJFCell].get());
		shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[3].setPName("main");
		shader_info[4].setModule(m_shader[Shader_MakeRB_MakeSDF].get());
		shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[4].setPName("main");
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayout_ToFluid].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[1])
			.setLayout(m_pipeline_layout[PipelineLayout_ToFluidWall].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[2])
			.setLayout(m_pipeline_layout[PipelineLayout_MakeRB].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[3])
			.setLayout(m_pipeline_layout[PipelineLayout_MakeRB].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[4])
			.setLayout(m_pipeline_layout[PipelineLayout_MakeRB].get()),
		};
		auto compute_pipeline = m_context->m_device->createComputePipelinesUnique(m_context->m_cache.get(), compute_pipeline_info);
		m_pipeline[Pipeline_ToFluid] = std::move(compute_pipeline[0]);
		m_pipeline[Pipeline_ToFluidWall] = std::move(compute_pipeline[1]);
		m_pipeline[Pipeline_MakeRB_Register] = std::move(compute_pipeline[2]);
		m_pipeline[Pipeline_MakeRB_MakeJFCell] = std::move(compute_pipeline[3]);
		m_pipeline[Pipeline_MakeRB_MakeSDF] = std::move(compute_pipeline[4]);
	}

	{
		b_world = m_context->m_storage_memory.allocateMemory<World>({ 1,{} });
		b_rigidbody = m_context->m_storage_memory.allocateMemory<Rigidbody>({ RB_NUM_MAX,{} });
		b_rbparticle = m_context->m_storage_memory.allocateMemory<rbParticle>({ RB_PARTICLE_NUM,{} });
		b_rbparticle_map = m_context->m_storage_memory.allocateMemory<uint32_t>({ RB_PARTICLE_NUM / RB_PARTICLE_BLOCK_SIZE,{} });
		b_fluid_counter = m_context->m_storage_memory.allocateMemory<uint32_t>({ gi2d_context->RenderSize.x*gi2d_context->RenderSize.y,{} });
		b_fluid = m_context->m_storage_memory.allocateMemory<rbFluid>({ 4 * gi2d_context->RenderSize.x*gi2d_context->RenderSize.y,{} });
		b_manager = m_context->m_storage_memory.allocateMemory<uvec4>({ 1,{} });
		b_rb_freelist = m_context->m_storage_memory.allocateMemory<uint>({ RB_NUM_MAX,{} });
		b_particle_freelist = m_context->m_storage_memory.allocateMemory<uint>({ RB_NUM_MAX,{} });

		{
			vk::DescriptorSetLayout layouts[] = {
				m_desc_layout[DescLayout_Data].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_descset[DescLayout_Data] = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo storages[] = {
				b_world.getInfo(),
				b_rigidbody.getInfo(),
				b_rbparticle.getInfo(),
				b_rbparticle_map.getInfo(),
				b_fluid_counter.getInfo(),
				b_fluid.getInfo(),
				b_manager.getInfo(),
				b_rb_freelist.getInfo(),
				b_particle_freelist.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(0)
				.setDstSet(m_descset[DescLayout_Data].get()),
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

		b_make_rigidbody = m_context->m_storage_memory.allocateMemory<Rigidbody>({ 1,{} });
		b_make_particle = m_context->m_storage_memory.allocateMemory<rbParticle>({ MAKE_RB_SIZE_MAX,{} });
		b_jfa_cell = m_context->m_storage_memory.allocateMemory<i16vec2>({ MAKE_RB_JFA_CELL, {} });
		{
			vk::DescriptorSetLayout layouts[] = {
				m_desc_layout[DescLayout_Make].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_descset[DescLayout_Make] = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo storages[] = {
				b_make_rigidbody.getInfo(),
				b_make_particle.getInfo(),
				b_jfa_cell.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(0)
				.setDstSet(m_descset[DescLayout_Make].get()),
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}


	}

	auto cmd = m_context->m_cmd_pool->allocCmdTempolary(0);
	{
		World w;
		w.DT = 0.016f;
		w.STEP = 100;
		w.step = 0;
		w.rigidbody_num = m_rigidbody_id;
		w.rigidbody_max = RB_NUM_MAX;
		w.particleblock_max = RB_PARTICLE_BLOCK_NUM_MAX;
		cmd.updateBuffer<World>(b_world.getInfo().buffer, b_world.getInfo().offset, w);
	}

	{
		{
			std::vector<uint> freelist(RB_NUM_MAX);
			for (uint i = 0; i < freelist.size(); i++)
			{
				freelist[i] = i;
			}
			cmd.updateBuffer<uint>(b_rb_freelist.getInfo().buffer, b_rb_freelist.getInfo().offset, freelist);
		}
		{
			std::vector<uint> freelist(RB_PARTICLE_NUM);
			for (uint i = 0; i < freelist.size(); i++)
			{
				freelist[i] = i;
			}
			cmd.updateBuffer<uint>(b_particle_freelist.getInfo().buffer, b_particle_freelist.getInfo().offset, freelist);
		}

		cmd.fillBuffer(b_manager.getInfo().buffer, b_manager.getInfo().offset, b_manager.getInfo().range, 0);

		vk::BufferMemoryBarrier to_read[] = {
			b_world.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			b_manager.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead |vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}
}

void PhysicsWorld::make(vk::CommandBuffer cmd, const uvec4& box)
{
	auto particle_num = box.z * box.w;
	auto r_id = m_rigidbody_id++;

	uint color = glm::packUnorm4x8(vec4(0.f, 0.f, 1.f, 1.f));
	uint edgecolor = glm::packUnorm4x8(vec4(1.f, 0.f, 0.f, 1.f));
	uint linecolor = glm::packUnorm4x8(vec4(0.f, 1.f, 0.f, 1.f));

	rbParticle _def;
	_def.contact_index = -1;
	_def.color = color;
	_def.is_active = false;
	std::vector<vec2> pos(particle_num);
	std::vector<rbParticle> pstate((particle_num+63)/64*64, _def);

	uint32_t contact_index = 0;
	vec2 size_max = vec2(-999999.f);
	vec2 size_min = vec2(999999.f);
	vec2 center_of_mass = vec2(0.f);
	for (uint32_t y = 0; y < box.w; y++)
	{
		for (uint32_t x = 0; x < box.z; x++)
		{
			uint i = x + y * box.z;
			pos[i] = vec2(box) + rotate(vec2(x, y) + 0.5f, 0.f);
			pstate[i].pos = pos[i];
			pstate[i].pos_old = pos[i];
			pstate[i].contact_index = contact_index++;
			if (y == 0 || y == box.w - 1 || x == 0 || x == box.z - 1)
			{
				pstate[i].color = edgecolor;
			}
			pstate[i].is_contact = 0;
			pstate[i].is_active = true;

			center_of_mass += pos[i];
			size_max = glm::max(pos[i], size_max);
			size_min = glm::min(pos[i], size_min);
		}
	}
	center_of_mass /= particle_num;

	ivec2 jfa_max = ivec2(ceil(size_max)) + ivec2(1);
	ivec2 jfa_min = ivec2(trunc(size_min)) - ivec2(1);
	auto area = glm::powerOfTwoAbove(jfa_max - jfa_min);
	assert(area.x*area.y < MAKE_RB_JFA_CELL);

	std::vector<i16vec2> jfa_cell(area.x*area.y);
	for (int y = 0; y<area.y; y++)
	{
		for (int x = 0; x < area.x; x++)
		{
			jfa_cell[x + y * area.x] = i16vec2(x, y);
		}
	}
	for (int32_t i = 0; i < particle_num; i++)
	{
		pstate[i].relative_pos = pos[i] - center_of_mass;
		pstate[i].local_pos = pos[i] - vec2(jfa_min);

		ivec2 local_pos = ivec2(pstate[i].local_pos);
		jfa_cell[local_pos.x + local_pos.y*area.x] = i16vec2(-1);
	}

	Rigidbody rb;
	rb.R = vec4(1.f, 0.f, 0.f, 1.f);
	rb.cm = center_of_mass;
	rb.size_min = jfa_min;
	rb.size_max = jfa_max;
	rb.pnum = particle_num;
	rb.cm_work = ivec2(0);
	rb.Apq_work= ivec4(0);

	cmd.updateBuffer<i16vec2>(b_jfa_cell.getInfo().buffer, b_jfa_cell.getInfo().offset, jfa_cell);
	cmd.updateBuffer<Rigidbody>(b_make_rigidbody.getInfo().buffer, b_make_rigidbody.getInfo().offset, rb);
	cmd.updateBuffer<rbParticle>(b_make_particle.getInfo().buffer, b_make_particle.getInfo().offset, pstate);




	uint32_t block_num = pstate.size() / RB_PARTICLE_BLOCK_SIZE;
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 0, getDescriptorSet(PhysicsWorld::DescLayout_Data), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 1, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 2, getDescriptorSet(PhysicsWorld::DescLayout_Make), {});

		{
			// register
			{
				vk::BufferMemoryBarrier to_read[] = {
					b_manager.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_rbparticle_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer|vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);
			}

			cmd.pushConstants<uint>(m_pipeline_layout[PipelineLayout_MakeRB].get(), vk::ShaderStageFlagBits::eCompute, 0, block_num);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeRB_Register].get());
			auto num = app::calcDipatchGroups(uvec3(1, 1, 1), uvec3(1, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}

		{
			// make sdf
			vk::BufferMemoryBarrier to_read[] = 
			{
				b_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeRB_MakeJFCell].get());
			auto num = app::calcDipatchGroups(uvec3(area, 1), uvec3(8, 8, 1));

			uint area_max = glm::max(area.x, area.y);
			for (int distance = area_max >> 1; distance != 0; distance >>= 1)
			{
				vk::BufferMemoryBarrier to_read[] = {
					b_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);

				cmd.pushConstants<uvec4>(m_pipeline_layout[PipelineLayout_MakeRB].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec4{ distance, 0, area });
				cmd.dispatch(num.x, num.y, num.z);
			}
		}

		{
			// setup
			vk::BufferMemoryBarrier to_read[] =
			{
				b_make_particle.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer|vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.pushConstants<uvec4>(m_pipeline_layout[PipelineLayout_MakeRB].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec4{ block_num, 0, area });
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeRB_MakeSDF].get());
			cmd.dispatch(block_num, 1, 1);

		}
	}

	vk::BufferMemoryBarrier to_read[] =
	{
		b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		b_make_particle.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		b_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
		0, nullptr, array_length(to_read), to_read, 0, nullptr);


	m_particle_id += block_num;
}

void PhysicsWorld::execute(vk::CommandBuffer cmd)
{
	{
		vk::BufferMemoryBarrier to_write[] = {
			b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			b_world.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
			0, nullptr, array_length(to_write), to_write, 0, nullptr);
	}
	uint32_t data = 0;
	cmd.fillBuffer(b_fluid_counter.getInfo().buffer, b_fluid_counter.getInfo().offset, b_fluid_counter.getInfo().range, data);

	{
		World w;
		w.DT = 0.016f;
		w.STEP = 100;
		w.step = 0;
		w.rigidbody_num = m_rigidbody_id;
		w.rigidbody_max = RB_NUM_MAX;
		w.particleblock_max = RB_PARTICLE_BLOCK_NUM_MAX;
		cmd.updateBuffer<World>(b_world.getInfo().buffer, b_world.getInfo().offset, w);
	}

	{
		vk::BufferMemoryBarrier to_read[] = {
			b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			b_world.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}

}

void PhysicsWorld::executeMakeFluidWall(vk::CommandBuffer cmd)
{

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_ToFluidWall].get(), 0, m_descset[DescLayout_Data].get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_ToFluidWall].get(), 1, m_gi2d_context->getDescriptorSet(), {});

	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_ToFluidWall].get());
		cmd.dispatch(m_gi2d_context->RenderSize.x / 8, m_gi2d_context->RenderSize.y / 8, 1);
	}

}

