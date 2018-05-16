#include <thread>
#include <btrlib/GPU.h>
#include <btrlib/sGlobal.h>

void cGPU::setup(vk::PhysicalDevice pd)
{
	m_handle = pd;
	std::vector<const char*> extensionName = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
#if _DEBUG
//		VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#endif
	};

	auto gpu_propaty = m_handle.getProperties();
	auto gpu_feature = m_handle.getFeatures();
	assert(gpu_feature.multiDrawIndirect);

	auto queueFamilyProperty = m_handle.getQueueFamilyProperties();

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

	vk::DeviceCreateInfo device_info;
	device_info.setQueueCreateInfoCount((uint32_t)queue_info.size());
	device_info.setPQueueCreateInfos(queue_info.data());
	device_info.setPEnabledFeatures(&feature);
	device_info.setEnabledExtensionCount((uint32_t)extensionName.size());
	device_info.setPpEnabledExtensionNames(extensionName.data());

	auto device = m_handle.createDevice(device_info, nullptr);
	m_device.m_gpu = m_handle;
	m_device.m_handle = device;
	m_device.m_queue_priority = queue_priority;
	m_device.m_family_index = family_index;
	m_device.m_vk_debug_marker_set_object_tag = (PFN_vkDebugMarkerSetObjectTagEXT)device.getProcAddr("vkDebugMarkerSetObjectTagEXT");
	m_device.m_vk_debug_marker_set_object_name = (PFN_vkDebugMarkerSetObjectNameEXT)device.getProcAddr("vkDebugMarkerSetObjectNameEXT");
	m_device.m_vk_cmd_debug_marker_begin = (PFN_vkCmdDebugMarkerBeginEXT)device.getProcAddr("vkCmdDebugMarkerBeginEXT");
	m_device.m_vk_cmd_debug_marker_end = (PFN_vkCmdDebugMarkerEndEXT)device.getProcAddr("vkCmdDebugMarkerEndEXT");
	m_device.m_vk_cmd_debug_marker_insert = (PFN_vkCmdDebugMarkerInsertEXT)device.getProcAddr("vkCmdDebugMarkerInsertEXT");
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
		auto bit = memory_type_bits >> i;
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
