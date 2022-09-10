#include <applib/App.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Context.h>
#include <btrlib/sDebug.h>

#include <applib/sCameraManager.h>
#include <applib/sParticlePipeline.h>
#include <applib/sSystem.h>
#include <applib/GraphicsResource.h>
#include <applib/sAppImGui.h>

vk::UniqueDescriptorSetLayout RenderTarget::s_descriptor_set_layout;

VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData)
{
	auto message_size = strlen(callbackData->pMessage) + 2000;
	char *message = (char *)malloc(message_size);
	assert(message);
	sprintf_s(message, message_size,
		"%s\n",
		callbackData->pMessage);

	char tmp_message[2000];
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

	if (messageSeverity< VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		return true;
	}
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

	vk::ApplicationInfo appInfo = { "Vulkan Test", 1, "EngineName", 0, VK_API_VERSION_1_3 };
	std::vector<const char*> LayerName =
	{
 #if _DEBUG
 			"VK_LAYER_KHRONOS_validation"
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

	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = m_dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	vk::InstanceCreateInfo instanceInfo = {};
	instanceInfo.setPApplicationInfo(&appInfo);
	instanceInfo.setEnabledExtensionCount((uint32_t)ExtensionName.size());
	instanceInfo.setPpEnabledExtensionNames(ExtensionName.data());
	instanceInfo.setEnabledLayerCount((uint32_t)LayerName.size());
	instanceInfo.setPpEnabledLayerNames(LayerName.data());
	m_instance = vk::createInstanceUnique(instanceInfo);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance.get());

#if USE_DEBUG_REPORT
	vk::DebugUtilsMessengerCreateInfoEXT debug_create_info;
	debug_create_info.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning/* | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose*/);
	debug_create_info.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
	debug_create_info.setPfnUserCallback(debug_messenger_callback);
	m_debug_messenger = m_instance->createDebugUtilsMessengerEXTUnique(debug_create_info);
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
			VK_KHR_MAINTENANCE3_EXTENSION_NAME,
			VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
			VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
			VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
			VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
			VK_KHR_RAY_QUERY_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
			VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
			VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME, 
			VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
			VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,
			VK_NV_MESH_SHADER_EXTENSION_NAME,
		};

		auto gpu_propaty = m_physical_device.getProperties();
		auto gpu_feature = m_physical_device.getFeatures();
		assert(gpu_feature.multiDrawIndirect);
		auto p2 = m_physical_device.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceMeshShaderPropertiesNV>();
	
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

		vk::PhysicalDevice16BitStorageFeatures storahe16_F;
		storahe16_F.storageBuffer16BitAccess = VK_TRUE;
		storahe16_F.storagePushConstant16 = VK_TRUE;
		storage8bit_feature.setPNext(&storahe16_F);

		vk::PhysicalDeviceFloat16Int8FeaturesKHR  f16s8_feature;
		f16s8_feature.setShaderInt8(VK_TRUE);
		f16s8_feature.setShaderFloat16(VK_TRUE);
		storahe16_F.setPNext(&f16s8_feature);

		vk::PhysicalDeviceImagelessFramebufferFeatures imageless_feature;
		imageless_feature.setImagelessFramebuffer(VK_TRUE);
		f16s8_feature.setPNext(&imageless_feature);

		vk::PhysicalDeviceBufferDeviceAddressFeatures BufferDeviceAddres_Feature;
		BufferDeviceAddres_Feature.bufferDeviceAddress = VK_TRUE;
		imageless_feature.setPNext(&BufferDeviceAddres_Feature);

		vk::PhysicalDeviceAccelerationStructureFeaturesKHR AS_Feature;
		AS_Feature.accelerationStructure = VK_TRUE;
		AS_Feature.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
		BufferDeviceAddres_Feature.setPNext(&AS_Feature);

		vk::PhysicalDeviceRayTracingPipelineFeaturesKHR RT_Pipeline_Feature;
		RT_Pipeline_Feature.rayTracingPipeline = VK_TRUE;
		AS_Feature.setPNext(&RT_Pipeline_Feature);

		vk::PhysicalDeviceRayQueryFeaturesKHR RayQuery_Feature;
		RayQuery_Feature.rayQuery = VK_TRUE;
		RT_Pipeline_Feature.setPNext(&RayQuery_Feature);

		vk::PhysicalDeviceScalarBlockLayoutFeatures ScalarBlock_Feature;
		ScalarBlock_Feature.scalarBlockLayout = VK_TRUE;
		RayQuery_Feature.setPNext(&ScalarBlock_Feature);

		vk::PhysicalDeviceDescriptorIndexingFeatures DescriptorIndexing_Feature;
 		DescriptorIndexing_Feature.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
 		DescriptorIndexing_Feature.descriptorBindingVariableDescriptorCount = VK_TRUE;
		DescriptorIndexing_Feature.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
		DescriptorIndexing_Feature.descriptorBindingPartiallyBound = VK_TRUE;
		DescriptorIndexing_Feature.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		DescriptorIndexing_Feature.runtimeDescriptorArray = VK_TRUE;
		DescriptorIndexing_Feature.descriptorBindingVariableDescriptorCount = VK_TRUE;
		ScalarBlock_Feature.setPNext(&DescriptorIndexing_Feature);

		vk::PhysicalDeviceSubgroupSizeControlFeaturesEXT Subgroup_Feature;
		Subgroup_Feature.subgroupSizeControl = VK_TRUE;
		DescriptorIndexing_Feature.setPNext(&Subgroup_Feature);

		vk::PhysicalDeviceMeshShaderFeaturesNV MeshShader_Feature;
		MeshShader_Feature.meshShader = VK_TRUE;
		MeshShader_Feature.taskShader = VK_TRUE;
		Subgroup_Feature.setPNext(&MeshShader_Feature);

		vk::PhysicalDeviceDynamicRenderingFeaturesKHR DynamicRender_Feature;
		DynamicRender_Feature.dynamicRendering = VK_TRUE;
		MeshShader_Feature.setPNext(&DynamicRender_Feature);

		vk::PhysicalDeviceShaderAtomicFloatFeaturesEXT AtomicFloat_Feature;
		AtomicFloat_Feature.shaderBufferFloat32Atomics = VK_TRUE;
		AtomicFloat_Feature.shaderSharedFloat32Atomics = VK_TRUE;
		AtomicFloat_Feature.shaderBufferFloat32AtomicAdd = VK_TRUE;
		AtomicFloat_Feature.shaderSharedFloat32AtomicAdd = VK_TRUE;
		//		AtomicFloat_Feature.shaderSharedFloat64AtomicMinMax = VK_TRUE;
		DynamicRender_Feature.setPNext(&AtomicFloat_Feature);

		vk::PhysicalDeviceShaderAtomicInt64Features Atomic_Feature;
		Atomic_Feature.shaderBufferInt64Atomics = VK_TRUE;
		Atomic_Feature.shaderSharedInt64Atomics = VK_TRUE;
		AtomicFloat_Feature.setPNext(&Atomic_Feature);

		m_device = m_physical_device.createDeviceUnique(device_info, nullptr);

	}
	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance.get(), vkGetInstanceProcAddr, m_device.get(), &::vkGetDeviceProcAddr);

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
		m_context->m_vertex_memory.setup(m_physical_device, m_device.get(), vk::BufferUsageFlagBits::eVertexBuffer|vk::BufferUsageFlagBits::eIndexBuffer|vk::BufferUsageFlagBits::eIndirectBuffer| vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderBindingTableKHR |vk::BufferUsageFlagBits::eTransferDst|vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, device_memory, 1000 * 1000 * 50);
		m_context->m_uniform_memory.setup(m_physical_device, m_device.get(), vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000*1000*10);
		m_context->m_storage_memory.setup(m_physical_device, m_device.get(), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eShaderDeviceAddress| vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR| vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, device_memory, 1024 * 1024 * (1500));
		m_context->m_staging_memory.setup(m_physical_device, m_device.get(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst, host_memory, 1000 * 1000 * 500);
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
			pool_info.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
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
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageImage, 1, stage),
		};
		vk::DescriptorSetLayoutCreateInfo desc_layout_info;
		desc_layout_info.setBindingCount(array_length(binding));
		desc_layout_info.setPBindings(binding);
		RenderTarget::s_descriptor_set_layout = m_device->createDescriptorSetLayoutUnique(desc_layout_info);

	}

	sSystem::Create(m_context);
	sCameraManager::Order().setup(m_context);
	sGraphicsResource::Order().setup(m_context);
	sAppImGui::Create(m_context);

	cWindowDescriptor window_desc;
	window_desc.surface_format_request = vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	window_desc.size = vk::Extent2D(desc.m_window_size.x, desc.m_window_size.y);
	window_desc.window_name = L"Vulkan Test";

	auto window = sWindow::Order().createWindow<AppWindow>(m_context, window_desc);
	m_window = window;
	m_window_list.emplace_back(window);
	m_context->m_window = window;
}

App::~App()
{
	{
		auto precmds = m_cmd_pool->submit();
	}
	m_sync_point.wait();
	for (int i = 0; i < sGlobal::FRAME_COUNT_MAX; i++)
	{
		{
			sGlobal::Order().sync();

			uint32_t index = sGlobal::Order().getCurrentFrame();
			auto r = m_device->getFenceStatus(m_fence_list[index].get());
			if (r == vk::Result::eNotReady) {
				break;
			}
			sDebug::Order().waitFence(m_device.get(), m_fence_list[index].get());
			m_cmd_pool->resetPool();

			sCameraManager::Order().sync();
			sDeleter::Order().sync();
			m_context->m_vertex_memory.gc();
			m_context->m_uniform_memory.gc();
			m_context->m_storage_memory.gc();
			m_context->m_staging_memory.gc();

		}
	}

	RenderTarget::s_descriptor_set_layout.release();
}

void App::setup()
{
	// loop以前のcmdを実行
	auto setup_cmds = m_cmd_pool->submit();
	std::vector<vk::SubmitInfo> submitInfo =
	{
		vk::SubmitInfo()
		.setCommandBufferCount((uint32_t)setup_cmds.size())
		.setPCommandBuffers(setup_cmds.data())
	};

	auto queue = m_device->getQueue(0, 0);

	vk::FenceCreateInfo fence_info;
	auto fence = m_device->createFenceUnique(fence_info);
	queue.submit(submitInfo, fence.get());


	sDebug::Order().waitFence(m_device.get(), fence.get());

	uint32_t index = sGlobal::Order().getCurrentFrame();
	m_cmd_pool->resetPool();


	sCameraManager::Order().sync();
	sDeleter::Order().sync();
	m_context->m_vertex_memory.gc();
	m_context->m_uniform_memory.gc();
	m_context->m_storage_memory.gc();
	m_context->m_staging_memory.gc();

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
		auto precmds = m_cmd_pool->submit();
		cmds.insert(cmds.end(), std::make_move_iterator(precmds.begin()), std::make_move_iterator(precmds.end()));
	}

	m_sync_point.wait();

	cmds.insert(cmds.end(), std::make_move_iterator(m_system_cmds.begin()), std::make_move_iterator(m_system_cmds.end()));

	submit_cmds.erase(std::remove_if(submit_cmds.begin(), submit_cmds.end(), [&](auto& d) { return !d; }), submit_cmds.end());
	cmds.insert(cmds.end(), std::make_move_iterator(submit_cmds.begin()), std::make_move_iterator(submit_cmds.end()));

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

	MSG msg = {};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	for (auto it = m_window_list.begin(); it != m_window_list.end();)
	{
		(*it)->getSwapchain()->swap();
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

	{
		sGlobal::Order().sync();

		uint32_t index = sGlobal::Order().getCurrentFrame();
		sDebug::Order().waitFence(m_device.get(), m_fence_list[index].get());
		m_device->resetFences({ m_fence_list[index].get() });
		m_cmd_pool->resetPool();

		sCameraManager::Order().sync();
		sDeleter::Order().sync();
		m_context->m_vertex_memory.gc();
		m_context->m_uniform_memory.gc();
		m_context->m_storage_memory.gc();
		m_context->m_staging_memory.gc();

	}

	for (auto& request : m_window_request)
	{
		auto new_window = sWindow::Order().createWindow<AppWindow>(m_context, request);
		new_window->getSwapchain()->swap();
		m_window_list.emplace_back(std::move(new_window));
	}
	m_window_request.clear();

	{
		if (!ImGui::IsAnyWindowHovered())
		{
			auto& m_camera = cCamera::sCamera::Order().getCameraList()[0];
			m_camera->control(m_window->getInput(), 0.016f);
		}
	}

	enum 
	{
		job_system,
		job_camera,
//		job_imgui,
		job_MAX,
	};
	m_system_cmds.resize(job_MAX);
	m_sync_point.reset(job_MAX);

	{
		MAKE_THREAD_JOB(job);
		job.mJob.emplace_back(
			[&]()
		{
			m_system_cmds[job_system] = sSystem::Order().execute(m_context);
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
			m_system_cmds[job_camera] = sCameraManager::Order().draw(m_context);
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
//			m_system_cmds[2] = sParticlePipeline::Order().draw(m_context);
//			auto cmd = m_context->m_cmd_pool->allocCmdOnetime(0);
//			sAppImGui::Order().Render(cmd);

//			cmd.end();
//			m_system_cmds[job_imgui] = cmd;
//			m_sync_point.arrive();
		}
		);
		m_thread_pool.enque(job);
	}

}

void App::postUpdate()
{

}

bool App::isEnd() const
{
	return m_window_list.empty();
}

glm::uvec3 calcDipatchGroups(const glm::uvec3& num, const glm::uvec3& local_size)
{
	glm::uvec3 ret = (num + local_size - glm::uvec3(1)) / local_size;
	return ret;
}

}

AppImgui::AppImgui(const std::shared_ptr<btr::Context>& context, AppWindow* const window)
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

	
}

AppImgui::~AppImgui()
{
	int a = 0;
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
//		depth_info.flags = vk::ImageCreateFlagBits::eMutableFormat;
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
//		depth_view_info.format = vk::Format::eR32Sfloat;
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
		vk::DescriptorImageInfo images2[] = {
			vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eGeneral).setImageView(m_front_buffer->m_depth_view),
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
// 			vk::WriteDescriptorSet()
// 			.setDescriptorType(vk::DescriptorType::eStorageImage)
// 			.setDescriptorCount(array_length(images2))
// 			.setPImageInfo(images2)
// 			.setDstBinding(2)
// 			.setDstSet(m_front_buffer->m_descriptor.get()),
		};
		context->m_device.updateDescriptorSets(std::size(write), write, 0, nullptr);
	}

	m_imgui_pipeline = std::make_unique<AppImgui>(context, this);


#if USE_DEBUG_REPORT
	vk::DebugUtilsObjectNameInfoEXT name_info;

	char buf[256];
	for (int i = 0; i < m_swapchain->m_backbuffer_image.size(); i++)
	{
		sprintf_s(buf, "AppWindow Backbuffer[i]", i);
		name_info.objectHandle = reinterpret_cast<uint64_t &>(m_swapchain->m_backbuffer_image[i]);
		name_info.objectType = vk::ObjectType::eImage;
		name_info.pObjectName = buf;
		context->m_device.setDebugUtilsObjectNameEXT(name_info);
	}

	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_front_buffer->m_image);
	name_info.objectType = vk::ObjectType::eImage;
	name_info.pObjectName = "AppWindow FrontBufferImage";
	context->m_device.setDebugUtilsObjectNameEXT(name_info);

	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_front_buffer->m_view);
	name_info.objectType = vk::ObjectType::eImageView;
	name_info.pObjectName = "AppWindow FrontBufferImageView";
	context->m_device.setDebugUtilsObjectNameEXT(name_info);

	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_front_buffer->m_memory);
	name_info.objectType = vk::ObjectType::eDeviceMemory;
	name_info.pObjectName = "AppWindow FrontBufferImageMemory";
	context->m_device.setDebugUtilsObjectNameEXT(name_info);

	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_depth_image.get());
	name_info.objectType = vk::ObjectType::eImage;
	name_info.pObjectName = "AppWindow DepthImage";
	context->m_device.setDebugUtilsObjectNameEXT(name_info);

	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_depth_view.get());
	name_info.objectType = vk::ObjectType::eImageView;
	name_info.pObjectName = "AppWindow DepthImageView";
	context->m_device.setDebugUtilsObjectNameEXT(name_info);

	name_info.objectHandle = reinterpret_cast<uint64_t &>(m_depth_memory.get());
	name_info.objectType = vk::ObjectType::eDeviceMemory;
	name_info.pObjectName = "AppWindow DepthImageMemory";
	context->m_device.setDebugUtilsObjectNameEXT(name_info);
#endif

}

AppWindow::~AppWindow()
{
	int a = 0;

}
