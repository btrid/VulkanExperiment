#pragma once
#include <memory>
#include <array>
#include <btrlib/Singleton.h>
#include <btrlib/Loader.h>

struct Pipeline
{

};

struct sCollisionSystem : public Singleton<sCollisionSystem>
{
	friend Singleton<sCollisionSystem>;

	enum : uint32_t {
		PIPELINE_COLLISION_TEST,
		PIPELINE_NUM,
	};

	enum : uint32_t {
		SHADER_COMPUTE_COLLISION,
		SHADER_NUM,
	};

	enum : uint32_t
	{
		PIPELINE_LAYOUT_COLLISION,
		PIPELINE_LAYOUT_NUM,
	};
	enum : uint32_t
	{
		DESCRIPTOR_COLLISION_TEST,
		DESCRIPTOR_NUM,
	};

	std::array<vk::DescriptorSetLayout, DESCRIPTOR_NUM> m_descriptor_set_layout;
	std::array<vk::DescriptorSet, DESCRIPTOR_NUM> m_descriptor_set;
	std::array<vk::PipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::Pipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;

	void setup(std::shared_ptr<btr::Loader>& loader);
	void execute(std::shared_ptr<btr::Executer>& executer);

};