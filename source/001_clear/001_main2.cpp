#if 0
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
#include <btrlib/cWindow.h>
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>

#include <vma/vk_mem_alloc.h>
//#include <taskflow/taskflow/taskflow.hpp>
#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <btrlib/Context.h>
#include <imgui/imgui.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#pragma comment(lib, "SDL2.lib")
#pragma comment(lib, "SDL2main.lib")
SDL_Window* window;
char* window_name = "example SDL2 Vulkan application";

#if 1
VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData)
{
	auto message_size = strlen(callbackData->pMessage) + 2000;
	char* message = (char*)malloc(message_size);
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

	if (messageSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		return true;
	}
	// Don't bail out, but keep going.
	DebugBreak();
	return false;
}

class Deleter
{
public:
	template<typename... T>
	void enque(T&&... handle)
	{
		struct HandleHolder : _Deleter
		{
			std::tuple<T...> m_handle;
			HandleHolder(T&&... arg) : m_handle(std::move(arg)...) {}
		};

		auto ptr = std::make_unique<HandleHolder>(std::move(handle)...);
		{
			std::lock_guard<std::mutex> lk(m_deleter_mutex);
			m_deleter_list[m_index].push_back(std::move(ptr));
		}
	};

	void sync()
	{
		std::lock_guard<std::mutex> lk(m_deleter_mutex);
		m_index = (m_index + 1) % m_deleter_list.size();
		m_deleter_list[m_index] = {};
	}

private:
	struct _Deleter
	{
		virtual ~_Deleter() { ; }
	};
	std::array<std::vector<std::unique_ptr<Deleter>>, sGlobal::FRAME_COUNT_MAX> m_deleter_list;
	std::mutex m_deleter_mutex;
	int m_index = 0;

};

struct GPU
{
	vk::PhysicalDevice m_physical_device;
	vk::UniqueDevice m_device;
	vk::DispatchLoaderDynamic m_dispacher;

	std::shared_ptr<cCmdPool> m_cmd_pool;
	std::shared_ptr<btr::Context> m_context;
	std::vector<vk::UniqueFence> m_fence_list;

	std::shared_ptr<cCmdPool> m_cmd_pool;

};
struct App2
{
	vk::DynamicLoader m_dl;
	vk::UniqueInstance m_instance;
	vk::UniqueDebugUtilsMessengerEXT m_debug_messenger;

	std::vector<GPU> m_gpu;
	vk::PhysicalDevice m_physical_device;
	vk::Device m_device_default;

	App2()
	{
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
		m_gpu.reserve(gpus.size());
		for (auto& _gpu:gpus)
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
			auto gpu_features = m_physical_device.getFeatures2<vk::PhysicalDeviceFeatures2
				, vk::PhysicalDeviceVulkan11Features
				, vk::PhysicalDeviceVulkan12Features
				, vk::PhysicalDeviceVulkan13Features
				, vk::PhysicalDeviceAccelerationStructureFeaturesKHR
				, vk::PhysicalDeviceRayTracingPipelineFeaturesKHR
				, vk::PhysicalDeviceRayQueryFeaturesKHR
				, vk::PhysicalDeviceShaderAtomicFloatFeaturesEXT
				, vk::PhysicalDeviceMeshShaderFeaturesNV
				, vk::PhysicalDeviceShaderIntegerFunctions2FeaturesINTEL
				, vk::PhysicalDeviceDescriptorSetHostMappingFeaturesVALVE
			>();

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

			auto features = gpu_features.get<vk::PhysicalDeviceFeatures2>();
			vk::DeviceCreateInfo device_info;
			device_info.setQueueCreateInfoCount((uint32_t)queue_info.size());
			device_info.setPQueueCreateInfos(queue_info.data());
			device_info.setEnabledExtensionCount((uint32_t)extensionName.size());
			device_info.setPpEnabledExtensionNames(extensionName.data());
			device_info.setPNext(&features);
			auto _device = _gpu.createDeviceUnique(device_info, nullptr);

			GPU gpu;
			gpu.m_physical_device = _gpu;
			gpu.m_dispacher.init(m_instance.get(), vkGetInstanceProcAddr, _device.get(), &::vkGetDeviceProcAddr);
			gpu.m_device = std::move(_device);

			{
				VmaAllocator allocator;
				VmaVulkanFunctions funcs;
				funcs.vkAllocateMemory = gpu.m_dispacher.vkAllocateMemory;
				funcs.vkBindBufferMemory = gpu.m_dispacher.vkBindBufferMemory;
				funcs.vkBindImageMemory = gpu.m_dispacher.vkBindImageMemory;
				funcs.vkCmdCopyBuffer = gpu.m_dispacher.vkCmdCopyBuffer;
				funcs.vkCreateBuffer = gpu.m_dispacher.vkCreateBuffer;
				funcs.vkCreateImage = gpu.m_dispacher.vkCreateImage;
				funcs.vkDestroyBuffer = gpu.m_dispacher.vkDestroyBuffer;
				funcs.vkDestroyImage = gpu.m_dispacher.vkDestroyImage;
				funcs.vkFlushMappedMemoryRanges = gpu.m_dispacher.vkFlushMappedMemoryRanges;
				funcs.vkFreeMemory = gpu.m_dispacher.vkFreeMemory;
				funcs.vkGetBufferMemoryRequirements = gpu.m_dispacher.vkGetBufferMemoryRequirements;
				funcs.vkGetBufferMemoryRequirements2KHR = gpu.m_dispacher.vkGetBufferMemoryRequirements2KHR;
				funcs.vkGetDeviceBufferMemoryRequirements = gpu.m_dispacher.vkGetDeviceBufferMemoryRequirements;
				funcs.vkGetDeviceImageMemoryRequirements = gpu.m_dispacher.vkGetDeviceImageMemoryRequirements;
				funcs.vkGetDeviceProcAddr = gpu.m_dispacher.vkGetDeviceProcAddr;
				funcs.vkGetImageMemoryRequirements = gpu.m_dispacher.vkGetImageMemoryRequirements;
				funcs.vkGetImageMemoryRequirements2KHR = gpu.m_dispacher.vkGetImageMemoryRequirements2KHR;
				funcs.vkGetInstanceProcAddr = gpu.m_dispacher.vkGetInstanceProcAddr;
				funcs.vkGetPhysicalDeviceMemoryProperties = gpu.m_dispacher.vkGetPhysicalDeviceMemoryProperties;
				funcs.vkGetPhysicalDeviceMemoryProperties2KHR = gpu.m_dispacher.vkGetPhysicalDeviceMemoryProperties2KHR;
				funcs.vkGetPhysicalDeviceProperties = gpu.m_dispacher.vkGetPhysicalDeviceProperties;
				funcs.vkInvalidateMappedMemoryRanges = gpu.m_dispacher.vkInvalidateMappedMemoryRanges;
				funcs.vkMapMemory = gpu.m_dispacher.vkMapMemory;
				funcs.vkUnmapMemory = gpu.m_dispacher.vkUnmapMemory;

				VmaAllocatorCreateInfo allocatorCI{
					.physicalDevice = gpu.m_physical_device,
					.device = gpu.m_device.get(),
					.preferredLargeHeapBlockSize = 0,
					.pVulkanFunctions = &funcs,
					.instance = m_instance.get(),
					.vulkanApiVersion = appInfo.apiVersion,
				};
				vmaCreateAllocator(&allocatorCI, &allocator);

				VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
				bufferInfo.size = 65536;
				bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;


				VkBuffer buffer;
				VmaAllocation allocation;
				VmaAllocationCreateInfo allocInfo = {};
				allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
				vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

			}
			m_gpu.push_back(gpu);
		}

		m_device_default = m_gpu[0].m_device.get();
		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance.get(), vkGetInstanceProcAddr, m_device_default, &::vkGetDeviceProcAddr);


//		m_cmd_pool = std::make_shared<cCmdPool>(m_context);
//		m_context->m_cmd_pool = m_cmd_pool;
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
			RenderTarget::s_descriptor_set_layout = m_device_default.createDescriptorSetLayoutUnique(desc_layout_info);
		}

//		sSystem::Create(m_context);
//		sCameraManager::Order().setup(m_context);
//		sGraphicsResource::Order().setup(m_context);
//		sAppImGui::Create(m_context);
	}

	~App2()
	{
		for (int i = 0; i < sGlobal::FRAME_COUNT_MAX; i++)
		{
			{
				sGlobal::Order().sync();

// 				uint32_t index = sGlobal::Order().getCurrentFrame();
// 				auto r = m_device_default.getFenceStatus(m_fence_list[index].get());
// 				if (r == vk::Result::eNotReady) {
// 					break;
// 				}
// 				if (m_device_default.getFenceStatus(m_fence_list[index].get()) != vk::Result::eSuccess) {
// 					auto result = m_device_default.waitForFences(m_fence_list[index].get(), VK_TRUE, 0xffffffffLLu);
// 					assert(result == vk::Result::eSuccess);
// 				}
			}
		}
		RenderTarget::s_descriptor_set_layout.release();
	}

	void setup()
	{
		// loop以前のcmdを実行
		auto setup_cmds = m_cmd_pool->submit();
		std::vector<vk::SubmitInfo> submitInfo =
		{
			vk::SubmitInfo()
			.setCommandBufferCount((uint32_t)setup_cmds.size())
			.setPCommandBuffers(setup_cmds.data())
		};

		auto queue = m_device_default.getQueue(0, 0);

// 		vk::FenceCreateInfo fence_info;
// 		auto fence = m_device_default.createFenceUnique(fence_info);
// 		queue.submit(submitInfo, fence.get());

//		sDebug::Order().waitFence(m_device_default.get(), fence.get());

		uint32_t index = sGlobal::Order().getCurrentFrame();
		m_cmd_pool->resetPool();


//		sCameraManager::Order().sync();
		sDeleter::Order().sync();

	}


	void submit(std::vector<vk::CommandBuffer>&& submit_cmds)
	{
		std::vector<vk::Semaphore> waitSemaphores(m_window_list.size());
		std::vector<vk::Semaphore> signalSemaphores(m_window_list.size());
		std::vector<vk::SwapchainKHR> swapchains(m_window_list.size());
		std::vector<uint32_t> backbuffer_indexs(m_window_list.size());
		std::vector<vk::PipelineStageFlags> wait_pipelines(m_window_list.size());
		for (size_t i = 0; i < m_window_list.size(); i++)
		{
			auto& window = m_window_list[i];
			waitSemaphores[i] = window->getSwapchain()->m_semaphore_imageAvailable.get();
			signalSemaphores[i] = window->getSwapchain()->m_semaphore_renderFinished.get();
			swapchains[i] = window->getSwapchain()->m_swapchain_handle.get();
			backbuffer_indexs[i] = window->getSwapchain()->m_backbuffer_index;
			wait_pipelines[i] = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		}

		std::vector<vk::CommandBuffer> cmds;
		cmds.reserve(32);

		{
			auto precmds = m_cmd_pool->submit();
			cmds.insert(cmds.end(), std::make_move_iterator(precmds.begin()), std::make_move_iterator(precmds.end()));
		}


		vk::SubmitInfo submit_info;
		submit_info.setCommandBuffers(cmds);
		submit_info.setWaitSemaphores(waitSemaphores);
		submit_info.setWaitDstStageMask(wait_pipelines);
		submit_info.setSignalSemaphores(signalSemaphores);

		auto queue = m_device_default.getQueue(0, 0);
// 		queue.submit(submit_info, m_fence_list[sGlobal::Order().getCurrentFrame()].get());

		vk::PresentInfoKHR present_info;
		present_info.setSwapchains(swapchains);
		present_info.setImageIndices(backbuffer_indexs);
		present_info.setWaitSemaphores(signalSemaphores);
		queue.presentKHR(present_info);
	}

	void preUpdate()
	{
		sGlobal::Order().sync();

		uint32_t index = sGlobal::Order().getCurrentFrame();
//		m_cmd_pool->resetPool();


		{
			sDeleter::Order().sync();
		}

 		{
 			if (!ImGui::IsAnyWindowHovered())
// 			{
// 				auto& m_camera = cCamera::sCamera::Order().getCameraList()[0];
// 				m_camera->control(m_window->getInput(), 0.016f);
// 			}
 		}

	}
};
#endif

#include <taskflow/taskflow/taskflow.hpp>
int main(int argc, char* argv[])
{
	SDL_Init(SDL_INIT_EVERYTHING);

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	window = SDL_CreateWindow(window_name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

	VkSurfaceKHR surface;
	SDL_Vulkan_CreateSurface(window, app.m_instance.get(), &surface);
	SDL_Vulkan_CreateSurface()

	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto context = app.m_context;

	app.setup();

	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_render_target(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

	SDL_Event event;
	bool running = true;
	while (running)
	{
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
			{
				running = false;
			}
		}
		cStopWatch time;

		app.preUpdate();
		{
			std::vector<vk::CommandBuffer> cmds =
			{
				clear_render_target.execute(),
				present_pipeline.execute(),
			};
			app.submit(std::move(cmds));
		}
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

#endif