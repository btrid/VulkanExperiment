#pragma once
#include <memory>
#include <btrlib/Context.h>
#include <013_2DGI/SV2D/SV2D.h>
namespace sv2d
{

struct SV2DDebug : public SV2DPipeline
{
	SV2DDebug(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<SV2DContext>& pm2d_context);
	void execute(vk::CommandBuffer cmd) override;

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<SV2DContext> m_sv2d_context;

	std::vector<SV2DLightData> m_emission;
	btr::BufferMemoryEx<SV2DContext::Fragment> m_map_data;

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