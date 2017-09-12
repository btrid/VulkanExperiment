#include <btrlib/cCmdPool.h>
#include <btrlib/Loader.h>

void* VKAPI_PTR Allocation(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	return malloc(size);
}

void* VKAPI_PTR Reallocation(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	return realloc(pOriginal, size);

}

void VKAPI_PTR Free(void* pUserData, void* pMemory)
{
	free(pMemory);
}

void VKAPI_PTR InternalAllocationNotification(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
	printf("alloc\n");
}

void VKAPI_PTR InternalFreeNotification(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
	printf("free\n");
}

std::shared_ptr<cCmdPool> cCmdPool::MakeCmdPool(cGPU& gpu)
{
	auto& device = gpu.getDevice();

	vk::AllocationCallbacks cb;
	cb.setPfnAllocation(Allocation);
	cb.setPfnFree(Free);
	cb.setPfnReallocation(Reallocation);
	cb.setPfnInternalAllocation(InternalAllocationNotification);
	cb.setPfnInternalFree(InternalFreeNotification);

	auto cmd_pool = std::make_shared<cCmdPool>();

	cmd_pool->m_per_thread.resize(std::thread::hardware_concurrency());
	for (auto& per_thread : cmd_pool->m_per_thread)
	{
		per_thread.m_per_family.resize(device.getQueueFamilyIndex().size());
		for (size_t family = 0; family < per_thread.m_per_family.size(); family++)
		{
			auto& per_family = per_thread.m_per_family[family];
			vk::CommandPoolCreateInfo cmd_pool_onetime;
			cmd_pool_onetime.queueFamilyIndex = (uint32_t)family;
			cmd_pool_onetime.flags = vk::CommandPoolCreateFlagBits::eTransient;
			for (auto& pool_per_frame : per_family.m_cmd_pool_onetime)
			{
				pool_per_frame = device->createCommandPoolUnique(cmd_pool_onetime, cb);
			}

			vk::CommandPoolCreateInfo cmd_pool_compiled;
			cmd_pool_compiled.queueFamilyIndex = (uint32_t)family;
			cmd_pool_compiled.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
			per_family.m_cmd_pool_compiled = device->createCommandPoolUnique(cmd_pool_onetime, cb);

			vk::CommandPoolCreateInfo cmd_pool_temporary;
			cmd_pool_temporary.queueFamilyIndex = (uint32_t)family;
			cmd_pool_temporary.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient;
			per_family.m_cmd_pool_temporary = device->createCommandPoolUnique(cmd_pool_onetime, cb);
		}
	}

	return cmd_pool;
}

vk::CommandPool cCmdPool::getCmdPool(cCmdPool::CmdPoolType type, int device_family_index) const
{
	auto& pool_per_family = m_per_thread[sThreadLocal::Order().getThreadIndex()].m_per_family[device_family_index];
	switch (type)
	{
	case sGlobal::CMD_POOL_TYPE_ONETIME:
		return pool_per_family.m_cmd_pool_onetime[sGlobal::Order().getCurrentFrame()].get();
	case sGlobal::CMD_POOL_TYPE_TEMPORARY:
		return pool_per_family.m_cmd_pool_temporary.get();
	case sGlobal::CMD_POOL_TYPE_COMPILED:
	default:
		return pool_per_family.m_cmd_pool_compiled.get();
	}
}

void cCmdPool::resetPool(std::shared_ptr<btr::Executer>& executer)
{
	for (auto& tls : m_per_thread)
	{
		for (auto& pool_family : tls.m_per_family)
		{
			executer->m_gpu.getDevice()->resetCommandPool(pool_family.m_cmd_pool_onetime[executer->getGPUFrame()].get(), vk::CommandPoolResetFlagBits::eReleaseResources);
			pool_family.m_cmd_onetime_deleter[executer->getGPUFrame()].clear();
		}
	}
}

void cCmdPool::enque(vk::UniqueCommandBuffer&& cmd, uint32_t family_index)
{
	auto& pool_per_family = m_per_thread[sThreadLocal::Order().getThreadIndex()].m_per_family[family_index];
	pool_per_family.m_cmd_queue.push_back(std::move(cmd));

}

vk::CommandBuffer cCmdPool::getCmdOnetime(int device_family_index)
{
	vk::CommandBufferAllocateInfo cmd_buffer_info;
	cmd_buffer_info.commandBufferCount = 1;
	cmd_buffer_info.commandPool = getCmdPool(CMD_POOL_TYPE_ONETIME, device_family_index);
	cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
	auto cmd_unique = std::move(sGlobal::Order().getGPU(0).getDevice()->allocateCommandBuffersUnique(cmd_buffer_info)[0]);
	auto cmd = cmd_unique.get();
	m_per_thread[sThreadLocal::Order().getThreadIndex()].m_per_family[device_family_index].m_cmd_onetime_deleter[sGlobal::Order().getGPUIndex()].push_back(std::move(cmd_unique));

	vk::CommandBufferBeginInfo begin_info;
	begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	cmd.begin(begin_info);

	return cmd;
}
