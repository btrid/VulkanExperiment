#pragma once

#include "Define.h"
#include <vector>

#include "Singleton.h"
namespace btr
{

class sValidationLayer : public Singleton<sValidationLayer>
{
	friend Singleton<sValidationLayer>;

protected:
	sValidationLayer()
	{
		// layer prop
		mLayerProperty = vk::enumerateInstanceLayerProperties();

		mLayerName =
		{
			"VK_LAYER_LUNARG_standard_validation"
// 			"VK_LAYER_LUNARG_monitor",
// 			"VK_LAYER_GOOGLE_unique_objects",
//			"VK_LAYER_LUNARG_core_validation",
// 			"VK_LAYER_LUNARG_image"
// 			"VK_LAYER_LUNARG_object_tracker"
// 			"VK_LAYER_LUNARG_parameter_validation",
// 			"VK_LAYER_LUNARG_screenshot",
// 			"VK_LAYER_LUNARG_swapchain",
// 			"VK_LAYER_GOOGLE_threading",
//			"VK_LAYER_LUNARG_api_dump",
		};
		mExtensionName.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
//		mExtensionName.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
		mExtensionName.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		mExtensionName.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
	}

	std::vector<vk::LayerProperties> mLayerProperty;
	std::vector<const char*> mLayerName;
	std::vector<const char*> mExtensionName;


	std::vector<vk::LayerProperties> mDeviceLayerProperty;
	std::vector<const char*> mDeviceLayerName;
public:
	const std::vector<vk::LayerProperties>& getLayerPropery()const { return mLayerProperty; }
	const std::vector<const char*>& getLayerName()const { return mLayerName; }
	const std::vector<const char*>& getExtensionName()const { return mExtensionName; }
	const std::vector<const char*>& getDeviceLayerName()const { return mDeviceLayerName; }
};


}
