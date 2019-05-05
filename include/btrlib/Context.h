#pragma once

#include <vector>
#include <btrlib/Define.h>
#include <btrlib/GPU.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cCmdPool.h>

class cWindow;
namespace btr
{
struct Context
{
	vk::Instance m_instance;
	cGPU m_gpu;
	cDevice m_device;

	btr::AllocatedMemory m_vertex_memory;
	btr::AllocatedMemory m_uniform_memory;
	btr::AllocatedMemory m_storage_memory;
	btr::AllocatedMemory m_staging_memory;

	vk::UniqueDescriptorPool m_descriptor_pool;
	vk::UniquePipelineCache m_cache;

	std::shared_ptr<cWindow> m_window;
	std::shared_ptr<cCmdPool> m_cmd_pool;

	vk::DispatchLoaderDynamic m_dispach;

//	uint32_t getCPUFrame()const { return (m_window->getSwapchain().m_backbuffer_index + 1) % m_window->getSwapchain().getBackbufferNum(); }
//	uint32_t getGPUFrame()const { return m_window->getSwapchain().m_backbuffer_index; }
	uint32_t getCPUFrame()const { return sGlobal::Order().getWorkerFrame(); }
	uint32_t getGPUFrame()const { return sGlobal::Order().getRenderFrame(); }
};
}

#include <btrlib/Module.h>