#include <applib/App.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cDebug.h>
#include <btrlib/Loader.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "vulkan-1.lib")

namespace app
{


App::App()
{

}

void App::setup(std::shared_ptr<btr::Loader>& loader)
{
	sWindow& w = sWindow::Order();
	vk::Instance instance = sGlobal::Order().getVKInstance();

#if _DEBUG
	static cDebug debug(instance);
#endif

	cWindow::CreateInfo windowInfo;
	windowInfo.surface_format_request = vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	windowInfo.gpu = sGlobal::Order().getGPU(0);
	windowInfo.size = vk::Extent2D(640, 480);
	windowInfo.window_name = L"Vulkan Test";
	windowInfo.class_name = L"VulkanMainWindow";

	m_window = std::make_shared<cWindow>();
	m_window->setup(loader, windowInfo);

	// backbufferÇÃèâä˙âª
	vk::ImageSubresourceRange subresource_range;
	subresource_range.setAspectMask(vk::ImageAspectFlagBits::eColor);
	subresource_range.setBaseArrayLayer(0);
	subresource_range.setLayerCount(1);
	subresource_range.setBaseMipLevel(0);
	subresource_range.setLevelCount(1);
	std::vector<vk::ImageMemoryBarrier> barrier(m_window->getSwapchain().getBackbufferNum() + 1);
	barrier[0].setSubresourceRange(subresource_range);
	barrier[0].setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
	barrier[0].setOldLayout(vk::ImageLayout::eUndefined);
	barrier[0].setNewLayout(vk::ImageLayout::ePresentSrcKHR);
	barrier[0].setImage(m_window->getSwapchain().m_backbuffer[0].m_image);
	for (uint32_t i = 1 ; i<m_window->getSwapchain().getBackbufferNum(); i++)
	{
		barrier[i] = barrier[0];
		barrier[i].setImage(m_window->getSwapchain().m_backbuffer[i].m_image);
	}

	vk::ImageSubresourceRange subresource_depth_range;
	subresource_depth_range.aspectMask = vk::ImageAspectFlagBits::eDepth;
	subresource_depth_range.baseArrayLayer = 0;
	subresource_depth_range.baseMipLevel = 0;
	subresource_depth_range.layerCount = 1;
	subresource_depth_range.levelCount = 1;

	barrier.back().setImage(m_window->getSwapchain().m_depth.m_image);
	barrier.back().setSubresourceRange(subresource_depth_range);
	barrier.back().setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
	barrier.back().setOldLayout(vk::ImageLayout::eUndefined);
	barrier.back().setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

	loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::DependencyFlags(), {}, {}, barrier);

	m_cmd_pool = cCmdPool::MakeCmdPool(m_gpu);
	return;

	// ñ¢é¿ëï
	m_loader = std::make_shared<btr::Loader>();
	m_loader->m_window = m_window;
	auto device = sGlobal::Order().getGPU(0).getDevice();
	m_loader->m_device = device;
	vk::MemoryPropertyFlags host_memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached;
	vk::MemoryPropertyFlags device_memory = vk::MemoryPropertyFlagBits::eDeviceLocal;
	//	device_memory = host_memory; // debug
	m_loader->m_vertex_memory.setup(device, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 100);
	m_loader->m_uniform_memory.setup(device, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 20);
	m_loader->m_storage_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 200);
	m_loader->m_staging_memory.setup(device, vk::BufferUsageFlagBits::eTransferSrc, host_memory, 1000 * 1000 * 100);
	{
		std::vector<vk::DescriptorPoolSize> pool_size(4);
		pool_size[0].setType(vk::DescriptorType::eUniformBuffer);
		pool_size[0].setDescriptorCount(10);
		pool_size[1].setType(vk::DescriptorType::eStorageBuffer);
		pool_size[1].setDescriptorCount(20);
		pool_size[2].setType(vk::DescriptorType::eCombinedImageSampler);
		pool_size[2].setDescriptorCount(10);
		pool_size[3].setType(vk::DescriptorType::eStorageImage);
		pool_size[3].setDescriptorCount(10);

		vk::DescriptorPoolCreateInfo pool_info;
		pool_info.setPoolSizeCount((uint32_t)pool_size.size());
		pool_info.setPPoolSizes(pool_size.data());
		pool_info.setMaxSets(20);
		m_loader->m_descriptor_pool = device->createDescriptorPoolUnique(pool_info);

		vk::PipelineCacheCreateInfo cacheInfo = vk::PipelineCacheCreateInfo();
		m_loader->m_cache.get() = device->createPipelineCache(cacheInfo);

	}

}

glm::uvec3 calcDipatchGroups(const glm::uvec3& num, const glm::uvec3& local_size)
{
	glm::uvec3 ret = (num + local_size - glm::uvec3(1)) / local_size;
	return ret;
}

}
