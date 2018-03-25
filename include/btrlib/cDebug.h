#pragma once
#include <btrlib/Define.h>


struct cDebug {
	explicit cDebug(vk::Instance&  instance)
	{
		static auto dbgfnc = [](VkDebugReportFlagsEXT flag, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* userData) -> VkBool32
		{
			char buf[29019] = {};
			sprintf_s(buf, "%s %s %s\n  %s\n",
				vk::to_string(vk::DebugReportFlagBitsEXT(flag)).c_str(), vk::to_string(vk::DebugReportObjectTypeEXT(objectType)).c_str(), pLayerPrefix, pMessage);
//			OutputDebugStringA("------------------------callback------------------------\n");
//			OutputDebugStringA(buf);
			printf("------------------------callback------------------------\n");
			printf(buf);
	 		if (vk::DebugReportFlagBitsEXT(flag) == vk::DebugReportFlagBitsEXT::eError) {
				return static_cast<VkBool32>(vk::Result::eSuccess);
			}
			return static_cast<VkBool32>(vk::Result::eSuccess);
		};
		vk::DebugReportCallbackCreateInfoEXT debugInfo =
		{
			vk::DebugReportFlagBitsEXT::eError
//			| vk::DebugReportFlagBitsEXT::eDebug
			| vk::DebugReportFlagBitsEXT::eWarning
//			| vk::DebugReportFlagBitsEXT::eInformation 
			| vk::DebugReportFlagBitsEXT::ePerformanceWarning
			, dbgfnc, nullptr
		};
		m_create_debug_report = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(instance.getProcAddr("vkCreateDebugReportCallbackEXT"));
		m_destroy_debug_report = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(instance.getProcAddr("vkDestroyDebugReportCallbackEXT"));

		VkDebugReportCallbackEXT debugReport;
		m_create_debug_report(instance.operator VkInstance(), reinterpret_cast<const VkDebugReportCallbackCreateInfoEXT*>(&debugInfo), nullptr, &debugReport);

	}

	PFN_vkCreateDebugReportCallbackEXT m_create_debug_report;
	PFN_vkDestroyDebugReportCallbackEXT m_destroy_debug_report;
};