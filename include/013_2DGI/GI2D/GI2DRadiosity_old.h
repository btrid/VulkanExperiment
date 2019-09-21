#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>

struct GI2DRadiosity_old
{
	enum {
		Frame_Num = 4,
		Dir_Num = 33,
		Bounce_Num = 2,
	};
	enum Shader
	{
		Shader_MakeHitpoint,
		Shader_RayMarch,
		Shader_RayBounce,

		Shader_RadiosityVS,
		Shader_RadiosityGS,
		Shader_RadiosityFS,

		Shader_RenderingVS,
		Shader_RenderingFS,

		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Radiosity,
		PipelineLayout_Radiosity_GP,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_MakeHitpoint,
		Pipeline_RayMarch,
		Pipeline_RayBounce,
		Pipeline_Radiosity,

		Pipeline_Rendering,

		Pipeline_Num,
	};

	struct GI2DRadiosityInfo
	{
		uint ray_num_max;
		uint ray_frame_max;
		uint frame_max;
		uint frame;
	};

	struct VertexInfo
	{
		uvec2 id;
	};
	struct RadiosityVertex
	{
		VertexInfo vertex[Dir_Num];
		u16vec2 pos;
		u16vec2 _p;
		u16vec4 radiance[2];
		u16vec4 albedo;
	};

	struct VertexCounter
	{
		vk::DrawIndirectCommand cmd;
		uvec4 bounce_cmd;

	};
	GI2DRadiosity_old(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context, const std::shared_ptr<RenderTarget>& render_target);
	void executeRadiosity(const vk::CommandBuffer& cmd);
	void executeRendering(const vk::CommandBuffer& cmd);

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;
	std::shared_ptr<RenderTarget> m_render_target;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	btr::BufferMemoryEx<GI2DRadiosityInfo> u_radiosity_info;
	btr::BufferMemoryEx<VertexCounter> b_vertex_counter;
	btr::BufferMemoryEx<uint> b_vertex_index;
	btr::BufferMemoryEx<RadiosityVertex> b_vertex;
	btr::BufferMemoryEx<uint64_t> b_edge;

	GI2DRadiosityInfo m_info;

	vk::ImageCreateInfo m_image_info;
	vk::UniqueImage m_image;
	std::array<vk::UniqueImageView, Frame_Num> m_image_view;
	vk::UniqueImageView m_image_rtv;
	vk::UniqueDeviceMemory m_image_memory;
	vk::UniqueSampler m_image_sampler;
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::UniqueRenderPass m_radiosity_pass;
	vk::UniqueFramebuffer m_radiosity_framebuffer;

	vk::UniqueRenderPass m_rendering_pass;
	vk::UniqueFramebuffer m_rendering_framebuffer;


};

