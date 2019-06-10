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
		Frame = 1,
		Ray_Direction_Num = 85,
		Ray_Frame_Num = 1024 * Ray_Direction_Num*2,
		Ray_All_Num = Ray_Frame_Num * Frame,
		Segment_Num = Ray_Frame_Num * 8,// ‚Æ‚è‚ ‚¦‚¸‚Ì’l
		Bounce_Num = 0,
	};
	enum Shader
	{
		Shader_Radiosity,
		Shader_RenderingVS,
		Shader_RenderingFS,

		Shader_RayGenerate,
		Shader_RayGenerate2,
		Shader_MakeHitpoint,
		Shader_RayMarch,
		Shader_SortVertex,
		Shader_RayHit,
		Shader_RayBounce,

		Shader_RadiateVS,
		Shader_RadiateGS,
		Shader_RadiateFS,

		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Radiosity,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_Radiosity,
		Pipeline_Radiosity2,
		Pipeline_Output,

		Pipeline_RayGenerate,
		Pipeline_RayGenerate2,
		Pipeline_MakeHitpoint,
		Pipeline_RayMarch,
		Pipeline_SortVertex,
		Pipeline_RayHit,
		Pipeline_RayBounce,

		Pipeline_Num,
	};

	struct GI2DRadiosityInfo
	{
		uint ray_num_max;
		uint ray_frame_max;
		uint frame_max;
		uint a;
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
	btr::BufferMemoryEx<D2Segment> b_segment;
	btr::BufferMemoryEx<ivec4> b_segment_counter;
	btr::BufferMemoryEx<u16vec4> b_segment_ex;
	btr::BufferMemoryEx<vk::DrawIndirectCommand> b_vertex_array_counter;
	btr::BufferMemoryEx<uint> b_vertex_array_index;
	btr::BufferMemoryEx<u16vec2> b_vertex_array;
	
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;


};

