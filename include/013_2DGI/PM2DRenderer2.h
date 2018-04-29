#pragma once
#include <array>
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/PM2D.h>

namespace pm2d_2
{

struct PM2DRenderer
{
	enum
	{
		BounceNum = 4, //!< ƒŒƒC”½ŽË‰ñ”
		// 		RenderWidth = 1024,
		// 		RenderHeight = 1024,
		//		RenderDepth = 1,
		//		FragmentBufferSize = RenderWidth * RenderHeight*RenderDepth,
	};
	int RenderWidth;
	int RenderHeight;
	int RenderDepth;
	int FragmentBufferSize;

	enum Shader
	{
		//		ShaderClear,
		ShaderMakeFragmentMap,
		ShaderMakeFragmentHierarchy,
		ShaderLightCulling,
		ShaderPhotonMapping,
		ShaderRenderingVS,
		ShaderRenderingFS,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutMakeFragmentMap,
		PipelineLayoutMakeFragmentHierarchy,
		PipelineLayoutLightCulling,
		PipelineLayoutPhotonMapping,
		PipelineLayoutRendering,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineMakeFragmentMap,
		PipelineMakeFragmentHierarchy,
		PipelineLightCulling,
		PipelinePhotonMapping,
		PipelineRendering,
		PipelineNum,
	};
	struct Info
	{
		ivec2 m_resolution;
		uvec2 _p;
		int m_fragment_hierarchy_offset[8];
		int m_fragment_map_hierarchy_offset[8];
		int m_emission_buffer_size[BounceNum];
		int m_emission_buffer_offset[BounceNum];
	};
	struct Fragment
	{
		vec3 albedo;
		float _p;
	};
	struct Emission
	{
		vec4 value;
	};

	PM2DRenderer(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target);
	vk::CommandBuffer execute(const std::vector<PM2DPipeline*>& pipeline);

	btr::BufferMemoryEx<Info> m_fragment_info;
	btr::BufferMemoryEx<Fragment> m_fragment_buffer;
	btr::BufferMemoryEx<int64_t> m_fragment_map;
	btr::BufferMemoryEx<int32_t> m_fragment_hierarchy;
	btr::BufferMemoryEx<ivec4> m_emission_counter;
	btr::BufferMemoryEx<Emission> m_emission_buffer;
	btr::BufferMemoryEx<int32_t> m_emission_list;
	btr::BufferMemoryEx<int32_t> m_emission_map;	//!< ==-1 emitter‚ª‚È‚¢ !=0‚ ‚é
	btr::BufferMemoryEx<vec4> m_color;

	struct TextureResource
	{
		vk::ImageCreateInfo m_image_info;
		vk::UniqueImage m_image;
		vk::UniqueImageView m_image_view;
		vk::UniqueDeviceMemory m_memory;
		vk::UniqueSampler m_sampler;

		vk::ImageSubresourceRange m_subresource_range;

	};
	std::array<TextureResource, BounceNum> m_color_tex;
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	std::shared_ptr<RenderTarget> m_render_target;
	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

	vk::UniqueShaderModule m_shader[ShaderNum];
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

	std::shared_ptr<btr::Context> m_context;
};


struct DebugPM2D : public PM2DPipeline
{
	DebugPM2D(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DRenderer>& renderer);
	void execute(vk::CommandBuffer cmd)override;

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PM2DRenderer> m_renderer;

	btr::BufferMemoryEx<PM2DRenderer::Fragment> m_map_data;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

	enum Shader
	{
		ShaderPointLightVS,
		ShaderPointLightFS,
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
