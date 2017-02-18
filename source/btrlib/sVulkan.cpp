#include <btrlib/cWindow.h>
#include <btrlib/ThreadPool.h>

sVulkan::sVulkan()
{
	{
		vk::ApplicationInfo appInfo = { "Vulkan Test", 1, "EngineName", 0, VK_API_VERSION_1_0 };
		vk::InstanceCreateInfo instanceInfo = {};
		instanceInfo.setPApplicationInfo(&appInfo);
		instanceInfo.setEnabledExtensionCount(btr::sValidationLayer::Order().getExtensionName().size());
		instanceInfo.setPpEnabledExtensionNames(btr::sValidationLayer::Order().getExtensionName().data());
		instanceInfo.setEnabledLayerCount(btr::sValidationLayer::Order().getLayerName().size());
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

			auto queueFamilyProperty = gpu->getQueueFamilyProperties();
			std::vector<const char*> extensionName = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			};

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
					.setQueueCount(queuePriority.size())
					.setPQueuePriorities(queuePriority.data())
					.setQueueFamilyIndex(i),
				};

				vk::PhysicalDeviceFeatures f = vk::PhysicalDeviceFeatures()
					.setMultiDrawIndirect(VK_TRUE);
				std::vector<vk::PhysicalDeviceFeatures> features(prop.queueCount, f);

				vk::DeviceCreateInfo deviceInfo = vk::DeviceCreateInfo()
					.setQueueCreateInfoCount(queueInfo.size())
					.setPQueueCreateInfos(queueInfo.data())
					.setPEnabledFeatures(features.data())
					.setEnabledExtensionCount(extensionName.size())
					.setPpEnabledExtensionNames(extensionName.data())
					;

				auto device = gpu->createDevice(deviceInfo, nullptr);
				gpu.m_device_list.emplace_back(std::move(device));
			}

		}
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
			queueIndexList.emplace_back(i);
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
		if ((queueProp[i].queueFlags & flag) == flag) {
			index.emplace_back((uint32_t)i);
		}
	}
	assert(!index.empty());
	return index;
}

int cGPU::getMemoryTypeIndex(const vk::MemoryRequirements& request, vk::MemoryPropertyFlags flag) const
{
	auto& prop = getHandle().getMemoryProperties();
	auto typeBits = static_cast<std::int32_t>(request.memoryTypeBits);
	for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((prop.memoryTypes[i].propertyFlags & flag) == flag)
			{
				return static_cast<int>(i);
			}
		}
		typeBits >>= 1;
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
	device.m_handle = m_device_list[index];
	device.m_family_index = index;
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

// vk::SurfaceFormatKHR cGPU::Helper::getSurfaceFormat(vk::PhysicalDevice gpu, vk::SurfaceKHR surface, std::vector<vk::SurfaceFormatKHR> search)
// {
// 
// }
