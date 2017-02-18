#pragma once

#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <btrlib/sValidationLayer.h>
#include <vector>

#include <iostream>
#include <fstream>
#include <filesystem>

class sShaderModule : public Singleton<sShaderModule>
{
	friend Singleton<sShaderModule>;

};

/// Logical Device
class cDevice
{
	friend class cGPU;
public:
	cDevice() {}
	~cDevice()
	{}

	vk::Device*			operator->()		{ return &m_handle; }
	const vk::Device*	operator->()const	{ return &m_handle; }
	uint32_t getQueueFamilyIndex()const { return m_family_index; }
	const vk::Device& getHandle()const { return m_handle; }

private:
	vk::Device m_handle;
	uint32_t m_family_index;

};

class cGPU
{
//	cGPU(const cGPU&) = delete;
//	cGPU& operator=(const cGPU&) = delete;
public:
	cGPU() = default;
	~cGPU() = default;
	friend class sVulkan;
public:
	vk::PhysicalDevice*			operator->() { return &m_handle; }
	const vk::PhysicalDevice*	operator->()const { return &m_handle; }
	const vk::PhysicalDevice&	getHandle()const { return m_handle; }

	std::vector<uint32_t> getQueueFamilyIndexList(vk::QueueFlags flag)const;
	std::vector<uint32_t> getQueueFamilyIndexList(vk::QueueFlags flag, const std::vector<uint32_t>& useIndex);
	int getMemoryTypeIndex(const vk::MemoryRequirements& request, vk::MemoryPropertyFlags flag)const;
	vk::Format getSupportedDepthFormat(const std::vector<vk::Format>& depthFormat)const;

	std::vector<cDevice> getDevice(vk::QueueFlags flag) const;
	cDevice getDeviceByFamilyIndex(uint32_t index) const;

	vk::PhysicalDevice m_handle;
	std::vector<vk::Device> m_device_list;


	struct Helper
	{
//		vk::SurfaceFormatKHR getSurfaceFormat(vk::PhysicalDevice gpu, vk::SurfaceKHR surface, std::vector<vk::SurfaceFormatKHR> search);
	};
};


class sVulkan : public Singleton<sVulkan>
{
	friend Singleton<sVulkan>;
protected:
	sVulkan();
public:
	vk::Instance& getVKInstance() { return m_instance; }
	cGPU& getGPU(int index) { return m_gpu[index]; }
private:
	vk::Instance m_instance;
	std::vector<cGPU> m_gpu;

};
