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
#include <btrlib/Singleton.h>
#include <btrlib/sValidationLayer.h>
#include <btrlib/cWindow.h>
//#include "Camera.h"
#include <btrlib/cThreadPool.h>
#include <btrlib/cDebug.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cStopWatch.h>

#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "btrlib.lib")

int main()
{

	sWindow& w = sWindow::Order();
	vk::Instance instance = sGlobal::Order().getVKInstance();

#if _DEBUG
	cDebug debug(instance);
#endif

 	cWindow window;
	cWindow::CreateInfo windowInfo;
	windowInfo.surface_format_request = vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear};
	windowInfo.gpu = sGlobal::Order().getGPU(0);
	windowInfo.size = vk::Extent2D(640, 480);
	windowInfo.window_name = L"Vulkan Test";
	windowInfo.class_name = L"VulkanMainWindow";

	window.setup(windowInfo);

	cGPU& gpu = sGlobal::Order().getGPU(0);
	cDevice device = gpu.getDevice(vk::QueueFlagBits::eGraphics)[0];
	cDevice other_device = gpu.getDevice(vk::QueueFlagBits::eTransfer)[0];
	vk::Queue queue = device->getQueue(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics), 0);


	// setup用コマンドバッファ
	vk::CommandPool cmd_pool;
	{
		vk::CommandPoolCreateInfo poolInfo = vk::CommandPoolCreateInfo()
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			.setQueueFamilyIndex(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));
		cmd_pool = device->createCommandPool(poolInfo);
	}

	std::vector<vk::CommandBuffer> present_cmd;
	{
		// present barrier cmd
		vk::CommandBufferAllocateInfo cmd_info = vk::CommandBufferAllocateInfo()
			.setCommandPool(cmd_pool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount((uint32_t)window.getSwapchain().m_backbuffer_image.size());
		present_cmd = device->allocateCommandBuffers(cmd_info);

		for (size_t i = 0; i < present_cmd.size(); i++)
		{
			auto& cmd = present_cmd[i];
			vk::CommandBufferBeginInfo cmd_begin_info = vk::CommandBufferBeginInfo()
				.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
			cmd.begin(cmd_begin_info);

			vk::ImageMemoryBarrier present_barrier = vk::ImageMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlags())
				.setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setOldLayout(vk::ImageLayout::eUndefined)
				.setNewLayout(vk::ImageLayout::eTransferDstOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })
				.setImage(window.getSwapchain().m_backbuffer_image[i]);

			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eTransfer,
				vk::DependencyFlags(),
				0, nullptr, // No memory barriers,
				0, nullptr, // No buffer barriers,
				1, &present_barrier);

			cmd.end();
		}
	}

	std::vector<vk::CommandBuffer> clear_cmd;
	{
		// clear cmd
		vk::CommandBufferAllocateInfo cmd_info = vk::CommandBufferAllocateInfo()
			.setCommandPool(cmd_pool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount((uint32_t)window.getSwapchain().m_backbuffer_image.size());
		clear_cmd = device->allocateCommandBuffers(cmd_info);
		for (size_t i = 0; i < clear_cmd.size(); i++)
		{
			auto& cmd = clear_cmd[i];
			vk::CommandBufferBeginInfo cmd_begin_info = vk::CommandBufferBeginInfo()
				.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
			cmd.begin(cmd_begin_info);

			vk::ClearColorValue color = vk::ClearColorValue().setFloat32({ 0.2f, 0.2f, 0.8f, 1.f });
			vk::ImageSubresourceRange range = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
			cmd.clearColorImage(window.getSwapchain().m_backbuffer_image[i], vk::ImageLayout::eTransferDstOptimal, color, range);

			vk::ImageMemoryBarrier clear_barrier = vk::ImageMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setDstAccessMask(vk::AccessFlagBits::eMemoryRead)
				.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
				.setNewLayout(vk::ImageLayout::ePresentSrcKHR)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })
				.setImage(window.getSwapchain().m_backbuffer_image[i]);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::DependencyFlags(),
				0, nullptr, 
				0, nullptr,
				1, &clear_barrier);
			cmd.end();
		}
	}


	vk::SemaphoreCreateInfo semaphoreInfo = vk::SemaphoreCreateInfo();
	vk::Semaphore swapbuffer_semaphore = device->createSemaphore(semaphoreInfo);
	vk::Semaphore cmdsubmit_semaphore = device->createSemaphore(semaphoreInfo);
	while (true)
	{
		uint32_t backbuffer_index = window.getSwapchain().swap(swapbuffer_semaphore);
		{
			std::vector<vk::CommandBuffer> cmds = {
				present_cmd[backbuffer_index],
				clear_cmd[backbuffer_index],
			};
			vk::PipelineStageFlags waitPipeline = vk::PipelineStageFlagBits::eTransfer;
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
			queue.submit(submitInfo, vk::Fence());
			vk::PresentInfoKHR present_info = vk::PresentInfoKHR()
				.setWaitSemaphoreCount(1)
				.setPWaitSemaphores(&cmdsubmit_semaphore)
				.setSwapchainCount(1)
				.setPSwapchains(&window.getSwapchain().m_swapchain_handle)
				.setPImageIndices(&backbuffer_index);
				queue.presentKHR(present_info);
		}

		window.update(sGlobal::Order().getThreadPool());
		sGlobal::Order().swap();
	}

	return 0;
}

