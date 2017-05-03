#pragma once

#include <vector>
#include <list>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/BufferMemory.h>


struct cModelRenderer_t
{
	struct iPipeline
	{
		virtual void setup(cModelRenderer_t& renderer);
	};
	struct cModelDrawPipeline
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
		void setup(cModelRenderer_t& renderer);
	};
	struct cModelComputePipeline
	{

		enum {
			SHADER_COMPUTE_CLEAR,
			SHADER_COMPUTE_ANIMATION_UPDATE,
			SHADER_COMPUTE_MOTION_UPDATE,
			SHADER_COMPUTE_NODE_TRANSFORM,
			SHADER_COMPUTE_CULLING,
			SHADER_COMPUTE_BONE_TRANSFORM,
			SHADER_NUM,
		};
		std::array<vk::ShaderModule, SHADER_NUM> m_shader_list;
		std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

		vk::DescriptorPool m_descriptor_pool;
		vk::PipelineCache m_cache;
		std::vector<vk::Pipeline> m_pipeline;
		std::vector<vk::PipelineLayout> m_pipeline_layout;
		std::vector<vk::DescriptorSetLayout> m_descriptor_set_layout;

		btr::UpdateBuffer<std::array<Plane, 6>>	m_camera_frustom;

		void setup(cModelRenderer_t& renderer);
	};

protected:
	cModelDrawPipeline m_draw_pipeline;
	cModelComputePipeline m_compute_pipeline;
public:
	cModelRenderer_t();
	void setup(vk::RenderPass render_pass);
public:
	btr::BufferMemory m_storage_memory;
	btr::BufferMemory m_uniform_memory;
	btr::BufferMemory m_staging_memory;
	vk::RenderPass m_render_pass;
public:

	cModelDrawPipeline& getDrawPipeline() { return m_draw_pipeline; }
	cModelComputePipeline& getComputePipeline() { return m_compute_pipeline; }
};



