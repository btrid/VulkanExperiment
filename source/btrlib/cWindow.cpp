#include <btrlib/cWindow.h>

namespace
{

std::vector<uint32_t> getSupportSurfaceQueue(vk::PhysicalDevice gpu, vk::SurfaceKHR surface)
{
	auto queueProp = gpu.getQueueFamilyProperties();
	std::vector<uint32_t> queueIndexList;
	queueIndexList.reserve(queueProp.size());
	for (size_t i = 0; i < queueProp.size(); i++)
	{
		if (gpu.getSurfaceSupportKHR((uint32_t)i, surface)) {
			queueIndexList.emplace_back((uint32_t)i);
		}
	}

	assert(!queueIndexList.empty());
	return queueIndexList;
}

}

void cWindow::Swapchain::setup(const CreateInfo& descriptor, vk::SurfaceKHR surface)
{
	std::vector<vk::PresentModeKHR> presentModeList = descriptor.gpu->getSurfacePresentModesKHR(surface);

	m_surface_format.format = vk::Format::eUndefined;
	std::vector<vk::SurfaceFormatKHR> support_format = descriptor.gpu->getSurfaceFormatsKHR(surface);

	auto request = { 
		descriptor.surface_format_request,
	};
	for (auto& r : request)
	{
		auto it = std::find(support_format.begin(), support_format.end(), r);
		if (it != support_format.end()) {
			m_surface_format = r;
			break;
		}
	}

	// サーフェスに使用できるフォーマットが見つからなかった
	assert(m_surface_format.format != vk::Format::eUndefined);

	auto support_surface = getSupportSurfaceQueue(descriptor.gpu.getHandle(), surface);
	m_use_device = descriptor.gpu.getDeviceByFamilyIndex(support_surface[0]);

	auto old_swap_chain = m_swapchain_handle;

	vk::SurfaceCapabilitiesKHR capability = descriptor.gpu->getSurfaceCapabilitiesKHR(surface);
	vk::SwapchainCreateInfoKHR swapchain_info;
	swapchain_info.setSurface(surface);
	swapchain_info.setMinImageCount(3);
	swapchain_info.setImageFormat(m_surface_format.format);
	swapchain_info.setImageColorSpace(m_surface_format.colorSpace);
	swapchain_info.setImageExtent(capability.currentExtent);
	swapchain_info.setPreTransform(capability.currentTransform);
	swapchain_info.setImageArrayLayers(1);
	swapchain_info.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);
	swapchain_info.setImageSharingMode(vk::SharingMode::eExclusive);
	swapchain_info.setQueueFamilyIndexCount((uint32_t)support_surface.size());
	swapchain_info.setPQueueFamilyIndices(support_surface.data());
	swapchain_info.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
	swapchain_info.setPresentMode(vk::PresentModeKHR::eMailbox);
	swapchain_info.setClipped(true);
	swapchain_info.setOldSwapchain(m_swapchain_handle);
	m_swapchain_handle = m_use_device->createSwapchainKHR(swapchain_info);
	if (old_swap_chain) {
		m_use_device->destroySwapchainKHR(old_swap_chain);
	}

	m_backbuffer_image = m_use_device->getSwapchainImagesKHR(m_swapchain_handle);

}

uint32_t cWindow::Swapchain::swap(vk::Semaphore& semaphore)
{
	m_use_device->acquireNextImageKHR(m_swapchain_handle, 1000, semaphore, vk::Fence(), &m_backbuffer_index);
	return m_backbuffer_index;
}


