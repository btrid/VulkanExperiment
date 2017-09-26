#pragma once

#include <vector>
#include <list>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Loader.h>

struct cModelRender;
struct cModelPipeline
{
	enum {
		DESCRIPTOR_TEXTURE_NUM = 16,
	};
	enum {
		SHADER_RENDER_VERT,
		SHADER_RENDER_FRAG,
		SHADER_NUM,
	};
	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_MODEL,
		DESCRIPTOR_SET_LAYOUT_PER_MESH,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum PipelineLayout
	{
		PIPELINE_LAYOUT_RENDER,
		PIPELINE_LAYOUT_NUM,
	};

	vk::UniqueRenderPass m_render_pass;
	std::vector<vk::UniqueFramebuffer> m_framebuffer;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_list;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	vk::UniqueDescriptorPool m_descriptor_pool;
	vk::UniquePipeline m_graphics_pipeline;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	btr::BufferMemory m_bone_buffer;

	std::vector<std::shared_ptr<cModelRender>> m_model;
	void setup(std::shared_ptr<btr::Loader>& loader);
	void addModel(std::shared_ptr<btr::Executer>& executer, const std::shared_ptr<cModelRender>& model);

	vk::CommandBuffer draw(std::shared_ptr<btr::Executer>& executer);

};
