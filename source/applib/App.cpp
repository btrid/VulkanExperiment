#include <applib/App.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cDebug.h>
#include <btrlib/Context.h>

#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>
#include <applib/sParticlePipeline.h>
#include <applib/sSystem.h>
#include <applib/GraphicsResource.h>
#include <applib/sImGuiRenderer.h>

namespace app
{
App* g_app_instance = nullptr;


App::App(const AppDescriptor& desc)
{
	// ウインドウリストを取りたいけどいい考えがない。後で考える
	g_app_instance = this;

	vk::Instance instance = sGlobal::Order().getVKInstance();

#if _DEBUG
	static cDebug debug(instance);
#endif

	m_gpu = desc.m_gpu;
	auto device = sGlobal::Order().getGPU(0).getDevice();

	m_context = std::make_shared<btr::Context>();
	{
		m_context->m_gpu = m_gpu;
		m_context->m_device = device;

		vk::MemoryPropertyFlags host_memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached;
		vk::MemoryPropertyFlags device_memory = vk::MemoryPropertyFlagBits::eDeviceLocal;
		vk::MemoryPropertyFlags transfer_memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
		//		device_memory = host_memory; // debug
		m_context->m_vertex_memory.setup(device, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 100);
		m_context->m_uniform_memory.setup(device, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 20);
		m_context->m_storage_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 200);
		m_context->m_staging_memory.setup(device, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst, host_memory, 1000 * 1000 * 100);
		{
			vk::DescriptorPoolSize pool_size[5];
			pool_size[0].setType(vk::DescriptorType::eUniformBuffer);
			pool_size[0].setDescriptorCount(20);
			pool_size[1].setType(vk::DescriptorType::eStorageBuffer);
			pool_size[1].setDescriptorCount(30);
			pool_size[2].setType(vk::DescriptorType::eCombinedImageSampler);
			pool_size[2].setDescriptorCount(30);
			pool_size[3].setType(vk::DescriptorType::eStorageImage);
			pool_size[3].setDescriptorCount(30);
			pool_size[4].setType(vk::DescriptorType::eUniformBufferDynamic);
			pool_size[4].setDescriptorCount(5);

			vk::DescriptorPoolCreateInfo pool_info;
			pool_info.setPoolSizeCount(array_length(pool_size));
			pool_info.setPPoolSizes(pool_size);
			pool_info.setMaxSets(20);
			m_context->m_descriptor_pool = device->createDescriptorPoolUnique(pool_info);

			vk::PipelineCacheCreateInfo cacheInfo = vk::PipelineCacheCreateInfo();
			m_context->m_cache = device->createPipelineCacheUnique(cacheInfo);
		}
	}

	m_cmd_pool = std::make_shared<cCmdPool>(m_context);
	m_context->m_cmd_pool = m_cmd_pool;
	{
		vk::FenceCreateInfo fence_info;
		fence_info.setFlags(vk::FenceCreateFlagBits::eSignaled);
		m_fence_list.reserve(sGlobal::FRAME_MAX);
		for (size_t i = 0; i < sGlobal::FRAME_MAX; i++)
		{
			m_fence_list.emplace_back(m_context->m_gpu.getDevice()->createFenceUnique(fence_info));
		}
	}
	sSystem::Create(m_context);
	sCameraManager::Order().setup(m_context);
	sGraphicsResource::Order().setup(m_context);
	sImGuiRenderer::Create(m_context);

	cWindowDescriptor window_desc;
	window_desc.surface_format_request = vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	window_desc.size = vk::Extent2D(desc.m_window_size.x, desc.m_window_size.y);
	window_desc.window_name = L"Vulkan Test";

	auto window = sWindow::Order().createWindow<AppWindow>(m_context, window_desc);
	m_window = window;
	m_window_list.emplace_back(window);
	m_context->m_window = window;

//	sParticlePipeline::Order().setup(m_context);
//	DrawHelper::Order().setup(m_context);

}

void App::setup()
{
	auto setup_cmds = m_context->m_cmd_pool->submit();
	std::vector<vk::SubmitInfo> submitInfo =
	{
		vk::SubmitInfo()
		.setCommandBufferCount((uint32_t)setup_cmds.size())
		.setPCommandBuffers(setup_cmds.data())
	};

	auto queue = m_context->m_device->getQueue(0, 0);
	queue.submit(submitInfo, nullptr);
	queue.waitIdle();
}


void App::submit(std::vector<vk::CommandBuffer>&& submit_cmds)
{

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

	std::vector<vk::CommandBuffer> cmds;
	cmds.reserve(32);

	{
		auto precmds = m_context->m_cmd_pool->submit();
		cmds.insert(cmds.end(), std::make_move_iterator(precmds.begin()), std::make_move_iterator(precmds.end()));
	}

	for (auto& window : m_window_list)
	{
//		cmds.push_back(window->m_cmd_present_to_render[window->getSwapchain().m_backbuffer_index].get());
	}

	m_sync_point.wait();

	cmds.insert(cmds.end(), std::make_move_iterator(m_system_cmds.begin()), std::make_move_iterator(m_system_cmds.end()));

	submit_cmds.erase(std::remove_if(submit_cmds.begin(), submit_cmds.end(), [&](auto& d) { return !d; }), submit_cmds.end());
	cmds.insert(cmds.end(), std::make_move_iterator(submit_cmds.begin()), std::make_move_iterator(submit_cmds.end()));

	for (auto& window : m_window_list)
	{
//		cmds.push_back(window->m_cmd_render_to_present[window->getSwapchain().m_backbuffer_index].get());
	}

	vk::SubmitInfo submit_info;
	submit_info.setCommandBufferCount((uint32_t)cmds.size());
	submit_info.setPCommandBuffers(cmds.data());
	submit_info.setWaitSemaphoreCount((uint32_t)swap_wait_semas.size());
	submit_info.setPWaitSemaphores(swap_wait_semas.data());
	submit_info.setPWaitDstStageMask(wait_pipelines.data());
	submit_info.setSignalSemaphoreCount((uint32_t)submit_wait_semas.size());
	submit_info.setPSignalSemaphores(submit_wait_semas.data());

	auto queue = m_gpu.getDevice()->getQueue(0, 0);
	queue.submit(submit_info, m_fence_list[sGlobal::Order().getCurrentFrame()].get());

	vk::PresentInfoKHR present_info;
	present_info.setWaitSemaphoreCount((uint32_t)submit_wait_semas.size());
	present_info.setPWaitSemaphores(submit_wait_semas.data());
	present_info.setSwapchainCount((uint32_t)swapchains.size());
	present_info.setPSwapchains(swapchains.data());
	present_info.setPImageIndices(backbuffer_indexs.data());
	queue.presentKHR(present_info);
}

void App::preUpdate()
{

	for (auto& window : m_window_list)
	{
		window->getSwapchain().swap();
	}

	{
		uint32_t index = sGlobal::Order().getCurrentFrame();
		sDebug::Order().waitFence(m_context->m_device.getHandle(), m_fence_list[index].get());
		m_context->m_device->resetFences({ m_fence_list[index].get() });
		m_cmd_pool->resetPool();
	}

	for (auto& request : m_window_request)
	{
		auto new_window = sWindow::Order().createWindow<AppWindow>(m_context, request);
		new_window->getSwapchain().swap();
		m_window_list.emplace_back(std::move(new_window));
	}
	m_window_request.clear();

	{
		auto& m_camera = cCamera::sCamera::Order().getCameraList()[0];
		m_camera->control(m_window->getInput(), 0.016f);
	}

	m_system_cmds.resize(2);
	m_sync_point.reset(4);

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
//			m_system_cmds[3] = DrawHelper::Order().draw(m_context);
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
// 			m_system_cmds[2] = sParticlePipeline::Order().draw(m_context);
			m_sync_point.arrive();
		}
		);
		sGlobal::Order().getThreadPool().enque(job);
	}

}

void App::postUpdate()
{
	MSG msg = {};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	for (auto it = m_window_list.begin(); it != m_window_list.end();)
	{
		(*it)->execute();
		(*it)->swap();
		if ((*it)->isClose())
		{
			sDeleter::Order().enque(std::move(*it));
			it = m_window_list.erase(it);
		}
		else {
			it++;
		}
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

AppWindow::ImguiRenderPipeline::ImguiRenderPipeline(const std::shared_ptr<btr::Context>& context, AppWindow* const window)
{
	const auto& render_target = window->getRenderTarget();

	// レンダーパス
	{
		// sub pass
		vk::AttachmentReference color_ref[] =
		{
			vk::AttachmentReference()
			.setAttachment(0)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
		};
		vk::SubpassDescription subpass;
		subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
		subpass.setInputAttachmentCount(0);
		subpass.setPInputAttachments(nullptr);
		subpass.setColorAttachmentCount(array_length(color_ref));
		subpass.setPColorAttachments(color_ref);

		vk::AttachmentDescription attach_description[] =
		{
			// color1
			vk::AttachmentDescription()
			.setFormat(render_target->m_info.format)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eLoad)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
		};
		vk::RenderPassCreateInfo renderpass_info;
		renderpass_info.setAttachmentCount(array_length(attach_description));
		renderpass_info.setPAttachments(attach_description);
		renderpass_info.setSubpassCount(1);
		renderpass_info.setPSubpasses(&subpass);

		m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
	}

	{
		vk::ImageView view[] = {
			render_target->m_view
		};
		vk::FramebufferCreateInfo framebuffer_info;
		framebuffer_info.setRenderPass(m_render_pass.get());
		framebuffer_info.setAttachmentCount(array_length(view));
		framebuffer_info.setPAttachments(view);
		framebuffer_info.setWidth(render_target->m_info.extent.width);
		framebuffer_info.setHeight(render_target->m_info.extent.height);
		framebuffer_info.setLayers(1);

		m_framebuffer = context->m_device->createFramebufferUnique(framebuffer_info);
	}

	// setup pipeline
	{
		// assembly
		vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
		{
			vk::PipelineInputAssemblyStateCreateInfo()
			.setPrimitiveRestartEnable(VK_FALSE)
			.setTopology(vk::PrimitiveTopology::eTriangleList),
		};

		// viewport
		vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)render_target->m_info.extent.width, (float)render_target->m_info.extent.height, 0.f, 1.f);
		vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(render_target->m_info.extent.width, render_target->m_info.extent.height));
		vk::PipelineViewportStateCreateInfo viewportInfo;
		viewportInfo.setViewportCount(1);
		viewportInfo.setPViewports(&viewport);
		viewportInfo.setScissorCount(1);
		viewportInfo.setPScissors(&scissor);

		vk::PipelineRasterizationStateCreateInfo rasterization_info;
		rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
		rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
		rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
		rasterization_info.setLineWidth(1.f);

		vk::PipelineMultisampleStateCreateInfo sample_info;
		sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

		vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
		depth_stencil_info.setDepthTestEnable(VK_FALSE);

		std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
			vk::PipelineColorBlendAttachmentState()
			.setBlendEnable(VK_TRUE)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
			.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setAlphaBlendOp(vk::BlendOp::eAdd)
			.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setColorWriteMask(vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB
				| vk::ColorComponentFlagBits::eA)
		};
		vk::PipelineColorBlendStateCreateInfo blend_info;
		blend_info.setAttachmentCount(blend_state.size());
		blend_info.setPAttachments(blend_state.data());

		vk::VertexInputAttributeDescription attr[] =
		{
			vk::VertexInputAttributeDescription().setLocation(0).setOffset(0).setBinding(0).setFormat(vk::Format::eR32G32Sfloat),
			vk::VertexInputAttributeDescription().setLocation(1).setOffset(8).setBinding(0).setFormat(vk::Format::eR32G32Sfloat),
			vk::VertexInputAttributeDescription().setLocation(2).setOffset(16).setBinding(0).setFormat(vk::Format::eR8G8B8A8Unorm),
		};
		vk::VertexInputBindingDescription binding[] =
		{
			vk::VertexInputBindingDescription().setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(20)
		};
		vk::PipelineVertexInputStateCreateInfo vertex_input_info;
		vertex_input_info.setVertexAttributeDescriptionCount(array_length(attr));
		vertex_input_info.setPVertexAttributeDescriptions(attr);
		vertex_input_info.setVertexBindingDescriptionCount(array_length(binding));
		vertex_input_info.setPVertexBindingDescriptions(binding);

		vk::PipelineShaderStageCreateInfo shader_info[] = {
			vk::PipelineShaderStageCreateInfo()
			.setModule(sImGuiRenderer::Order().getShaderModle(sImGuiRenderer::SHADER_VERT_RENDER))
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eVertex),
			vk::PipelineShaderStageCreateInfo()
			.setModule(sImGuiRenderer::Order().getShaderModle(sImGuiRenderer::SHADER_FRAG_RENDER))
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eFragment),
		};

		vk::PipelineDynamicStateCreateInfo dynamic_info;
		vk::DynamicState dynamic_state[] = {
			vk::DynamicState::eScissor,
		};
		dynamic_info.setDynamicStateCount(array_length(dynamic_state));
		dynamic_info.setPDynamicStates(dynamic_state);

		std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
		{
			vk::GraphicsPipelineCreateInfo()
			.setStageCount(array_length(shader_info))
			.setPStages(shader_info)
			.setPVertexInputState(&vertex_input_info)
			.setPInputAssemblyState(&assembly_info[0])
			.setPViewportState(&viewportInfo)
			.setPRasterizationState(&rasterization_info)
			.setPMultisampleState(&sample_info)
			.setLayout(sImGuiRenderer::Order().getPipelineLayout(sImGuiRenderer::PIPELINE_LAYOUT_RENDER))
			.setRenderPass(m_render_pass.get())
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info)
			.setPDynamicState(&dynamic_info),
		};
		auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
		m_pipeline = std::move(pipelines[0]);

	}

	m_imgui_context = ImGui::CreateContext();
	ImGui::SetCurrentContext(m_imgui_context);
	auto& io = ImGui::GetIO();
	io.DisplaySize = window->getClientSize<ImVec2>();
	io.FontGlobalScale = 1.f;
	io.RenderDrawListsFn = nullptr;  // Setup a render function, or set to NULL and call GetDrawData() after Render() to access the render data.
	io.KeyMap[ImGuiKey_Tab] = VK_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
	io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
	//		io.KeyMap[ImGuiKey_PageUp] = VK_PAGE;
	// 		io.KeyMap[ImGuiKey_PageDown] = i;
	// 		io.KeyMap[ImGuiKey_Home] = i;
	// 		io.KeyMap[ImGuiKey_End] = i;
	io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
	io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
	io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
	// 		io.KeyMap[ImGuiKey_A] = i;
	// 		io.KeyMap[ImGuiKey_C] = i;
	// 		io.KeyMap[ImGuiKey_V] = i;
	// 		io.KeyMap[ImGuiKey_X] = i;
	// 		io.KeyMap[ImGuiKey_Y] = i;
	// 		io.KeyMap[ImGuiKey_Z] = i;

}

AppWindow::ImguiRenderPipeline::~ImguiRenderPipeline()
{
	ImGui::DestroyContext(m_imgui_context);
	m_imgui_context = nullptr;
}

AppWindow::AppWindow(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor)
	: cWindow(context, descriptor)
{
	auto& device = context->m_device;

	{
		vk::SurfaceCapabilitiesKHR capability = context->m_gpu->getSurfaceCapabilitiesKHR(m_surface.get());
		// デプス生成
		vk::ImageCreateInfo depth_info;
		depth_info.format = vk::Format::eD32Sfloat;
		depth_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferDst;
		depth_info.arrayLayers = 1;
		depth_info.mipLevels = 1;
		depth_info.extent.width = capability.currentExtent.width;
		depth_info.extent.height = capability.currentExtent.height;
		depth_info.extent.depth = 1;
		depth_info.imageType = vk::ImageType::e2D;
		depth_info.initialLayout = vk::ImageLayout::eUndefined;
		depth_info.queueFamilyIndexCount = (uint32_t)context->m_device.getQueueFamilyIndex().size();
		depth_info.pQueueFamilyIndices = context->m_device.getQueueFamilyIndex().data();
		m_depth_image = context->m_device->createImageUnique(depth_info);
		m_depth_info = depth_info;
					// メモリ確保
		auto memory_request = context->m_device->getImageMemoryRequirements(m_depth_image.get());
		uint32_t memory_index = cGPU::Helper::getMemoryTypeIndex(context->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::MemoryAllocateInfo memory_info;
		memory_info.allocationSize = memory_request.size;
		memory_info.memoryTypeIndex = memory_index;
		m_depth_memory = context->m_device->allocateMemoryUnique(memory_info);

		context->m_device->bindImageMemory(m_depth_image.get(), m_depth_memory.get(), 0);

		vk::ImageViewCreateInfo depth_view_info;
		depth_view_info.format = depth_info.format;
		depth_view_info.image = m_depth_image.get();
		depth_view_info.viewType = vk::ImageViewType::e2D;
		depth_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
		depth_view_info.subresourceRange.baseArrayLayer = 0;
		depth_view_info.subresourceRange.baseMipLevel = 0;
		depth_view_info.subresourceRange.layerCount = 1;
		depth_view_info.subresourceRange.levelCount = 1;
		m_depth_view = context->m_device->createImageViewUnique(depth_view_info);
	}
	{
		vk::SurfaceCapabilitiesKHR capability = context->m_gpu->getSurfaceCapabilitiesKHR(m_surface.get());
		vk::ImageCreateInfo image_info;
		image_info.format = vk::Format::eR16G16B16A16Sfloat;
		image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
		image_info.arrayLayers = 1;
		image_info.mipLevels = 1;
		image_info.extent.width = capability.currentExtent.width;
		image_info.extent.height = capability.currentExtent.height;
		image_info.extent.depth = 1;
		image_info.imageType = vk::ImageType::e2D;
		image_info.initialLayout = vk::ImageLayout::eUndefined;
		m_render_target_image = context->m_device->createImageUnique(image_info);
		m_render_target_info = image_info;
		// メモリ確保
		auto memory_request = context->m_device->getImageMemoryRequirements(m_render_target_image.get());
		uint32_t memory_index = cGPU::Helper::getMemoryTypeIndex(context->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::MemoryAllocateInfo memory_info;
		memory_info.allocationSize = memory_request.size;
		memory_info.memoryTypeIndex = memory_index;
		m_render_target_memory = context->m_device->allocateMemoryUnique(memory_info);
		context->m_device->bindImageMemory(m_render_target_image.get(), m_render_target_memory.get(), 0);

		vk::ImageViewCreateInfo view_info;
		view_info.format = image_info.format;
		view_info.image = m_render_target_image.get();
		view_info.viewType = vk::ImageViewType::e2D;
		view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.layerCount = 1;
		view_info.subresourceRange.levelCount = 1;
		m_render_target_view = context->m_device->createImageViewUnique(view_info);

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		vk::ImageSubresourceRange subresource_range;
		subresource_range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		subresource_range.setBaseArrayLayer(0);
		subresource_range.setLayerCount(1);
		subresource_range.setBaseMipLevel(0);
		subresource_range.setLevelCount(1);
		vk::ImageMemoryBarrier barrier;
		barrier.setSubresourceRange(subresource_range);
		barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
		barrier.setOldLayout(vk::ImageLayout::eUndefined);
		barrier.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
		barrier.setImage(m_render_target_image.get());

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, barrier);

	}

	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		vk::ImageSubresourceRange subresource_range;
		subresource_range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		subresource_range.setBaseArrayLayer(0);
		subresource_range.setLayerCount(1);
		subresource_range.setBaseMipLevel(0);
		subresource_range.setLevelCount(1);
		std::vector<vk::ImageMemoryBarrier> barrier(getSwapchain().getBackbufferNum() + 1);
		barrier[0].setSubresourceRange(subresource_range);
		barrier[0].setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
		barrier[0].setOldLayout(vk::ImageLayout::eUndefined);
		barrier[0].setNewLayout(vk::ImageLayout::ePresentSrcKHR);
		barrier[0].setImage(getSwapchain().m_backbuffer_image[0]);
		for (uint32_t i = 1; i < getSwapchain().getBackbufferNum(); i++)
		{
			barrier[i] = barrier[0];
			barrier[i].setImage(getSwapchain().m_backbuffer_image[i]);
		}

		vk::ImageSubresourceRange subresource_depth_range;
		subresource_depth_range.aspectMask = vk::ImageAspectFlagBits::eDepth;
		subresource_depth_range.baseArrayLayer = 0;
		subresource_depth_range.baseMipLevel = 0;
		subresource_depth_range.layerCount = 1;
		subresource_depth_range.levelCount = 1;

		barrier.back().setImage(m_depth_image.get());
		barrier.back().setSubresourceRange(subresource_depth_range);
		barrier.back().setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		barrier.back().setOldLayout(vk::ImageLayout::eUndefined);
		barrier.back().setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::DependencyFlags(), {}, {}, barrier);

	}

	m_render_target = std::make_shared<RenderTarget>();
	m_render_target->m_info = m_render_target_info;
	m_render_target->m_image = m_render_target_image.get();
	m_render_target->m_view = m_render_target_view.get();
	m_render_target->m_memory = m_render_target_memory.get();
	m_render_target->m_depth_info = m_depth_info;
	m_render_target->m_depth_image = m_depth_image.get();
	m_render_target->m_depth_memory = m_depth_memory.get();
	m_render_target->m_depth_view = m_depth_view.get();
	m_render_target->is_dynamic_resolution = false;
	m_render_target->m_resolution.width = m_render_target_info.extent.width;
	m_render_target->m_resolution.height = m_render_target_info.extent.height;

	m_imgui_pipeline = std::make_unique<ImguiRenderPipeline>(context, this);

}
