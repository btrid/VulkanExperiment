#pragma once

#include <applib/PM2D/PM2D.h>

namespace pm2d
{

struct AppModelPM2D : public PM2DPipeline
{
	enum Shader
	{
		ShaderRenderingVS,
		ShaderRenderingFS,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutRendering,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineRendering,
		PipelineNum,
	};

	AppModelPM2D(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DContext>& pm2d_context)
	{
		{
			const char* name[] =
			{
				"OITAppModel.vert.spv",
				"OITAppModel.frag.spv",
			};
			static_assert(array_length(name) == array_length(m_shader), "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}
		}

		{

		}
	}
	void execute(vk::CommandBuffer cmd)override
	{

	}
	std::array<vk::UniqueShaderModule, ShaderNum> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;
};

}