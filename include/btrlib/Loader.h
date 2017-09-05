#pragma once

#include <btrlib/Define.h>
#include <btrlib/GPU.h>
#include <btrlib/BufferMemory.h>
#include <btrlib/cWindow.h>

namespace btr
{

struct Loader
{
	cDevice m_device;
	vk::RenderPass m_render_pass;
	btr::BufferMemory m_vertex_memory;
	btr::BufferMemory m_uniform_memory;
	btr::BufferMemory m_storage_memory;
	btr::BufferMemory m_staging_memory;

	vk::DescriptorPool m_descriptor_pool;
	vk::PipelineCache m_cache;

	vk::CommandBuffer m_cmd;
	cWindow* m_window;
};
struct Executer
{
	cDevice m_device;
	vk::CommandBuffer m_cmd;
	btr::BufferMemory m_vertex_memory;
	btr::BufferMemory m_uniform_memory;
	btr::BufferMemory m_storage_memory;
	btr::BufferMemory m_staging_memory;

	cThreadPool* m_thread_pool;
	cWindow* m_window;

	uint32_t getCPUFrame()const { return (m_window->getSwapchain().m_backbuffer_index+1)% m_window->getSwapchain().getBackbufferNum(); }
	uint32_t getGPUFrame()const { return m_window->getSwapchain().m_backbuffer_index; }
};

}
