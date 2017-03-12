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
#include <btrlib/sVulkan.h>

#include <002_model/cModelRenderer.h>
#include <002_model/cModelRender.h>
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "btrlib.lib")



int main()
{
	btr::setResourcePath("..\\..\\resource\\002_model\\");
	sWindow& w = sWindow::Order();
	vk::Instance instance = sVulkan::Order().getVKInstance();

#if _DEBUG
	cDebug debug(instance);
#endif

	cWindow window;
	cWindow::CreateInfo windowInfo;
	windowInfo.surface_format_request = vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	windowInfo.gpu = sVulkan::Order().getGPU(0);
	windowInfo.size = vk::Extent2D(640, 480);
	windowInfo.window_name = L"Vulkan Test";
	windowInfo.class_name = L"VulkanMainWindow";

	window.setup(windowInfo);


	cGPU& gpu = sVulkan::Order().getGPU(0);
	cDevice device = gpu.getDevice(vk::QueueFlagBits::eGraphics)[0];
	vk::Queue queue = device->getQueue(device.getQueueFamilyIndex(), 0);

	// setup用コマンドバッファ
	vk::CommandPool cmd_pool;
	{
		vk::CommandPoolCreateInfo poolInfo = vk::CommandPoolCreateInfo()
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			.setQueueFamilyIndex(device.getQueueFamilyIndex());

		cmd_pool = device->createCommandPool(poolInfo);
	}

	std::vector<vk::CommandBuffer> render_to_present_cmd;
	{
		// present barrier cmd
		vk::CommandBufferAllocateInfo cmd_info = vk::CommandBufferAllocateInfo()
			.setCommandPool(cmd_pool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount((uint32_t)window.getSwapchain().m_backbuffer.size());
		render_to_present_cmd = device->allocateCommandBuffers(cmd_info);

		for (size_t i = 0; i < render_to_present_cmd.size(); i++)
		{
			auto& cmd = render_to_present_cmd[i];
			vk::CommandBufferBeginInfo cmd_begin_info = vk::CommandBufferBeginInfo()
				.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
			cmd.begin(cmd_begin_info);

			vk::ImageMemoryBarrier present_barrier = vk::ImageMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
				.setDstAccessMask(vk::AccessFlagBits::eMemoryRead)
				.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setNewLayout(vk::ImageLayout::ePresentSrcKHR)
				.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })
				.setImage(window.getSwapchain().m_backbuffer[i].image);

			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::DependencyFlags(),
				0, nullptr, // No memory barriers,
				0, nullptr, // No buffer barriers,
				1, &present_barrier);

			cmd.end();
		}
	}

// 	std::vector<vk::CommandBuffer> clear_cmd;
// 	{
// 		// clear cmd
// 		vk::CommandBufferAllocateInfo cmd_info = vk::CommandBufferAllocateInfo()
// 			.setCommandPool(cmd_pool)
// 			.setLevel(vk::CommandBufferLevel::ePrimary)
// 			.setCommandBufferCount((uint32_t)window.getSwapchain().m_backbuffer.size());
// 		clear_cmd = device->allocateCommandBuffers(cmd_info);
// 		for (size_t i = 0; i < clear_cmd.size(); i++)
// 		{
// 			auto& cmd = clear_cmd[i];
// 			vk::CommandBufferBeginInfo cmd_begin_info = vk::CommandBufferBeginInfo()
// 				.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
// 			cmd.begin(cmd_begin_info);
// 
// 			vk::ClearColorValue color = vk::ClearColorValue().setFloat32({ 0.2f, 0.2f, 0.8f, 1.f });
// 			vk::ImageSubresourceRange range = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
// 			cmd.clearColorImage(window.getSwapchain().m_backbuffer[i].image, vk::ImageLayout::eTransferDstOptimal, color, range);
// 
// 			vk::ImageMemoryBarrier clear_barrier = vk::ImageMemoryBarrier()
// 				.setSrcAccessMask(vk::AccessFlagBits::eMemoryRead)
// 				.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
// 				.setOldLayout(vk::ImageLayout::ePresentSrcKHR)
// 				.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
// 				.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })
// 				.setImage(window.getSwapchain().m_backbuffer[i].image);
// 			cmd.pipelineBarrier(
// 				vk::PipelineStageFlagBits::eTopOfPipe,
// 				vk::PipelineStageFlagBits::eBottomOfPipe,
// 				vk::DependencyFlags(),
// 				0, nullptr,
// 				0, nullptr,
// 				1, &clear_barrier);
// 			cmd.end();
// 		}
// 	}

	// clear cmd
	vk::CommandBufferAllocateInfo cmd_info = vk::CommandBufferAllocateInfo()
		.setCommandPool(cmd_pool)
		.setLevel(vk::CommandBufferLevel::ePrimary)
		.setCommandBufferCount(1);

	vk::SemaphoreCreateInfo semaphoreInfo = vk::SemaphoreCreateInfo();
	vk::Semaphore swapbuffer_semaphore = device->createSemaphore(semaphoreInfo);
	vk::Semaphore cmdsubmit_semaphore = device->createSemaphore(semaphoreInfo);

#if 1
	std::shared_ptr<std::promise<std::unique_ptr<ModelRender>>> task = std::make_shared<std::promise<std::unique_ptr<ModelRender>>>();
	auto modelFuture = task->get_future();
	{
		cThreadJob job;
		auto load = [task]()
		{
			auto model = std::make_unique<ModelRender>();
			model->load(btr::getResourcePath() + "tiny.x");
			task->set_value(std::move(model));
		};
		job.mFinish = load;
		sVulkan::Order().getThreadPool().enque(std::move(job));
	}

	while (modelFuture.valid() && modelFuture.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
		printf("wait...\n");
	}

	auto model = modelFuture.get();
#else
	std::unique_ptr<cModel> model = std::make_unique<cModel>();
	model->load(btr::getResourcePath() + "002_model\\" + "tiny.x");
#endif
	auto* camera = cCamera::sCamera::Order().create();
	camera->mPosition = glm::vec3(0.f, 0.f, -200.f);
	camera->mUp = glm::vec3(0.f, 1.f, 0.f);
	camera->mWidth = 640;
	camera->mHeight = 480;
	camera->mFar = 1000.f;
	camera->mNear = 0.01f;
	vk::RenderPass render_pass;
	// レンダーパス
	{
		// sub pass
		std::vector<vk::AttachmentReference> colorRef =
		{
			vk::AttachmentReference()
			.setAttachment(0)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
		};

		vk::SubpassDescription subpass;
		subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
		subpass.setInputAttachmentCount(0);
		subpass.setPInputAttachments(nullptr);
		subpass.setColorAttachmentCount(colorRef.size());
		subpass.setPColorAttachments(colorRef.data());
//		subpass.setPDepthStencilAttachment(&depthRef);

		// render pass
		std::vector<vk::AttachmentDescription> attachDescription = {
			// color1
			vk::AttachmentDescription()
			.setFormat(window.getSwapchain().m_surface_format.format)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),

		};
		vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
			.setAttachmentCount(attachDescription.size())
			.setPAttachments(attachDescription.data())
			.setSubpassCount(1)
			.setPSubpasses(&subpass);

		render_pass = device->createRenderPass(renderpass_info);
	}

	std::vector<vk::Framebuffer> framebuffer;
	{
		std::vector<vk::ImageView> view(1);

		vk::FramebufferCreateInfo framebuffer_info;
		framebuffer_info.setRenderPass(render_pass);
		framebuffer_info.setAttachmentCount(view.size());
		framebuffer_info.setPAttachments(view.data());
		framebuffer_info.setWidth(window.getClientSize().x);
		framebuffer_info.setHeight(window.getClientSize().y);
		framebuffer_info.setLayers(1);

		framebuffer.resize(2);
		for (uint32_t i = 0; i < 2; i++) {
			view[0] = window.getSwapchain().m_backbuffer[i].view;
			framebuffer[i] = device->createFramebuffer(framebuffer_info);
		}
	}

	cModelRenderer renderer;
	renderer.setup(render_pass);
	renderer.addModel(model.get());

	while (true)
	{
		uint32_t backbuffer_index = window.getSwapchain().swap(swapbuffer_semaphore);
		{
			auto pool = sThreadData::Order().getCmdPoolOnetime(device.getQueueFamilyIndex());
			vk::CommandBufferAllocateInfo cmd_info = vk::CommandBufferAllocateInfo()
				.setCommandPool(pool)
				.setLevel(vk::CommandBufferLevel::ePrimary)
				.setCommandBufferCount(1);
			auto render_cmd = device->allocateCommandBuffers(cmd_info)[0];

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
			present_to_render_barrier.setImage(window.getSwapchain().m_backbuffer[backbuffer_index].image);

			render_cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eAllCommands,
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::DependencyFlags(),
				0, nullptr, // No memory barriers,
				0, nullptr, // No buffer barriers,
				1, &present_to_render_barrier);

			// begin cmd render pass
			std::vector<vk::ClearValue> clearValue = {
				vk::ClearValue().setColor(vk::ClearColorValue(std::array<float, 4>{0.7f, 0.7f, 0.7f, 1.f})),
			};
			vk::RenderPassBeginInfo begin_render_Info = vk::RenderPassBeginInfo()
				.setRenderPass(render_pass)
				.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(window.getClientSize().x, window.getClientSize().y)))
				.setClearValueCount(clearValue.size())
				.setPClearValues(clearValue.data())
				.setFramebuffer(framebuffer[backbuffer_index]);
			render_cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			// draw
			renderer.draw(render_cmd);
			render_cmd.endRenderPass();

			vk::ImageMemoryBarrier render_to_present_barrier;
			render_to_present_barrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			render_to_present_barrier.setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
			render_to_present_barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			render_to_present_barrier.setNewLayout(vk::ImageLayout::ePresentSrcKHR);
			render_to_present_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			render_to_present_barrier.setImage(window.getSwapchain().m_backbuffer[backbuffer_index].image);

			render_cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eAllCommands,
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::DependencyFlags(),
				0, nullptr, // No memory barriers,
				0, nullptr, // No buffer barriers,
				1, &render_to_present_barrier);

			std::vector<vk::CommandBuffer> cmds = {
				render_cmd,
//				present_cmd[backbuffer_index],
//				clear_cmd[backbuffer_index],
			};

			render_cmd.end();

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

		window.update();
		sVulkan::Order().swap();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

	}

	return 0;
}

