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
#include <btrlib/Loader.h>

#include <applib/sCameraManager.h>
#include <003_particle/sParticlePipeline.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\003_particle\\");
	app::App app;
	auto* camera = cCamera::sCamera::Order().create();
	camera->m_position = glm::vec3(20.f, 10.f, 30.f);
	camera->m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->m_width = 640;
	camera->m_height = 480;
	camera->m_far = 5000.f;
	camera->m_near = 0.01f;

	auto device = sThreadLocal::Order().m_device[0];
	auto pool_list = sThreadLocal::Order().getCmdPoolOnetime(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));
	std::vector<vk::CommandBuffer> render_cmds(sGlobal::FRAME_MAX);
	for (int i = 0; i < render_cmds.size(); i++)
	{
		vk::CommandBufferAllocateInfo cmd_info = vk::CommandBufferAllocateInfo()
			.setCommandPool(pool_list[i])
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);
		render_cmds[i] = device->allocateCommandBuffers(cmd_info)[0];
	}


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
	vk::MemoryPropertyFlags host_memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached;
	vk::MemoryPropertyFlags device_memory = vk::MemoryPropertyFlagBits::eDeviceLocal;
	device_memory = host_memory; // debug
	loader->m_vertex_memory.setup(device, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000*1000 * 100);
	loader->m_uniform_memory.setup(device, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 20);
	loader->m_storage_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 200);
	loader->m_staging_memory.setup(device, vk::BufferUsageFlagBits::eTransferSrc, host_memory, 1000 * 1000 * 100);
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
	executer->m_vertex_memory	= loader->m_vertex_memory;
	executer->m_uniform_memory	= loader->m_uniform_memory;
	executer->m_storage_memory	= loader->m_storage_memory;
	executer->m_staging_memory	= loader->m_staging_memory;
	cThreadPool& pool = sGlobal::Order().getThreadPool();
	executer->m_thread_pool = &pool;
	executer->m_window = &app.m_window;

	cModelPipeline model_pipeline;
	cModelRender model_render;
	cModel model;

	{
		vk::CommandBufferBeginInfo begin_info;
		begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		render_cmds[0].begin(begin_info);
		loader->m_cmd = render_cmds[0];

		sCameraManager::Order().setup(loader);
		sParticlePipeline::Order().setup(loader);

		model.load(loader.get(), "..\\..\\resource\\tiny.x");
		model_render.setup(loader, model.getResource());
		model_pipeline.setup(*loader);
		model_pipeline.addModel(&model_render);
		{
			PlayMotionDescriptor desc;
			desc.m_data = model.getResource()->getAnimation().m_motion[0];
			desc.m_play_no = 0;
			desc.m_start_time = 0.f;
			model_render.getMotionList().play(desc);

			auto& transform = model_render.getModelTransform();
			transform.m_local_scale = glm::vec3(0.002f);
			transform.m_local_rotate = glm::quat(1.f, 0.f, 0.f, 0.f);
			transform.m_local_translate = glm::vec3(0.f, 280.f, 0.f);
		}

		render_cmds[0].end();
		std::vector<vk::CommandBuffer> cmds = {
			render_cmds[0],
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


	while (true)
	{
		cStopWatch time;

		uint32_t backbuffer_index = app.m_window.getSwapchain().swap(swapbuffer_semaphore);
		{
			auto render_cmd = render_cmds[backbuffer_index];
			{
				sDebug::Order().waitFence(device.getHandle(), fence_list[backbuffer_index]);
				device->resetFences({ fence_list[backbuffer_index] });
				device->resetCommandPool(pool_list[backbuffer_index], vk::CommandPoolResetFlagBits::eReleaseResources);

				// begin cmd
				vk::CommandBufferBeginInfo begin_info;
				begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
				render_cmd.begin(begin_info);

				executer->m_cmd = render_cmd;

			}

			{
				auto* m_camera = cCamera::sCamera::Order().getCameraList()[0];
				m_camera->control(app.m_window.getInput(), 0.016f);

				sCameraManager::Order().execute(executer);
				sParticlePipeline::Order().execute(executer);

			}


			vk::ImageMemoryBarrier present_to_render_barrier;
			present_to_render_barrier.setSrcAccessMask(vk::AccessFlagBits::eMemoryRead);
			present_to_render_barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			present_to_render_barrier.setOldLayout(vk::ImageLayout::ePresentSrcKHR);
			present_to_render_barrier.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			present_to_render_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			present_to_render_barrier.setImage(app.m_window.getSwapchain().m_backbuffer_image[backbuffer_index]);

			render_cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::DependencyFlags(),
				nullptr, nullptr, present_to_render_barrier);

			model_render.execute(executer);
			model_pipeline.execute(render_cmd);

			// begin cmd render pass
			std::vector<vk::ClearValue> clearValue = {
				vk::ClearValue().setColor(vk::ClearColorValue(std::array<float, 4>{0.3f, 0.3f, 0.8f, 1.f})),
				vk::ClearValue().setDepthStencil(vk::ClearDepthStencilValue(1.f)),
			};
			vk::RenderPassBeginInfo begin_render_Info = vk::RenderPassBeginInfo()
				.setRenderPass(app.m_render_pass)
				.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(app.m_window.getClientSize().x, app.m_window.getClientSize().y)))
				.setClearValueCount(clearValue.size())
				.setPClearValues(clearValue.data())
				.setFramebuffer(app.m_framebuffer[backbuffer_index]);
			render_cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);
			// draw
			model_pipeline.draw(render_cmd);
			sParticlePipeline::Order().draw(render_cmd);
			render_cmd.endRenderPass();

			vk::ImageMemoryBarrier render_to_present_barrier;
			render_to_present_barrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			render_to_present_barrier.setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
			render_to_present_barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			render_to_present_barrier.setNewLayout(vk::ImageLayout::ePresentSrcKHR);
			render_to_present_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			render_to_present_barrier.setImage(app.m_window.getSwapchain().m_backbuffer_image[backbuffer_index]);
			render_cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::PipelineStageFlagBits::eTransfer,
				vk::DependencyFlags(),
				nullptr, nullptr, render_to_present_barrier);

			render_cmd.end();
			std::vector<vk::CommandBuffer> cmds = {
				render_cmd,
			};

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
			queue.waitIdle();
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

