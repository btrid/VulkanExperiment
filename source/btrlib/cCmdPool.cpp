#include <btrlib/cCmdPool.h>
#include <btrlib/Context.h>

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

cCmdPool::cCmdPool(const std::shared_ptr<btr::Context>& context)
{
	m_context = context;
	vk::AllocationCallbacks cb;
	cb.setPfnAllocation(Allocation);
	cb.setPfnFree(Free);
	cb.setPfnReallocation(Reallocation);
	cb.setPfnInternalAllocation(InternalAllocationNotification);
	cb.setPfnInternalFree(InternalFreeNotification);

	m_per_thread.resize(std::thread::hardware_concurrency());
	for (auto& per_thread : m_per_thread)
	{
		per_thread.m_per_family.resize(context->m_device.getQueueFamilyIndex().size());
		for (size_t family = 0; family < per_thread.m_per_family.size(); family++)
		{
			auto& per_family = per_thread.m_per_family[family];
			vk::CommandPoolCreateInfo cmd_pool_onetime;
			cmd_pool_onetime.queueFamilyIndex = (uint32_t)family;
			cmd_pool_onetime.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
			for (auto& pool_per_frame : per_family.m_cmd_pool_onetime)
			{
				pool_per_frame = context->m_device->createCommandPoolUnique(cmd_pool_onetime, cb);
			}

			vk::CommandPoolCreateInfo cmd_pool_compiled;
			cmd_pool_compiled.queueFamilyIndex = (uint32_t)family;
			cmd_pool_compiled.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
			per_family.m_cmd_pool_compiled = context->m_device->createCommandPoolUnique(cmd_pool_onetime, cb);
		}
	}

	for (auto& c : m_cmds)
	{
		c.resize(std::thread::hardware_concurrency());
	}
	
}



vk::CommandPool cCmdPool::getCmdPool(cCmdPool::CmdPoolType type, int device_family_index) const
{
	auto& pool_per_family = m_per_thread[sThreadLocal::Order().getThreadIndex()].m_per_family[device_family_index];
	switch (type)
	{
	case CMD_POOL_TYPE_ONETIME:
		return pool_per_family.m_cmd_pool_onetime[sGlobal::Order().getCurrentFrame()].get();
	case CMD_POOL_TYPE_COMPILED:
	default:
		return pool_per_family.m_cmd_pool_compiled.get();
	}
}

void cCmdPool::resetPool()
{
	for (auto& tls : m_per_thread)
	{
		for (auto& pool_family : tls.m_per_family)
		{
			if (!pool_family.m_cmd_onetime_deleter[m_context->getGPUFrame()].empty())
			{
				m_context->m_gpu.getDevice()->resetCommandPool(pool_family.m_cmd_pool_onetime[m_context->getGPUFrame()].get(), vk::CommandPoolResetFlagBits::eReleaseResources);
				pool_family.m_cmd_onetime_deleter[m_context->getGPUFrame()].clear();
			}
		}
	}
}

std::vector<vk::CommandBuffer> cCmdPool::submit()
{
	auto cmds = m_cmds[sGlobal::Order().getCurrentFrame()];
	m_cmds[sGlobal::Order().getCurrentFrame()] = std::vector<vk::CommandBuffer>(std::thread::hardware_concurrency());

	cmds.erase(std::remove_if(cmds.begin(), cmds.end(), [&](auto& d) { return !d; }), cmds.end());
	for (auto& c : cmds)
	{
		c.end();
	}
	return cmds;

}
vk::CommandBuffer cCmdPool::allocCmdOnetime(int device_family_index)
{
	vk::CommandBufferAllocateInfo cmd_buffer_info;
	cmd_buffer_info.commandBufferCount = 1;
	cmd_buffer_info.commandPool = getCmdPool(CMD_POOL_TYPE_ONETIME, device_family_index);
	cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
	auto cmd_unique = std::move(m_context->m_device->allocateCommandBuffersUnique(cmd_buffer_info)[0]);
	auto cmd = cmd_unique.get();
	m_per_thread[sThreadLocal::Order().getThreadIndex()].m_per_family[device_family_index].m_cmd_onetime_deleter[sGlobal::Order().getRenderFrame()].push_back(std::move(cmd_unique));

	vk::CommandBufferBeginInfo begin_info;
	begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	cmd.begin(begin_info);

	return cmd;
}

vk::CommandBuffer cCmdPool::allocCmdTempolary(uint32_t device_family_index)
{
	return get();
}

vk::CommandBuffer cCmdPool::get()
{
	auto& cmd = m_cmds[sGlobal::Order().getCurrentFrame()][sThreadLocal::Order().getThreadIndex()];
	if (!cmd)
	{
		cmd = allocCmdOnetime(0);

		m_context->m_device.DebugMarkerSetObjectName(cmd, "TempCommandBuffer");

	}
	return cmd;
}
