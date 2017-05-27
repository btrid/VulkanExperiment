#pragma once

#include <btrlib/Define.h>
#include <btrlib/GPU.h>
#include <btrlib/BufferMemory.h>

namespace app
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

}
