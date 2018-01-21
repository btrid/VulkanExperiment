#pragma once
#include <btrlib/Context.h>

struct Component
{
	virtual ~Component() = default;
};

struct RenderComponent : public Component {};
struct Drawable : public Component {};

struct RenderPassModule
{
	virtual vk::RenderPass getRenderPass()const = 0;
	virtual vk::Framebuffer getFramebuffer(uint32_t index)const = 0;
	virtual vk::Extent2D getResolution()const = 0;
};

struct ShaderDescriptor
{
	std::string filepath;
	vk::ShaderStageFlagBits stage;
};
struct ShaderModule
{
	ShaderModule(const std::shared_ptr<btr::Context>& context, const std::vector<ShaderDescriptor>& desc)
	{
		auto& device = context->m_device;
		// setup shader
		{
			m_shader_list.resize(desc.size());
			m_stage_info.resize(desc.size());
			for (size_t i = 0; i < desc.size(); i++) {
				m_shader_list[i] = std::move(loadShaderUnique(device.getHandle(), desc[i].filepath));
				m_stage_info[i].setModule(m_shader_list[i].get());
				m_stage_info[i].setStage(desc[i].stage);
				m_stage_info[i].setPName("main");
			}
		}
	}
	const std::vector<vk::PipelineShaderStageCreateInfo>& getShaderStageInfo()const { return m_stage_info; }

private:
	std::vector<vk::UniqueShaderModule> m_shader_list;
	std::vector<vk::PipelineShaderStageCreateInfo> m_stage_info;
};

struct PipelineComponent
{
};

struct InstancingModule
{
	virtual vk::DescriptorBufferInfo getModelInfo()const = 0;
	virtual vk::DescriptorBufferInfo getInstancingInfo()const = 0;
};

struct DescriptorModule
{
	virtual vk::DescriptorPool getPool()const = 0;
	virtual vk::DescriptorSetLayout getLayout()const = 0;
//	virtual vk::DescriptorSetLayout getLayout()const = 0;
};

struct DescriptorModuleOld
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


namespace btr
{
struct Descriptor
{
public:
	static vk::UniqueDescriptorSet allocateDescriptorSet(const std::shared_ptr<btr::Context>& context, vk::DescriptorPool pool, vk::DescriptorSetLayout layout)
	{
		auto& device = context->m_device;
		vk::DescriptorSetLayout layouts[] =
		{
			layout
		};
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(pool);
		descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
		descriptor_set_alloc_info.setPSetLayouts(layouts);
		auto descriptor_set = std::move(device->allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);
		return descriptor_set;
	}
	static vk::UniqueDescriptorSetLayout createDescriptorSetLayout(const std::shared_ptr<btr::Context>& context, const std::vector<vk::DescriptorSetLayoutBinding>& binding)
	{
		vk::DescriptorSetLayoutCreateInfo descriptor_layout_info;
		descriptor_layout_info.setBindingCount((uint32_t)binding.size());
		descriptor_layout_info.setPBindings(binding.data());
		return context->m_device->createDescriptorSetLayoutUnique(descriptor_layout_info);
	}
	static vk::UniqueDescriptorPool createDescriptorPool(const std::shared_ptr<btr::Context>& context, const std::vector<vk::DescriptorSetLayoutBinding>& binding, uint32_t set_size)
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

};
}
