#pragma once
#include <memory>
#include <btrlib/Context.h>
#include <applib/PM2D/PM2D.h>
namespace pm2d
{

struct PM2DDebug : public PM2DPipeline
{
	PM2DDebug(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DContext>& pm2d_context);
	void execute(vk::CommandBuffer cmd) override;

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PM2DContext> m_pm2d_context;

	std::vector<PM2DContext::Emission> m_emission;
	btr::BufferMemoryEx<PM2DContext::Fragment> m_map_data;

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