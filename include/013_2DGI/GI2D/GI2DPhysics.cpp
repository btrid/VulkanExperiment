#pragma once
#include "GI2DPhysics.h"
#include "GI2DContext.h"

GI2DPhysics::GI2DPhysics(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
{
	m_context = context;
	m_gi2d_context = gi2d_context;

	{
		auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
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
			vk::DescriptorSetLayoutBinding(9, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(11, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(12, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(13, vk::DescriptorType::eStorageBuffer, 1, stage),
		};
		vk::DescriptorSetLayoutCreateInfo desc_layout_info;
		desc_layout_info.setBindingCount(array_length(binding));
		desc_layout_info.setPBindings(binding);
		m_desc_layout[DescLayout_Data] = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
	}
	{
		auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
		vk::DescriptorSetLayoutBinding binding[] = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
		};
		vk::DescriptorSetLayoutCreateInfo desc_layout_info;
		desc_layout_info.setBindingCount(array_length(binding));
		desc_layout_info.setPBindings(binding);
		m_desc_layout[DescLayout_Make] = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
	}

	{
		const char* name[] =
		{
			"Rigid_ToFluid.comp.spv",
			"Rigid_ToFluidWall.comp.spv",

			"RigidMake_SetupRigidbody.comp.spv",
			"RigidMake_MakeJFA.comp.spv",
			"RigidMake_SetupParticle.comp.spv",
		};
		static_assert(array_length(name) == Shader_Num, "not equal shader num");

		std::string path = btr::getResourceShaderPath();
		for (size_t i = 0; i < array_length(name); i++) {
			m_shader[i] = loadShaderUnique(m_context->m_device, path + name[i]);
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
		m_pipeline_layout[PipelineLayout_ToFluid] = m_context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

#if USE_DEBUG_REPORT
		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.pObjectName = "PipelineLayout_ToFluid";
		name_info.objectType = vk::ObjectType::ePipelineLayout;
		name_info.objectHandle = reinterpret_cast<uint64_t &>(m_pipeline_layout[PipelineLayout_ToFluid].get());
		m_context->m_device.setDebugUtilsObjectNameEXT(name_info);
#endif
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
		m_pipeline_layout[PipelineLayout_MakeRB] = m_context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

#if USE_DEBUG_REPORT
		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.pObjectName = "PipelineLayout_MakeRigidbody";
		name_info.objectType = vk::ObjectType::ePipelineLayout;
		name_info.objectHandle = reinterpret_cast<uint64_t &>(m_pipeline_layout[PipelineLayout_MakeRB].get());
		m_context->m_device.setDebugUtilsObjectNameEXT(name_info);
#endif
	}

	// pipeline
	{
		std::array<vk::PipelineShaderStageCreateInfo, 11> shader_info;
		shader_info[0].setModule(m_shader[Shader_ToFluid].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		shader_info[1].setModule(m_shader[Shader_ToFluidWall].get());
		shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[1].setPName("main");
		shader_info[2].setModule(m_shader[Shader_MakeRB_SetupRigidbody].get());
		shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[2].setPName("main");
		shader_info[3].setModule(m_shader[Shader_MakeRB_MakeJFCell].get());
		shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[3].setPName("main");
		shader_info[4].setModule(m_shader[Shader_MakeRB_SetupParticle].get());
		shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[4].setPName("main");
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayout_ToFluid].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[1])
			.setLayout(m_pipeline_layout[PipelineLayout_ToFluid].get()),
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
// #if USE_DEBUG_REPORT
// 		vk::DebugUtilsObjectNameInfoEXT name_info;
// 		name_info.pObjectName = "Pipeline_ToFluid";
// 		name_info.objectType = vk::ObjectType::ePipeline;
// 		name_info.objectHandle = reinterpret_cast<uint64_t &>(compute_pipeline[0].get());
// 		m_context->m_device.setDebugUtilsObjectNameEXT(name_info, m_context->m_dispach);
// #endif

		m_pipeline[Pipeline_ToFluid] = m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
		m_pipeline[Pipeline_ToFluidWall] = m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
		m_pipeline[Pipeline_MakeRB_SetupRigidbody] = m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[2]).value;
		m_pipeline[Pipeline_MakeRB_MakeJFCell] = m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[3]).value;
		m_pipeline[Pipeline_MakeRB_SetupParticle] = m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[4]).value;
	}

	{
		b_world = m_context->m_storage_memory.allocateMemory<World>({ 1,{} });
		b_rigidbody = m_context->m_storage_memory.allocateMemory<Rigidbody>({ RB_NUM_MAX,{} });
		b_rbparticle = m_context->m_storage_memory.allocateMemory<rbParticle>({ RB_PARTICLE_NUM,{} });
		b_rbparticle_map = m_context->m_storage_memory.allocateMemory<uint32_t>({ RB_PARTICLE_BLOCK_NUM_MAX,{} });
		b_collidable_counter = m_context->m_storage_memory.allocateMemory<uint32_t>({ gi2d_context->m_desc.Resolution.x*gi2d_context->m_desc.Resolution.y,{} });
		b_collidable = m_context->m_storage_memory.allocateMemory<rbCollidable>({ COLLIDABLE_NUM * gi2d_context->m_desc.Resolution.x*gi2d_context->m_desc.Resolution.y,{} });
		b_collidable_wall = m_context->m_storage_memory.allocateMemory<f16vec2>({ gi2d_context->m_desc.Resolution.x*gi2d_context->m_desc.Resolution.y,{} });
		b_fluid_counter = m_context->m_storage_memory.allocateMemory<uint>({ gi2d_context->m_desc.Resolution.x*gi2d_context->m_desc.Resolution.y,{} });
		b_manager = m_context->m_storage_memory.allocateMemory<BufferManage>({ 1,{} });
		b_rb_memory_list = m_context->m_storage_memory.allocateMemory<uint>({ RB_NUM_MAX,{} });
		b_pb_memory_list = m_context->m_storage_memory.allocateMemory<uint>({ RB_PARTICLE_BLOCK_NUM_MAX,{} });
		b_update_counter = m_context->m_storage_memory.allocateMemory<uvec4>({ 4,{} });
		b_rb_update_list = m_context->m_storage_memory.allocateMemory<uint>({ RB_NUM_MAX*2,{} });
		b_pb_update_list = m_context->m_storage_memory.allocateMemory<uint>({ RB_PARTICLE_BLOCK_NUM_MAX*2,{} });
		{
			vk::DescriptorSetLayout layouts[] = {
				m_desc_layout[DescLayout_Data].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_descset[DescLayout_Data] = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo storages[] = {
				b_world.getInfo(),
				b_rigidbody.getInfo(),
				b_rbparticle.getInfo(),
				b_rbparticle_map.getInfo(),
				b_collidable_counter.getInfo(),
				b_collidable.getInfo(),
				b_collidable_wall.getInfo(),
				b_fluid_counter.getInfo(),
				b_manager.getInfo(),
				b_rb_memory_list.getInfo(),
				b_pb_memory_list.getInfo(),
				b_update_counter.getInfo(),
				b_rb_update_list.getInfo(),
				b_pb_update_list.getInfo(),
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
			context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

		b_make_rigidbody = m_context->m_storage_memory.allocateMemory<Rigidbody>({ 1,{} });
		b_make_particle = m_context->m_storage_memory.allocateMemory<rbParticle>({ MAKE_RB_SIZE_MAX,{} });
		b_make_jfa_cell = m_context->m_storage_memory.allocateMemory<i16vec2>({ MAKE_RB_JFA_CELL, {} });
		b_make_param = m_context->m_storage_memory.allocateMemory<RBMakeParam>({ 1,{} });
		b_make_callback = m_context->m_storage_memory.allocateMemory<RBMakeCallback>({ 1,{} });

		{
			vk::DescriptorSetLayout layouts[] = {
				m_desc_layout[DescLayout_Make].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_descset[DescLayout_Make] = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo storages[] = {
				b_make_rigidbody.getInfo(),
				b_make_particle.getInfo(),
				b_make_jfa_cell.getInfo(),
				b_make_param.getInfo(),
				b_make_callback.getInfo(),
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
			context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
		}


	}

	auto cmd = m_context->m_cmd_pool->allocCmdTempolary(0);
	{
		m_world.DT = 0.016f;
		m_world.STEP = 100;
		m_world.step = 0;
		m_world.rigidbody_num = 0;
		m_world.rigidbody_max = RB_NUM_MAX;
		m_world.particleblock_max = RB_PARTICLE_BLOCK_NUM_MAX;
		m_world.gpu_index = 0;
		m_world.cpu_index = 1;
		cmd.updateBuffer<World>(b_world.getInfo().buffer, b_world.getInfo().offset, m_world);
	}

	{
		{
			std::vector<uint> freelist(RB_NUM_MAX);
			for (uint i = 0; i < freelist.size(); i++)
			{
				freelist[i] = i;
			}
			cmd.updateBuffer<uint>(b_rb_memory_list.getInfo().buffer, b_rb_memory_list.getInfo().offset, freelist);
		}
		{
			std::vector<uint> freelist(RB_PARTICLE_BLOCK_NUM_MAX);
			for (uint i = 0; i < freelist.size(); i++)
			{
				freelist[i] = i;
			}
			cmd.updateBuffer<uint>(b_pb_memory_list.getInfo().buffer, b_pb_memory_list.getInfo().offset, freelist);
		}
		cmd.updateBuffer<BufferManage>(b_manager.getInfo().buffer, b_manager.getInfo().offset, BufferManage{ RB_NUM_MAX, RB_PARTICLE_BLOCK_NUM_MAX, 0, 0, 0, 0 });
		std::array<uvec4, 4> ac;
		ac.fill(uvec4(0, 1, 1, 0));
		cmd.updateBuffer<std::array<uvec4, 4>>(b_update_counter.getInfo().buffer, b_update_counter.getInfo().offset, ac);

		vk::BufferMemoryBarrier to_read[] = {
			b_world.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			b_manager.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead |vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}
}


void GI2DPhysics::execute(vk::CommandBuffer cmd)
{
	{
		vk::BufferMemoryBarrier to_write[] = {
			b_collidable_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			b_world.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
			0, nullptr, array_length(to_write), to_write, 0, nullptr);
	}
	uint32_t data = 0;
	cmd.fillBuffer(b_collidable_counter.getInfo().buffer, b_collidable_counter.getInfo().offset, b_collidable_counter.getInfo().range, data);
	cmd.fillBuffer(b_fluid_counter.getInfo().buffer, b_fluid_counter.getInfo().offset, b_fluid_counter.getInfo().range, data);

	{
		// 		static uint a;
		// 		World w;
		// 		w.DT = 0.016f;
		// 		w.STEP = 100;
		// 		w.step = 0;
		// 		w.rigidbody_max = RB_NUM_MAX;
		// 		w.particleblock_max = RB_PARTICLE_BLOCK_NUM_MAX;
		m_world.gpu_index = (m_world.gpu_index + 1) % 2;
		m_world.cpu_index = (m_world.cpu_index + 1) % 2;
		cmd.updateBuffer<World>(b_world.getInfo().buffer, b_world.getInfo().offset, m_world);
	}

	{
		vk::BufferMemoryBarrier to_read[] = {
			b_collidable_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			b_world.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}
}
void GI2DPhysics::make(vk::CommandBuffer cmd, const GI2DRB_MakeParam& param)
{
	DebugLabel _label(cmd, __FUNCTION__);
	const auto& box = param.aabb;

	auto particle_num = box.z * box.w;
	assert(particle_num <= MAKE_RB_SIZE_MAX);

	uint color = glm::packUnorm4x8(vec4(0.f, 0.f, 1.f, 1.f));
	uint edgecolor = glm::packUnorm4x8(vec4(1.f, 0.f, 0.f, 1.f));
	uint linecolor = glm::packUnorm4x8(vec4(0.f, 1.f, 0.f, 1.f));

	rbParticle _def;
	_def.flag = 0;
	_def.color = color;
	std::vector<vec2> pos(particle_num);
	std::vector<rbParticle> pstate((particle_num+63)/64*64, _def);
	uint32_t block_num = ceil(pstate.size() / (float)RB_PARTICLE_BLOCK_SIZE);

	vec2 size_max = vec2(-999999.f);
	vec2 size_min = vec2(999999.f);
	for (uint32_t y = 0; y < box.w; y++)
	{
		for (uint32_t x = 0; x < box.z; x++)
		{
			uint i = x + y * box.z;
			pos[i] = vec2(box.xy()) + vec2(x, y) + 0.5f;
//			pos[i] = vec2(box) + rotate(vec2(x, y) + 0.5f, 0.f);
			pstate[i].pos = pos[i];
			pstate[i].pos_old = pos[i];
			if (y == 0 || y == box.w - 1 || x == 0 || x == box.z - 1)
			{
				pstate[i].color = edgecolor;
			}
			pstate[i].flag |= RBP_FLAG_ACTIVE;

			size_max = glm::max(pos[i], size_max);
			size_min = glm::min(pos[i], size_min);
		}
	}
/*	{
		for (uint32_t x = 0; x < box.z; x++)
		{
			uint i1 = x + 0 * box.z;
			uint i2 = x + (box.w - 1) * box.z;
			pstate[i1].flag |= RBP_FLAG_COLLIDABLE;
			pstate[i2].flag |= RBP_FLAG_COLLIDABLE;
		}
		for (uint32_t y = 0; y < box.w; y++)
		{
			uint i1 = 0 + y * box.z;
			uint i2 = (box.z-1) + y * box.z;
			pstate[i1].flag |= RBP_FLAG_COLLIDABLE;
			pstate[i2].flag |= RBP_FLAG_COLLIDABLE;
		}
	}
	*/
	ivec2 jfa_max = ivec2(ceil(size_max));
	ivec2 jfa_min = ivec2(trunc(size_min));
	auto area = jfa_max - jfa_min;
	assert(area.x*area.y <= MAKE_RB_JFA_CELL);

	vec2 pos_sum = vec2(0.f);
	vec2 center_of_mass = vec2(0.f);
	for (int32_t i = 0; i < particle_num; i++)
	{
		pos_sum += pos[i];
	}
	center_of_mass = pos_sum / (double)particle_num;


	std::vector<i16vec2> jfa_cell(area.x*area.y, i16vec2(0xffff));
	for (int32_t i = 0; i < particle_num; i++)
	{
		pstate[i].relative_pos = glm::packHalf2x16( pos[i]- vec2(center_of_mass));
		pstate[i].local_pos = glm::packHalf2x16(pos[i] - vec2(jfa_min));

		ivec2 local_pos = ivec2(pos[i] - vec2(jfa_min));
		jfa_cell[local_pos.x + local_pos.y*area.x] = i16vec2(0xfffe);
	}
	cmd.updateBuffer<i16vec2>(b_make_jfa_cell.getInfo().buffer, b_make_jfa_cell.getInfo().offset, jfa_cell);

	{
		Rigidbody rb;
		rb.R = vec4(1.f, 0.f, 0.f, 1.f);
		rb.R_old = rb.R;
		rb.cm = center_of_mass;
		rb.cm_old = rb.cm;
		rb.flag = 0;
		rb.flag |= param.is_fluid ? RB_FLAG_FLUID : 0;
		rb.flag |= param.is_usercontrol ? RB_FLAG_USER_CONTROL : 0;
		rb.life = (std::rand() % 10) + 55;
		rb.pnum = particle_num;
		rb.cm_work = vec2(0);
		rb.Apq_work = ivec4(0);
		cmd.updateBuffer<Rigidbody>(b_make_rigidbody.getInfo().buffer, b_make_rigidbody.getInfo().offset, rb);
	}
	{
		RBMakeParam make_param;
		make_param.pb_num = uvec4(block_num, 1, 1, 0);
		make_param.registered_num = uvec4(0, 1, 1, 0);
		make_param.rb_aabb = ivec4(jfa_min, jfa_max);
		cmd.updateBuffer<RBMakeParam>(b_make_param.getInfo().buffer, b_make_param.getInfo().offset, make_param);
	}
	cmd.updateBuffer<rbParticle>(b_make_particle.getInfo().buffer, b_make_particle.getInfo().offset, pstate);


	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 0, getDescriptorSet(GI2DPhysics::DescLayout_Data), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 1, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 2, getDescriptorSet(GI2DPhysics::DescLayout_Make), {});

		// ŽžŠÔ‚Ì‚©‚©‚éjfa‚ðæ‚ÉŽÀs‚µ‚½‚¢
		_label.insert("GI2DPhysics::make_jfa");
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_make_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_make_param.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeRB_MakeJFCell].get());
			auto num = app::calcDipatchGroups(uvec3(area, 1), uvec3(8, 8, 1));

			uint area_max = glm::ceilPowerOfTwo(glm::max(area.x, area.y));
			for (int distance = area_max >> 1; distance != 0; distance >>= 1)
			{
				vk::BufferMemoryBarrier to_read[] = {
					b_make_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);

				cmd.pushConstants<uvec2>(m_pipeline_layout[PipelineLayout_MakeRB].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec2{ distance, 0 });
				cmd.dispatch(num.x, num.y, num.z);
			}
		}

	}
	_make(cmd);

// 	static int a;
// 	a++;
// 	printf("%d\n", a);

}
void GI2DPhysics::getRBID(vk::CommandBuffer cmd, const vk::DescriptorBufferInfo& info)
{
	vk::BufferMemoryBarrier to_read[] =
	{
		b_make_callback.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
		0, nullptr, array_length(to_read), to_read, 0, nullptr);

	vk::BufferCopy region;
	region.setSrcOffset(b_make_callback.getInfo().offset);
	region.setDstOffset(info.offset);
	region.setSize(4);
	cmd.copyBuffer(b_make_callback.getInfo().buffer, info.buffer, region);
}

void GI2DPhysics::_make(vk::CommandBuffer &cmd)
{
	{
		DebugLabel _label(cmd, __FUNCTION__);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 0, getDescriptorSet(GI2DPhysics::DescLayout_Data), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 1, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 2, getDescriptorSet(GI2DPhysics::DescLayout_Make), {});

		_label.insert("GI2DPhysics::SetupRigidbody");
		{
			{
				vk::BufferMemoryBarrier to_read[] = {
					b_manager.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_rbparticle_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					b_update_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					b_make_param.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eDrawIndirect, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeRB_SetupRigidbody].get());
			auto num = app::calcDipatchGroups(uvec3(1, 1, 1), uvec3(1, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}

		_label.insert("GI2DPhysics::SetupParticle");
		{
			vk::BufferMemoryBarrier to_read[] =
			{
				b_make_particle.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_make_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_make_param.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
				b_make_callback.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eDrawIndirect, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeRB_SetupParticle].get());
			cmd.dispatchIndirect(b_make_param.getInfo().buffer, b_make_param.getInfo().offset + offsetof(RBMakeParam, registered_num));
		}
	}

	vk::BufferMemoryBarrier to_read[] =
	{
		b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		b_make_particle.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		b_update_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
		0, nullptr, array_length(to_read), to_read, 0, nullptr);
}


