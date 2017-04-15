#include <btrlib/cWindow.h>
#include <btrlib/ThreadPool.h>

#include <filesystem>
#include <vector>
#include <iostream>
#include <fstream>

sGlobal::sGlobal()
	: m_current_frame(0)
{
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
//				VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
			};
//			auto extension_prop = gpu->enumerateDeviceExtensionProperties();

			auto queueFamilyProperty = gpu->getQueueFamilyProperties();
			gpu.m_device_list.reserve(queueFamilyProperty.size());
			// デバイス
			for (size_t i = 0; i < queueFamilyProperty.size(); i++) {
				auto& prop = queueFamilyProperty[i];
				std::vector<float> queuePriority(prop.queueCount, 0.f);
				for (size_t i = 0; i < prop.queueCount; i++){
					queuePriority[i] = 1.f / prop.queueCount * i;
				}

				std::vector<vk::DeviceQueueCreateInfo> queueInfo =
				{
					vk::DeviceQueueCreateInfo()
					.setQueueCount((uint32_t)queuePriority.size())
					.setPQueuePriorities(queuePriority.data())
					.setQueueFamilyIndex((uint32_t)i),
				};

				vk::PhysicalDeviceFeatures f = vk::PhysicalDeviceFeatures()
					.setMultiDrawIndirect(VK_TRUE);
				std::vector<vk::PhysicalDeviceFeatures> features(prop.queueCount, f);

				vk::DeviceCreateInfo deviceInfo = vk::DeviceCreateInfo()
					.setQueueCreateInfoCount((uint32_t)queueInfo.size())
					.setPQueueCreateInfos(queueInfo.data())
					.setPEnabledFeatures(features.data())
					.setEnabledExtensionCount((uint32_t)extensionName.size())
					.setPpEnabledExtensionNames(extensionName.data())
					;

				auto device = gpu->createDevice(deviceInfo, nullptr);
				cGPU::Device device_holder;
				device_holder.m_device = device;
				device_holder.m_vk_debug_marker_set_object_tag = (PFN_vkDebugMarkerSetObjectTagEXT)device.getProcAddr("vkDebugMarkerSetObjectTagEXT");
				device_holder.m_vk_debug_marker_set_object_name = (PFN_vkDebugMarkerSetObjectNameEXT)device.getProcAddr("vkDebugMarkerSetObjectNameEXT");
				device_holder.m_vk_cmd_debug_marker_begin = (PFN_vkCmdDebugMarkerBeginEXT)device.getProcAddr("vkCmdDebugMarkerBeginEXT");
				device_holder.m_vk_cmd_debug_marker_end = (PFN_vkCmdDebugMarkerEndEXT)device.getProcAddr("vkCmdDebugMarkerEndEXT");
				device_holder.m_vk_cmd_debug_marker_insert = (PFN_vkCmdDebugMarkerInsertEXT)device.getProcAddr("vkCmdDebugMarkerInsertEXT");
				gpu.m_device_list.emplace_back(std::move(device_holder));
			}

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

// 		data.m_cmd_pool_tempolary.resize(data.m_gpu.m_device_list.size());
// 		for (size_t family = 0; family < data.m_cmd_pool_tempolary.size(); family++)
// 		{
// 			vk::CommandPoolCreateInfo cmd_pool_info;
// 			cmd_pool_info.queueFamilyIndex = (uint32_t)family;
// 			cmd_pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
// 			data.m_cmd_pool_tempolary[family] = data.m_gpu.getDeviceByFamilyIndex(family)->createCommandPool(cmd_pool_info);
// 			for (int frame = 0; frame < FRAME_MAX; frame++)
// 			{
// 				vk::CommandBufferAllocateInfo cmd_buffer_info;
// 				cmd_buffer_info.commandBufferCount = 100;
// 				cmd_buffer_info.commandPool = data.m_cmd_pool_tempolary[family];
// 				cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
// 				data.m_cmd_tempolary[frame] = data.m_gpu.getDeviceByFamilyIndex(family)->allocateCommandBuffers(cmd_buffer_info);
// 			}
// 		}
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

std::vector<uint32_t> cGPU::getQueueFamilyIndexList(vk::QueueFlags flag, const std::vector<uint32_t>& useIndex)
{
	std::vector<vk::QueueFamilyProperties> queueProperty = getHandle().getQueueFamilyProperties();
	std::vector<uint32_t> queueIndexList;
	queueIndexList.reserve(queueProperty.size());
	for (size_t i = 0; i < queueProperty.size(); i++)
	{
		if (std::find(useIndex.begin(), useIndex.end(), (uint32_t)i) == useIndex.end()) {
			// 使うリストに入ってないならスキップ
			continue;
		}

		if ((queueProperty[i].queueFlags & flag) != vk::QueueFlagBits()) {
			queueIndexList.emplace_back((uint32_t)i);
		}
	}

	assert(!queueIndexList.empty());
	return queueIndexList;
}

std::vector<uint32_t> cGPU::getQueueFamilyIndexList(vk::QueueFlags flag) const
{
	std::vector<uint32_t> index;
	auto queueProp = getHandle().getQueueFamilyProperties();
	for (size_t i = 0; i < queueProp.size(); i++) {
		if ((queueProp[i].queueFlags & flag) != vk::QueueFlags()) {
			index.emplace_back((uint32_t)i);
		}
	}
	assert(!index.empty());
	return index;
}

int cGPU::getMemoryTypeIndex(const vk::MemoryRequirements& request, vk::MemoryPropertyFlags flag) const
{
	auto& prop = getHandle().getMemoryProperties();
	auto memory_type_bits = static_cast<std::int32_t>(request.memoryTypeBits);
	for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
	{
		auto bit  = memory_type_bits >> i;
		if ((bit & 1) == 0) {
			continue;
		}

		if ((prop.memoryTypes[i].propertyFlags & flag) == flag)
		{
			return static_cast<int>(i);
		}
	}
	assert(false);
	return -1;
}

std::vector<cDevice> cGPU::getDevice(vk::QueueFlags flag) const
{
	std::vector<uint32_t> index;
	auto queueProp = m_handle.getQueueFamilyProperties();
	// 専用のDeviceをまず入れる
	for (size_t i = 0; i < queueProp.size(); i++) {
		if (queueProp[i].queueFlags == flag) {
			index.emplace_back((uint32_t)i);
		}
	}
	// 汎用のデバイス
	for (size_t i = 0; i < queueProp.size(); i++) {
		if ((queueProp[i].queueFlags | flag) == queueProp[i].queueFlags
			&& (queueProp[i].queueFlags != flag)) 
		{
			index.emplace_back((uint32_t)i);
		}
	}

	std::vector<cDevice> device;
	device.reserve(index.size());
	for (auto&& i : index)
	{
		device.emplace_back(getDeviceByFamilyIndex(i));
	}
	return device;
}
cDevice cGPU::getDeviceByFamilyIndex(uint32_t index) const
{
	cDevice device;
	device.m_gpu = m_handle;
	device.m_handle = m_device_list[index].m_device;
	device.m_vk_debug_marker_set_object_tag = m_device_list[index].m_vk_debug_marker_set_object_tag;
	device.m_vk_debug_marker_set_object_name = m_device_list[index].m_vk_debug_marker_set_object_name;
	device.m_vk_cmd_debug_marker_begin = m_device_list[index].m_vk_cmd_debug_marker_begin;
	device.m_vk_cmd_debug_marker_end = m_device_list[index].m_vk_cmd_debug_marker_end;
	device.m_vk_cmd_debug_marker_insert = m_device_list[index].m_vk_cmd_debug_marker_insert;
	device.m_family_index = index;
	device.m_queue_num = getHandle().getQueueFamilyProperties()[index].queueCount;
	return device;
}

vk::Format cGPU::getSupportedDepthFormat(const std::vector<vk::Format>& depthFormat) const
{
	for (auto& format : depthFormat)
	{
		vk::FormatProperties formatProperty = getHandle().getFormatProperties(format);
		if (formatProperty.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
			return format;
		}
	}

	assert(false);
	return vk::Format::eUndefined;
}

void sGlobal::swap()
{
	auto prev = m_current_frame;
	m_current_frame = ++m_current_frame % FRAME_MAX;
	auto& deletelist = m_cmd_delete[m_current_frame];
	for (auto it = deletelist.begin(); it != deletelist.end();)
	{
		if ((*it)->isReady()) {
			it = deletelist.erase(it);
		}
		else {
			it++;
		}
	}
	GPUResource::Manager::Order().swap(m_current_frame);
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
