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
			cmd_pool_onetime.flags = vk::CommandPoolCreateFlagBits::eTransient;
			for (auto& pool_per_frame : per_family.m_cmd_pool_onetime)
			{
				pool_per_frame = context->m_device->createCommandPoolUnique(cmd_pool_onetime, cb);
			}

			vk::CommandPoolCreateInfo cmd_pool_compiled;
			cmd_pool_compiled.queueFamilyIndex = (uint32_t)family;
			cmd_pool_compiled.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
			per_family.m_cmd_pool_compiled = context->m_device->createCommandPoolUnique(cmd_pool_onetime, cb);

			vk::CommandPoolCreateInfo cmd_pool_temporary;
			cmd_pool_temporary.queueFamilyIndex = (uint32_t)family;
			cmd_pool_temporary.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient;
			per_family.m_cmd_pool_temporary = context->m_device->createCommandPoolUnique(cmd_pool_onetime, cb);
		}
	}

}



vk::CommandPool cCmdPool::getCmdPool(cCmdPool::CmdPoolType type, int device_family_index) const
{
	auto& pool_per_family = m_per_thread[sThreadLocal::Order().getThreadIndex()].m_per_family[device_family_index];
	switch (type)
	{
	case CMD_POOL_TYPE_ONETIME:
		return pool_per_family.m_cmd_pool_onetime[sGlobal::Order().getCurrentFrame()].get();
	case CMD_POOL_TYPE_TEMPORARY:
		return pool_per_family.m_cmd_pool_temporary.get();
	case CMD_POOL_TYPE_COMPILED:
	default:
		return pool_per_family.m_cmd_pool_compiled.get();
	}
}

void cCmdPool::resetPool(std::shared_ptr<btr::Context>& executer)
{
	{
		auto& fences = m_fences[executer->getGPUFrame()];
		for (auto& f : fences)
		{
			sDebug::Order().waitFence(executer->m_device.getHandle(), f.get());
			executer->m_device->resetFences({ f.get() });
		}
		fences.clear();
	}
	
	for (auto& tls : m_per_thread)
	{
		for (auto& pool_family : tls.m_per_family)
		{
			if (!pool_family.m_cmd_onetime_deleter[executer->getGPUFrame()].empty())
			{
				executer->m_gpu.getDevice()->resetCommandPool(pool_family.m_cmd_pool_onetime[executer->getGPUFrame()].get(), vk::CommandPoolResetFlagBits::eReleaseResources);
				pool_family.m_cmd_onetime_deleter[executer->getGPUFrame()].clear();

			}
		}
	}
}

void cCmdPool::enqueCmd(vk::UniqueCommandBuffer&& cmd, uint32_t family_index)
{
	auto& pool_per_family = m_per_thread[sThreadLocal::Order().getThreadIndex()].m_per_family[family_index];
	{
		std::lock_guard<std::mutex> lk(m_cmd_queue_mutex);
		pool_per_family.m_cmd_queue.push_back(std::move(cmd));

	}
}
PerFamilyIndex<std::vector<vk::UniqueCommandBuffer>> cCmdPool::submitCmd()
{
	PerFamilyIndex<std::vector<vk::UniqueCommandBuffer>> cmds;
	cmds.resize(m_per_thread[0].m_per_family.size());
	{
		std::lock_guard<std::mutex> lk(m_cmd_queue_mutex);
		for (size_t thread = 0; thread < m_per_thread.size(); thread++)
		{
			auto& per_thread = m_per_thread[thread];
			for (size_t family = 0; family < per_thread.m_per_family.size(); family++)
			{
				auto& per_family = per_thread.m_per_family[family].m_cmd_queue;
				cmds[family].insert(cmds[family].end(), std::make_move_iterator(per_family.begin()), std::make_move_iterator(per_family.end()));
				per_family.clear();
			}
		}
	}
	return cmds;
}

void cCmdPool::submit(std::shared_ptr<btr::Context>& context)
{
	auto cmd_queues = submitCmd();
	auto& fences = m_fences[sGlobal::Order().getPrevFrame()];
	for (size_t i = 0; i < cmd_queues.size(); i++)
	{
		auto& cmds = cmd_queues[i];
		if (cmds.empty())
		{
			continue;
		}
		std::vector<vk::CommandBuffer> cmd(cmds.size());
		for (size_t i = 0; i < cmds.size(); i++)
		{
			cmd[i] = cmds[i].get();
		}

		vk::PipelineStageFlags wait_pipelines[] = {
			vk::PipelineStageFlagBits::eAllCommands,
		};
		std::vector<vk::SubmitInfo> submit_info =
		{
			vk::SubmitInfo()
			.setCommandBufferCount((uint32_t)cmd.size())
			.setPCommandBuffers(cmd.data())
			.setWaitSemaphoreCount(0)
			.setPWaitSemaphores(nullptr)
			.setPWaitDstStageMask(wait_pipelines)
			.setSignalSemaphoreCount(0)
			.setPSignalSemaphores(nullptr)
		};

		auto q = context->m_device->getQueue(i, 0);

		vk::FenceCreateInfo info;
		fences.emplace_back(std::move(context->m_device->createFenceUnique(info)));
		q.submit(submit_info, fences.back().get());
	}
	sDeleter::Order().enque(std::move(cmd_queues));

}
vk::CommandBuffer cCmdPool::allocCmdOnetime(int device_family_index)
{
	vk::CommandBufferAllocateInfo cmd_buffer_info;
	cmd_buffer_info.commandBufferCount = 1;
	cmd_buffer_info.commandPool = getCmdPool(CMD_POOL_TYPE_ONETIME, device_family_index);
	cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
	auto cmd_unique = std::move(sGlobal::Order().getGPU(0).getDevice()->allocateCommandBuffersUnique(cmd_buffer_info)[0]);
	auto cmd = cmd_unique.get();
	m_per_thread[sThreadLocal::Order().getThreadIndex()].m_per_family[device_family_index].m_cmd_onetime_deleter[sGlobal::Order().getGPUFrame()].push_back(std::move(cmd_unique));

	vk::CommandBufferBeginInfo begin_info;
	begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	cmd.begin(begin_info);

	return cmd;
}

ScopedCommand cCmdPool::allocCmdTempolary(uint32_t device_family_index)
{
	vk::CommandBufferAllocateInfo cmd_buffer_info;
	cmd_buffer_info.commandBufferCount = 1;
	cmd_buffer_info.commandPool = getCmdPool(CMD_POOL_TYPE_ONETIME, device_family_index);
	cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
	auto cmd_unique = std::move(sGlobal::Order().getGPU(0).getDevice()->allocateCommandBuffersUnique(cmd_buffer_info)[0]);

	vk::CommandBufferBeginInfo begin_info;
	begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	cmd_unique->begin(begin_info);

	ScopedCommand scoped;
	scoped.m_resource = std::make_shared<ScopedCommand::Resource>();
	scoped.m_resource->m_cmd = std::move(cmd_unique);
	scoped.m_resource->m_family_index = device_family_index;
	scoped.m_resource->m_parent = this;
	return std::move(scoped);
}

ScopedCommand::Resource::Resource() : m_cmd()
, m_family_index(0)
, m_parent(nullptr)
{
}

ScopedCommand::Resource::~Resource()
{
	if (m_cmd)
	{
		m_cmd->end();
		m_parent->enqueCmd(std::move(m_cmd), m_family_index);
	}
}
