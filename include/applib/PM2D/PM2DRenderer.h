#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/PM2D/PM2D.h>

namespace pm2d
{

struct PM2DMakeHierarchy
{
	enum Shader
	{
		ShaderMakeFragmentMap,
		ShaderMakeFragmentMapHierarchy,
		ShaderMakeFragmentHierarchy,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutMakeFragmentMap,
		PipelineLayoutMakeFragmentMapHierarchy,
		PipelineLayoutMakeFragmentHierarchy,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineMakeFragmentMap,
		PipelineMakeFragmentMapHierarchy,
		PipelineMakeFragmentHierarchy,
		PipelineNum,
	};

	PM2DMakeHierarchy(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DContext>& pm2d_context)
	{
		m_context = context;
		m_pm2d_context = pm2d_context;

		{
			const char* name[] =
			{
				"MakeFragmentMap.comp.spv",
				"MakeFragmentMapHierarchy.comp.spv",
				"MakeFragmentHierarchy.comp.spv",
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
				pm2d_context->getDescriptorSetLayout()
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
			m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 3> shader_info;
			shader_info[0].setModule(m_shader[ShaderMakeFragmentMap].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[ShaderMakeFragmentMapHierarchy].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[ShaderMakeFragmentHierarchy].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayoutMakeFragmentMap].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayoutMakeFragmentMapHierarchy].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[PipelineMakeFragmentMap] = std::move(compute_pipeline[0]);
			m_pipeline[PipelineMakeFragmentMapHierarchy] = std::move(compute_pipeline[1]);
			m_pipeline[PipelineMakeFragmentHierarchy] = std::move(compute_pipeline[2]);
		}

	}
	void execute(vk::CommandBuffer cmd)
	{
		// make fragment map
		{
			vk::BufferMemoryBarrier to_write[] = {
				m_pm2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				m_pm2d_context->b_fragment_hierarchy.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakeFragmentMap].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakeFragmentMap].get(), 0, m_pm2d_context->getDescriptorSet(), {});

			auto num = app::calcDipatchGroups(uvec3(m_pm2d_context->RenderWidth, m_pm2d_context->RenderHeight, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		// make map_hierarchy
		{
			for (int i = 1; i < PM2DContext::_Hierarchy_Num; i++)
			{
				vk::BufferMemoryBarrier to_write[] = {
					m_pm2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_write), to_write, 0, nullptr);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakeFragmentMapHierarchy].get());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakeFragmentMapHierarchy].get(), 0, m_pm2d_context->getDescriptorSet(), {});
				cmd.pushConstants<int32_t>(m_pipeline_layout[PipelineLayoutMakeFragmentMapHierarchy].get(), vk::ShaderStageFlagBits::eCompute, 0, i);

				auto num = app::calcDipatchGroups(uvec3(m_pm2d_context->RenderWidth >> i, m_pm2d_context->RenderHeight >> i, 1), uvec3(32, 32, 1));
				cmd.dispatch(num.x, num.y, num.z);

			}
		}
		// make hierarchy
		{
			for (int i = 1; i < PM2DContext::_Hierarchy_Num; i++)
			{
				vk::BufferMemoryBarrier to_write[] = {
					m_pm2d_context->b_fragment_hierarchy.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_write), to_write, 0, nullptr);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakeFragmentHierarchy].get());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy].get(), 0, m_pm2d_context->getDescriptorSet(), {});
				cmd.pushConstants<int32_t>(m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy].get(), vk::ShaderStageFlagBits::eCompute, 0, i);

				auto num = app::calcDipatchGroups(uvec3((m_pm2d_context->RenderWidth >> (i - 1)), (m_pm2d_context->RenderHeight >> (i - 1)), 1), uvec3(32, 32, 1));
				cmd.dispatch(num.x, num.y, num.z);

			}
		}
	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PM2DContext> m_pm2d_context;

	std::array<vk::UniqueShaderModule, ShaderNum> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

};


struct PM2DRenderer
{

	enum Shader
	{
		ShaderLightCulling,
		ShaderPhotonMapping,
		ShaderRenderingVS,
		ShaderRenderingFS,
		ShaderDebugRenderFragmentMap,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutLightCulling,
		PipelineLayoutPhotonMapping,
		PipelineLayoutRendering,
		PipelineLayoutDebugRenderFragmentMap,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineLightCulling,
		PipelinePhotonMapping,
		PipelineRendering,
		PipelineDebugRenderFragmentMap,
		PipelineNum,
	};

	struct TextureResource
	{
		vk::ImageCreateInfo m_image_info;
		vk::UniqueImage m_image;
		vk::UniqueImageView m_image_view;
		vk::UniqueDeviceMemory m_memory;
		vk::UniqueSampler m_sampler;

		vk::ImageSubresourceRange m_subresource_range;
	};

	PM2DRenderer(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target, const std::shared_ptr<PM2DContext>& pm2d_context);
	void execute(vk::CommandBuffer cmd);

	std::array < TextureResource, PM2DContext::_BounceNum > m_color_tex;


	std::shared_ptr<RenderTarget> m_render_target;
	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::UniqueShaderModule m_shader[ShaderNum];
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PM2DContext> m_pm2d_context;
private:
	void DebugRnederFragmentMap(vk::CommandBuffer &cmd);
};

}