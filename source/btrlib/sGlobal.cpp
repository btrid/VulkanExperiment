#include <btrlib/cWindow.h>
#include <btrlib/sValidationLayer.h>

#include <filesystem>
#include <vector>
#include <iostream>
#include <fstream>

namespace btr{
void* VKAPI_PTR Allocation(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	return malloc(size);
}

void* VKAPI_PTR Reallocation(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	return realloc(pOriginal, size);

}

void VKAPI_PTR Free(void* pUserData, void* pMemory)
{
	free(pMemory);
}

void VKAPI_PTR InternalAllocationNotification(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
	printf("alloc\n");
}

void VKAPI_PTR InternalFreeNotification(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
	printf("free\n");
}

}

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
		strcpy_s(prefix, "VERBOSE : ");
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
	{
		strcpy_s(prefix, "INFO : ");
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		strcpy_s(prefix, "WARNING : ");
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		strcpy_s(prefix, "ERROR : ");
	}
	if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
	{
		strcat_s(prefix, "GENERAL");
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
		"%s - Message ID Number %d, Message ID Name %s\n %s",
		prefix,
		callbackData->messageIdNumber,
		callbackData->pMessageIdName,
		callbackData->pMessage);
	if (callbackData->objectCount > 0) {
		char tmp_message[500];
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
	if (callbackData->cmdBufLabelCount > 0) {
		char tmp_message[500];
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


sGlobal::sGlobal()
	: m_current_frame(0)
	, m_game_frame(0)
	, m_tick_tock(0)
	, m_totaltime(0.f)
{
	m_deltatime = 0.016f;
	{
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
		m_instance = vk::createInstance(instanceInfo);

#if USE_DEBUG_REPORT
		m_dispatch = vk::DispatchLoaderDynamic(m_instance);
		vk::DebugUtilsMessengerCreateInfoEXT debug_create_info;
		debug_create_info.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning/* | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose*/);
		debug_create_info.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
		debug_create_info.setPfnUserCallback(debug_messenger_callback);
		m_debug_messenger = m_instance.createDebugUtilsMessengerEXTUnique(debug_create_info, nullptr, m_dispatch);
#endif
	}

	{
		auto gpus = m_instance.enumeratePhysicalDevices();
		m_gpu.reserve(gpus.size());
		for (auto&& pd : gpus)
		{
			m_gpu.emplace_back();
			m_gpu.back().setup(pd);
		}
	}
	auto init_thread_data_func = [=](const cThreadPool::InitParam& param)
	{
		SetThreadIdealProcessor(::GetCurrentThread(), param.m_index);
		auto& data = sThreadLocal::Order();
		data.m_thread_index = param.m_index;
	};
	m_thread_pool.start(std::thread::hardware_concurrency() - 1, init_thread_data_func);
// 	m_thread_pool_sound.start(1, [](const cThreadPool::InitParam& param){
// 		if (param.m_index == 1)
// 		{
// 			SetThreadIdealProcessor(::GetCurrentThread(), 7);
// 		}
// 	});
}


void sGlobal::sync()
{
	m_game_frame++;
	m_game_frame = m_game_frame % (std::numeric_limits<decltype(m_game_frame)>::max() / FRAME_MAX*FRAME_MAX);
	m_current_frame = m_game_frame % FRAME_MAX;
	m_tick_tock = (m_tick_tock + 1) % 2;
	auto next = (m_current_frame+1) % FRAME_MAX;
	m_deltatime = m_timer.getElapsedTimeAsSeconds();
	m_deltatime = glm::min(m_deltatime, 0.02f);
	m_totaltime += m_deltatime;
}


vk::UniqueShaderModule loadShaderUnique(const vk::Device& device, const std::string& filename)
{
	std::experimental::filesystem::path filepath(filename);
	std::ifstream file(filepath, std::ios_base::ate | std::ios::binary);
	assert(file.is_open());

	size_t file_size = (size_t)file.tellg();
	file.seekg(0);

	std::vector<char> buffer(file_size);
	file.read(buffer.data(), buffer.size());

	vk::ShaderModuleCreateInfo shaderInfo = vk::ShaderModuleCreateInfo()
		.setPCode(reinterpret_cast<const uint32_t*>(buffer.data()))
		.setCodeSize(buffer.size());
	return device.createShaderModuleUnique(shaderInfo);
}
vk::UniqueDescriptorPool createDescriptorPool(vk::Device device, const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& bindings, uint32_t set_size)
{
	std::vector<vk::DescriptorPoolSize> pool_size(VK_DESCRIPTOR_TYPE_RANGE_SIZE);
	for (auto& binding : bindings)
	{
		for (auto& b : binding)
		{
			pool_size[(uint32_t)b.descriptorType].setType(b.descriptorType);
			pool_size[(uint32_t)b.descriptorType].descriptorCount += b.descriptorCount*set_size;
		}
	}
	pool_size.erase(std::remove_if(pool_size.begin(), pool_size.end(), [](auto& p) {return p.descriptorCount == 0; }), pool_size.end());
	vk::DescriptorPoolCreateInfo pool_info;
	pool_info.setPoolSizeCount((uint32_t)pool_size.size());
	pool_info.setPPoolSizes(pool_size.data());
	pool_info.setMaxSets(set_size);
	return device.createDescriptorPoolUnique(pool_info);

}
