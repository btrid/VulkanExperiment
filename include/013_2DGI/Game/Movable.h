#pragma once

#include <013_2DGI/Game/Game.h>




// struct Movable
// {
// 	Movable(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GameContext>& game_context, uint size)
// 	{
// 		vk::DescriptorSetLayout layouts[] = {
// 			game_context->getDescriptorSetLayout(GameContext::DSL_Movable),
// 		};
// 		vk::DescriptorSetAllocateInfo desc_info;
// 		desc_info.setDescriptorPool(context->m_descriptor_pool.get());
// 		desc_info.setDescriptorSetCount(array_length(layouts));
// 		desc_info.setPSetLayouts(layouts);
// 		m_descriptor_set = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
// 
// 		b_movable = context->m_storage_memory.allocateMemory<cMovable>({ size * 64,{} });
// 
// 		vk::DescriptorBufferInfo storages[] = {
// 			b_movable.getInfo(),
// 		};
// 
// 		vk::WriteDescriptorSet write[] =
// 		{
// 			vk::WriteDescriptorSet()
// 			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
// 			.setDescriptorCount(array_length(storages))
// 			.setPBufferInfo(storages)
// 			.setDstBinding(0)
// 			.setDstSet(m_descriptor_set.get()),
// 		};
// 		context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
// 	}
// 
// 	vk::UniqueDescriptorSet m_descriptor_set;
//};

