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

struct GI2DFluid2
{
	enum
	{
		Particle_Num = 1024,
	};
	enum Shader
	{
		Shader_Move_Grid,
		Shader_Pressure,
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
		Pipeline_Move_Grid,
		Pipeline_Pressure,
		Pipeline_ToFragment,
		Pipeline_Num,
	};

	GI2DFluid2(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		{
			b_pos = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num,{} });
			b_vel = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num,{} });
			b_acc = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num,{} });
			b_grid_head = m_context->m_storage_memory.allocateMemory<int32_t>({ (uint32_t)m_gi2d_context->RenderWidth*m_gi2d_context->RenderHeight,{} });
			b_grid_node = m_context->m_storage_memory.allocateMemory<int32_t>({ Particle_Num,{} });
			b_grid_counter = m_context->m_storage_memory.allocateMemory<int32_t>({ (uint32_t)m_gi2d_context->RenderWidth*m_gi2d_context->RenderHeight,{} });
			b_type = m_context->m_storage_memory.allocateMemory<int32_t>({ Particle_Num,{} });

			cmd.fillBuffer(b_vel.getInfo().buffer, b_vel.getInfo().offset, b_vel.getInfo().range, 0);
			cmd.fillBuffer(b_acc.getInfo().buffer, b_acc.getInfo().offset, b_acc.getInfo().range, 0);
			cmd.fillBuffer(b_grid_head.getInfo().buffer, b_grid_head.getInfo().offset, b_grid_head.getInfo().range, -1);

			{
				// debug用初期データ
				std::array<vec2, Particle_Num> pos;
				pos.fill(vec2(10.f));
				for (int i = 0; i < Particle_Num; i++)
				{
#define Scale (100.)
#define area (20)
					auto& p = pos[i];
					p.x = 65*100 + std::rand() % (area*100) + 10*100;
					p.y = 75 * 100 + std::rand() % (area*100) + 500;
					p.x /= 100.f;
					p.y /= 100.f;
					p.x /= Scale;
					p.y /= Scale;
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
					b_grid_head.getInfo(),
					b_grid_node.getInfo(),
					b_grid_counter.getInfo(),
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
				"Boid_Move_Grid.comp.spv",
				"Boid_CalcPressure.comp.spv",
				"Boid_ToFragment.comp.spv",
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
			shader_info[0].setModule(m_shader[Shader_Move_Grid].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[Shader_Pressure].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[Shader_ToFragment].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
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
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[Pipeline_Move_Grid] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_Pressure] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_ToFragment] = std::move(compute_pipeline[2]);
		}

	}
	void execute(vk::CommandBuffer cmd)
	{

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});

		{
			// linklist更新のため初期化
			vk::BufferMemoryBarrier to_write[] = {
				b_grid_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				b_grid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.fillBuffer(b_grid_head.getInfo().buffer, b_grid_head.getInfo().offset, b_grid_head.getInfo().range, -1);
			cmd.fillBuffer(b_grid_counter.getInfo().buffer, b_grid_counter.getInfo().offset, b_grid_counter.getInfo().range, 0);

		}

		{
			vk::BufferMemoryBarrier to_read[] = {
				b_grid_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				b_grid_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				b_acc.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Move_Grid].get());
			cmd.dispatch(1, 1, 1);

		}

		{
			vk::BufferMemoryBarrier to_read[] = {
				b_grid_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_grid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_acc.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Pressure].get());
			cmd.dispatch(1, 1, 1);
		}
		// fragment_dataに書き込む
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_pos.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_gi2d_context->b_fragment_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_ToFragment].get());
			cmd.dispatch(1, 1, 1);
		}

		vk::BufferMemoryBarrier to_read[] = {
			m_gi2d_context->b_fragment_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
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
	btr::BufferMemoryEx<int32_t> b_grid_head;
	btr::BufferMemoryEx<int32_t> b_grid_node;
	btr::BufferMemoryEx<int32_t> b_grid_counter;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;


};

}