#include <btrlib/cWindow.h>
#include <btrlib/sValidationLayer.h>

#include <filesystem>
#include <vector>
#include <iostream>
#include <fstream>

sGlobal::sGlobal()
	: m_current_frame(0)
{
	m_deltatime = 0.016f;
	{
		vk::ApplicationInfo appInfo = { "Vulkan Test", 1, "EngineName", 0, VK_API_VERSION_1_0 };
		vk::InstanceCreateInfo instanceInfo = {};
		instanceInfo.setPApplicationInfo(&appInfo);
		instanceInfo.setEnabledExtensionCount((uint32_t)btr::sValidationLayer::Order().getExtensionName().size());
		instanceInfo.setPpEnabledExtensionNames(btr::sValidationLayer::Order().getExtensionName().data());
		instanceInfo.setEnabledLayerCount((uint32_t)btr::sValidationLayer::Order().getLayerName().size());
		instanceInfo.setPpEnabledLayerNames(btr::sValidationLayer::Order().getLayerName().data());
		m_instance = vk::createInstance(instanceInfo);
	}

	{
		auto gpus = m_instance.enumeratePhysicalDevices();
		m_gpu.reserve(gpus.size());
		for (auto&& pd : gpus)
		{
			m_gpu.emplace_back();
			auto& gpu = m_gpu.back();
			gpu.m_handle = pd;
			std::vector<const char*> extensionName = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			};

			auto gpu_propaty = gpu->getProperties();
			auto gpu_feature = gpu->getFeatures();
// 			auto memory_prop = gpu->getMemoryProperties();
// 			for (uint32_t i = 0; i < memory_prop.memoryTypeCount; i++)
// 			{
// 				auto memory_type = memory_prop.memoryTypes[i];
// 				auto memory_heap = memory_prop.memoryHeaps[memory_type.heapIndex];
// 				printf("[%2d]%s %s size = %lld\n", i, vk::to_string(memory_type.propertyFlags).c_str(), vk::to_string(memory_heap.flags).c_str(), memory_heap.size);
// 			}
			auto queueFamilyProperty = gpu->getQueueFamilyProperties();
			gpu.m_device_list.reserve(queueFamilyProperty.size());

			std::vector<std::vector<float>> queue_priority(queueFamilyProperty.size());
			for (size_t i = 0; i < queueFamilyProperty.size(); i++) 
			{
				auto& queue_property = queueFamilyProperty[i];
				auto& priority = queue_priority[i];
				priority.resize(1);
				for (size_t q = 1; q < priority.size(); q++)
				{
					priority[q] = 1.f / (priority.size() - 1) * q;
				}
 			}

			// デバイス
			std::vector<vk::DeviceQueueCreateInfo> queue_info(queueFamilyProperty.size());
			std::vector<uint32_t> family_index;
			for (size_t i = 0; i < queueFamilyProperty.size(); i++)
			{
				queue_info[i].queueCount = (uint32_t)queue_priority[i].size();
				queue_info[i].pQueuePriorities = queue_priority[i].data();
				queue_info[i].queueFamilyIndex = (uint32_t)i;
				family_index.push_back((uint32_t)i);
			}

 			vk::PhysicalDeviceFeatures feature = gpu_feature;

			vk::DeviceCreateInfo deviceInfo = vk::DeviceCreateInfo()
				.setQueueCreateInfoCount((uint32_t)queue_info.size())
				.setPQueueCreateInfos(queue_info.data())
				.setPEnabledFeatures(nullptr)
				.setEnabledExtensionCount((uint32_t)extensionName.size())
				.setPpEnabledExtensionNames(extensionName.data())
				;
			auto device = gpu->createDevice(deviceInfo, nullptr);
			cGPU::Device device_holder;
			device_holder.m_device = device;
			device_holder.m_queue = queue_priority;
			device_holder.m_family_index = family_index;
			device_holder.m_vk_debug_marker_set_object_tag = (PFN_vkDebugMarkerSetObjectTagEXT)device.getProcAddr("vkDebugMarkerSetObjectTagEXT");
			device_holder.m_vk_debug_marker_set_object_name = (PFN_vkDebugMarkerSetObjectNameEXT)device.getProcAddr("vkDebugMarkerSetObjectNameEXT");
			device_holder.m_vk_cmd_debug_marker_begin = (PFN_vkCmdDebugMarkerBeginEXT)device.getProcAddr("vkCmdDebugMarkerBeginEXT");
			device_holder.m_vk_cmd_debug_marker_end = (PFN_vkCmdDebugMarkerEndEXT)device.getProcAddr("vkCmdDebugMarkerEndEXT");
			device_holder.m_vk_cmd_debug_marker_insert = (PFN_vkCmdDebugMarkerInsertEXT)device.getProcAddr("vkCmdDebugMarkerInsertEXT");
			gpu.m_device_list.emplace_back(std::move(device_holder));

		}
	}

	auto init_thread_data_func = [=](const cThreadPool::InitParam& param)
	{
		SetThreadIdealProcessor(::GetCurrentThread(), param.m_index);
		auto& data = sThreadLocal::Order();
		data.m_thread_index = param.m_index;
		data.m_gpu = m_gpu[0];

		data.m_device[sThreadLocal::DEVICE_GRAPHICS] = data.m_gpu.getDevice(vk::QueueFlagBits::eGraphics)[0];
		data.m_device[sThreadLocal::DEVICE_COMPUTE] = data.m_gpu.getDevice(vk::QueueFlagBits::eCompute)[0];
		data.m_device[sThreadLocal::DEVICE_TRANSFAR] = data.m_gpu.getDevice(vk::QueueFlagBits::eTransfer)[0];

		data.m_cmd_pool_onetime.resize(data.m_gpu.m_device_list.size());
		for (int family = 0; family < data.m_cmd_pool_onetime.size(); family++)
		{
			auto& cmd_pool_per_device = data.m_cmd_pool_onetime[family];
			for (auto& cmd_pool_per_frame : cmd_pool_per_device)
			{
				vk::CommandPoolCreateInfo cmd_pool_info = vk::CommandPoolCreateInfo()
				.setQueueFamilyIndex(family)
				.setFlags(vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
				cmd_pool_per_frame = data.m_gpu.getDeviceByFamilyIndex(family)->createCommandPool(cmd_pool_info);
			}
		}

		data.m_cmd_pool_compiled.resize(data.m_gpu.m_device_list.size());
		for (int family = 0; family < data.m_cmd_pool_compiled.size(); family++)
		{
			auto& cmd_pool_per_device = data.m_cmd_pool_compiled[family];
			vk::CommandPoolCreateInfo cmd_pool_info;
			cmd_pool_info.setQueueFamilyIndex(family);
			cmd_pool_info.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
			cmd_pool_per_device = data.m_gpu.getDeviceByFamilyIndex(family)->createCommandPool(cmd_pool_info);
		}
	};

	m_thread_pool.start(std::thread::hardware_concurrency()-1, init_thread_data_func);

	m_cmd_pool_tempolary.resize(m_gpu[0].m_device_list.size());
	for (size_t family = 0; family < m_cmd_pool_tempolary.size(); family++)
	{
		vk::CommandPoolCreateInfo cmd_pool_info;
		cmd_pool_info.queueFamilyIndex = (uint32_t)family;
		cmd_pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
		m_cmd_pool_tempolary[family] = m_gpu[0].getDeviceByFamilyIndex((uint32_t)family)->createCommandPool(cmd_pool_info);
	}

}


void sGlobal::swap()
{
	m_current_frame = ++m_current_frame % FRAME_MAX;
	auto next = (m_current_frame+1) % FRAME_MAX;
	auto& deletelist = m_cmd_delete[next];
	for (auto it = deletelist.begin(); it != deletelist.end();)
	{
		if ((*it)->isReady()) {
			it = deletelist.erase(it);
		}
		else {
			it++;
		}
	}
}
vk::ShaderModule loadShader(const vk::Device& device, const std::string& filename)
{
	std::experimental::filesystem::path filepath(filename);
	std::ifstream file(filepath, std::ios_base::ate | std::ios::binary);
	if (!file.is_open()) {
		assert(false);
		return vk::ShaderModule();
	}

	size_t file_size = (size_t)file.tellg();
	file.seekg(0);

	std::vector<char> buffer(file_size);
	file.read(buffer.data(), buffer.size());

	vk::ShaderModuleCreateInfo shaderInfo = vk::ShaderModuleCreateInfo()
		.setPCode(reinterpret_cast<const uint32_t*>(buffer.data()))
		.setCodeSize(buffer.size());
	return device.createShaderModule(shaderInfo);
}

void vk::FenceShared::create(vk::Device device, const vk::FenceCreateInfo& fence_info)
{
	auto deleter = [=](vk::Fence* fence)
	{
		auto deleter = std::make_unique<Deleter>();
		deleter->device = device;
		deleter->fence = { *fence };
		sGlobal::Order().destroyResource(std::move(deleter));
		delete fence;
	};
	m_fence = std::shared_ptr<vk::Fence>(new vk::Fence(), deleter);
	*m_fence = device.createFence(fence_info);
}
