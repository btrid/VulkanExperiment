#pragma once
#include <memory>
#include <btrlib/Context.h>
#include <013_2DGI/PM2D/PM2D.h>
namespace pm2d
{


struct PM2DLight : public PM2DPipeline
{
	PM2DLight(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DContext>& pm2d_context);
	void execute(vk::CommandBuffer cmd) override;

//	std::vector<PM2DContext::Emission> m_emission;
	btr::BufferMemoryEx<uvec4> b_light_count;
	btr::BufferMemoryEx<PM2DLightData> b_light_data;

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
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout; //!< copy用データ
	vk::UniqueDescriptorSet m_descriptor_set;

	std::array<vk::UniqueShaderModule, ShaderNum> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PM2DContext> m_pm2d_context;
};


}