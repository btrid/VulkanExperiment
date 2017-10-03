#pragma once
#include <array>
#include <memory>
#include <mutex>
#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>

struct cModelInstancingRenderer;

struct LightData {
	glm::vec4 m_position;
	glm::vec4 m_emission;
};

struct Light
{
	virtual bool update()
	{
		return true;
	}

	virtual LightData getParam()const
	{
		return LightData();
	}
};
struct LightLL
{
	uint32_t next;
	uint32_t light_index;
};


struct cFowardPlusPipeline
{
	struct LightInfo
	{
		glm::uvec2 m_resolution;
		glm::uvec2 m_tile_size;

		glm::uvec2 m_tile_num;
		uint32_t m_active_light_num;
		uint32_t m_light_max_num;
	};

	enum ShaderModule
	{
		SHADER_COMPUTE_CULL_LIGHT,
		SHADER_NUM,
	};

	enum DescriptorSet
	{
		DESCRIPTOR_SET_LIGHT,
		DESCRIPTOR_SET_NUM,
	};
	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_LIGHT,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum PipelineLayout
	{
		PIPELINE_LAYOUT_CULL_LIGHT,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_CULL_LIGHT,
		PIPELINE_NUM,
	};

	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout index)const { return m_descriptor_set_layout[index].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorSet index)const { return m_descriptor_set[index].get(); }

	LightInfo m_light_info;
	btr::UpdateBuffer<LightInfo> m_light_info_gpu;
	btr::UpdateBuffer<LightData> m_light;
	btr::BufferMemory m_lightLL_head;
	btr::BufferMemory m_lightLL;
	btr::BufferMemory m_light_counter;

	std::vector<std::unique_ptr<Light>> m_light_list;
	std::vector<std::unique_ptr<Light>> m_light_list_new;
	std::mutex m_light_new_mutex;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
	std::array<vk::PipelineShaderStageCreateInfo, 2> m_shader_info_debug;

	void setup(const std::shared_ptr<btr::Context>& context);
	vk::CommandBuffer execute(const std::shared_ptr<btr::Context>& context);

	void add(std::unique_ptr<Light>&& light)
	{
		m_light_list_new.push_back(std::move(light));
	}
	void executeDebug(vk::CommandBuffer cmd)
	{

	}

};

