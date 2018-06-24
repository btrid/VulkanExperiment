#pragma once
#include <memory>
#include <btrlib/Context.h>
#include <013_2DGI/GI2D/GI2DContext.h>

namespace gi2d
{

struct GI2DDebug : public GI2DPipeline
{
	GI2DDebug(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context);
	void execute(vk::CommandBuffer cmd) override;

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	std::vector<GI2DLightData> m_emission;
	btr::BufferMemoryEx<GI2DContext::Fragment> m_map_data;

	enum Shader
	{
		ShaderPointLight,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutPointLight,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelinePointLight,
		PipelineNum,
	};
	vk::UniqueShaderModule m_shader[ShaderNum];
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

};


}