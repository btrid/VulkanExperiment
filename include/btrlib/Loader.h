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

	vk::CommandBuffer m_cmd;
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
};

}