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
	}

	std::vector<vk::LayerProperties> mLayerProperty;

	std::vector<vk::LayerProperties> mDeviceLayerProperty;
	std::vector<const char*> mDeviceLayerName;
public:
	const std::vector<vk::LayerProperties>& getLayerPropery()const { return mLayerProperty; }
	const std::vector<const char*>& getDeviceLayerName()const { return mDeviceLayerName; }
};


}
