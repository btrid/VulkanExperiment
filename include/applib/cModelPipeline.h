#pragma once

#include <vector>
#include <list>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/BufferMemory.h>
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
	enum DescriptorLayout
	{
		DESCRIPTOR_MODEL,
		DESCRIPTOR_PER_MESH,
		DESCRIPTOR_SCENE,
		DESCRIPTOR_NUM,
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
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_NUM> m_descriptor_set_layout;

	vk::UniqueDescriptorSet m_descriptor_set_scene;
	btr::AllocatedMemory m_bone_buffer;

	std::vector<cModelRender*> m_model;
	void setup(std::shared_ptr<btr::Loader>& loader);
	void addModel(cModelRender* model);

	vk::CommandBuffer draw(std::shared_ptr<btr::Executer>& executer);

};
