#include <applib/App.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cDebug.h>
#include <btrlib/Loader.h>

#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>


// #pragma comment(lib, "btrlib.lib")
// #pragma comment(lib, "vulkan-1.lib")

namespace app
{


App::App()
{

}

void App::setup(const cGPU& gpu)
{
	vk::Instance instance = sGlobal::Order().getVKInstance();

#if _DEBUG
	static cDebug debug(instance);
#endif

	m_gpu = gpu;
	auto device = sGlobal::Order().getGPU(0).getDevice();
	m_cmd_pool = cCmdPool::MakeCmdPool(m_gpu);

	m_loader = std::make_shared<btr::Loader>();
	{
		m_loader->m_gpu = gpu;
		m_loader->m_device = device;
		m_loader->m_cmd_pool = m_cmd_pool;

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
			m_loader->m_cache = device->createPipelineCacheUnique(cacheInfo);

		}

	}

	cWindow::CreateInfo windowInfo;
	windowInfo.surface_format_request = vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	windowInfo.gpu = sGlobal::Order().getGPU(0);
	windowInfo.size = vk::Extent2D(640, 480);
	windowInfo.window_name = L"Vulkan Test";
	windowInfo.class_name = L"VulkanMainWindow";

	auto window = std::make_shared<cWindow>();
	window->setup(m_loader, windowInfo);
	m_window = window;
	m_window_list.push_back(window);
	m_loader->m_window = window;

	m_executer = std::make_shared<btr::Executer>();
	m_executer->m_gpu = gpu;
	m_executer->m_device = device;
	m_executer->m_vertex_memory = m_loader->m_vertex_memory;
	m_executer->m_uniform_memory = m_loader->m_uniform_memory;
	m_executer->m_storage_memory = m_loader->m_storage_memory;
	m_executer->m_staging_memory = m_loader->m_staging_memory;
	m_executer->m_cmd_pool = m_cmd_pool;
	m_executer->m_window = window;

	sCameraManager::Order().setup(m_loader);
	DrawHelper::Order().setup(m_loader);

}

void App::submit(std::vector<vk::CommandBuffer>&& submit_cmds)
{
	std::vector<vk::CommandBuffer> cmds;
	cmds.reserve(32);

	for (auto& window : m_window_list)
	{
		cmds.push_back(window->getSwapchain().m_cmd_present_to_render[window->getSwapchain().m_backbuffer_index]);
	}

	for (auto& window : m_window_list)
	{
		cmds.push_back(window->getSwapchain().m_cmd_render_to_present[window->getSwapchain().m_backbuffer_index]);
	}
	cmds.push_back(sCameraManager::Order().draw(m_executer));

	cmds.insert(cmds.end(), std::make_move_iterator(submit_cmds.begin()), std::make_move_iterator(submit_cmds.end()));

	std::vector<vk::Semaphore> swap_wait_semas(m_window_list.size());
	std::vector<vk::Semaphore> submit_wait_semas(m_window_list.size());
	std::vector<vk::SwapchainKHR> swapchains(m_window_list.size());
	std::vector<uint32_t> backbuffer_indexs(m_window_list.size());

	for (size_t i = 0; i < m_window_list.size(); i++)
	{
		auto& window = m_window_list[i];
		swap_wait_semas[i] = window->getSwapchain().m_swapbuffer_semaphore.get();
		submit_wait_semas[i] = window->getSwapchain().m_submit_semaphore.get();
		swapchains[i] = window->getSwapchain().m_swapchain_handle.get();
		backbuffer_indexs[i] = window->getSwapchain().m_backbuffer_index;
	}

	vk::PipelineStageFlags wait_pipelines[] = {
		vk::PipelineStageFlagBits::eAllGraphics,
	};
	std::vector<vk::SubmitInfo> submitInfo =
	{
		vk::SubmitInfo()
		.setCommandBufferCount((uint32_t)cmds.size())
		.setPCommandBuffers(cmds.data())
		.setWaitSemaphoreCount((uint32_t)swap_wait_semas.size())
		.setPWaitSemaphores(swap_wait_semas.data())
		.setPWaitDstStageMask(wait_pipelines)
		.setSignalSemaphoreCount((uint32_t)submit_wait_semas.size())
		.setPSignalSemaphores(submit_wait_semas.data())
	};

	auto queue = m_gpu.getDevice()->getQueue(0, 0);
	queue.submit(submitInfo, m_window_list[0]->getFence(m_window_list[0]->getSwapchain().m_backbuffer_index));

	vk::PresentInfoKHR present_info = vk::PresentInfoKHR()
		.setWaitSemaphoreCount((uint32_t)submit_wait_semas.size())
		.setPWaitSemaphores(submit_wait_semas.data())
		.setSwapchainCount((uint32_t)swapchains.size())
		.setPSwapchains(swapchains.data())
		.setPImageIndices(backbuffer_indexs.data());
	queue.presentKHR(present_info);

}

void App::preUpdate()
{

}

void App::postUpdate()
{
	for (auto& window : m_window_list)
	{
		window->update();
	}
	sGlobal::Order().swap();
	sCameraManager::Order().sync();
	sDeleter::Order().swap();

}

glm::uvec3 calcDipatchGroups(const glm::uvec3& num, const glm::uvec3& local_size)
{
	glm::uvec3 ret = (num + local_size - glm::uvec3(1)) / local_size;
	return ret;
}

}
