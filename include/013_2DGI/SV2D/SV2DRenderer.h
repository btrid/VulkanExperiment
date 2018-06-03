#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <013_2DGI/SV2D/SV2D.h>

namespace sv2d
{

struct SV2DRenderer
{
	struct TextureResource
	{
		vk::ImageCreateInfo m_image_info;
		vk::UniqueImage m_image;
		vk::UniqueImageView m_image_view;
		vk::UniqueDeviceMemory m_memory;
		vk::UniqueSampler m_sampler;

		vk::ImageSubresourceRange m_subresource_range;
	};

	enum Shader
	{
		ShaderLightCulling,
		ShaderMakeShadowVolume,
		ShaderMakeLightmapVS,
		ShaderMakeLightmapFS,
		ShaderDrawShadowVolumeVS,
		ShaderDrawShadowVolumeFS,
		ShaderBlitShadowVolumeVS,
		ShaderBlitShadowVolumeFS,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutLightCulling,
		PipelineLayoutMakeShadowVolume,
		PipelineLayoutMakeLightmap,
		PipelineLayoutDrawShadowVolume,
		PipelineLayoutBlitShadowVolume,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineLightCulling,
		PipelineMakeShadowVolume,
		PipelineMakeLightmap,
		PipelineDrawShadowVolume,
		PipelineBlitShadowVolume,
		PipelineNum,
	};

	enum {
		LightNum = 64,
	};
	SV2DRenderer(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target, const std::shared_ptr<SV2DContext>& pm2d_context);
	void execute(vk::CommandBuffer cmd);


	std::shared_ptr<RenderTarget> m_render_target;
	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

	TextureResource m_light_map;
	vk::UniqueRenderPass m_lightmap_render_pass;
	vk::UniqueFramebuffer m_lightmap_framebuffer;

	TextureResource m_output;
	vk::UniqueRenderPass m_output_render_pass;
	vk::UniqueFramebuffer m_output_framebuffer;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	std::array<vk::UniqueShaderModule, ShaderNum> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<SV2DContext> m_sv2d_context;
};

}