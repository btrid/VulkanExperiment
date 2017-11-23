#pragma once

#include <array>
#include <vector>
#include <btrlib/Singleton.h>
#include <btrlib/Context.h>

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include <imgui/imgui.h>

struct sImGuiRenderer : SingletonEx<sImGuiRenderer>
{
	friend SingletonEx<sImGuiRenderer>;

	sImGuiRenderer(const std::shared_ptr<btr::Context>& context);

	vk::CommandBuffer Render();

private:
	enum Shader
	{
		SHADER_VERT_RENDER,
		SHADER_FRAG_RENDER,

		SHADER_NUM,
	};
	enum Pipeline
	{
		PIPELINE_RENDER,
		PIPELINE_NUM,
	};
	enum PipelineLayout
	{
		PIPELINE_LAYOUT_RENDER,
		PIPELINE_LAYOUT_NUM,
	};

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<RenderPassModule> m_render_pass;

	vk::UniqueDescriptorSetLayout	m_descriptor_set_layout;
	vk::UniqueDescriptorSet			m_descriptor_set;

	std::array<vk::UniqueShaderModule, SHADER_NUM>				m_shader_module;
	std::array<vk::UniquePipeline, PIPELINE_NUM>				m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM>	m_pipeline_layout;

	vk::UniqueImage m_image;
	vk::UniqueDeviceMemory m_image_memory;
	vk::UniqueSampler m_sampler;

	std::vector<vk::UniqueImageView> m_image_view;

	btr::BufferMemoryEx<ImDrawVert> m_vertex;
	btr::BufferMemoryEx<ImDrawIdx> m_index;
	uint32_t m_vertex_offset;
	uint32_t m_index_offset;
};
