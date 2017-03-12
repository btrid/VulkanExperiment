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
	std::vector<vk::SurfaceFormatKHR> supportSurfaceFormat = descriptor.gpu->getSurfaceFormatsKHR(surface);

	auto request = { 
		descriptor.surface_format_request,
	};
	for (auto& r : request)
	{
		auto it = std::find(supportSurfaceFormat.begin(), supportSurfaceFormat.end(), r);
		if (it != supportSurfaceFormat.end()) {
			m_surface_format = r;
			break;
		}
	}

	assert(m_surface_format.format != vk::Format::eUndefined);

	auto supportSurface = getSupportSurfaceQueue(descriptor.gpu.getHandle(), surface);
	m_use_device = descriptor.gpu.getDeviceByFamilyIndex(supportSurface[0]);

	auto old = m_swapchain_handle;

	vk::SurfaceCapabilitiesKHR capability = descriptor.gpu->getSurfaceCapabilitiesKHR(surface);
	vk::SwapchainCreateInfoKHR swapchainInfo = vk::SwapchainCreateInfoKHR()
		.setSurface(surface)
		.setMinImageCount(2)
		.setImageFormat(m_surface_format.format)
		.setImageColorSpace(m_surface_format.colorSpace)
		.setImageExtent(capability.currentExtent)
		.setPreTransform(capability.currentTransform)
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
		.setImageSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount((uint32_t)supportSurface.size())
		.setPQueueFamilyIndices(supportSurface.data())
		.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
		.setPresentMode(vk::PresentModeKHR::eMailbox)
		.setClipped(true)
		.setOldSwapchain(m_swapchain_handle)
		;
	m_swapchain_handle = m_use_device->createSwapchainKHR(swapchainInfo);
	if (old) {
		m_use_device->destroySwapchainKHR(old);
	}

	// setup用コマンドバッファ
	vk::CommandPool commandPool;
	std::vector<vk::CommandBuffer> commandBuffer;
	{
		vk::CommandPoolCreateInfo poolInfo = vk::CommandPoolCreateInfo()
			.setFlags(vk::CommandPoolCreateFlagBits::eTransient)
			.setQueueFamilyIndex(m_use_device.getQueueFamilyIndex());
		commandPool = m_use_device->createCommandPool(poolInfo);
		vk::CommandBufferAllocateInfo cmdBufferInfo = vk::CommandBufferAllocateInfo()
			.setCommandPool(commandPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);
		commandBuffer = m_use_device->allocateCommandBuffers(cmdBufferInfo);
	}

	{
		vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo();
		commandBuffer[0].begin(beginInfo);
	}

	std::vector<vk::Image> imageList = m_use_device->getSwapchainImagesKHR(m_swapchain_handle);
	m_backbuffer.resize(imageList.size());
	for (uint32_t i = 0; i < m_backbuffer.size(); i++) {
		m_backbuffer[i].image = imageList[i];
	}

	for (uint32_t i = 0; i < m_backbuffer.size(); i++)
	{
		auto subresourceRange = vk::ImageSubresourceRange()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setBaseArrayLayer(0)
			.setLayerCount(1)
			.setBaseMipLevel(0)
			.setLevelCount(1);

		// ばりあ
		vk::ImageMemoryBarrier barrier = vk::ImageMemoryBarrier()
			.setImage(m_backbuffer[i].image)
			.setSubresourceRange(subresourceRange)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setOldLayout(vk::ImageLayout::eUndefined)
			.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			;
		commandBuffer[0].pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &barrier);

		vk::ImageViewCreateInfo colorAttachmentView = vk::ImageViewCreateInfo()
			.setImage(m_backbuffer[i].image)
			.setFormat(m_surface_format.format)
			.setViewType(vk::ImageViewType::e2D)
			.setComponents(vk::ComponentMapping
		{
			vk::ComponentSwizzle::eR,
			vk::ComponentSwizzle::eG,
			vk::ComponentSwizzle::eB,
			vk::ComponentSwizzle::eA,
		})
		.setSubresourceRange(subresourceRange);
		m_backbuffer[i].view = m_use_device->createImageView(colorAttachmentView);

	}
	{
		commandBuffer[0].end();
		auto queue = m_use_device->getQueue(0, 0);
		vk::SubmitInfo submitInfo = vk::SubmitInfo()
			.setCommandBufferCount(1)
			.setPCommandBuffers(&commandBuffer[0]);
		queue.submit(1, &submitInfo, vk::Fence());

		// @ToDo cmdbufferがローカルで作ってあるので待ってる
		queue.waitIdle();

	}

	m_use_device->freeCommandBuffers(commandPool, commandBuffer);

}

uint32_t cWindow::Swapchain::swap(vk::Semaphore& semaphore)
{
	m_use_device->acquireNextImageKHR(m_swapchain_handle, 1000, semaphore, vk::Fence(), &m_backbuffer_index);
	return m_backbuffer_index;
}


