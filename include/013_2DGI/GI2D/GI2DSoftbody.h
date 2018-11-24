#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>
#include <013_2DGI/GI2D/GI2DFluid.h>


struct GI2DSoftbody
{
	enum 
	{
		xx = 4,
		yy = 30,
		Particle_Num = xx*yy,
	};
	struct Softbody
	{
		int32_t pnum;
		int32_t particle_offset;
		int32_t softbody_offset;
		int32_t _p;
		vec2 center;
		ivec2 center_work;
	};
	enum Shader
	{
		Shader_CalcCenter,
		Shader_CalcCenter_Post,
		Shader_CalcForce,
		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_Softbody,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_CalcCenter,
		Pipeline_CalcCenter_Post,
		Pipeline_CalcForce,
		Pipeline_Num,
	};


	GI2DSoftbody(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context, const std::shared_ptr<GI2DFluid>& gi2d_fluid)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;
		m_gi2d_fluid = gi2d_fluid;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

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
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

			}

			{
				const char* name[] =
				{
					"Softbody_CalcCenter.comp.spv",
					"Softbody_CalcCenter_Post.comp.spv",
					"Softbody_CalcForce.comp.spv",
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
					m_gi2d_context->getDescriptorSetLayout(),
					m_gi2d_fluid->getDescriptorSetLayout(),
					sSystem::Order().getSystemDescriptorLayout(),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_Softbody] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			}

			// pipeline
			{
				std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
				shader_info[0].setModule(m_shader[Shader_CalcCenter].get());
				shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[0].setPName("main");
				shader_info[1].setModule(m_shader[Shader_CalcCenter_Post].get());
				shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[1].setPName("main");
				shader_info[2].setModule(m_shader[Shader_CalcForce].get());
				shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[2].setPName("main");
				std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
				{
					vk::ComputePipelineCreateInfo()
					.setStage(shader_info[0])
					.setLayout(m_pipeline_layout[PipelineLayout_Softbody].get()),
					vk::ComputePipelineCreateInfo()
					.setStage(shader_info[1])
					.setLayout(m_pipeline_layout[PipelineLayout_Softbody].get()),
					vk::ComputePipelineCreateInfo()
					.setStage(shader_info[2])
					.setLayout(m_pipeline_layout[PipelineLayout_Softbody].get()),
				};
				auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
				m_pipeline[Pipeline_CalcCenter] = std::move(compute_pipeline[0]);
				m_pipeline[Pipeline_CalcCenter_Post] = std::move(compute_pipeline[1]);
				m_pipeline[Pipeline_CalcForce] = std::move(compute_pipeline[2]);
			}

		}

		{
			{
				b_softbody = m_context->m_storage_memory.allocateMemory<Softbody>({ 1,{} });
				b_softbody_ideal_pos = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num,{} });

				{

					std::vector<vec4> pos(Particle_Num);
					std::vector<vec2> rela_pos(Particle_Num);
					vec2 center = vec2(0.f);
#if 1
					for (int y = 0; y < yy; y++)
					{
						for (int x = 0; x < xx; x++)
						{
							pos[x + y * xx].x = 103.f + x;
							pos[x + y * xx].y = 220.f + y;
							pos[x + y * xx].z = pos[x + y * xx].x;
							pos[x + y * xx].w = pos[x + y * xx].y;
						}
					}
#else
					for (auto& p : pos) {
						p = glm::diskRand(12.f) + 150.f;
					}
#endif
					for (auto& p : pos) {
						center += p.xy();
					}
					center /= pos.size();

					vec2 size = vec2(0.f);
					vec2 size_max = vec2(0.f);
					for (int32_t i = 0; i < rela_pos.size(); i++)
					{
						rela_pos[i] = pos[i].xy() - center;
						size += rela_pos[i];
						size_max = glm::max(abs(rela_pos[i]), size_max);
					}

					Softbody sb;
					sb.pnum = Particle_Num;
					sb.particle_offset = 0;
					sb.softbody_offset = 0;
					sb.center = center;
					sb.center_work = uvec2(0);
					cmd.updateBuffer<Softbody>(b_softbody.getInfo().buffer, b_softbody.getInfo().offset, sb);
					cmd.updateBuffer<vec2>(b_softbody_ideal_pos.getInfo().buffer, b_softbody_ideal_pos.getInfo().offset, rela_pos);

					cmd.updateBuffer<vec4>(m_gi2d_fluid->b_pos.getInfo().buffer, m_gi2d_fluid->b_pos.getInfo().offset, pos);

					vk::BufferMemoryBarrier to_read[] = {
						b_softbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
						b_softbody_ideal_pos.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
						m_gi2d_fluid->b_pos.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {},
						0, nullptr, array_length(to_read), to_read, 0, nullptr);
				}

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
					b_softbody.getInfo(),
					b_softbody_ideal_pos.getInfo(),
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

	}
	void execute(vk::CommandBuffer cmd)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Softbody].get(), 0, m_descriptor_set.get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Softbody].get(), 1, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Softbody].get(), 2, m_gi2d_fluid->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Softbody].get(), 3, sSystem::Order().getSystemDescriptorSet(), { 0 * sSystem::Order().getSystemDescriptorStride() });

		{
			// 重心の計算
			vk::BufferMemoryBarrier to_read[] = {
				b_softbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderWrite),
				m_gi2d_fluid->b_pos.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_CalcCenter].get());

			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}
		{
			// 重心の計算2
			vk::BufferMemoryBarrier to_read[] = {
				b_softbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_CalcCenter_Post].get());

//			auto num = app::calcDipatchGroups(uvec3(1, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(1, 1, 1);
		}
		{
			// 所定の位置に移動
			vk::BufferMemoryBarrier to_read[] = {
				b_softbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_CalcForce].get());

			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;
	std::shared_ptr<GI2DFluid> m_gi2d_fluid;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	btr::BufferMemoryEx<Softbody> b_softbody;
	btr::BufferMemoryEx<vec2> b_softbody_ideal_pos;


	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;


};
