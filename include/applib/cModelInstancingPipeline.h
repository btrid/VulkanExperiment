#pragma once

#include <vector>
#include <list>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <btrlib/cMotion.h>
#include <applib/cLightPipeline.h>
#include <applib/cModelPipeline.h>

struct ModelInstancingRender;

struct InstancingAnimationModule : public AnimationModule
{
	virtual vk::DescriptorBufferInfo getAnimationInfoBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getPlayingAnimationBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getNodeBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getBoneMap()const = 0;
	virtual vk::DescriptorBufferInfo getNodeInfoBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getBoneInfoBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getWorldBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getDrawIndirect()const = 0;
	virtual const std::vector<MotionTexture>& getMotionTexture()const = 0;

};
struct InstancingAnimationDescriptorModule : public DescriptorModule
{
	InstancingAnimationDescriptorModule(const std::shared_ptr<btr::Context>& context)
	{
		std::vector<vk::DescriptorSetLayoutBinding> binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(4),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(5),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(6),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(7),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(8),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setBinding(32),
		};
		m_descriptor_set_layout = createDescriptorSetLayout(context, binding);
		m_descriptor_pool = createDescriptorPool(context, binding, 30);
	}

	void updateAnimation(const std::shared_ptr<btr::Context>& context, vk::DescriptorSet descriptor_set, const std::shared_ptr<InstancingAnimationModule>& animation)
	{
		std::vector<vk::DescriptorBufferInfo> storages = {
			animation->getAnimationInfoBuffer(),
			animation->getPlayingAnimationBuffer(),
			animation->getNodeInfoBuffer(),
			animation->getBoneInfoBuffer(),
			animation->getNodeBuffer(),
			animation->getWorldBuffer(),
			animation->getBoneMap(),
			animation->getDrawIndirect(),
		};

		std::vector<vk::DescriptorImageInfo> images =
		{
			vk::DescriptorImageInfo()
			.setImageView(animation->getMotionTexture()[0].getImageView())
			.setSampler(animation->getMotionTexture()[0].getSampler())
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		std::vector<vk::WriteDescriptorSet> write =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount((uint32_t)storages.size())
			.setPBufferInfo(storages.data())
			.setDstBinding(0)
			.setDstSet(descriptor_set),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(images.size())
			.setPImageInfo(images.data())
			.setDstBinding(32)
			.setDstSet(descriptor_set),
		};
		context->m_device->updateDescriptorSets(write, {});
	}
};

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

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	vk::UniquePipeline m_graphics_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	std::shared_ptr<ModelDescriptorModule> m_model_descriptor;
	std::shared_ptr<InstancingAnimationDescriptorModule> m_animation_descriptor;

	std::shared_ptr<cFowardPlusPipeline> m_light_pipeline;
	std::vector<ModelInstancingRender*> m_model;


	void setup(const std::shared_ptr<btr::Context>& context);
	vk::CommandBuffer execute(std::shared_ptr<btr::Context>& context);
	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context);

	void addModel(const std::shared_ptr<btr::Context>& context, ModelInstancingRender* model);

	std::shared_ptr<cFowardPlusPipeline> getLight() { return m_light_pipeline; }

};
