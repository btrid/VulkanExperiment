#include <btrlib/GPU.h>

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

std::vector<cDevice> cGPU::getDevice(vk::QueueFlags flag) const
{
	return { getDeviceByFamilyIndex(0) };
}
cDevice cGPU::getDeviceByFamilyIndex(uint32_t index) const
{
	auto& d = m_device_list[index];
	cDevice device;
	device.m_gpu = m_handle;
	device.m_handle = m_device_list[index].m_device;
	device.m_family_index = d.m_family_index;
	device.m_queue_info = d.m_queue;
	device.m_vk_debug_marker_set_object_tag = m_device_list[index].m_vk_debug_marker_set_object_tag;
	device.m_vk_debug_marker_set_object_name = m_device_list[index].m_vk_debug_marker_set_object_name;
	device.m_vk_cmd_debug_marker_begin = m_device_list[index].m_vk_cmd_debug_marker_begin;
	device.m_vk_cmd_debug_marker_end = m_device_list[index].m_vk_cmd_debug_marker_end;
	device.m_vk_cmd_debug_marker_insert = m_device_list[index].m_vk_cmd_debug_marker_insert;
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
