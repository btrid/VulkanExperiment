#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/sSystem.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>

namespace gi2d
{

struct GI2DFluid
{
	enum
	{
		Particle_Num = 1024,
		Particle_Type_Num = 128,
	};
	enum Shader
	{
		Shader_Update,
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
		Pipeline_Update,
		Pipeline_Pressure,
		Pipeline_ToFragment,
		Pipeline_Num,
	};
	struct Joint
	{
		int32_t parent;
		float rate;
		float linear_limit;
		float angle_limit;
	};
	struct ParticleData
	{
		float mass;
		float viscosity;
		float linear_limit;
		float _p;
	};

	GI2DFluid(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		{
			b_pos = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num*2,{} });
			b_grid_head = m_context->m_storage_memory.allocateMemory<int32_t>({ (uint32_t)m_gi2d_context->RenderWidth*m_gi2d_context->RenderHeight,{} });
			b_grid_node = m_context->m_storage_memory.allocateMemory<int32_t>({ Particle_Num,{} });
			b_type = m_context->m_storage_memory.allocateMemory<int32_t>({ Particle_Num,{} });
			b_data = m_context->m_storage_memory.allocateMemory<ParticleData>({ Particle_Type_Num,{} });

			cmd.fillBuffer(b_grid_head.getInfo().buffer, b_grid_head.getInfo().offset, b_grid_head.getInfo().range, -1);

//			if(0)
			{
				// debug用初期データ
				std::vector<vec4> pos(Particle_Num);
#if 1
				for (int i = 0; i < Particle_Num; i++)
				{
					auto& p = pos[i];
#if 1
					if (std::rand() % 100 > 97)
					{
#define area (800)
						p.x = 65 + std::rand() % area + (std::rand() % 10000) / 10000.f;
						p.y = 45 + std::rand() % area + (std::rand() % 10000) / 10000.f;
					}
#else
					int size = 1;
 					if (i < size*size)
					{
						p.x = 333 + (i / size);
						p.y = 333 + (i % size);
					}
#endif
					else
					{
						p.x = -1000.f;
						p.y = -1000.f;
					}
					p.z = p.x;
					p.w = p.y;
				}
#else
				std::vector<Joint> joint(Particle_Num);
				{
					auto& p = pos[0];
					p.x = 165.f;
					p.y = 145.f;
					p.z = p.x;
					p.w = p.y;

					joint[0].parent = -1;

				}
				for (int n = 0; n < 10; n++)
				{
					#define Angle_Num (30)
					for (int m = 0; m < Angle_Num; m++)
					{
						auto angle = m * (glm::two_pi<float>() / Angle_Num);
						auto r = vec2(0.f, 2.f + n * 2.f);
						r = glm::rotate(r, angle);

						auto i = n * Angle_Num + m + 1;
						auto& p = pos[i];
						p.x = 165 + r.x;
						p.y = 145 + r.y;
						p.z = p.x;
						p.w = p.y;

						joint[i].parent = n == 0 ? 0 : (n - 1)* Angle_Num + m;
						joint[i].rate = 0.5f;
						joint[i].angle_limit = 0.f;
						joint[i].linear_limit = 5.f;
					}
				}
#endif

				cmd.updateBuffer<vec4>(b_pos.getInfo().buffer, b_pos.getInfo().offset, pos);
				cmd.fillBuffer(b_type.getInfo().buffer, b_type.getInfo().offset, b_type.getInfo().range, 0);
				std::vector<ParticleData> pdata(Particle_Type_Num);
				pdata[0].mass = 10.f;
				pdata[0].linear_limit = 10.f;
				pdata[0].viscosity = 10.f;
				cmd.updateBuffer<ParticleData>(b_data.getInfo().buffer, b_data.getInfo().offset, pdata);

				vk::BufferMemoryBarrier to_read[] = {
					b_pos.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_data.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
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
					b_type.getInfo(),
					b_data.getInfo(),
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
				"Fluid_Update.comp.spv",
				"Fluid_CalcPressure.comp.spv",
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
				sSystem::Order().getSystemDescriptorLayout(),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayout_Fluid] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
			shader_info[0].setModule(m_shader[Shader_Update].get());
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
			m_pipeline[Pipeline_Update] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_Pressure] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_ToFragment] = std::move(compute_pipeline[2]);
		}

	}
	void execute(vk::CommandBuffer cmd)
	{

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 2, sSystem::Order().getSystemDescriptorSet(), { 0 * sSystem::Order().getSystemDescriptorStride() });

		{
			// linklist更新のため初期化
			vk::BufferMemoryBarrier to_write[] = {
				b_grid_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.fillBuffer(b_grid_head.getInfo().buffer, b_grid_head.getInfo().offset, b_grid_head.getInfo().range, -1);

		}

		{
			// 位置の更新
			vk::BufferMemoryBarrier to_read[] = {
				b_grid_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				m_gi2d_context->b_grid_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Update].get());

			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}

	}

	void executeCalc(vk::CommandBuffer cmd)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 2, sSystem::Order().getSystemDescriptorSet(), { 0 * sSystem::Order().getSystemDescriptorStride() });

		{
			// 圧力の更新
			vk::BufferMemoryBarrier to_read[] = {
				b_grid_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_gi2d_context->b_grid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_pos.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Pressure].get());
			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}

	void executePost(vk::CommandBuffer cmd)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 0, m_descriptor_set.get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Fluid].get(), 2, sSystem::Order().getSystemDescriptorSet(), { 0 * sSystem::Order().getSystemDescriptorStride() });
		// fragment_dataに書き込む
		{
			vk::BufferMemoryBarrier to_read[] = {
				m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				b_pos.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_ToFragment].get());
			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		vk::BufferMemoryBarrier to_read[] = {
			m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
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
	btr::BufferMemoryEx<int32_t> b_type;
	btr::BufferMemoryEx<ParticleData> b_data;
//	btr::BufferMemoryEx<Softbody> b_softbody;
	btr::BufferMemoryEx<int32_t> b_grid_head;
	btr::BufferMemoryEx<int32_t> b_grid_node;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;
public:
	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }



};

}