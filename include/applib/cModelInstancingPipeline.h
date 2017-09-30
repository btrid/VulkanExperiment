#pragma once

#include <vector>
#include <list>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <applib/cLightPipeline.h>
#include <applib/cModelPipeline.h>

struct ModelInstancingRender;

// pipelineÅAÉâÉCÉgÇ»Ç«ÇÃä«óù
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
	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_MODEL,
		DESCRIPTOR_SET_LAYOUT_ANIMATION,
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

	std::shared_ptr<RenderPassModule> m_render_pass;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_list;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	vk::UniqueDescriptorPool m_descriptor_pool;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	vk::UniquePipeline m_graphics_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_NUM> m_descriptor_set_layout;

	std::shared_ptr<ModelDescriptorModule> m_model_descriptor;

	std::shared_ptr<cFowardPlusPipeline> m_light_pipeline;
	std::vector<ModelInstancingRender*> m_model;


	void setup(const std::shared_ptr<btr::Context>& loader);
	vk::CommandBuffer execute(std::shared_ptr<btr::Context>& executer);
	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& executer);

	void addModel(ModelInstancingRender* model);

	std::shared_ptr<cFowardPlusPipeline> getLight() { return m_light_pipeline; }

};
