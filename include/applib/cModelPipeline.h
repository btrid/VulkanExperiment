#pragma once

#include <vector>
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <btrlib/cModel.h>
#include <btrlib/Material.h>
#include <applib/DrawHelper.h>

struct DescriptorModule
{
public:
	vk::UniqueDescriptorSet allocateDescriptorSet(const std::shared_ptr<btr::Context>& context)
	{
		auto& device = context->m_device;
		vk::DescriptorSetLayout layouts[] =
		{
			m_descriptor_set_layout.get()
		};
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(getPool());
		descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
		descriptor_set_alloc_info.setPSetLayouts(layouts);
		auto descriptor_set = std::move(device->allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);
		return descriptor_set;
	}
protected:
	vk::UniqueDescriptorSetLayout createDescriptorSetLayout(const std::shared_ptr<btr::Context>& context, const std::vector<vk::DescriptorSetLayoutBinding>& binding)
	{
		vk::DescriptorSetLayoutCreateInfo descriptor_layout_info;
		descriptor_layout_info.setBindingCount((uint32_t)binding.size());
		descriptor_layout_info.setPBindings(binding.data());
		return context->m_device->createDescriptorSetLayoutUnique(descriptor_layout_info);
	}
	vk::UniqueDescriptorPool createDescriptorPool(const std::shared_ptr<btr::Context>& context, const std::vector<vk::DescriptorSetLayoutBinding>& binding, uint32_t set_size)
	{
		auto& device = context->m_device;
		std::vector<vk::DescriptorPoolSize> pool_size(VK_DESCRIPTOR_TYPE_RANGE_SIZE);
		for (auto& b : binding)
		{
			pool_size[(uint32_t)b.descriptorType].setType(b.descriptorType);
			pool_size[(uint32_t)b.descriptorType].descriptorCount += b.descriptorCount*set_size;
		}
		pool_size.erase(std::remove_if(pool_size.begin(), pool_size.end(), [](auto& p) {return p.descriptorCount == 0; }), pool_size.end());
		vk::DescriptorPoolCreateInfo pool_info;
		pool_info.setPoolSizeCount((uint32_t)pool_size.size());
		pool_info.setPPoolSizes(pool_size.data());
		pool_info.setMaxSets(set_size);
		pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
		return device->createDescriptorPoolUnique(pool_info);
	}

protected:
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorPool m_descriptor_pool;
public:
	vk::DescriptorSetLayout getLayout()const { return m_descriptor_set_layout.get(); }
protected:
	vk::DescriptorPool getPool()const { return m_descriptor_pool.get(); }

};

struct ModelDescriptorModule : public DescriptorModule
{
	ModelDescriptorModule(const std::shared_ptr<btr::Context>& context)
	{
		std::vector<vk::DescriptorSetLayoutBinding> binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(4),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorCount(DESCRIPTOR_TEXTURE_NUM)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setBinding(5),
		};
		m_descriptor_set_layout = createDescriptorSetLayout(context, binding);
		m_descriptor_pool = createDescriptorPool(context, binding, 30);
	}

	void updateMaterial(const std::shared_ptr<btr::Context>& context, vk::DescriptorSet descriptor_set, const std::shared_ptr<MaterialModule>& material)
	{
		std::vector<vk::DescriptorBufferInfo> storages = {
			material->getMaterialIndexBuffer(),
			material->getMaterialBuffer(),
		};

		std::vector<vk::DescriptorImageInfo> color_images(DESCRIPTOR_TEXTURE_NUM, vk::DescriptorImageInfo(DrawHelper::Order().getWhiteTexture().m_sampler.get(), DrawHelper::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
		for (size_t i = 0; i < material->getTextureList().size(); i++)
		{
			const auto& tex = material->getTextureList()[i];
			if (tex.isReady()) {
				color_images[i] = vk::DescriptorImageInfo(tex.getSampler(), tex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
			}
		}
		std::vector<vk::WriteDescriptorSet> write =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount((uint32_t)storages.size())
			.setPBufferInfo(storages.data())
			.setDstBinding(3)
			.setDstSet(descriptor_set),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount((uint32_t)color_images.size())
			.setPImageInfo(color_images.data())
			.setDstBinding(5)
			.setDstSet(descriptor_set),
		};
		context->m_device->updateDescriptorSets(write, {});
	}
	void updateAnimation(const std::shared_ptr<btr::Context>& context, vk::DescriptorSet descriptor_set, const std::shared_ptr<AnimationModule>& animation)
	{
		std::vector<vk::DescriptorBufferInfo> storages = {
			animation->getBoneBuffer(),
		};
		std::vector<vk::WriteDescriptorSet> drawWriteDescriptorSets =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount((uint32_t)storages.size())
			.setPBufferInfo(storages.data())
			.setDstBinding(2)
			.setDstSet(descriptor_set),
		};
		context->m_device->updateDescriptorSets(drawWriteDescriptorSets, {});
	}
	void updateInstancing(const std::shared_ptr<btr::Context>& context, vk::DescriptorSet descriptor_set, const std::shared_ptr<InstancingModule>& instancing)
	{
		std::vector<vk::DescriptorBufferInfo> storages = {
			instancing->getModelInfo(),
			instancing->getInstancingInfo(),
		};
		std::vector<vk::WriteDescriptorSet> write =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount((uint32_t)storages.size())
			.setPBufferInfo(storages.data())
			.setDstBinding(0)
			.setDstSet(descriptor_set),
		};
		context->m_device->updateDescriptorSets(write, {});
	}

private:
	enum {
		DESCRIPTOR_TEXTURE_NUM = 16,
	};
};


struct ModelRender : public RenderComponent
{
	std::vector<vk::UniqueCommandBuffer> m_draw_cmd;
	vk::UniqueDescriptorSet m_descriptor_set_model;

	void draw(std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd)
	{
		cmd.executeCommands(m_draw_cmd[context->getGPUFrame()].get());
	}

};

struct Model : public Drawable
{
	std::shared_ptr<cModel::Resource> m_model_resource;
	std::shared_ptr<MaterialModule> m_material;
	std::shared_ptr<AnimationModule> m_animation;
	
	std::shared_ptr<ModelRender> m_render;
};
struct ModelDrawPipelineComponent : public PipelineComponent
{
	virtual std::shared_ptr<ModelRender> createRender(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<Model>& model) = 0;
	virtual const std::shared_ptr<RenderPassModule>& getRenderPassModule()const = 0;
};


struct cModelPipeline
{
	std::shared_ptr<ModelDrawPipelineComponent> m_pipeline;

	std::vector<std::shared_ptr<Model>> m_model;
	std::shared_ptr<Model> createRender(std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource);

	void setup(std::shared_ptr<btr::Context>& context, const std::shared_ptr<ModelDrawPipelineComponent>& pipeline = nullptr);
	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context);

};

