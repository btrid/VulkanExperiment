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
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(9, vk::DescriptorType::eStorageBuffer, 1, stage),
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

// 				vk::DescriptorBufferInfo uniforms[] = {
// 					u_gi2d_info.getInfo(),
// 					u_gi2d_scene.getInfo(),
// 				};
// 				vk::DescriptorBufferInfo storages[] = {
// 					b_fragment.getInfo(),
// 					b_fragment_map.getInfo(),
// 					b_grid_counter.getInfo(),
// 					b_light.getInfo(),
// 					b_jfa.getInfo(),
// 					b_sdf.getInfo(),
// 					b_diffuse_map.getInfo(),
// 					b_emissive_map.getInfo(),
// 				};
// 
// 				vk::WriteDescriptorSet write[] = {
// 					vk::WriteDescriptorSet()
// 					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
// 					.setDescriptorCount(array_length(uniforms))
// 					.setPBufferInfo(uniforms)
// 					.setDstBinding(0)
// 					.setDstSet(m_descriptor_set.get()),
// 					vk::WriteDescriptorSet()
// 					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
// 					.setDescriptorCount(array_length(storages))
// 					.setPBufferInfo(storages)
// 					.setDstBinding(2)
// 					.setDstSet(m_descriptor_set.get()),
// 				};
// 				context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
			}

		}
	}

	void execute(vk::CommandBuffer cmd)
	{
	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<gi2d::GI2DContext> m_gi2d_context;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }
};
