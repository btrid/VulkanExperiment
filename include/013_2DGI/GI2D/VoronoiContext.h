#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>

struct GI2DSDF
{
	// https://postd.cc/voronoi-diagrams/

	GI2DSDF(const std::shared_ptr<btr::Context>& context)
	{
		m_context = context;
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
 			b_jfa = context->m_storage_memory.allocateMemory<i16vec2>({ 1024*1024,{} });
		}

		{
			{
//				vk::DescriptorSetLayout layouts[] = {
//					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_SDF),
//				};
// 				vk::DescriptorSetAllocateInfo desc_info;
// 				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
// 				desc_info.setDescriptorSetCount(array_length(layouts));
// 				desc_info.setPSetLayouts(layouts);
// 				m_descriptor_set = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
// 
// 				vk::DescriptorBufferInfo storages[] = {
//  					b_jfa.getInfo(),
// 				};
// 
// 				vk::WriteDescriptorSet write[] = {
// 					vk::WriteDescriptorSet()
// 					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
// 					.setDescriptorCount(array_length(storages))
// 					.setPBufferInfo(storages)
// 					.setDstBinding(0)
// 					.setDstSet(m_descriptor_set.get()),
// 				};
// 				context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
			}
		}
	}
	void execute(vk::CommandBuffer cmd)
	{
	}
	std::shared_ptr<btr::Context> m_context;

 	btr::BufferMemoryEx<i16vec2> b_jfa;

	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
};