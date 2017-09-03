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
	m_use_device = descriptor.gpu.getDevice();

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
	swapchain_info.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst);
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

	auto backbuffer_image = m_use_device->getSwapchainImagesKHR(m_swapchain_handle);
	m_backbuffer.reserve(backbuffer_image.size());
	{
		for (uint32_t i = 0; i < backbuffer_image.size(); i++)
		{
			auto subresourceRange = vk::ImageSubresourceRange()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setBaseArrayLayer(0)
				.setLayerCount(1)
				.setBaseMipLevel(0)
				.setLevelCount(1);

			vk::ImageViewCreateInfo backbuffer_view_info;
			backbuffer_view_info.setImage(backbuffer_image[i]);
			backbuffer_view_info.setFormat(m_surface_format.format);
			backbuffer_view_info.setViewType(vk::ImageViewType::e2D);
			backbuffer_view_info.setComponents(vk::ComponentMapping{
				vk::ComponentSwizzle::eR,
				vk::ComponentSwizzle::eG,
				vk::ComponentSwizzle::eB,
				vk::ComponentSwizzle::eA,
			});
			backbuffer_view_info.setSubresourceRange(subresourceRange);
			auto backbuffer_view = m_use_device->createImageView(backbuffer_view_info);

			RenderTarget rendertarget;
			rendertarget.m_image = backbuffer_image[i];
			rendertarget.m_view = backbuffer_view;
			rendertarget.m_format = m_surface_format.format;
			rendertarget.m_size = capability.currentExtent;
			m_backbuffer.emplace_back(rendertarget);
		}
	}

	{
		// デプス生成
		vk::ImageCreateInfo depth_info;
		m_depth.m_format = vk::Format::eD32Sfloat;
		depth_info.format = vk::Format::eD32Sfloat;
		depth_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferDst;
		depth_info.arrayLayers = 1;
		depth_info.mipLevels = 1;
		depth_info.extent.width = capability.currentExtent.width;
		depth_info.extent.height = capability.currentExtent.height;
		depth_info.extent.depth = 1;
		depth_info.imageType = vk::ImageType::e2D;
		depth_info.initialLayout = vk::ImageLayout::eUndefined;
		depth_info.queueFamilyIndexCount = m_use_device.getQueueFamilyIndex().size();
		depth_info.pQueueFamilyIndices = m_use_device.getQueueFamilyIndex().data();
		m_depth.m_image = m_use_device->createImage(depth_info);
		m_depth.m_size.setWidth(depth_info.extent.width);
		m_depth.m_size.setHeight(depth_info.extent.height);

		// メモリ確保
		auto memory_request = m_use_device->getImageMemoryRequirements(m_depth.m_image);
		uint32_t memory_index = cGPU::Helper::getMemoryTypeIndex(m_use_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::MemoryAllocateInfo memory_info;
		memory_info.allocationSize = memory_request.size;
		memory_info.memoryTypeIndex = memory_index;
		m_depth.m_memory = m_use_device->allocateMemory(memory_info);

		m_use_device->bindImageMemory(m_depth.m_image, m_depth.m_memory, 0);

		vk::ImageViewCreateInfo depth_view_info;
		depth_view_info.format = depth_info.format;
		depth_view_info.image = m_depth.m_image;
		depth_view_info.viewType = vk::ImageViewType::e2D;
		depth_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
		depth_view_info.subresourceRange.baseArrayLayer = 0;
		depth_view_info.subresourceRange.baseMipLevel = 0;
		depth_view_info.subresourceRange.layerCount = 1;
		depth_view_info.subresourceRange.levelCount = 1;
		m_depth.m_view = m_use_device->createImageView(depth_view_info);

	}

	// present cmdの設定
	{
		{
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandPool = sThreadLocal::Order().getCmdPool(sGlobal::CMD_POOL_TYPE_COMPILED, 0);
			cmd_buffer_info.commandBufferCount = sGlobal::FRAME_MAX;
			cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
			m_cmd_present_to_render = m_use_device->allocateCommandBuffers(cmd_buffer_info);
			m_cmd_render_to_present = m_use_device->allocateCommandBuffers(cmd_buffer_info);
		}

		vk::CommandBufferBeginInfo begin_info;
		begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);

		for (int i = 0; i < sGlobal::FRAME_MAX; i++)
		{
			auto& cmd = m_cmd_present_to_render[i];
			cmd.begin(begin_info);

			vk::ImageMemoryBarrier present_to_clear;
			present_to_clear.setSrcAccessMask(vk::AccessFlagBits::eMemoryRead);
			present_to_clear.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			present_to_clear.setOldLayout(vk::ImageLayout::ePresentSrcKHR);
			present_to_clear.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
			present_to_clear.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			present_to_clear.setImage(m_backbuffer[i].m_image);

			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eTransfer,
				vk::DependencyFlags(),
				nullptr, nullptr, present_to_clear);

			vk::ClearColorValue clear_color;
			clear_color.setFloat32(std::array<float, 4>{0.f, 0.f, 1.f, 0.f});
			vk::ClearDepthStencilValue clear_depth;
			clear_depth.setDepth(1.f);
			cmd.clearColorImage(m_backbuffer[i].m_image, vk::ImageLayout::eTransferDstOptimal, clear_color, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

			vk::ImageMemoryBarrier clear_to_render;
			clear_to_render.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			clear_to_render.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			clear_to_render.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
			clear_to_render.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			clear_to_render.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			clear_to_render.setImage(m_backbuffer[i].m_image);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::DependencyFlags(),
				nullptr, nullptr, clear_to_render);

			vk::ImageMemoryBarrier render_to_clear_depth;
			render_to_clear_depth.setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
			render_to_clear_depth.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			render_to_clear_depth.setOldLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
			render_to_clear_depth.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
			render_to_clear_depth.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
			render_to_clear_depth.setImage(m_depth.m_image);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eLateFragmentTests,
				vk::PipelineStageFlagBits::eTransfer,
				vk::DependencyFlags(),
				nullptr, nullptr, render_to_clear_depth);
			cmd.clearDepthStencilImage(m_depth.m_image, vk::ImageLayout::eTransferDstOptimal, clear_depth, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });

			vk::ImageMemoryBarrier clear_to_render_depth;
			clear_to_render_depth.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			clear_to_render_depth.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
			clear_to_render_depth.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
			clear_to_render_depth.setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
			clear_to_render_depth.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
			clear_to_render_depth.setImage(m_depth.m_image);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eLateFragmentTests,
				vk::DependencyFlags(),
				nullptr, nullptr, clear_to_render_depth);

			m_cmd_present_to_render[i].end();
		}

		for (int i = 0; i < sGlobal::FRAME_MAX; i++)
		{
			auto& cmd = m_cmd_render_to_present[i];
			cmd.begin(begin_info);
			vk::ImageMemoryBarrier render_to_present_barrier;
			render_to_present_barrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			render_to_present_barrier.setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
			render_to_present_barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			render_to_present_barrier.setNewLayout(vk::ImageLayout::ePresentSrcKHR);
			render_to_present_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			render_to_present_barrier.setImage(m_backbuffer[i].m_image);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::PipelineStageFlagBits::eTransfer,
				vk::DependencyFlags(),
				nullptr, nullptr, render_to_present_barrier);

			cmd.end();
		}
	}

}

uint32_t cWindow::Swapchain::swap(vk::Semaphore& semaphore)
{
	m_use_device->acquireNextImageKHR(m_swapchain_handle, 1000, semaphore, vk::Fence(), &m_backbuffer_index);
	return m_backbuffer_index;
}


