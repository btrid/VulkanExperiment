#pragma once

#include <vector>
#include <list>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/BufferMemory.h>
#include <btrlib/Loader.h>

struct cModelInstancingRenderer;

// pipeline�A���C�g�Ȃǂ̊Ǘ�
struct cModelInstancingPipeline
{

	enum {
		SHADER_COMPUTE_CLEAR,
		SHADER_COMPUTE_ANIMATION_UPDATE,
		SHADER_COMPUTE_MOTION_UPDATE,
		SHADER_COMPUTE_NODE_TRANSFORM,
		SHADER_COMPUTE_CULLING,
		SHADER_COMPUTE_BONE_TRANSFORM,

		SHADER_RENDER_VERT,
		SHADER_RENDER_FRAG,

		SHADER_NUM,
	};
	enum DescriptorLayout
	{
		DESCRIPTOR_LAYOUT_MODEL,
		DESCRIPTOR_LAYOUT_ANIMATION,
		DESCRIPTOR_LAYOUT_PER_MESH,
		DESCRIPTOR_LAYOUT_SCENE,
		DESCRIPTOR_LAYOUT_LIGHT,
		DESCRIPTOR_NUM,
	};
	enum PipelineLayout
	{
		PIPELINE_LAYOUT_COMPUTE,
		PIPELINE_LAYOUT_RENDER,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_COMPUTE_CLEAR,
		PIPELINE_COMPUTE_ANIMATION_UPDATE,
		PIPELINE_COMPUTE_MOTION_UPDATE,
		PIPELINE_COMPUTE_NODE_TRANSFORM,
		PIPELINE_COMPUTE_CULLING,
		PIPELINE_COMPUTE_BONE_TRANSFORM,
		PIPELINE_NUM,
	};
	vk::UniqueRenderPass m_render_pass;
	std::vector<vk::UniqueFramebuffer> m_framebuffer;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_list;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	vk::UniqueDescriptorPool m_descriptor_pool;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	vk::UniquePipeline m_graphics_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_NUM> m_descriptor_set_layout;
	btr::UpdateBuffer<CameraGPU2> m_camera;

	vk::UniqueDescriptorSet m_descriptor_set_scene;
	vk::UniqueDescriptorSet m_descriptor_set_light;

	void setup(std::shared_ptr<btr::Loader>& loader, cModelInstancingRenderer& renderer);
};
