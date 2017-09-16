#pragma once

#include <btrlib/Define.h>
#include <btrlib/GPU.h>
#include <btrlib/BufferMemory.h>
#include <btrlib/cWindow.h>
#include <btrlib/cCmdPool.h>

namespace btr
{

struct Loader
{
	cGPU m_gpu;
	cDevice m_device;

	btr::BufferMemory m_vertex_memory;
	btr::BufferMemory m_uniform_memory;
	btr::BufferMemory m_storage_memory;
	btr::BufferMemory m_staging_memory;

	vk::UniqueDescriptorPool m_descriptor_pool;
	vk::UniquePipelineCache m_cache;

	std::shared_ptr<cCmdPool> m_cmd_pool;
	std::shared_ptr<cWindow> m_window;
};
struct Executer
{
	cGPU m_gpu;
	cDevice m_device;

	btr::BufferMemory m_vertex_memory;
	btr::BufferMemory m_uniform_memory;
	btr::BufferMemory m_storage_memory;
	btr::BufferMemory m_staging_memory;

	std::shared_ptr<cWindow> m_window;
	std::shared_ptr<cCmdPool> m_cmd_pool;

	uint32_t getCPUFrame()const { return (m_window->getSwapchain().m_backbuffer_index+1)% m_window->getSwapchain().getBackbufferNum(); }
	uint32_t getGPUFrame()const { return m_window->getSwapchain().m_backbuffer_index; }
};

}
