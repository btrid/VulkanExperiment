#pragma once

#include <vector>
#include <list>
#include <btrlib/Define.h>
#include <btrlib/sVulkan.h>
#include <btrlib/cCamera.h>

template<typename T>
struct cModelRenderer_t
{
	struct Image
	{
		vk::Image image;
		vk::ImageView view;
		vk::Format format;
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
		enum {
			DESCRIPTOR_SET_LAYOUT_PER_MODEL,
			DESCRIPTOR_SET_LAYOUT_PER_MESH,
			DESCRIPTOR_SET_LAYOUT_PER_SCENE,
			DESCRIPTOR_SET_LAYOUT_MAX,
		};
		std::array<vk::DescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_MAX> m_descriptor_set_layout;

		UniformBuffer	m_camera_uniform;
		void setup(vk::RenderPass render_pass);
	};
	struct cModelComputePipeline {

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
		std::vector<vk::PipelineLayout> m_layout;
		std::vector<vk::DescriptorSetLayout> m_descriptor_set_layout;

		void setup();
	};


protected:
	using RenderPtr = std::unique_ptr<T>;
// 	std::list<std::future<std::unique_ptr<T>>> m_loading_model;
// 	std::vector<std::unique_ptr<T>> m_render;
	cGPU	m_gpu;

	cModelDrawPipeline m_draw_pipeline;
	cModelComputePipeline m_compute_pipeline;
public:
	cModelRenderer_t();
	void setup(vk::RenderPass render_pass);
	void execute(cThreadPool& threadpool);

// 	std::future<std::unique_ptr<T>> newModel(cThreadPool& thread_pool)
// 	{
// 		std::promise<std::unique_ptr<T>> task;
// 		std::shared_future<std::unique_ptr<T>> future = task->get_future().share();
// 		m_loading_model.emplace_back(task->get_future().share());
// 		{
// 			cThreadJob job;
// 			auto load = [task = std::move(task)]() mutable {
// 				decltype(auto) model = std::make_unique<T>();
// 				model->load(btr::getResourcePath() + "002_model\\" + "tiny.x");
// 				task->set_value(std::move(model));
// 			};
// 			job.mJob.push_back(load);
// 			thread_pool.enque(job);
// 		}
// 
// 	}
//	void draw();
public:
	vk::CommandPool getCmdPool()const { return m_cmd_pool; }

	cModelDrawPipeline getDrawPipeline() { return m_draw_pipeline; }
	cModelComputePipeline getComputePipeline() { return m_compute_pipeline; }
};


#include "002_model/cModelRenderer.inl"

