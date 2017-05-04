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
#include <btrlib/Singleton.h>
#include <btrlib/sValidationLayer.h>
#include <btrlib/cWindow.h>
#include <btrlib/cThreadPool.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/BufferMemory.h>

#include <applib/App.h>

//#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
//#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")

int main()
{
	btr::setResourcePath("..\\..\\resource\\003_particle\\");

	App app;

	auto* camera = cCamera::sCamera::Order().create();
	camera->m_position = glm::vec3(0.f, -500.f, -800.f);
	camera->m_target = glm::vec3(0.f, -100.f, 0.f);
	camera->m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->m_width = 640;
	camera->m_height = 480;
	camera->m_far = 100000.f;
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
	while (true)
	{
		cStopWatch time;
		auto* m_camera = cCamera::sCamera::Order().getCameraList()[0];
		m_camera->control(app.m_window.getInput(), 0.016f);

		uint32_t backbuffer_index = app.m_window.getSwapchain().swap(swapbuffer_semaphore);
		{
			auto render_cmd = render_cmds[backbuffer_index];
			sDebug::Order().waitFence(device.getHandle(), fence_list[backbuffer_index]);
			device->resetFences({ fence_list[backbuffer_index] });
			device->resetCommandPool(pool_list[backbuffer_index], vk::CommandPoolResetFlagBits::eReleaseResources);

			// begin cmd
			vk::CommandBufferBeginInfo begin_info;
			begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
			render_cmd.begin(begin_info);

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

