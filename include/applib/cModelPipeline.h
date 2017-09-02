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
	std::array<vk::ShaderModule, SHADER_NUM> m_shader_list;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	vk::DescriptorPool m_descriptor_pool;
	vk::PipelineCache m_cache;
	vk::Pipeline m_graphics_pipeline;
	std::array<vk::PipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::DescriptorSetLayout, DESCRIPTOR_NUM> m_descriptor_set_layout;
	btr::UpdateBuffer<CameraGPU2> m_camera;

	vk::DescriptorSet m_descriptor_set_scene;

	std::vector<cModelRender*> m_model;
	void setup(btr::Loader& loader);
	void addModel(cModelRender* model);

	void draw();

};
