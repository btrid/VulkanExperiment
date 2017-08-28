#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <utility>
#include <array>
#include <unordered_set>
#include <vector>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
#include <memory>
#include <filesystem>
#include <atomic>
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/BufferMemory.h>

#include <applib/App.h>
#include <applib/cModelPipeline.h>
#include <applib/cModelRender.h>
#include <applib/sCameraManager.h>

#include <btrlib/Loader.h>
#include <005_volume_rendering/VolumeRenderer.h>
#include <005_volume_rendering/DrawHelper.h>


#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
//#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\005_volume_rendering\\");
	app::App app;
	auto* camera = cCamera::sCamera::Order().create();
	camera->m_position = glm::vec3(0.f, 0.f, -4000.f);
	camera->m_target = glm::vec3(0.f, 0.f, 201.f);
	camera->m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->m_width = 640;
	camera->m_height = 480;
	camera->m_far = 50000.f;
	camera->m_near = 0.01f;
	auto device = sGlobal::Order().getGPU(0).getDevice();
	auto setup_cmd = sThreadLocal::Order().getCmdOnetime(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));

	vk::FenceCreateInfo fence_info;
	fence_info.setFlags(vk::FenceCreateFlagBits::eSignaled);
	std::vector<vk::Fence> fence_list;
	fence_list.emplace_back(device->createFence(fence_info));
	fence_list.emplace_back(device->createFence(fence_info));
	fence_list.emplace_back(device->createFence(fence_info));

	vk::SemaphoreCreateInfo semaphoreInfo = vk::SemaphoreCreateInfo();
	vk::Semaphore swapbuffer_semaphore = device->createSemaphore(semaphoreInfo);
	vk::Semaphore cmdsubmit_semaphore = device->createSemaphore(semaphoreInfo);
	vk::Queue queue = device->getQueue(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics), 0);


	std::shared_ptr<btr::Loader> loader = std::make_shared<btr::Loader>();
	loader->m_device = device;
	loader->m_render_pass = app.m_render_pass;
	loader->m_window = &app.m_window;
	vk::MemoryPropertyFlags host_memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached;
	vk::MemoryPropertyFlags device_memory = vk::MemoryPropertyFlagBits::eDeviceLocal;
	//	device_memory = host_memory; // debug
	loader->m_vertex_memory.setup(device, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 100);
	loader->m_uniform_memory.setup(device, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 20);
	loader->m_storage_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 200);
	loader->m_staging_memory.setup(device, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eStorageBuffer, host_memory, 1000 * 1000 * 100);
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
		loader->m_descriptor_pool = device->createDescriptorPool(pool_info);

		vk::PipelineCacheCreateInfo cacheInfo = vk::PipelineCacheCreateInfo();
		loader->m_cache = device->createPipelineCache(cacheInfo);

	}

	auto executer = std::make_shared<btr::Executer>();
	executer->m_device = device;
	executer->m_vertex_memory = loader->m_vertex_memory;
	executer->m_uniform_memory = loader->m_uniform_memory;
	executer->m_storage_memory = loader->m_storage_memory;
	executer->m_staging_memory = loader->m_staging_memory;
	cThreadPool& pool = sGlobal::Order().getThreadPool();
	executer->m_thread_pool = &pool;
	executer->m_window = &app.m_window;

	volumeRenderer volume_renderer;

	{
		loader->m_cmd = setup_cmd;

		vk::ImageSubresourceRange subresource_range;
		subresource_range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		subresource_range.setBaseArrayLayer(0);
		subresource_range.setLayerCount(1);
		subresource_range.setBaseMipLevel(0);
		subresource_range.setLevelCount(1);
		std::array<vk::ImageMemoryBarrier, 3+1> barrier;
		barrier[0].setSubresourceRange(subresource_range);
		barrier[0].setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
		barrier[0].setOldLayout(vk::ImageLayout::eUndefined);
		barrier[0].setNewLayout(vk::ImageLayout::ePresentSrcKHR);
		barrier[2] = barrier[1] = barrier[0];
		barrier[0].setImage(app.m_window.getSwapchain().m_backbuffer[0].m_image);
		barrier[1].setImage(app.m_window.getSwapchain().m_backbuffer[1].m_image);
		barrier[2].setImage(app.m_window.getSwapchain().m_backbuffer[2].m_image);

		vk::ImageSubresourceRange subresource_depth_range;
		subresource_depth_range.aspectMask = vk::ImageAspectFlagBits::eDepth;
		subresource_depth_range.baseArrayLayer = 0;
		subresource_depth_range.baseMipLevel = 0;
		subresource_depth_range.layerCount = 1;
		subresource_depth_range.levelCount = 1;

		barrier[3].setImage(app.m_window.getSwapchain().m_depth.m_image);
		barrier[3].setSubresourceRange(subresource_depth_range);
		barrier[3].setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		barrier[3].setOldLayout(vk::ImageLayout::eUndefined);
		barrier[3].setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

		loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::DependencyFlags(), {}, {}, barrier);


		sCameraManager::Order().setup(loader);
		DrawHelper::Order().setup(loader);
		volume_renderer.setup(loader);

		setup_cmd.end();
		std::vector<vk::CommandBuffer> cmds = {
			setup_cmd,
		};

		vk::PipelineStageFlags waitPipeline = vk::PipelineStageFlagBits::eAllGraphics;
		std::vector<vk::SubmitInfo> submitInfo =
		{
			vk::SubmitInfo()
			.setCommandBufferCount((uint32_t)cmds.size())
			.setPCommandBuffers(cmds.data())
		};
		queue.submit(submitInfo, vk::Fence());
		queue.waitIdle();

	}

	std::vector < vk::CommandBuffer > cmd_present_to_render;
	std::vector < vk::CommandBuffer > cmd_render_to_present;
	{
		{
			auto pool = sGlobal::Order().getCmdPoolSytem(0);
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandPool = pool;
			cmd_buffer_info.commandBufferCount = sGlobal::FRAME_MAX;
			cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
			cmd_present_to_render = device->allocateCommandBuffers(cmd_buffer_info);
			cmd_render_to_present = device->allocateCommandBuffers(cmd_buffer_info);
		}

		vk::CommandBufferBeginInfo begin_info;
		begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);

		for (int i = 0; i < sGlobal::FRAME_MAX; i++)
		{
			cmd_present_to_render[i].begin(begin_info);

			vk::ImageMemoryBarrier present_to_render_barrier;
			present_to_render_barrier.setSrcAccessMask(vk::AccessFlagBits::eMemoryRead);
			present_to_render_barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			present_to_render_barrier.setOldLayout(vk::ImageLayout::ePresentSrcKHR);
			present_to_render_barrier.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			present_to_render_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			present_to_render_barrier.setImage(app.m_window.getSwapchain().m_backbuffer[i].m_image);

			cmd_present_to_render[i].pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::DependencyFlags(),
				nullptr, nullptr, present_to_render_barrier);

			cmd_present_to_render[i].end();
		}
		for (int i = 0; i < sGlobal::FRAME_MAX; i++)
		{
			cmd_render_to_present[i].begin(begin_info);
			vk::ImageMemoryBarrier render_to_present_barrier;
			render_to_present_barrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			render_to_present_barrier.setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
			render_to_present_barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			render_to_present_barrier.setNewLayout(vk::ImageLayout::ePresentSrcKHR);
			render_to_present_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			render_to_present_barrier.setImage(app.m_window.getSwapchain().m_backbuffer[i].m_image);
			cmd_render_to_present[i].pipelineBarrier(
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::PipelineStageFlagBits::eTransfer,
				vk::DependencyFlags(),
				nullptr, nullptr, render_to_present_barrier);

			cmd_render_to_present[i].end();
		}
	}

	while (true)
	{
		cStopWatch time;

		uint32_t backbuffer_index = app.m_window.getSwapchain().swap(swapbuffer_semaphore);

		sDebug::Order().waitFence(device.getHandle(), fence_list[backbuffer_index]);
		device->resetFences({ fence_list[backbuffer_index] });
		for (auto& tls : sGlobal::Order().getThreadLocalList())
		{
			for (auto& pool_family : tls.m_cmd_pool_onetime)
			{
				device->resetCommandPool(pool_family[sGlobal::Order().getCurrentFrame()], vk::CommandPoolResetFlagBits::eReleaseResources);
			}
		}		

		{

			{
//				sThreadLocal::Order().
				auto* m_camera = cCamera::sCamera::Order().getCameraList()[0];
				m_camera->control(app.m_window.getInput(), 0.016f);
			}

			// draw
			struct SynchronizedPoint 
			{
				std::atomic_uint m_count;
				std::condition_variable_any m_condition;
				std::mutex m_mutex;

				SynchronizedPoint(uint32_t count)
					: m_count(count)
				{}

				void arrive()
				{
					std::unique_lock<std::mutex> lock(m_mutex);
					m_count--;
					m_condition.notify_one();
				}

				void wait()
				{
					std::unique_lock<std::mutex> lock(m_mutex);
					m_condition.wait(lock, [&]() { return m_count == 0; });
				}
			};

			SynchronizedPoint render_syncronized_point(3);
			std::vector<vk::CommandBuffer> cmds(5);
			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]()
					{
						cmds[1] = sCameraManager::Order().draw();
						render_syncronized_point.arrive();
					}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]() 
					{
						cmds[3] = volume_renderer.draw(executer);
						render_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]() {
					cmds[2] = DrawHelper::Order().draw();
					render_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}
//			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			render_syncronized_point.wait();

			cmds[0] = cmd_present_to_render[backbuffer_index];
			cmds[4] = cmd_render_to_present[backbuffer_index];

			vk::PipelineStageFlags waitPipeline = vk::PipelineStageFlagBits::eAllGraphics;
			std::vector<vk::SubmitInfo> submitInfo =
			{
				vk::SubmitInfo()
				.setCommandBufferCount((uint32_t)cmds.size())
				.setPCommandBuffers(cmds.data())
				.setWaitSemaphoreCount(1)
				.setPWaitSemaphores(&swapbuffer_semaphore)
				.setPWaitDstStageMask(&waitPipeline)
				.setSignalSemaphoreCount(1)
				.setPSignalSemaphores(&cmdsubmit_semaphore)
			};
			queue.submit(submitInfo, fence_list[backbuffer_index]);
			vk::PresentInfoKHR present_info = vk::PresentInfoKHR()
				.setWaitSemaphoreCount(1)
				.setPWaitSemaphores(&cmdsubmit_semaphore)
				.setSwapchainCount(1)
				.setPSwapchains(&app.m_window.getSwapchain().m_swapchain_handle)
				.setPImageIndices(&backbuffer_index);
			queue.presentKHR(present_info);
		}

		app.m_window.update(sGlobal::Order().getThreadPool());
		sGlobal::Order().swap();
		printf("%6.3fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

