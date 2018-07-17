#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>

namespace gi2d
{

struct GI2DFluid
{
	enum
	{
		Particle_Num = 1024,
	};
	enum Shader
	{
		Shader_Viscosity,
		Shader_Move1,
		Shader_Collision,
		Shader_CollisionAfter,
		Shader_Pressure,
		Shader_PressureMinimum,
		Shader_PressureGradient,
		Shader_Move2,
		Shader_ToFragment,
		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_Fluid,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_Viscosity,
		Pipeline_Move1_Viscosity_Gravity_Grid,
		Pipeline_Collision,
		Pipeline_CollisionAfter,
		Pipeline_Pressure,
		Pipeline_PressureMinimum,
		Pipeline_PressureGradient,
		Pipeline_Move2,
		Pipeline_ToFragment,
		Pipeline_Num,
	};

	// calc viscosity
	// premove
	// grid 
	// collision
	// collision after
	// calc pressure
	// calc minimum pressure
	// calc pressure gradient
	// move

	GI2DFluid(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		{
			b_pos = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num,{} });
			b_vel = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num,{} });
			b_acc = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num,{} });
			b_type = m_context->m_storage_memory.allocateMemory<int32_t>({ Particle_Num,{} });
			b_pressure = m_context->m_storage_memory.allocateMemory<float>({ Particle_Num,{} });
			b_minimum_pressure = m_context->m_storage_memory.allocateMemory<float>({ Particle_Num,{} });
			b_grid_head = m_context->m_storage_memory.allocateMemory<int32_t>({ (uint32_t)m_gi2d_context->RenderWidth*m_gi2d_context->RenderHeight,{} });
			b_grid_node = m_context->m_storage_memory.allocateMemory<int32_t>({ Particle_Num,{} });

			cmd.fillBuffer(b_vel.getInfo().buffer, b_vel.getInfo().offset, b_vel.getInfo().range, 0);
			cmd.fillBuffer(b_acc.getInfo().buffer, b_acc.getInfo().offset, b_acc.getInfo().range, 0);
			cmd.fillBuffer(b_grid_head.getInfo().buffer, b_grid_head.getInfo().offset, b_grid_head.getInfo().range, -1);

			{
				// debug用初期データ
				std::vector<vec2> pos(Particle_Num);
				for (auto& p : pos) {
					p.x = std::rand() % 450 + 50;
					p.y = std::rand() % 20 + 30;
				}
				cmd.updateBuffer<vec2>(b_pos.getInfo().buffer, b_pos.getInfo().offset, pos);
				vk::BufferMemoryBarrier to_read[] = {
					b_pos.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);

			}
		}

		{

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
				m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

				vk::DescriptorBufferInfo storages[] = {
					b_pos.getInfo(),
					b_vel.getInfo(),
					b_acc.getInfo(),
					b_type.getInfo(),
					b_pressure.getInfo(),
					b_minimum_pressure.getInfo(),
					b_grid_head.getInfo(),
					b_grid_node.getInfo(),
				};

				vk::WriteDescriptorSet write[] = 
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(0)
					.setDstSet(m_descriptor_set.get()),
				};
				context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
			}

		}

		{
			const char* name[] =
			{
				"Fluid_CalcViscosity.comp.spv",
				"Fluid_Move1.comp.spv",
				"Fluid_Collision.comp.spv",
				"Fluid_CollisionAfter.comp.spv",
				"Fluid_CalcPressure.comp.spv",
				"Fluid_CalcPressureMinimum.comp.spv",
				"Fluid_CalcPressureGradient.comp.spv",
				"Fluid_Move2.comp.spv",
				"Fluid_ToFragment.comp.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout.get(),
				gi2d_context->getDescriptorSetLayout(),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayout_Fluid] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
			shader_info[0].setModule(m_shader[Shader_Viscosity].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[Shader_Move1].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[Shader_Collision].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			shader_info[3].setModule(m_shader[Shader_CollisionAfter].get());
			shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[3].setPName("main");
			shader_info[4].setModule(m_shader[Shader_Pressure].get());
			shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[4].setPName("main");
			shader_info[5].setModule(m_shader[Shader_PressureMinimum].get());
			shader_info[5].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[5].setPName("main");
			shader_info[6].setModule(m_shader[Shader_PressureGradient].get());
			shader_info[6].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[6].setPName("main");
			shader_info[7].setModule(m_shader[Shader_Move2].get());
			shader_info[7].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[7].setPName("main");
			shader_info[8].setModule(m_shader[Shader_ToFragment].get());
			shader_info[8].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[8].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Fluid].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_Fluid].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_Fluid].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[3])
				.setLayout(m_pipeline_layout[PipelineLayout_Fluid].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[4])
				.setLayout(m_pipeline_layout[PipelineLayout_Fluid].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[5])
				.setLayout(m_pipeline_layout[PipelineLayout_Fluid].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[6])
				.setLayout(m_pipeline_layout[PipelineLayout_Fluid].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[7])
				.setLayout(m_pipeline_layout[PipelineLayout_Fluid].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[8])
				.setLayout(m_pipeline_layout[PipelineLayout_Fluid].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[Pipeline_Viscosity] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_Move1_Viscosity_Gravity_Grid] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_Collision] = std::move(compute_pipeline[2]);
			m_pipeline[Pipeline_CollisionAfter] = std::move(compute_pipeline[3]);
			m_pipeline[Pipeline_Pressure] = std::move(compute_pipeline[4]);
			m_pipeline[Pipeline_PressureMinimum] = std::move(compute_pipeline[5]);
			m_pipeline[Pipeline_PressureGradient] = std::move(compute_pipeline[6]);
			m_pipeline[Pipeline_Move2] = std::move(compute_pipeline[7]);
			m_pipeline[Pipeline_ToFragment] = std::move(compute_pipeline[8]);
		}


	}
	void execute(vk::CommandBuffer cmd)
	{

		{

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Viscosity].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
			cmd.dispatch(1, 1, 1);
		}

		{
			// linklist更新のため初期化
			vk::BufferMemoryBarrier to_write[] = {
				b_grid_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.fillBuffer(b_grid_head.getInfo().buffer, b_grid_head.getInfo().offset, b_grid_head.getInfo().range, -1);

			vk::BufferMemoryBarrier to_read[] = {
				b_grid_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_acc.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer|vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Move1_Viscosity_Gravity_Grid].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
			cmd.dispatch(1, 1, 1);
		}

		{
			vk::BufferMemoryBarrier to_read[] = {
				b_grid_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Collision].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
			cmd.dispatch(1, 1, 1);
		}
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_acc.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_CollisionAfter].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
			cmd.dispatch(1, 1, 1);
		}
		{

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Pressure].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
			cmd.dispatch(1, 1, 1);
		}
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_pressure.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_PressureMinimum].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
			cmd.dispatch(1, 1, 1);
		}
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_pressure.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_PressureMinimum].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
			cmd.dispatch(1, 1, 1);
		}
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_minimum_pressure.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_acc.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_PressureGradient].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
			cmd.dispatch(1, 1, 1);
		}
		{
			if(0)
			{
				// linklist更新のため初期化
				vk::BufferMemoryBarrier to_write[] = {
					b_grid_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
					0, nullptr, array_length(to_write), to_write, 0, nullptr);

				cmd.fillBuffer(b_grid_head.getInfo().buffer, b_grid_head.getInfo().offset, b_grid_head.getInfo().range, -1);

				vk::BufferMemoryBarrier to_read[] = {
					b_grid_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);
			}
			vk::BufferMemoryBarrier to_read[] = {
				b_vel.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_acc.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Move2].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
			cmd.dispatch(1, 1, 1);

		}

		// fragment_dataに書き込む
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_pos.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_gi2d_context->b_fragment_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_ToFragment].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
			cmd.dispatch(1, 1, 1);

		}
	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	btr::BufferMemoryEx<vec2> b_pos;
	btr::BufferMemoryEx<vec2> b_vel;
	btr::BufferMemoryEx<vec2> b_acc;
	btr::BufferMemoryEx<int32_t> b_type;
	btr::BufferMemoryEx<float> b_pressure;
	btr::BufferMemoryEx<float> b_minimum_pressure;
	btr::BufferMemoryEx<int32_t> b_grid_head;
	btr::BufferMemoryEx<int32_t> b_grid_node;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;


};

}