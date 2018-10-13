#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <013_2DGI/GI2D/GI2DContext.h>

struct CrowdContext
{

	CrowdContext(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<gi2d::GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
		}

		{
			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);
			}
		}

		{
			b_grid_counter = m_context->m_storage_memory.allocateMemory<uint32_t>({ m_gi2d_context->FragmentBufferSize, {} });
			vk::DescriptorBufferInfo storages[] = {
				b_grid_counter.getInfo(),
			};
			vk::WriteDescriptorSet write[] = {
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(0)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}
	}

	void execute(vk::CommandBuffer cmd)
	{
	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<gi2d::GI2DContext> m_gi2d_context;

	btr::BufferMemoryEx<uint32_t> b_grid_counter;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }
};
