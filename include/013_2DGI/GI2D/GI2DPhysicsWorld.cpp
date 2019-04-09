#pragma once
#include "GI2DPhysicsWorld.h"
#include "GI2DContext.h"
#include "GI2DRigidbody.h"

namespace
{

void jacobiRotate(mat2& A, mat2& R)
{
	// rotates A through phi in 01-plane to set A(0,1) = 0
	// rotation stored in R whose columns are eigenvectors of A
	if (A[0][1] == 0.f) return;
	float d = (A[0][0] - A[1][1]) / (2.0*A[0][1]);
	float t = 1.0 / (abs(d) + sqrt(d*d + 1.0));
	if (d < 0.0) t = -t;
	float c = 1.0 / sqrt(t*t + 1);
	float s = t * c;
	A[0][0] += t * A[0][1];
	A[1][1] -= t * A[0][1];
	A[0][1] = A[1][0] = 0.0;
	// store rotation in R
	for (int k = 0; k < 2; k++) {
		float Rkp = c * R[k][0] + s * R[k][1];
		float Rkq = -s * R[k][0] + c * R[k][1];
		R[k][0] = Rkp;
		R[k][1] = Rkq;
	}
}


// --------------------------------------------------
void eigenDecomposition(mat2& A, mat2& R)
{
	// only for symmetric matrices!
	// A = R A' R^T, where A' is diagonal and R orthonormal

	R = mat2(1.0);	// unit matrix
	jacobiRotate(A, R);
}


// --------------------------------------------------
void polarDecomposition(const mat2& A, mat2& R, mat2& S)
{
	// A = RS, where S is symmetric and R is orthonormal
	// -> S = (A^T A)^(1/2)

	mat2 ATA = transpose(A) * A;
	//	ATA.multiplyTransposedLeft(A, A);

	mat2 U = mat2(1.0);
	eigenDecomposition(ATA, U);

	float l0 = ATA[0][0]; if (l0 <= 0.0) l0 = 0.0; else l0 = 1.0 / sqrt(l0);
	float l1 = ATA[1][1]; if (l1 <= 0.0) l1 = 0.0; else l1 = 1.0 / sqrt(l1);

	mat2 S1;
	S1[0][0] = l0 * U[0][0] * U[0][0] + l1 * U[0][1] * U[0][1];
	S1[0][1] = l0 * U[0][0] * U[1][0] + l1 * U[0][1] * U[1][1];
	S1[1][0] = S1[0][1];
	S1[1][1] = l0 * U[1][0] * U[1][0] + l1 * U[1][1] * U[1][1];
	R = A * S1;
	S = transpose(R)*A;
	//	S.multiplyTransposedLeft(R, A);
}

}

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
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(6),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(7),
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
		b_rbparticle_map = m_context->m_storage_memory.allocateMemory<uint32_t>({ RB_PARTICLE_NUM / RB_PARTICLE_BLOCK_SIZE,{} });
		b_fluid_counter = m_context->m_storage_memory.allocateMemory<uint32_t>({ gi2d_context->RenderSize.x*gi2d_context->RenderSize.y,{} });
		b_fluid = m_context->m_storage_memory.allocateMemory<rbFluid>({ 8 * gi2d_context->RenderSize.x*gi2d_context->RenderSize.y,{} });
		b_constraint_counter = m_context->m_storage_memory.allocateMemory<uvec4>({ 1,{} });
		b_constraint = m_context->m_storage_memory.allocateMemory<rbConstraint>({ RB_PARTICLE_NUM,{} });

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
				b_constraint_counter.getInfo(),
				b_constraint.getInfo(),
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

	uint32_t contact_index = 0;
	for (uint32_t y = 0; y < box.w; y++)
	{
		for (uint32_t x = 0; x < box.z; x++)
		{
//			pos[x + y * box.z].x = box.x + x + 0.5f;
//			pos[x + y * box.z].y = box.y + y + 0.5f;
			pos[x + y * box.z] = vec2(box) + rotate(vec2(x, y) + 0.5f, 0.2f);
//			pos[x + y * box.z] = vec2(box) + rotate(vec2(x, y) + 0.5f, 0.f);
			pstate[x + y * box.z].pos = pos[x + y * box.z];
			pstate[x + y * box.z].pos_old = pos[x + y * box.z];
			pstate[x + y * box.z].pos_predict = pos[x + y * box.z];
			pstate[x + y * box.z].contact_index = -1;
//			if (y == 0 || y == box.w - 1 || x == 0 || x == box.z - 1) 
			{
				pstate[x + y * box.z].contact_index = contact_index++;
			}
			pstate[x + y * box.z].is_contact = 0;
			pstate[x + y * box.z].is_active = true;
		}
	}

	ivec2 pos_integer = ivec2(0);
	ivec2 pos_decimal = ivec2(0);
	for (int32_t i = 0; i < particle_num; i++)
	{
		pos_integer += ivec2(round(pos[i]/ particle_num * 65535.f));
		pos_decimal += ivec2((pos[i] - trunc(pos[i]))*65535.f);
	}
	vec2 _center = vec2(pos_integer) / 65535.f;// + vec2(pos_decimal) / 65535.f;
	vec2 center = vec2(0.f);
	for (int32_t i = 0; i < particle_num; i++)
	{
		center += pos[i];
	}
	center /= particle_num;
//	_center = trunc(_center / particle_num);


	vec2 size = vec2(0.f);
	vec2 cm = vec2(0.f);
	vec2 size_max = vec2(0.f);
	vec2 size_min = vec2(0.f);
	for (int32_t i = 0; i < particle_num; i++)
	{
		pstate[i].relative_pos = pos[i] - center;
		cm += pos[i];
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
	inertia /= 12.f;

	auto p = pstate[0].relative_pos;
	vec2 v = vec2(9999999.f);
	float distsq = 9999999999999.f;
	auto f = [&](const vec2& n)
	{
		auto sq = dot(p - n, p - n);
		if (sq < distsq) 
		{
			distsq = sq;
			v = n - p;
		}
		else if (sq - FLT_EPSILON < distsq)
		{
			v += n - p;
			v = normalize(v);
		}
	};
	for (uint32_t i = 0; i < particle_num; i++)
	{
		p = pstate[i].relative_pos;
		v = vec2(9999999.f);
		distsq = 9999999999999.f;

		f(vec2(p.x, size_max.y + 1.f));
		f(vec2(p.x, size_min.y - 1.f));
		f(vec2(size_max.x + 1.f, p.y));
		f(vec2(size_min.x - 1.f, p.y));

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

	mat2 Aqq = mat2(0.f);
	for (uint32_t i = 0; i < particle_num; i++)
	{
		const auto& q = pstate[i].relative_pos;
		auto qq = q.xxyy() * q.xyxy();
		Aqq[0][0] += qq.x;
		Aqq[0][1] += qq.y;
		Aqq[1][0] += qq.z;
		Aqq[1][1] += qq.w;
	}
//	rb.Aqq_inv = inverse(rb.Aqq);

	mat2 S, R;
	polarDecomposition(Aqq, R, S);
	rb.R = vec4(R[0][0], R[1][0], R[0][1], R[1][1]);
	rb.cm = cm/ particle_num;
	rb.pnum = particle_num;
	rb.solver_count = 0;
	rb.inertia = inertia;
	rb.mass = 1;

	rb.center = size / 2.f;
	rb.size = ceil(size_max - size_min);

	rb.pos = center;
	rb.pos_predict = rb.pos;
//	rb.pos_old = rb.pos - vec2(0.f, 0.5f);
	rb.pos_old = rb.pos;
	//	rb.angle = 3.14f / 4.f + 0.2f;
	rb.angle = 0.f;
	rb.angle_predict = rb.angle;
	rb.angle_old = rb.angle;
	rb.cm_work = ivec2(0);
	rb.Apq_work= ivec4(0);

	rb.exclusion = ivec2(0);
	rb.exclusion_angle = 0;
	rb.is_exclusive = 0;
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

