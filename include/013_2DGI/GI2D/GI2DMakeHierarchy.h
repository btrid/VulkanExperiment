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
		ShaderMakeFragmentMap,
		ShaderMakeFragmentMapHierarchy,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutMakeFragmentMap,
		PipelineLayoutMakeFragmentMapHierarchy,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineMakeFragmentMap,
		PipelineMakeFragmentMapHierarchy,
		PipelineNum,
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
			m_pipeline_layout[PipelineLayoutMakeFragmentMap] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setOffset(0).setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayoutMakeFragmentMapHierarchy] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 2> shader_info;
			shader_info[0].setModule(m_shader[ShaderMakeFragmentMap].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[ShaderMakeFragmentMapHierarchy].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayoutMakeFragmentMap].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayoutMakeFragmentMapHierarchy].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[PipelineMakeFragmentMap] = std::move(compute_pipeline[0]);
			m_pipeline[PipelineMakeFragmentMapHierarchy] = std::move(compute_pipeline[1]);
		}

	}
	void execute(vk::CommandBuffer cmd)
	{
		// make fragment map
		{
			vk::BufferMemoryBarrier to_write[] = {
				m_gi2d_context->b_diffuse_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				m_gi2d_context->b_emissive_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakeFragmentMap].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakeFragmentMap].get(), 0, m_gi2d_context->getDescriptorSet(), {});

			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth, m_gi2d_context->RenderHeight, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		// make map_hierarchy
		{
			for (int i = 1; i < GI2DContext::_Hierarchy_Num; i++)
			{
				vk::BufferMemoryBarrier to_write[] = {
					m_gi2d_context->b_diffuse_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_write), to_write, 0, nullptr);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakeFragmentMapHierarchy].get());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakeFragmentMapHierarchy].get(), 0, m_gi2d_context->getDescriptorSet(), {});
				cmd.pushConstants<int32_t>(m_pipeline_layout[PipelineLayoutMakeFragmentMapHierarchy].get(), vk::ShaderStageFlagBits::eCompute, 0, i);

				auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth >> i, m_gi2d_context->RenderHeight >> i, 1), uvec3(32, 32, 1));
				cmd.dispatch(num.x, num.y, num.z);

			}
		}
	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	std::array<vk::UniqueShaderModule, ShaderNum> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

};

}