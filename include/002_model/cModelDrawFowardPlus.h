#pragma once
#include <002_model/cModelRenderer.h>
struct cModelDrawFowardPlusPipeline
{
	enum
	{
		SHADER_RENDER_VERT,
		SHADER_RENDER_FRAG,
		SHADER_NUM,
	};
	std::array<vk::ShaderModule, SHADER_NUM> m_shader_list;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	vk::Pipeline m_pipeline;
	vk::PipelineLayout m_pipeline_layout;
	vk::DescriptorPool m_descriptor_pool;
	vk::DescriptorSet m_draw_descriptor_set_per_scene;
	enum
	{
		DESCRIPTOR_SET_LAYOUT_PER_MODEL,
		DESCRIPTOR_SET_LAYOUT_PER_MESH,
		DESCRIPTOR_SET_LAYOUT_PER_SCENE,
		DESCRIPTOR_SET_LAYOUT_MAX,
	};
	std::array<vk::DescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_MAX> m_descriptor_set_layout;

	btr::UpdateBuffer<CameraGPU> m_camera;
	void setup(cModelRenderer& renderer);
};
