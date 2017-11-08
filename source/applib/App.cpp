#include <applib/App.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cDebug.h>
#include <btrlib/Context.h>

#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>
#include <applib/sParticlePipeline.h>
#include <applib/sSystem.h>
#include <applib/GraphicsResource.h>

namespace app
{


App::App()
{

}

void App::setup(const AppDescriptor& desc)
{
	vk::Instance instance = sGlobal::Order().getVKInstance();

#if _DEBUG
	static cDebug debug(instance);
#endif

	m_gpu = desc.m_gpu;
	auto device = sGlobal::Order().getGPU(0).getDevice();
	m_cmd_pool = cCmdPool::MakeCmdPool(m_gpu);

	m_context = std::make_shared<btr::Context>();
	{
		m_context->m_gpu = m_gpu;
		m_context->m_device = device;
		m_context->m_cmd_pool = m_cmd_pool;

		vk::MemoryPropertyFlags host_memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached;
		vk::MemoryPropertyFlags device_memory = vk::MemoryPropertyFlagBits::eDeviceLocal;
		vk::MemoryPropertyFlags transfer_memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
//		device_memory = host_memory; // debug
		m_context->m_vertex_memory.setup(device, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 100);
		m_context->m_uniform_memory.setup(device, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 20);
		m_context->m_storage_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 200);
		m_context->m_staging_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc, host_memory, 1000 * 1000 * 100);
		{
			std::vector<vk::DescriptorPoolSize> pool_size(4);
			pool_size[0].setType(vk::DescriptorType::eUniformBuffer);
			pool_size[0].setDescriptorCount(20);
			pool_size[1].setType(vk::DescriptorType::eStorageBuffer);
			pool_size[1].setDescriptorCount(30);
			pool_size[2].setType(vk::DescriptorType::eCombinedImageSampler);
			pool_size[2].setDescriptorCount(30);
			pool_size[3].setType(vk::DescriptorType::eStorageImage);
			pool_size[3].setDescriptorCount(30);

			vk::DescriptorPoolCreateInfo pool_info;
			pool_info.setPoolSizeCount((uint32_t)pool_size.size());
			pool_info.setPPoolSizes(pool_size.data());
			pool_info.setMaxSets(20);
			m_context->m_descriptor_pool = device->createDescriptorPoolUnique(pool_info);

			vk::PipelineCacheCreateInfo cacheInfo = vk::PipelineCacheCreateInfo();
			m_context->m_cache = device->createPipelineCacheUnique(cacheInfo);

		}

	}

	cWindowDescriptor window_desc;
	window_desc.surface_format_request = vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	window_desc.gpu = sGlobal::Order().getGPU(0);
	window_desc.size = vk::Extent2D(desc.m_window_size.x, desc.m_window_size.y);
	window_desc.window_name = L"Vulkan Test";
	window_desc.class_name = L"VulkanMainWindow";

	auto window = std::make_shared<cWindow>();
	window->setup(m_context, window_desc);
	m_window = window;
	m_window_list.push_back(window);
	m_context->m_window = window;

	sSystem::Order().setup(m_context);
	sCameraManager::Order().setup(m_context);
	sParticlePipeline::Order().setup(m_context);
	DrawHelper::Order().setup(m_context);
	sGraphicsResource::Order().setup(m_context);
}

void App::submit(std::vector<vk::CommandBuffer>&& submit_cmds)
{
	submit_cmds.erase(std::remove_if(submit_cmds.begin(), submit_cmds.end(), [&](auto& d) { return !d; }), submit_cmds.end());

	std::vector<vk::CommandBuffer> cmds;
	cmds.reserve(32);

	for (auto& window : m_window_list)
	{
		cmds.push_back(window->getSwapchain().m_cmd_present_to_render[window->getSwapchain().m_backbuffer_index].get());
	}

	m_sync_point.wait();
	cmds.insert(cmds.end(), std::make_move_iterator(m_system_cmds.begin()), std::make_move_iterator(m_system_cmds.end()));

	cmds.insert(cmds.end(), std::make_move_iterator(submit_cmds.begin()), std::make_move_iterator(submit_cmds.end()));

	for (auto& window : m_window_list)
	{
		cmds.push_back(window->getSwapchain().m_cmd_render_to_present[window->getSwapchain().m_backbuffer_index].get());
	}

	std::vector<vk::Semaphore> swap_wait_semas(m_window_list.size());
	std::vector<vk::Semaphore> submit_wait_semas(m_window_list.size());
	std::vector<vk::SwapchainKHR> swapchains(m_window_list.size());
	std::vector<uint32_t> backbuffer_indexs(m_window_list.size());
	std::vector<vk::PipelineStageFlags> wait_pipelines(m_window_list.size());

	for (size_t i = 0; i < m_window_list.size(); i++)
	{
		auto& window = m_window_list[i];
		swap_wait_semas[i] = window->getSwapchain().m_swapbuffer_semaphore.get();
		submit_wait_semas[i] = window->getSwapchain().m_submit_semaphore.get();
		swapchains[i] = window->getSwapchain().m_swapchain_handle.get();
		backbuffer_indexs[i] = window->getSwapchain().m_backbuffer_index;
		wait_pipelines[i] = vk::PipelineStageFlagBits::eAllGraphics;
	}

	std::vector<vk::SubmitInfo> submitInfo =
	{
		vk::SubmitInfo()
		.setCommandBufferCount((uint32_t)cmds.size())
		.setPCommandBuffers(cmds.data())
		.setWaitSemaphoreCount((uint32_t)swap_wait_semas.size())
		.setPWaitSemaphores(swap_wait_semas.data())
		.setPWaitDstStageMask(wait_pipelines.data())
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
	auto& device = m_gpu.getDevice();
	for (auto& window : m_window_list)
	{
		window->getSwapchain().swap();
	}
	uint32_t backbuffer_index = m_window->getSwapchain().m_backbuffer_index;
	sDebug::Order().waitFence(device.getHandle(), m_window->getFence(backbuffer_index));
	device->resetFences({ m_window->getFence(backbuffer_index) });
	m_cmd_pool->resetPool(m_context);

	{
		auto& m_camera = cCamera::sCamera::Order().getCameraList()[0];
		m_camera->control(m_window->getInput(), 0.016f);
	}

	m_system_cmds.resize(5);
	m_sync_point.reset(5);

	{
		MAKE_THREAD_JOB(job);
		job.mJob.emplace_back(
			[&]()
		{
			m_system_cmds[0] = sSystem::Order().execute(m_context);
			m_sync_point.arrive();
		}
		);
		sGlobal::Order().getThreadPool().enque(job);
	}
	{
		MAKE_THREAD_JOB(job);
		job.mJob.emplace_back(
			[&]()
		{
			m_system_cmds[1] = sCameraManager::Order().draw(m_context);
			m_sync_point.arrive();
		}
		);
		sGlobal::Order().getThreadPool().enque(job);
	}
	{
		MAKE_THREAD_JOB(job);
		job.mJob.emplace_back(
			[&]()
		{
			m_system_cmds[4] = DrawHelper::Order().draw(m_context);
			m_sync_point.arrive();
		}
		);
		sGlobal::Order().getThreadPool().enque(job);
	}
	{
		MAKE_THREAD_JOB(job);
		job.mJob.emplace_back(
			[&]()
		{
			m_system_cmds[2] = sParticlePipeline::Order().draw(m_context);
			m_system_cmds[3] = sParticlePipeline::Order().draw(m_context);
			m_sync_point.arrive();
		}
		);
		sGlobal::Order().getThreadPool().enque(job);
	}

	{
		MAKE_THREAD_JOB(job);
		job.mJob.emplace_back(
			[&]()
		{
			m_context->m_cmd_pool->submit(m_context);
			m_sync_point.arrive();
		}
		);
		sGlobal::Order().getThreadPool().enque(job);
	}

}

void App::postUpdate()
{

	for (auto& window : m_window_list)
	{
		window->sync();
	}
	sGlobal::Order().sync();
	sCameraManager::Order().sync();
	sDeleter::Order().sync();

}

glm::uvec3 calcDipatchGroups(const glm::uvec3& num, const glm::uvec3& local_size)
{
	glm::uvec3 ret = (num + local_size - glm::uvec3(1)) / local_size;
	return ret;
}

}
