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
#include <btrlib/cThreadPool.h>
#include <btrlib/cDebug.h>
#include <btrlib/sGlobal.h>

#include <002_model/cModelRenderer.h>
#include <002_model/cModelRender.h>
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "FreeImage.lib")

int main()
{
	btr::setResourcePath("..\\..\\resource\\002_model\\");
	sWindow& w = sWindow::Order();
	vk::Instance instance = sGlobal::Order().getVKInstance();

#if _DEBUG
	cDebug debug(instance);
#endif

	cWindow window;
	cWindow::CreateInfo windowInfo;
	windowInfo.surface_format_request = vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	windowInfo.gpu = sGlobal::Order().getGPU(0);
	windowInfo.size = vk::Extent2D(640, 480);
	windowInfo.window_name = L"Vulkan Test";
	windowInfo.class_name = L"VulkanMainWindow";

	window.setup(windowInfo);


	cGPU& gpu = sGlobal::Order().getGPU(0);
	cDevice device = gpu.getDevice(vk::QueueFlagBits::eGraphics)[0];
	vk::Queue queue = device->getQueue(device.getQueueFamilyIndex(), 0);

	// setup用コマンドバッファ
	vk::CommandPool setup_cmd_pool;
	vk::CommandPool cmd_pool;
	{
		vk::CommandPoolCreateInfo poolInfo = vk::CommandPoolCreateInfo()
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			.setQueueFamilyIndex(device.getQueueFamilyIndex());

		cmd_pool = device->createCommandPool(poolInfo);
		setup_cmd_pool = device->createCommandPool(poolInfo);
	}
	std::vector<vk::ImageView> backbuffer_view(window.getSwapchain().getSwapchainNum());
	vk::CommandBufferAllocateInfo setup_cmd_info;
	setup_cmd_info.setCommandPool(setup_cmd_pool);
	setup_cmd_info.setLevel(vk::CommandBufferLevel::ePrimary);
	setup_cmd_info.setCommandBufferCount(1);
	auto setup_cmd = device->allocateCommandBuffers(setup_cmd_info)[0];
	{
		vk::CommandBufferBeginInfo cmd_begin_info;
		cmd_begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
		setup_cmd.begin(cmd_begin_info);
		for (uint32_t i = 0; i < window.getSwapchain().getSwapchainNum(); i++)
		{
			auto subresourceRange = vk::ImageSubresourceRange()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setBaseArrayLayer(0)
				.setLayerCount(1)
				.setBaseMipLevel(0)
				.setLevelCount(1);

			// ばりあ
			vk::ImageMemoryBarrier barrier = vk::ImageMemoryBarrier()
				.setImage(window.getSwapchain().m_backbuffer_image[i])
				.setSubresourceRange(subresourceRange)
				.setDstAccessMask(vk::AccessFlagBits::eMemoryRead)
				.setOldLayout(vk::ImageLayout::eUndefined)
				.setNewLayout(vk::ImageLayout::ePresentSrcKHR)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				;
			setup_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &barrier);

			vk::ImageViewCreateInfo backbuffer_view_info;
			backbuffer_view_info.setImage(window.getSwapchain().m_backbuffer_image[i]);
			backbuffer_view_info.setFormat(window.getSwapchain().m_surface_format.format);
			backbuffer_view_info.setViewType(vk::ImageViewType::e2D);
			backbuffer_view_info.setComponents(vk::ComponentMapping{
				vk::ComponentSwizzle::eR,
				vk::ComponentSwizzle::eG,
				vk::ComponentSwizzle::eB,
				vk::ComponentSwizzle::eA,
			});
			backbuffer_view_info.setSubresourceRange(subresourceRange);
			backbuffer_view[i] = device->createImageView(backbuffer_view_info);

		}
		setup_cmd.end();

		vk::SubmitInfo setup_submit_info;
		setup_submit_info.setCommandBufferCount(1);
		setup_submit_info.setPCommandBuffers(&setup_cmd);
		queue.submit({ setup_submit_info }, vk::Fence());
		queue.waitIdle();
	}

	std::vector<vk::CommandBuffer> render_to_present_cmd;
	{
		// present barrier cmd
		vk::CommandBufferAllocateInfo cmd_info = vk::CommandBufferAllocateInfo()
			.setCommandPool(cmd_pool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount((uint32_t)window.getSwapchain().m_backbuffer_image.size());
		render_to_present_cmd = device->allocateCommandBuffers(cmd_info);

		for (size_t i = 0; i < render_to_present_cmd.size(); i++)
		{
			auto& cmd = render_to_present_cmd[i];
			vk::CommandBufferBeginInfo cmd_begin_info = vk::CommandBufferBeginInfo()
				.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
			cmd.begin(cmd_begin_info);

			vk::ImageMemoryBarrier present_barrier;
			present_barrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			present_barrier.setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
			present_barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			present_barrier.setNewLayout(vk::ImageLayout::ePresentSrcKHR);
			present_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			present_barrier.setImage(window.getSwapchain().m_backbuffer_image[i]);

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

	vk::SemaphoreCreateInfo semaphoreInfo = vk::SemaphoreCreateInfo();
	vk::Semaphore swapbuffer_semaphore = device->createSemaphore(semaphoreInfo);
	vk::Semaphore cmdsubmit_semaphore = device->createSemaphore(semaphoreInfo);

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
		sGlobal::Order().getThreadPool().enque(std::move(job));
	}

	while (modelFuture.valid() && modelFuture.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
		printf("wait...\n");
	}

	auto model = modelFuture.get();

	auto* camera = cCamera::sCamera::Order().create();
	camera->m_position = glm::vec3(0.f, -500.f, -800.f);
	camera->m_target = glm::vec3(0.f, -100.f, 0.f);
	camera->m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->m_width = 640;
	camera->m_height = 480;
	camera->m_far = 10000.f;
	camera->m_near = 0.0f;


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

		framebuffer.resize(backbuffer_view.size());
		for (size_t i = 0; i < backbuffer_view.size(); i++) {
			view[0] = backbuffer_view[i];
			framebuffer[i] = device->createFramebuffer(framebuffer_info);
		}
	}
	cModelRenderer renderer;
	renderer.setup(render_pass);
	renderer.addModel(model.get());
// 	auto model2 = std::make_unique<ModelRender>();
// 	model2->load(btr::getResourcePath() + "tiny.x");
// 	renderer.addModel(model2.get());

	auto pool_list = sThreadLocal::Order().getCmdPoolOnetime(device.getQueueFamilyIndex());
	std::vector<vk::CommandBuffer> render_cmds(3);
	for (int i = 0; i < 3; i++)
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
	while (true)
	{

		auto* m_camera = cCamera::sCamera::Order().getCameraList()[0];
		m_camera->control(window.getInput(), 0.016f);

		uint32_t backbuffer_index = window.getSwapchain().swap(swapbuffer_semaphore);
		{
			auto render_cmd = render_cmds[backbuffer_index];
			device->waitForFences(1, &fence_list[backbuffer_index], true, 0xffffffffu);
			device->resetFences({ fence_list[backbuffer_index] });
			device->resetCommandPool(pool_list[backbuffer_index], vk::CommandPoolResetFlagBits::eReleaseResources);

			// begin cmd
			vk::CommandBufferBeginInfo begin_info;
			begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
			render_cmd.begin(begin_info);

			renderer.execute(render_cmd);

			vk::ImageMemoryBarrier present_to_render_barrier;
			present_to_render_barrier.setSrcAccessMask(vk::AccessFlagBits::eMemoryRead);
			present_to_render_barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			present_to_render_barrier.setOldLayout(vk::ImageLayout::ePresentSrcKHR);
			present_to_render_barrier.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			present_to_render_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			present_to_render_barrier.setImage(window.getSwapchain().m_backbuffer_image[backbuffer_index]);

			render_cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::DependencyFlags(),
				0, nullptr, // No memory barriers,
				0, nullptr, // No buffer barriers,
				1, &present_to_render_barrier);

			// begin cmd render pass
			std::vector<vk::ClearValue> clearValue = {
				vk::ClearValue().setColor(vk::ClearColorValue(std::array<float, 4>{0.3f, 0.3f, 0.8f, 1.f})),
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
			render_to_present_barrier.setImage(window.getSwapchain().m_backbuffer_image[backbuffer_index]);
			render_cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::PipelineStageFlagBits::eTransfer,
				vk::DependencyFlags(),
				0, nullptr, // No memory barriers,
				0, nullptr, // No buffer barriers,
				1, &render_to_present_barrier);

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
				.setPSwapchains(&window.getSwapchain().m_swapchain_handle)
				.setPImageIndices(&backbuffer_index);
			queue.presentKHR(present_info);
		}

		window.update(sGlobal::Order().getThreadPool());
//		sGlobal::Order().getThreadPool().wait();
		sGlobal::Order().swap();

	}

	return 0;
}

