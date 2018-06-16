#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <013_2DGI/PM2D/PM2D.h>

namespace pm2d
{

struct PM2DPRT
{
	enum Shader
	{
		ShaderMakePRT,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutMakePRT,
//		PipelineLayoutMakeFragmentMapHierarchy,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineMakePRT,
//		PipelineMakeFragmentMapHierarchy,
		PipelineNum,
	};

	PM2DPRT(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DContext>& pm2d_context)
	{
		m_context = context;
		m_pm2d_context = pm2d_context;

		{
			{
				uint32_t map = (m_pm2d_context->RenderWidth*m_pm2d_context->RenderHeight) / 64;
				uint32_t mapp = map/4 * map;
				b_prt_data = m_context->m_storage_memory.allocateMemory<uint32_t>({ mapp, {} });
			}
			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(0),
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
					b_prt_data.getInfo(),
				};
				vk::WriteDescriptorSet write[] = {
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
				"ComputeRadianceTransfer.comp.spv",
			};
			static_assert(array_length(name) == ShaderNum, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				pm2d_context->getDescriptorSetLayout(),
				m_descriptor_set_layout.get(),
			};

			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setOffset(0).setSize(8).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayoutMakePRT] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 1> shader_info;
			shader_info[0].setModule(m_shader[ShaderMakePRT].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayoutMakePRT].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[PipelineMakePRT] = std::move(compute_pipeline[0]);
		}

	}
	void execute(vk::CommandBuffer cmd)
	{
		static int a = 0;
		if (a == 0) 
		{
			// Ž–‘OŒvŽZ
			a++;
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakePRT].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakePRT].get(), 0, m_pm2d_context->getDescriptorSet(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakePRT].get(), 1, m_descriptor_set.get(), {});
			auto num = app::calcDipatchGroups(uvec3(m_pm2d_context->RenderWidth / 32 / 8, m_pm2d_context->RenderHeight / 32 / 8, 1), uvec3(32, 32, 1));

			auto yy = m_pm2d_context->RenderHeight / 8;
			auto xx = m_pm2d_context->RenderWidth / 8;
			for (int y = 0; y < yy; y++)
			{
				for (int x = 0; x < xx; x++)
				{
					cmd.pushConstants<ivec2>(m_pipeline_layout[PipelineLayoutMakePRT].get(), vk::ShaderStageFlagBits::eCompute, 0, ivec2(y*xx + x, 0));
					cmd.dispatch(num.x, num.y, num.z);
				}

			}
		}


	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PM2DContext> m_pm2d_context;

	std::array<vk::UniqueShaderModule, ShaderNum> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

	btr::BufferMemoryEx<uint32_t> b_prt_data;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

};



}