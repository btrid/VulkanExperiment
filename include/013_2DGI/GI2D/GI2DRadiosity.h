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
		Dir_Num = 31,
		Vertex_Num = Dir_Num*2,
		Bounce_Num = 1,
	};
	enum Shader
	{
		Shader_MakeHitpoint,
		Shader_RayMarch,
		Shader_RayBounce,

		Shader_Render2VS,
		Shader_Render2GS,
		Shader_Render2FS,

		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Radiosity,
		PipelineLayout_Rendering,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_MakeHitpoint,
		Pipeline_RayMarch,
		Pipeline_RayBounce,

		Pipeline_Render,

		Pipeline_Num,
	};

	struct GI2DRadiosityInfo
	{
		uint ray_num_max;
		uint ray_frame_max;
		uint frame_max;
		uint a;
	};

	struct RadiosityVertex
	{
		u16vec4 vertex[Vertex_Num];
		u16vec2 pos;
		u16vec2 _p;
		u16vec4 radiance[2];
		u16vec4 albedo;

	};

	struct VertexCmd
	{
		vk::DrawIndirectCommand cmd;
		uvec4 bounce_cmd;

	};
	GI2DRadiosity(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context, const std::shared_ptr<RenderTarget>& render_target);
	void executeRadiosity(const vk::CommandBuffer& cmd);
	void executeRendering(const vk::CommandBuffer& cmd);

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;
	std::shared_ptr<RenderTarget> m_render_target;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	btr::BufferMemoryEx<GI2DRadiosityInfo> u_radiosity_info;
	btr::BufferMemoryEx<VertexCmd> b_vertex_array_counter;
	btr::BufferMemoryEx<uint> b_vertex_array_index;
	btr::BufferMemoryEx<RadiosityVertex> b_vertex_array;
	btr::BufferMemoryEx<uint64_t> b_edge;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;


};

