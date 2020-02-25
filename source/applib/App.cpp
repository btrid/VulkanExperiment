#include <applib/App.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cDebug.h>
#include <btrlib/Context.h>
#include <btrlib/sDebug.h>

#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>
#include <applib/sParticlePipeline.h>
#include <applib/sSystem.h>
#include <applib/GraphicsResource.h>
#include <applib/sImGuiRenderer.h>

vk::UniqueDescriptorSetLayout RenderTarget::s_descriptor_set_layout;

VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData)
{
	char prefix[64];
	auto message_size = strlen(callbackData->pMessage) + 500;
	char *message = (char *)malloc(message_size);
	assert(message);
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
	{
		strcpy_s(prefix, "VERBOSE :");
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
	{
		strcpy_s(prefix, "INFO :");
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		strcpy_s(prefix, "WARNING :");
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		strcpy_s(prefix, "ERROR :");
	}
	if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
	{
		strcat_s(prefix, "GENERAL :");
	}
	else
	{
		// 		if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_SPECIFICATION_BIT_EXT) {
		// 			strcat_s(prefix, "SPEC");
		// 			validation_error = 1;
		// 		}
		// 		if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
		// 			if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_SPECIFICATION_BIT_EXT) {
		// 				strcat(prefix, "|");
		// 			}
		// 			strcat_s(prefix, "PERF");
		// 		}
	}
	sprintf_s(message, message_size,
		"%s %s\n %s",
		prefix,
		callbackData->pMessageIdName,
		callbackData->pMessage);

	char tmp_message[500];
	if (callbackData->objectCount > 0)
	{
		sprintf_s(tmp_message, "\n Object Num is %d\n", callbackData->objectCount);
		strcat_s(message, message_size, tmp_message);
		for (uint32_t object = 0; object < callbackData->objectCount; ++object) {
			sprintf_s(tmp_message,
				" Object[%d] - Type %s, Value %p, Name \"%s\"\n",
				object,
				vk::to_string((vk::ObjectType)callbackData->pObjects[object].objectType).c_str(),
				(void*)(callbackData->pObjects[object].objectHandle),
				callbackData->pObjects[object].pObjectName ? callbackData->pObjects[object].pObjectName : "no name");
			strcat_s(message, message_size, tmp_message);
		}
	}
	if (callbackData->cmdBufLabelCount > 0)
	{
		sprintf_s(tmp_message, message_size,
			"\n Command Buffer Labels - %d\n",
			callbackData->cmdBufLabelCount);
		strcat_s(message, message_size, tmp_message);
		for (uint32_t label = 0; label < callbackData->cmdBufLabelCount; ++label) {
			sprintf_s(tmp_message, message_size,
				" Label[%d] - %s { %f, %f, %f, %f}\n",
				label,
				callbackData->pCmdBufLabels[label].pLabelName,
				callbackData->pCmdBufLabels[label].color[0],
				callbackData->pCmdBufLabels[label].color[1],
				callbackData->pCmdBufLabels[label].color[2],
				callbackData->pCmdBufLabels[label].color[3]);
			strcat_s(message, message_size, tmp_message);
		}
	}
	printf("%s\n", message);
	fflush(stdout);
	free(message);
	// Don't bail out, but keep going.
	DebugBreak();
	return false;
}

namespace app
{
App* g_app_instance = nullptr;


App::App(const AppDescriptor& desc)
{
	// ウインドウリストを取りたいけどいい考えがない。後で考える
	g_app_instance = this;

	vk::ApplicationInfo appInfo = { "Vulkan Test", 1, "EngineName", 0, VK_API_VERSION_1_1 };
	std::vector<const char*> LayerName =
	{
#if _DEBUG
			"VK_LAYER_LUNARG_standard_validation"
#endif
	};
	std::vector<const char*> ExtensionName =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#if USE_DEBUG_REPORT
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
	};

	vk::InstanceCreateInfo instanceInfo = {};
	instanceInfo.setPApplicationInfo(&appInfo);
	instanceInfo.setEnabledExtensionCount((uint32_t)ExtensionName.size());
	instanceInfo.setPpEnabledExtensionNames(ExtensionName.data());
	instanceInfo.setEnabledLayerCount((uint32_t)LayerName.size());
	instanceInfo.setPpEnabledLayerNames(LayerName.data());
	m_instance = vk::createInstanceUnique(instanceInfo);

//	vk::DynamicLoader dl;
//	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
//	m_dispatch.init(vkGetInstanceProcAddr);
	m_dispatch.init(m_instance.get());
#if USE_DEBUG_REPORT
	vk::DebugUtilsMessengerCreateInfoEXT debug_create_info;
	debug_create_info.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning/* | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose*/);
	debug_create_info.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
	debug_create_info.setPfnUserCallback(debug_messenger_callback);
	m_debug_messenger = m_instance->createDebugUtilsMessengerEXTUnique(debug_create_info, nullptr, m_dispatch);
#endif

	auto gpus = m_instance->enumeratePhysicalDevices();
	m_physical_device = gpus[0];

	{
		std::vector<const char*> extensionName = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
			VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME,
			VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
			VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
		};

		auto gpu_propaty = m_physical_device.getProperties();
		auto gpu_feature = m_physical_device.getFeatures();
		assert(gpu_feature.multiDrawIndirect);

		auto queueFamilyProperty = m_physical_device.getQueueFamilyProperties();

		std::vector<std::vector<float>> queue_priority(queueFamilyProperty.size());
		for (size_t i = 0; i < queueFamilyProperty.size(); i++)
		{
			auto& queue_property = queueFamilyProperty[i];
			auto& priority = queue_priority[i];
			priority.resize(1);
			for (size_t q = 1; q < priority.size(); q++)
			{
				priority[q] = 1.f / (priority.size() - 1) * q;
			}
		}

		// デバイス
		std::vector<vk::DeviceQueueCreateInfo> queue_info(queueFamilyProperty.size());
		std::vector<uint32_t> family_index;
		for (size_t i = 0; i < queueFamilyProperty.size(); i++)
		{
			queue_info[i].queueCount = (uint32_t)queue_priority[i].size();
			queue_info[i].pQueuePriorities = queue_priority[i].data();
			queue_info[i].queueFamilyIndex = (uint32_t)i;
			family_index.push_back((uint32_t)i);
		}

		vk::DeviceCreateInfo device_info;
		device_info.setQueueCreateInfoCount((uint32_t)queue_info.size());
		device_info.setPQueueCreateInfos(queue_info.data());
		device_info.setPEnabledFeatures(&gpu_feature);
		device_info.setEnabledExtensionCount((uint32_t)extensionName.size());
		device_info.setPpEnabledExtensionNames(extensionName.data());

		vk::PhysicalDevice8BitStorageFeaturesKHR  storage8bit_feature;
		storage8bit_feature.setStorageBuffer8BitAccess(VK_TRUE);
		storage8bit_feature.setUniformAndStorageBuffer8BitAccess(VK_TRUE);
		device_info.setPNext(&storage8bit_feature);

		vk::PhysicalDeviceFloat16Int8FeaturesKHR  f16s8_feature;
		f16s8_feature.setShaderInt8(VK_TRUE);
		f16s8_feature.setShaderFloat16(VK_TRUE);
		storage8bit_feature.setPNext(&f16s8_feature);

		m_device = m_physical_device.createDeviceUnique(device_info, nullptr);
	}

	m_dispatch = vk::DispatchLoaderDynamic(m_instance.get(), m_device.get());

//	auto device = sGlobal::Order().getGPU(0).getDevice();

	auto init_thread_data_func = [=](const cThreadPool::InitParam& param)
	{
		SetThreadIdealProcessor(::GetCurrentThread(), param.m_index);
		auto& data = sThreadLocal::Order();
		data.m_thread_index = param.m_index;
	};
	m_thread_pool.start(std::thread::hardware_concurrency() - 1, init_thread_data_func);

	m_context = std::make_shared<btr::Context>();
	m_context->m_instance = m_instance.get();
	m_context->m_physical_device = m_physical_device;
	m_context->m_device = m_device.get();
	m_context->m_dispach = m_dispatch;
	
	//	m_context = std::make_shared<AppContext>();
	{
		m_context->m_device = m_device.get();

//		VmaAllocatorCreateInfo allocator_info = {};
//		allocator_info.physicalDevice = m_physical_device;
//		allocator_info.device = m_device.get();
//		vmaCreateAllocator(&allocator_info, &m_context->m_allocator);

		vk::MemoryPropertyFlags host_memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached;
		vk::MemoryPropertyFlags device_memory = vk::MemoryPropertyFlagBits::eDeviceLocal;
		vk::MemoryPropertyFlags transfer_memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
//		device_memory = host_memory; // debug
		m_context->m_vertex_memory.setup(m_physical_device, m_device.get(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 100);
		m_context->m_uniform_memory.setup(m_physical_device, m_device.get(), vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000*1000 * 20);
		m_context->m_storage_memory.setup(m_physical_device, m_device.get(), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc, device_memory, 1024 * 1024 * (1024+200));
		m_context->m_staging_memory.setup(m_physical_device, m_device.get(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst, host_memory, 1000 * 1000 * 500);
		{
			vk::DescriptorPoolSize pool_size[5];
			pool_size[0].setType(vk::DescriptorType::eUniformBuffer);
			pool_size[0].setDescriptorCount(200);
			pool_size[1].setType(vk::DescriptorType::eStorageBuffer);
			pool_size[1].setDescriptorCount(3000);
			pool_size[2].setType(vk::DescriptorType::eCombinedImageSampler);
			pool_size[2].setDescriptorCount(300);
			pool_size[3].setType(vk::DescriptorType::eStorageImage);
			pool_size[3].setDescriptorCount(300);
			pool_size[4].setType(vk::DescriptorType::eUniformBufferDynamic);
			pool_size[4].setDescriptorCount(5);

			vk::DescriptorPoolCreateInfo pool_info;
			pool_info.setPoolSizeCount(array_length(pool_size));
			pool_info.setPPoolSizes(pool_size);
			pool_info.setMaxSets(2000);
			m_context->m_descriptor_pool = m_device->createDescriptorPoolUnique(pool_info);

		}
	}

	m_cmd_pool = std::make_shared<cCmdPool>(m_context);
	m_context->m_cmd_pool = m_cmd_pool;
	{
		vk::FenceCreateInfo fence_info;
		fence_info.setFlags(vk::FenceCreateFlagBits::eSignaled);
		m_fence_list.reserve(sGlobal::FRAME_COUNT_MAX);
		for (size_t i = 0; i < sGlobal::FRAME_COUNT_MAX; i++)
		{
			m_fence_list.emplace_back(m_device->createFenceUnique(fence_info));
		}
	}

	{
		auto stage = vk::ShaderStageFlagBits::eAll;
		vk::DescriptorSetLayoutBinding binding[] = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, stage),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, stage),
		};
		vk::DescriptorSetLayoutCreateInfo desc_layout_info;
		desc_layout_info.setBindingCount(array_length(binding));
		desc_layout_info.setPBindings(binding);
		RenderTarget::s_descriptor_set_layout = m_device->createDescriptorSetLayoutUnique(desc_layout_info);

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

	auto queue = m_device->getQueue(0, 0);

	vk::FenceCreateInfo fence_info;
//	fence_info.setFlags(vk::FenceCreateFlagBits::eSignaled);
	auto fence = m_device->createFenceUnique(fence_info);
	queue.submit(submitInfo, fence.get());

	sDebug::Order().waitFence(m_device.get(), fence.get());
//	m_context->m_device.resetFences({ m_fence_list[index].get() });

//	// 適当な実装なので絶対待つ
//	queue.waitIdle();
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
		swap_wait_semas[i] = window->getSwapchain()->m_swapbuffer_semaphore.get();
		submit_wait_semas[i] = window->getSwapchain()->m_submit_semaphore.get();
		swapchains[i] = window->getSwapchain()->m_swapchain_handle.get();
		backbuffer_indexs[i] = window->getSwapchain()->m_backbuffer_index;
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

	auto queue = m_device->getQueue(0, 0);
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
		window->getSwapchain()->swap();
	}

	{
		uint32_t index = sGlobal::Order().getCurrentFrame();
		sDebug::Order().waitFence(m_device.get(), m_fence_list[index].get());
		m_device->resetFences({ m_fence_list[index].get() });
		m_cmd_pool->resetPool();
	}

	for (auto& request : m_window_request)
	{
		auto new_window = sWindow::Order().createWindow<AppWindow>(m_context, request);
		new_window->getSwapchain()->swap();
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
		m_thread_pool.enque(job);
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
		m_thread_pool.enque(job);
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
		m_thread_pool.enque(job);
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
		m_thread_pool.enque(job);
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
	const auto& render_target = window->getFrontBuffer();

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

		m_render_pass = context->m_device.createRenderPassUnique(renderpass_info);
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

		m_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
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
		auto pipelines = context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
		m_pipeline = std::move(pipelines[0]);

	}

	m_imgui_context = ImGui::CreateContext();
	ImGui::SetCurrentContext(m_imgui_context);
	auto& io = ImGui::GetIO();
	io.DisplaySize.x = window->getFrontBuffer()->m_info.extent.width;
	io.DisplaySize.y = window->getFrontBuffer()->m_info.extent.height;
	io.FontGlobalScale = 1.f;
	io.RenderDrawListsFnUnused = nullptr;  // Setup a render function, or set to NULL and call GetDrawData() after Render() to access the render data.
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

	vk::ImageCreateInfo depth_info;
	vk::ImageCreateInfo image_info;
	vk::SurfaceCapabilitiesKHR capability = context->m_physical_device.getSurfaceCapabilitiesKHR(m_surface.get());
	{
		// デプス生成
		depth_info.format = vk::Format::eD32Sfloat;
		depth_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferDst;
		depth_info.arrayLayers = 1;
		depth_info.mipLevels = 1;
		depth_info.extent.width = capability.currentExtent.width;
		depth_info.extent.height = capability.currentExtent.height;
		depth_info.extent.depth = 1;
		depth_info.imageType = vk::ImageType::e2D;
		depth_info.initialLayout = vk::ImageLayout::eUndefined;
//		depth_info.queueFamilyIndexCount = (uint32_t)context->m_device.getQueueFamilyIndex().size();
//		depth_info.pQueueFamilyIndices = context->m_device.getQueueFamilyIndex().data();
		m_depth_image = context->m_device.createImageUnique(depth_info);
		m_depth_info = depth_info;
					// メモリ確保
		auto memory_request = context->m_device.getImageMemoryRequirements(m_depth_image.get());
		uint32_t memory_index = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::MemoryAllocateInfo memory_info;
		memory_info.allocationSize = memory_request.size;
		memory_info.memoryTypeIndex = memory_index;
		m_depth_memory = context->m_device.allocateMemoryUnique(memory_info);

		context->m_device.bindImageMemory(m_depth_image.get(), m_depth_memory.get(), 0);

		vk::ImageViewCreateInfo depth_view_info;
		depth_view_info.format = depth_info.format;
		depth_view_info.image = m_depth_image.get();
		depth_view_info.viewType = vk::ImageViewType::e2D;
		depth_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
		depth_view_info.subresourceRange.baseArrayLayer = 0;
		depth_view_info.subresourceRange.baseMipLevel = 0;
		depth_view_info.subresourceRange.layerCount = 1;
		depth_view_info.subresourceRange.levelCount = 1;
		m_depth_view = context->m_device.createImageViewUnique(depth_view_info);

	}
	{
		image_info.format = vk::Format::eR16G16B16A16Sfloat;
		image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
		image_info.arrayLayers = 1;
		image_info.mipLevels = 1;
		image_info.extent.width = capability.currentExtent.width;
		image_info.extent.height = capability.currentExtent.height;
		image_info.extent.depth = 1;
		image_info.imageType = vk::ImageType::e2D;
		image_info.initialLayout = vk::ImageLayout::eUndefined;
		m_front_buffer_image = context->m_device.createImageUnique(image_info);
		m_front_buffer_info = image_info;
		// メモリ確保
		auto memory_request = context->m_device.getImageMemoryRequirements(m_front_buffer_image.get());
		uint32_t memory_index = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::MemoryAllocateInfo memory_info;
		memory_info.allocationSize = memory_request.size;
		memory_info.memoryTypeIndex = memory_index;
		m_front_buffer_memory = context->m_device.allocateMemoryUnique(memory_info);
		context->m_device.bindImageMemory(m_front_buffer_image.get(), m_front_buffer_memory.get(), 0);

		vk::ImageViewCreateInfo view_info;
		view_info.format = image_info.format;
		view_info.image = m_front_buffer_image.get();
		view_info.viewType = vk::ImageViewType::e2D;
		view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.layerCount = 1;
		view_info.subresourceRange.levelCount = 1;
		m_front_buffer_view = context->m_device.createImageViewUnique(view_info);
	}
	{

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		vk::ImageSubresourceRange subresource_range;
		subresource_range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		subresource_range.setBaseArrayLayer(0);
		subresource_range.setLayerCount(1);
		subresource_range.setBaseMipLevel(0);
		subresource_range.setLevelCount(1);

		vk::ImageMemoryBarrier barrier[2];
		barrier[0].setImage(m_front_buffer_image.get());
		barrier[0].setSubresourceRange(subresource_range);
		barrier[0].setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
		barrier[0].setOldLayout(vk::ImageLayout::eUndefined);
		barrier[0].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

		vk::ImageSubresourceRange subresource_depth_range;
		subresource_depth_range.aspectMask = vk::ImageAspectFlagBits::eDepth;
		subresource_depth_range.baseArrayLayer = 0;
		subresource_depth_range.baseMipLevel = 0;
		subresource_depth_range.layerCount = 1;
		subresource_depth_range.levelCount = 1;

		barrier[1].setImage(m_depth_image.get());
		barrier[1].setSubresourceRange(subresource_depth_range);
		barrier[1].setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		barrier[1].setOldLayout(vk::ImageLayout::eUndefined);
		barrier[1].setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { array_size(barrier), barrier });


	}

	{
		// buckbufferの初期化
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		vk::ImageSubresourceRange subresource_range;
		subresource_range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		subresource_range.setBaseArrayLayer(0);
		subresource_range.setLayerCount(1);
		subresource_range.setBaseMipLevel(0);
		subresource_range.setLevelCount(1);
		std::vector<vk::ImageMemoryBarrier> barrier(getSwapchain()->getBackbufferNum());
		barrier[0].setSubresourceRange(subresource_range);
		barrier[0].setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
		barrier[0].setOldLayout(vk::ImageLayout::eUndefined);
		barrier[0].setNewLayout(vk::ImageLayout::ePresentSrcKHR);
		barrier[0].setImage(getSwapchain()->m_backbuffer_image[0]);
		for (uint32_t i = 1; i < getSwapchain()->getBackbufferNum(); i++)
		{
			barrier[i] = barrier[0];
			barrier[i].setImage(getSwapchain()->m_backbuffer_image[i]);
		}
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, barrier);

	}

	m_front_buffer = std::make_shared<RenderTarget>();
	m_front_buffer->m_info = m_front_buffer_info;
	m_front_buffer->m_image = m_front_buffer_image.get();
	m_front_buffer->m_view = m_front_buffer_view.get();
	m_front_buffer->m_memory = m_front_buffer_memory.get();
	m_front_buffer->m_depth_info = m_depth_info;
	m_front_buffer->m_depth_image = m_depth_image.get();
	m_front_buffer->m_depth_memory = m_depth_memory.get();
	m_front_buffer->m_depth_view = m_depth_view.get();
	m_front_buffer->is_dynamic_resolution = false;
	m_front_buffer->m_resolution.width = m_front_buffer_info.extent.width;
	m_front_buffer->m_resolution.height = m_front_buffer_info.extent.height;

	{
		m_front_buffer->u_render_info = context->m_uniform_memory.allocateMemory<RenderTargetInfo>({ 1, {} });

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		auto staging = context->m_staging_memory.allocateMemory<RenderTargetInfo>({ 1, btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT });
		staging.getMappedPtr()->m_size = ivec2(m_front_buffer->m_info.extent.width, m_front_buffer->m_info.extent.height);

		vk::BufferCopy copy;
		copy.setSrcOffset(staging.getInfo().offset);
		copy.setDstOffset(m_front_buffer->u_render_info.getInfo().offset);
		copy.setSize(staging.getInfo().range);
		cmd.copyBuffer(staging.getInfo().buffer, m_front_buffer->u_render_info.getInfo().buffer, copy);
	}
	{
		vk::DescriptorSetLayout layouts[] = {
			RenderTarget::s_descriptor_set_layout.get(),
		};
		vk::DescriptorSetAllocateInfo desc_info;
		desc_info.setDescriptorPool(context->m_descriptor_pool.get());
		desc_info.setDescriptorSetCount(array_length(layouts));
		desc_info.setPSetLayouts(layouts);
		m_front_buffer->m_descriptor = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

		vk::DescriptorImageInfo images[] = {
			vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eGeneral).setImageView(m_front_buffer->m_view),
		};

		vk::DescriptorBufferInfo uniforms[] = {
			m_front_buffer->u_render_info.getInfo()
		};

		vk::WriteDescriptorSet write[] = {
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setDescriptorCount(array_length(images))
			.setPImageInfo(images)
			.setDstBinding(0)
			.setDstSet(m_front_buffer->m_descriptor.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(std::size(uniforms))
			.setPBufferInfo(uniforms)
			.setDstBinding(1)
			.setDstSet(m_front_buffer->m_descriptor.get()),
		};
		context->m_device.updateDescriptorSets(std::size(write), write, 0, nullptr);
	}

	m_imgui_pipeline = std::make_unique<ImguiRenderPipeline>(context, this);


#if USE_DEBUG_REPORT
	vk::DebugUtilsObjectNameInfoEXT name_info;

	char buf[256];
	for (int i = 0; i < m_swapchain->m_backbuffer_image.size(); i++)
	{
		sprintf_s(buf, "AppWindow Backbuffer[i]", i);
		name_info.objectHandle = reinterpret_cast<uint64_t &>(m_swapchain->m_backbuffer_image[i]);
		name_info.objectType = vk::ObjectType::eImage;
		name_info.pObjectName = buf;
		context->m_device.setDebugUtilsObjectNameEXT(name_info, context->m_dispach);
	}

	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_front_buffer->m_image);
	name_info.objectType = vk::ObjectType::eImage;
	name_info.pObjectName = "AppWindow FrontBufferImage";
	context->m_device.setDebugUtilsObjectNameEXT(name_info, context->m_dispach);

	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_front_buffer->m_view);
	name_info.objectType = vk::ObjectType::eImageView;
	name_info.pObjectName = "AppWindow FrontBufferImageView";
	context->m_device.setDebugUtilsObjectNameEXT(name_info, context->m_dispach);

	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_front_buffer->m_memory);
	name_info.objectType = vk::ObjectType::eDeviceMemory;
	name_info.pObjectName = "AppWindow FrontBufferImageMemory";
	context->m_device.setDebugUtilsObjectNameEXT(name_info, context->m_dispach);

	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_depth_image.get());
	name_info.objectType = vk::ObjectType::eImage;
	name_info.pObjectName = "AppWindow DepthImage";
	context->m_device.setDebugUtilsObjectNameEXT(name_info, context->m_dispach);

	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_depth_view.get());
	name_info.objectType = vk::ObjectType::eImageView;
	name_info.pObjectName = "AppWindow DepthImageView";
	context->m_device.setDebugUtilsObjectNameEXT(name_info, context->m_dispach);

	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_depth_memory.get());
	name_info.objectType = vk::ObjectType::eDeviceMemory;
	name_info.pObjectName = "AppWindow DepthImageMemory";
	context->m_device.setDebugUtilsObjectNameEXT(name_info, context->m_dispach);
#endif

}
