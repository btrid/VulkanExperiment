#pragma once

#include <array>
#include <vector>
#include <functional>
#include <btrlib/Singleton.h>
#include <btrlib/Context.h>

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include <imgui/imgui.h>

struct sImGuiRenderer : SingletonEx<sImGuiRenderer>
{
	friend SingletonEx<sImGuiRenderer>;

	sImGuiRenderer(const std::shared_ptr<btr::Context>& context);

	vk::CommandBuffer Render();

public:
	enum Shader
	{
		SHADER_VERT_RENDER,
		SHADER_FRAG_RENDER,

		SHADER_NUM,
	};
	enum PipelineLayout
	{
		PIPELINE_LAYOUT_RENDER,
		PIPELINE_LAYOUT_NUM,
	};

private:

	std::shared_ptr<btr::Context> m_context;

	vk::UniqueDescriptorSetLayout	m_descriptor_set_layout;
	vk::UniqueDescriptorSet			m_descriptor_set;

	std::array<vk::UniqueShaderModule, SHADER_NUM>				m_shader_module;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM>	m_pipeline_layout;

	vk::UniqueImage m_image;
	vk::UniqueDeviceMemory m_image_memory;
	vk::UniqueSampler m_sampler;
	std::vector<vk::UniqueImageView> m_image_view;	

public:
	vk::ShaderModule getShaderModle(Shader type) { return m_shader_module[type].get(); }
	vk::PipelineLayout getPipelineLayout(PipelineLayout type) { return m_pipeline_layout[type].get(); }
};
