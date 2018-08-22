#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>


mat2 crossMatrix2(float a)
{
	return mat2(0.f, a, -a, 0.f);
}

namespace gi2d
{

struct GI2DSoftbody
{
	enum 
	{
		Particle_Num = 1024,
	};
	struct Softbody
	{
		int32_t pnum;
		int32_t particle_offset;
		int32_t softbody_offset;
		int32_t _p;
		vec2 center;
		uvec2 center_work;
	};
	enum Shader
	{
		Shader_CalcForce,
		Shader_Integrate,
		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_Softbody,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_Softbody,
		Pipeline_Num,
	};


	GI2DSoftbody(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;

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
					gi2d_context->getDescriptorSetLayout(),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_Softbody] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			}

			// pipeline
			{
				std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
				shader_info[0].setModule(m_shader[Shader_CalcForce].get());
				shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[0].setPName("main");
				std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
				{
					vk::ComputePipelineCreateInfo()
					.setStage(shader_info[0])
					.setLayout(m_pipeline_layout[PipelineLayout_Softbody].get()),
				};
				auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
				m_pipeline[Pipeline_Softbody] = std::move(compute_pipeline[0]);
			}

		}

		{
			{
				b_softbody = m_context->m_storage_memory.allocateMemory<Softbody>({ 1,{} });
				b_softbody_ideal_pos = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num,{} });

				{

					std::vector<vec2> pos(Particle_Num);
					std::vector<vec2> rela_pos(Particle_Num);
					vec2 center = vec2(0.f);
#if 1

					for (int y = 0; y < 4; y++) 
					{
						for (int x = 0; x < 30; x++)
						{
							pos[x + y * 30].x = 123.f + x;
							pos[x + y * 30].y = 220.f + y;
						}
					}
#else
					for (auto& p : pos) {
						p = glm::diskRand(12.f) + 150.f;
					}
#endif
					for (auto& p : pos) {
						center += p;
					}
					center /= pos.size();

					vec2 size = vec2(0.f);
					vec2 size_max = vec2(0.f);
					for (int32_t i = 0; i < rela_pos.size(); i++)
					{
						rela_pos[i] = pos[i] - center;
						size += rela_pos[i];
						size_max = glm::max(abs(rela_pos[i]), size_max);
					}

					size /= pos.size();
//					cmd.updateBuffer<vec2>(b_rbpos.getInfo().buffer, b_rbpos.getInfo().offset, pos);
//					cmd.updateBuffer<vec2>(b_relative_pos.getInfo().buffer, b_relative_pos.getInfo().offset, rela_pos);

					Softbody sb;
					sb.center = size/2.f;
					sb.pnum = Particle_Num;
					sb.center_work = uvec2(0);
					sb.offset = 0;
					cmd.updateBuffer<Softbody>(b_softbody.getInfo().buffer, b_softbody.getInfo().offset, rb);

					vk::BufferMemoryBarrier to_read[] = {
						b_softbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
						b_softbody_ideal_pos.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
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

		{
			// à íuÇÃçXêV
// 			vk::BufferMemoryBarrier to_read[] = {
// 				b_rbacc.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
// 			};
// 			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
// 				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Softbody].get());

			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}
	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	btr::BufferMemoryEx<Softbody> b_softbody;
	btr::BufferMemoryEx<vec2> b_softbody_ideal_pos;


	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;


};

}