#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <btrlib/Define.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cCmdPool.h>

struct CopyEngine
{

};
class cWindow;
namespace btr
{
struct Context
{
	vk::Instance m_instance;
	vk::PhysicalDevice m_physical_device;
	vk::Device m_device;

	btr::AllocatedMemory m_vertex_memory;
	btr::AllocatedMemory m_uniform_memory;
	btr::AllocatedMemory m_storage_memory;
	btr::AllocatedMemory m_staging_memory;

	vk::UniqueDescriptorPool m_descriptor_pool;

	std::shared_ptr<cWindow> m_window;
	std::shared_ptr<cCmdPool> m_cmd_pool;

//	uint32_t getCPUFrame()const { return (m_window->getSwapchain().m_backbuffer_index + 1) % m_window->getSwapchain().getBackbufferNum(); }
//	uint32_t getGPUFrame()const { return m_window->getSwapchain().m_backbuffer_index; }
	uint32_t getCPUFrame()const { return sGlobal::Order().getWorkerFrame(); }
	uint32_t getGPUFrame()const { return sGlobal::Order().getRenderFrame(); }

	void copy(vk::CommandBuffer cmd, vk::DescriptorBufferInfo BI, const void* data)
	{
		auto staging = m_staging_memory.allocateMemory(BI.range);
		memcpy(staging.getMappedPtr(), data, BI.range);
		vk::BufferCopy2KHR region[] = { vk::BufferCopy2KHR(staging.getInfo().offset, BI.offset, BI.range) };
		vk::CopyBufferInfo2KHR ci(staging.getInfo().buffer, BI.buffer, array_size(region), region);
		cmd.copyBuffer2KHR(ci);
	}
};
}

#include <btrlib/Module.h>