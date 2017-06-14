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
#include <btrlib/cStopWatch.h>
#include <btrlib/BufferMemory.h>
#include <002_model/cModelPipeline.h>
#include <002_model/cModelRender.h>
#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")

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
	vk::Queue queue = device->getQueue(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics), 0);

	// setup用コマンドバッファ
	vk::CommandPool setup_cmd_pool;
	vk::CommandPool cmd_pool;
	{
		vk::CommandPoolCreateInfo poolInfo = vk::CommandPoolCreateInfo()
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			.setQueueFamilyIndex(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));

		cmd_pool = device->createCommandPool(poolInfo);
		setup_cmd_pool = device->createCommandPool(poolInfo);
	}
	std::vector<vk::ImageView> backbuffer_view(window.getSwapchain().getSwapchainNum());
	vk::CommandBufferAllocateInfo setup_cmd_info;
	setup_cmd_info.setCommandPool(setup_cmd_pool);
	setup_cmd_info.setLevel(vk::CommandBufferLevel::ePrimary);
	setup_cmd_info.setCommandBufferCount(1);
	auto setup_cmd = device->allocateCommandBuffers(setup_cmd_info)[0];
	vk::CommandBufferBeginInfo cmd_begin_info;
	cmd_begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
	setup_cmd.begin(cmd_begin_info);
	{
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
	}

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
		vk::AttachmentReference depth_ref;
		depth_ref.setAttachment(1);
		depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

		vk::SubpassDescription subpass;
		subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
		subpass.setInputAttachmentCount(0);
		subpass.setPInputAttachments(nullptr);
		subpass.setColorAttachmentCount(colorRef.size());
		subpass.setPColorAttachments(colorRef.data());
		subpass.setPDepthStencilAttachment(&depth_ref);
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
			vk::AttachmentDescription()
			.setFormat(vk::Format::eD32Sfloat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),

		};
		vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
			.setAttachmentCount(attachDescription.size())
			.setPAttachments(attachDescription.data())
			.setSubpassCount(1)
			.setPSubpasses(&subpass);

		render_pass = device->createRenderPass(renderpass_info);
	}

	vk::Image depth_image;
	vk::DeviceMemory depth_memory;
	vk::ImageView depth_view;
	{
		// イメージ生成
		vk::ImageCreateInfo depth_info;
		depth_info.format = vk::Format::eD32Sfloat;
		depth_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
		depth_info.arrayLayers = 1;
		depth_info.mipLevels = 1;
		depth_info.extent.width = window.getClientSize().x;
		depth_info.extent.height = window.getClientSize().y;
		depth_info.extent.depth = 1;
		depth_info.imageType = vk::ImageType::e2D;
		depth_info.initialLayout = vk::ImageLayout::eUndefined;
		depth_info.queueFamilyIndexCount = device.getQueueFamilyIndex().size();
		depth_info.pQueueFamilyIndices = device.getQueueFamilyIndex().data();
		depth_image = device->createImage(depth_info);

		// メモリ確保
		auto memory_request = device->getImageMemoryRequirements(depth_image);
		uint32_t memory_index = cGPU::Helper::getMemoryTypeIndex(device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::MemoryAllocateInfo memory_info;
		memory_info.allocationSize = memory_request.size;
		memory_info.memoryTypeIndex = memory_index;
		depth_memory = device->allocateMemory(memory_info);

		device->bindImageMemory(depth_image, depth_memory, 0);

		vk::ImageViewCreateInfo depth_view_info;
		depth_view_info.format = depth_info.format;
		depth_view_info.image = depth_image;
		depth_view_info.viewType = vk::ImageViewType::e2D;
		depth_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
		depth_view_info.subresourceRange.baseArrayLayer = 0;
		depth_view_info.subresourceRange.baseMipLevel = 0;
		depth_view_info.subresourceRange.layerCount = 1;
		depth_view_info.subresourceRange.levelCount = 1;
		depth_view = device->createImageView(depth_view_info);

		vk::ImageMemoryBarrier to_render_barrier;
		to_render_barrier.setImage(depth_image);
		to_render_barrier.setSubresourceRange(depth_view_info.subresourceRange);
		to_render_barrier.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		to_render_barrier.setOldLayout(vk::ImageLayout::eUndefined);
		to_render_barrier.setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
		to_render_barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
		to_render_barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

		setup_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &to_render_barrier);
	}
	std::vector<vk::Framebuffer> framebuffer(backbuffer_view.size());
	{
		std::vector<vk::ImageView> view(2);

		vk::FramebufferCreateInfo framebuffer_info;
		framebuffer_info.setRenderPass(render_pass);
		framebuffer_info.setAttachmentCount(view.size());
		framebuffer_info.setPAttachments(view.data());
		framebuffer_info.setWidth(window.getClientSize().x);
		framebuffer_info.setHeight(window.getClientSize().y);
		framebuffer_info.setLayers(1);

		for (size_t i = 0; i < backbuffer_view.size(); i++) {
			view[0] = backbuffer_view[i];
			view[1] = depth_view;
			framebuffer[i] = device->createFramebuffer(framebuffer_info);
		}
	}
	setup_cmd.end();

	vk::SubmitInfo setup_submit_info;
	setup_submit_info.setCommandBufferCount(1);
	setup_submit_info.setPCommandBuffers(&setup_cmd);
	queue.submit({ setup_submit_info }, vk::Fence());
	queue.waitIdle();

	auto task = std::make_shared<std::promise<std::unique_ptr<cModelInstancing>>>();
	auto modelFuture = task->get_future();
	{
		cThreadJob job;
		auto load = [task]()
		{
			auto model = std::make_unique<cModelInstancing>();
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
	camera->m_far = 100000.f;
	camera->m_near = 0.01f;
	ModelRender render;
	render.setup(model->getResource());

	cModelRenderer renderer;
	renderer.setup(render_pass);
	renderer.addModel(&render);

	std::vector<std::unique_ptr<cModelInstancing>> models;
	models.reserve(1000);
	for (int i = 0; i < 1000; i++)
	{
		auto m = std::make_unique<cModelInstancing>();
		m->load(btr::getResourcePath() + "tiny.x");
		render.addModel(m.get());
		m->getInstance()->m_world = glm::translate(glm::ballRand(2999.f));
		models.push_back(std::move(m));
	}

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

	for (int i = 0; i < 30; i++)
	{
		renderer.getLight().add(std::move(std::make_unique<LightSample>()));
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
	while (true)
	{
		cStopWatch time;
		auto* m_camera = cCamera::sCamera::Order().getCameraList()[0];
		m_camera->control(window.getInput(), 0.016f);

		uint32_t backbuffer_index = window.getSwapchain().swap(swapbuffer_semaphore);
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
			present_to_render_barrier.setImage(window.getSwapchain().m_backbuffer_image[backbuffer_index]);

			render_cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::DependencyFlags(),
				nullptr, nullptr, present_to_render_barrier);

			renderer.execute(render_cmd);

			// begin cmd render pass
			std::vector<vk::ClearValue> clearValue = {
				vk::ClearValue().setColor(vk::ClearColorValue(std::array<float, 4>{0.3f, 0.3f, 0.8f, 1.f})),
				vk::ClearValue().setDepthStencil(vk::ClearDepthStencilValue(1.f)),
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
				.setPSwapchains(&window.getSwapchain().m_swapchain_handle)
				.setPImageIndices(&backbuffer_index);
			queue.presentKHR(present_info);
		}

		window.update(sGlobal::Order().getThreadPool());
		sGlobal::Order().swap();
		printf("%6.3fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

