#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <013_2DGI/GI2D/GI2DContext.h>

namespace gi2d
{

struct GI2DMakeHierarchy
{
	enum Shader
	{
		Shader_MakeFragmentMap,
		Shader_MakeFragmentMapHierarchy,
		Shader_MakeDensityHierarchy,
		Shader_MakeLight,
		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Hierarchy,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_MakeFragmentMap,
		Pipeline_MakeFragmentMapHierarchy,
		Pipeline_MakeDensityHierarchy,
		Pipeline_MakeLight,
		Pipeline_Num,
	};

	GI2DMakeHierarchy(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;

		{
			const char* name[] =
			{
				"MakeFragmentMap.comp.spv",
				"MakeFragmentMapHierarchy.comp.spv",
				"GI2D_MakeDensityHierarchy.comp.spv",
				"GI2D_MakeLight.comp.spv",
			};
//			static_assert(array_length(name) == m_shader.max_size(), "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				gi2d_context->getDescriptorSetLayout()
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);

			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setOffset(0).setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayout_Hierarchy] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, Pipeline_Num> shader_info;
			shader_info[0].setModule(m_shader[Shader_MakeFragmentMap].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[Shader_MakeFragmentMapHierarchy].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[Shader_MakeDensityHierarchy].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			shader_info[3].setModule(m_shader[Pipeline_MakeLight].get());
			shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[3].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Hierarchy].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_Hierarchy].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_Hierarchy].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[3])
				.setLayout(m_pipeline_layout[PipelineLayout_Hierarchy].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[Pipeline_MakeFragmentMap] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_MakeFragmentMapHierarchy] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_MakeDensityHierarchy] = std::move(compute_pipeline[2]);
			m_pipeline[Pipeline_MakeLight] = std::move(compute_pipeline[3]);
		}

	}
	void execute(vk::CommandBuffer cmd)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Hierarchy].get(), 0, m_gi2d_context->getDescriptorSet(), {});
		// make fragment map
		{
			vk::BufferMemoryBarrier to_write[] = {
				m_gi2d_context->b_diffuse_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				m_gi2d_context->b_emissive_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				m_gi2d_context->b_light.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeFragmentMap].get());


			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth, m_gi2d_context->RenderHeight, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		// make map_hierarchy
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeFragmentMapHierarchy].get());
			for (int i = 1; i < GI2DContext::_Hierarchy_Num; i++)
			{
				vk::BufferMemoryBarrier to_write[] = {
					m_gi2d_context->b_diffuse_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_write), to_write, 0, nullptr);

				cmd.pushConstants<int32_t>(m_pipeline_layout[PipelineLayout_Hierarchy].get(), vk::ShaderStageFlagBits::eCompute, 0, i);

				auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth >> i, m_gi2d_context->RenderHeight >> i, 1), uvec3(32, 32, 1));
				cmd.dispatch(num.x, num.y, num.z);

			}
		}
	}
	void executeDensityHierarchy(vk::CommandBuffer cmd)
	{

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeDensityHierarchy].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Hierarchy].get(), 0, m_gi2d_context->getDescriptorSet(), {});

		for (int i = 1; i < GI2DContext::_Hierarchy_Num; i++)
		{
			vk::BufferMemoryBarrier to_write[] = {
				m_gi2d_context->b_grid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.pushConstants<int32_t>(m_pipeline_layout[PipelineLayout_Hierarchy].get(), vk::ShaderStageFlagBits::eCompute, 0, i);
			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth >> i, m_gi2d_context->RenderHeight >> i, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}

	void executeLight(vk::CommandBuffer cmd)
	{

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeLight].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Hierarchy].get(), 0, m_gi2d_context->getDescriptorSet(), {});

		vk::BufferMemoryBarrier to_write[] = {
			m_gi2d_context->b_light.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_write), to_write, 0, nullptr);

		auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth, m_gi2d_context->RenderHeight, 1), uvec3(32, 32, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}


	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

};

}