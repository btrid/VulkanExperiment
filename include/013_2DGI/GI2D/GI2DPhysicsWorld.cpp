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
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(4),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(5),
		};
		vk::DescriptorSetLayoutCreateInfo desc_layout_info;
		desc_layout_info.setBindingCount(array_length(binding));
		desc_layout_info.setPBindings(binding);
		m_physics_world_desc_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
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
			m_physics_world_desc_layout.get(),
			m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
		};

		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayout_ToFluid] = m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
	}
	{
		vk::DescriptorSetLayout layouts[] = {
			m_physics_world_desc_layout.get(),
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
		b_rbparticle_map = m_context->m_storage_memory.allocateMemory<uint32_t>({ RB_PARTICLE_NUM / 64,{} });
		b_fluid_counter = m_context->m_storage_memory.allocateMemory<uint32_t>({ gi2d_context->RenderSize.x*gi2d_context->RenderSize.y,{} });
		b_fluid = m_context->m_storage_memory.allocateMemory<rbFluid>({ 4 * gi2d_context->RenderSize.x*gi2d_context->RenderSize.y,{} });

		{
			vk::DescriptorSetLayout layouts[] = {
				m_physics_world_desc_layout.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_physics_world_desc = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo storages[] = {
				b_world.getInfo(),
				b_rigidbody.getInfo(),
				b_rbparticle.getInfo(),
				b_rbparticle_map.getInfo(),
				b_fluid_counter.getInfo(),
				b_fluid.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(0)
				.setDstSet(m_physics_world_desc.get()),
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

	}

	{
		World w;
		w.DT = 0.016f;
		w.STEP = 100;
		w.step = 0;
		w.rigidbody_num = m_rigidbody_id;
		m_context->m_cmd_pool->allocCmdTempolary(0).updateBuffer<World>(b_world.getInfo().buffer, b_world.getInfo().offset, w);
	}

}

void PhysicsWorld::make(vk::CommandBuffer cmd, const uvec4& box)
{
	auto particle_num = box.z * box.w;
	Rigidbody rb;
	auto r_id = m_rigidbody_id++;

	rbParticle _def;
	_def.contact_index = -1;
	_def.r_id = r_id;
	_def.is_active = false;
	std::vector<vec2> pos(particle_num);
	std::vector<rbParticle> pstate((particle_num+63)/64*64, _def);
	vec2 center = vec2(0.f);

	uint32_t contact_index = 0;
	for (uint32_t y = 0; y < box.w; y++)
	{
		for (uint32_t x = 0; x < box.z; x++)
		{
			pos[x + y * box.z].x = box.x + x;
			pos[x + y * box.z].y = box.y + y;

			pstate[x + y * box.z].contact_index = -1;
			if (y == 0 || y == box.w - 1 || x == 0 || x == box.z - 1) 
			{
				pstate[x + y * box.z].contact_index = contact_index++;
			}
			pstate[x + y * box.z].is_contact = 0;
			pstate[x + y * box.z].is_active = true;
		}
	}
	for (int32_t i = 0; i < particle_num; i++)
	{
		center += pos[i];
	}
	center /= particle_num;

	vec2 size = vec2(0.f);
	vec2 size_max = vec2(0.f);
	vec2 size_min = vec2(0.f);
	for (int32_t i = 0; i < particle_num; i++)
	{
		pstate[i].relative_pos = pos[i] - center;
		size += pstate[i].relative_pos;
		size_max = glm::max(pstate[i].relative_pos, size_max);
		size_min = glm::min(pstate[i].relative_pos, size_min);
	}
	size /= particle_num;

	float inertia = 0.f;
	for (uint32_t i = 0; i < particle_num; i++)
	{
		{
			inertia += dot(pstate[i].relative_pos, pstate[i].relative_pos) /** mass*/;
		}
	}

	auto p = pstate[0].relative_pos;
	vec2 v = vec2(9999999.f);
	float distsq = 9999999999999.f;
	auto f = [&](const vec2& n)
	{
		auto sq = dot(p - n, p - n);
		if (sq < distsq) {
			distsq = sq;
			v = n - p;
		}
	};
	for (uint32_t i = 0; i < particle_num; i++)
	{
		p = pstate[i].relative_pos;
		v = vec2(9999999.f);
		distsq = 9999999999999.f;

		f(vec2(size_max.x + 1.f, p.y));
		f(vec2(size_min.x - 1.f, p.y));
		f(vec2(p.x, size_max.y + 1.f));
		f(vec2(p.x, size_min.y - 1.f));

// 		for (uint32_t y = 0; y < box.w; y++)
// 		{
// 			for (uint32_t x = 0; x < box.z; x++)
// 			{
// 				auto pid = x + y * box.z;
// 				if (pid == i){ continue;}
// 				if (pstate[pid].is_active){ continue; }
// 				f(pstate[pid].relative_pos);
// 			}
// 		}
		pstate[i].sdf = v;
	}

	inertia /= 12.f;
	rb.pos = center;
	rb.pos_old = rb.pos;
	rb.center = size / 2.f;
	rb.inertia = inertia;
	rb.size = ceil(size_max - size_min);
	rb.vel = vec2(0.f);
	rb.pos_work = ivec2(0);
	rb.vel_work = ivec2(0);
	rb.angle_vel_work = 0;
	rb.pnum = particle_num;
	rb.angle = 3.14f / 4.f + 0.2f;
	rb.angle_vel = 0.f;
	rb.solver_count = 0;
	rb.dist = -1;
	rb.damping_work = ivec2(0);
	cmd.updateBuffer<Rigidbody>(b_rigidbody.getInfo().buffer, b_rigidbody.getInfo().offset + r_id * sizeof(Rigidbody), rb);

	uint32_t block_num = pstate.size() / 64;
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

void PhysicsWorld::executeMakeFluid(vk::CommandBuffer cmd)
{
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_ToFluid].get(), 0, m_physics_world_desc.get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_ToFluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_ToFluid].get());
// 	auto num = app::calcDipatchGroups(uvec3(r->m_particle_num, 1, 1), uvec3(1, 1, 1));
// 	cmd.dispatch(num.x, num.y, num.z);

	{
		vk::BufferMemoryBarrier to_read[] = {
			b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			b_fluid.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}
}
void PhysicsWorld::executeMakeFluidWall(vk::CommandBuffer cmd)
{

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_ToFluidWall].get(), 0, m_physics_world_desc.get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_ToFluidWall].get(), 1, m_gi2d_context->getDescriptorSet(), {});


	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_ToFluidWall].get());
		cmd.dispatch(m_gi2d_context->RenderSize.x / 8, m_gi2d_context->RenderSize.y / 8, 1);
	}

}

