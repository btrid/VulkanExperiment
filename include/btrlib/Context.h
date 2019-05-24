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

	vk::UniqueShaderModule loadShaderUnique(const std::string& filename)
	{
		std::experimental::filesystem::path filepath(filename);
		std::ifstream file(filepath, std::ios_base::ate | std::ios::binary);
		assert(file.is_open());

		size_t file_size = (size_t)file.tellg();
		file.seekg(0);

		std::vector<char> buffer(file_size);
		file.read(buffer.data(), buffer.size());

		vk::ShaderModuleCreateInfo shaderInfo = vk::ShaderModuleCreateInfo()
			.setPCode(reinterpret_cast<const uint32_t*>(buffer.data()))
			.setCodeSize(buffer.size());

		auto shader_module = m_device->createShaderModuleUnique(shaderInfo);

#if USE_DEBUG_REPORT
		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.pObjectName = filename.c_str();
		name_info.objectType = vk::ObjectType::eShaderModule;
		name_info.objectHandle = reinterpret_cast<uint64_t &>(shader_module);
		m_device->setDebugUtilsObjectNameEXT(name_info, m_dispach);
#endif
		return shader_module;
	}
};
}

#include <btrlib/Module.h>