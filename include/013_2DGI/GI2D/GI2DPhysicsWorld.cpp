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

	// pipeline
	{
		std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
		shader_info[0].setModule(m_shader[Shader_ToFluid].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		shader_info[1].setModule(m_shader[Shader_ToFluidWall].get());
		shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[1].setPName("main");
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayout_ToFluid].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[1])
			.setLayout(m_pipeline_layout[PipelineLayout_ToFluidWall].get()),
		};
		auto compute_pipeline = m_context->m_device->createComputePipelinesUnique(m_context->m_cache.get(), compute_pipeline_info);
		m_pipeline[Pipeline_ToFluid] = std::move(compute_pipeline[0]);
		m_pipeline[Pipeline_ToFluidWall] = std::move(compute_pipeline[1]);
	}

	{
		b_world = m_context->m_storage_memory.allocateMemory<World>({ 1,{} });
		b_rigidbody = m_context->m_storage_memory.allocateMemory<Rigidbody>({ RB_NUM,{} });
		b_rbparticle = m_context->m_storage_memory.allocateMemory<rbParticle>({ RB_PARTICLE_NUM,{} });
		b_rbparticle_map = m_context->m_storage_memory.allocateMemory<uint32_t>({ RB_PARTICLE_NUM / RB_PARTICLE_BLOCK_SIZE,{} });
		b_fluid_counter = m_context->m_storage_memory.allocateMemory<uint32_t>({ gi2d_context->RenderSize.x*gi2d_context->RenderSize.y,{} });
		b_fluid = m_context->m_storage_memory.allocateMemory<rbFluid>({ 4 * gi2d_context->RenderSize.x*gi2d_context->RenderSize.y,{} });
		b_manager = m_context->m_storage_memory.allocateMemory<uvec4>({ 1,{} });
		b_rb_freelist = m_context->m_storage_memory.allocateMemory<uint>({ RB_NUM,{} });
		b_particle_freelist = m_context->m_storage_memory.allocateMemory<uint>({ RB_NUM,{} });

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

		b_make_particle = m_context->m_storage_memory.allocateMemory<rbParticle>({ RB_PARTICLE_NUM,{} });
		b_posbit = m_context->m_storage_memory.allocateMemory<uint64_t>({ MAKE_RB_BIT_SIZE * MAKE_RB_BIT_SIZE,{} });
		b_jfa_cell = m_context->m_storage_memory.allocateMemory<u16vec2>({ MAKE_RB_SIZE_MAX* MAKE_RB_SIZE_MAX,{} });
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
				b_make_particle.getInfo(),
				b_posbit.getInfo(),
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
		cmd.updateBuffer<World>(b_world.getInfo().buffer, b_world.getInfo().offset, w);
	}

	{
		{
			std::vector<uint> freelist(RB_NUM);
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
	}
}

void PhysicsWorld::make(vk::CommandBuffer cmd, const uvec4& box)
{
	auto particle_num = box.z * box.w;
	Rigidbody rb;
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
	for (uint32_t y = 0; y < box.w; y++)
	{
		for (uint32_t x = 0; x < box.z; x++)
		{
			pos[x + y * box.z] = vec2(box) + rotate(vec2(x, y) + 0.5f, 0.f);
			pstate[x + y * box.z].pos = pos[x + y * box.z];
			pstate[x + y * box.z].pos_old = pos[x + y * box.z];
			pstate[x + y * box.z].contact_index = contact_index++;
			if (y == 0 || y == box.w - 1 || x == 0 || x == box.z - 1)
			{
				pstate[x + y * box.z].color = edgecolor;
			}
			pstate[x + y * box.z].is_contact = 0;
			pstate[x + y * box.z].is_active = true;
		}
	}

	vec2 center = vec2(0.f);
	for (int32_t i = 0; i < particle_num; i++)
	{
		center += pos[i];
	}
	center /= particle_num;


	vec2 size_max = vec2(0.f);
	vec2 size_min = vec2(0.f);
	for (int32_t i = 0; i < particle_num; i++)
	{
		pstate[i].relative_pos = pos[i] - center;
		size_max = glm::max(pstate[i].relative_pos, size_max);
		size_min = glm::min(pstate[i].relative_pos, size_min);
	}

	auto f = [&](const vec2& p, const vec2& target, float& distsq, vec2& sdf)
	{
		auto sq = dot(p - target, p - target);
		if (sq < distsq) 
		{
			distsq = sq;
			sdf = target - p;
		}
	};
	for (uint32_t i = 0; i < particle_num; i++)
	{
		const auto& p = pstate[i].relative_pos;
		auto sdf = vec2(0.f);
		auto distsq = 9999999999999.f;

		f(p, vec2(p.x, size_max.y + 1.f), distsq, sdf);
		f(p, vec2(p.x, size_min.y - 1.f), distsq, sdf);
		f(p, vec2(size_max.x + 1.f, p.y), distsq, sdf);
		f(p, vec2(size_min.x - 1.f, p.y), distsq, sdf);
		pstate[i].sdf = normalize(sdf);
	}
	rb.R = vec4(1.f, 0.f, 0.f, 1.f);
	rb.cm = center;
	rb.pnum = particle_num;
	rb.cm_work = ivec2(0);
	rb.Apq_work= ivec4(0);

	cmd.updateBuffer<Rigidbody>(b_rigidbody.getInfo().buffer, b_rigidbody.getInfo().offset + r_id * sizeof(Rigidbody), rb);

	uint32_t block_num = pstate.size() / RB_PARTICLE_BLOCK_SIZE;
	for (uint32_t i = 0; i < block_num; i++)
	{
		cmd.updateBuffer(b_rbparticle.getInfo().buffer, b_rbparticle.getInfo().offset + m_particle_id * sizeof(rbParticle) * 64, sizeof(rbParticle) * 64, &pstate[i * 64]);
		cmd.updateBuffer<uint32_t>(b_rbparticle_map.getInfo().buffer, b_rbparticle_map.getInfo().offset + m_particle_id * sizeof(uint32_t), r_id);
		m_particle_id++;
	}
	{
		vk::BufferMemoryBarrier to_read[] = {
			b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			b_rbparticle_map.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}

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

