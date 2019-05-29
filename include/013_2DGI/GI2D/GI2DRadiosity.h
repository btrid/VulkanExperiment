#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>

struct GI2DRadiosity
{
	enum {
		Frame = 4,
		Ray_Frame_Num = 1024 * 256, // é¿ç€ÇÕÇ‡Ç§è≠Çµè≠Ç»Ç¢
		Ray_All_Num = Ray_Frame_Num * Frame,
		Segment_Num = Ray_Frame_Num * 8,// Ç∆ÇËÇ†Ç¶Ç∏ÇÃíl
		Ray_Group = 1,
		Bounce_Num = 0,
	};
	enum Shader
	{
		Shader_Radiosity,
		Shader_Radiosity_Clear,
		Shader_RenderingVS,
		Shader_RenderingFS,

		Shader_RayGenerate,
		Shader_RaySort,
		Shader_RayMarch,
		Shader_RayMarchSDF,
		Shader_RayMarchSDF2,
		Shader_RayHit,
		Shader_RayBounce,

		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Radiosity,
		PipelineLayout_RadiositySDF,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_Radiosity,
		Pipeline_Radiosity_Clear,
		Pipeline_Output,

		Pipeline_RayGenerate,
		Pipeline_RaySort,
		Pipeline_RayMarch,
		Pipeline_RayMarchSDF,
		Pipeline_RayMarchSDF2,
		Pipeline_RayHit,
		Pipeline_RayBounce,

		Pipeline_Num,
	};

	struct GI2DRadiosityInfo
	{
		uint ray_num_max;
		uint ray_frame_max;
		uint frame_max;
		uint a[2];
	};
	struct D2Ray
	{
		vec2 origin;
		float angle;
		uint32_t march;
	};
	struct D2Segment
	{
		uint ray_index;
		uint begin;
		uint march;
		uint radiance;
	};


	GI2DRadiosity(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context, const std::shared_ptr<RenderTarget>& render_target);
	void executeGenerateRay(const vk::CommandBuffer& cmd);
	void executeRadiosity(const vk::CommandBuffer& cmd);
	void executeRendering(const vk::CommandBuffer& cmd);

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;
	std::shared_ptr<RenderTarget> m_render_target;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	btr::BufferMemoryEx<GI2DRadiosityInfo> u_radiosity_info;
	btr::BufferMemoryEx<uint32_t> b_radiance;
	btr::BufferMemoryEx<D2Ray> b_ray;
	btr::BufferMemoryEx<ivec4> b_ray_counter;
	btr::BufferMemoryEx<D2Segment> b_segment;
	btr::BufferMemoryEx<ivec4> b_segment_counter;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;


};

