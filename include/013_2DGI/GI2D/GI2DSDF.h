#pragma once
#include <memory>
#include <btrlib/Context.h>
#include <013_2DGI/GI2D/GI2DContext.h>
namespace gi2d
{


struct GI2DSDF : public GI2DPipeline
{
	GI2DSDF(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context);
	void execute(vk::CommandBuffer cmd) override;

	btr::BufferMemoryEx<uvec4> b_light_count;
	btr::BufferMemoryEx<GI2DLightData> b_light_data;

	enum Shader
	{
		ShaderMakeSDF,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutMakeSDF,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineMakeSDF,
		PipelineNum,
	};
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	std::array<vk::UniqueShaderModule, ShaderNum> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;
};


}