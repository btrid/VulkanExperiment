#pragma once
#include <memory>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <013_2DGI/GI2D/GI2DContext.h>

struct GI2DDebug
{
	GI2DDebug(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context);
	void executeUpdateMap(vk::CommandBuffer cmd, const std::vector<uint64_t>& map);
	void executeMakeFragment(vk::CommandBuffer cmd);
	void executeDrawFragmentMap(vk::CommandBuffer cmd, const std::shared_ptr<RenderTarget>& render_target);
	void executeDrawFragment(vk::CommandBuffer cmd, const std::shared_ptr<RenderTarget>& render_target);
	void executeDrawReachMap(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPathContext>& gi2d_path_context, const std::shared_ptr<RenderTarget>& render_target);

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	btr::BufferMemoryEx<GI2DContext::Fragment> m_map_data;

	enum Shader
	{
		Shader_PointLight,
		Shader_DrawFragmentMap,
		Shader_DrawFragment,
		Shader_DrawReachMap,
		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_PointLight,
		PipelineLayout_DrawFragmentMap,
		PipelineLayout_DrawReachMap,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_PointLight,
		Pipeline_DrawFragmentMap,
		Pipeline_DrawFragment,
		Pipeline_DrawReachMap,
		Pipeline_Num,
	};
	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

};
