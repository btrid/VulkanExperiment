#pragma once

#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>

#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <btrlib/sValidationLayer.h>
#include <btrlib/cThreadPool.h>

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
	const vk::PhysicalDevice& getGPU()const { return m_gpu; }

private:
	vk::PhysicalDevice m_gpu;
	vk::Device m_handle;
	uint32_t m_family_index;

};

class cGPU
{
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
		static int getMemoryTypeIndex(const vk::PhysicalDevice& gpu, const vk::MemoryRequirements& request, vk::MemoryPropertyFlags flag)
		{
			auto& prop = gpu.getMemoryProperties();
			auto typeBits = static_cast<std::int32_t>(request.memoryTypeBits);
			for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
			{
				if ((prop.memoryTypes[i].propertyFlags & flag) == flag)
				{
					return static_cast<int>(i);
				}
			}
			assert(false);
			return -1;
		}
	};
};


class sVulkan : public Singleton<sVulkan>
{
	friend Singleton<sVulkan>;
protected:
	sVulkan();
public:
	enum
	{
		FRAME_MAX = 3,
	};

	vk::Instance& getVKInstance() { return m_instance; }
	cGPU& getGPU(int index) { return m_gpu[index]; }
	void swap();
	int getCurrentFrame()const { return m_current_frame; }

public:
	cThreadPool& getThreadPool() { return m_thread_pool; }
private:
	vk::Instance m_instance;
	std::vector<cGPU> m_gpu;

	int m_current_frame;
	cThreadPool m_thread_pool;
};

struct sThreadData : public SingletonTLS<sThreadData>
{
	friend SingletonTLS<sThreadData>;
	cGPU m_gpu;
	int m_thread_index;

	enum DeviceIndex {
		DEVICE_GRAPHICS,
		DEVICE_COMPUTE,
		DEVICE_TRANSFAR,
		DEVICE_MAX,
	};
	cDevice m_device[DEVICE_MAX];


	std::vector<std::array<vk::CommandPool, sVulkan::FRAME_MAX>>	m_cmd_pool_onetime;
	std::vector<std::array<vk::CommandPool, sVulkan::FRAME_MAX>>	m_cmd_pool_compiled;

public:
	vk::CommandPool getCmdPoolOnetime(int device_family_index)const { return m_cmd_pool_onetime[device_family_index][sVulkan::Order().getCurrentFrame()]; }
	std::array<vk::CommandPool, sVulkan::FRAME_MAX> getCmdPoolCompiled(int device_family_index)const { return m_cmd_pool_onetime[device_family_index]; }

	vk::CommandBuffer allocateCmdOnetime(const cDevice& device)const 
	{
		auto pool = sThreadData::Order().getCmdPoolOnetime(device.getQueueFamilyIndex());
		vk::CommandBufferAllocateInfo cmd_info;
		cmd_info.setCommandPool(pool);
		cmd_info.setLevel(vk::CommandBufferLevel::ePrimary);
		cmd_info.setCommandBufferCount(1);
		return device->allocateCommandBuffers(cmd_info)[0];

	}

};


class GPUResource
{
public:
	class Manager : public Singleton<Manager>
	{
	protected:
		Manager()
		{}
		friend Singleton<Manager>;
		std::array<std::vector<GPUResource*>, 3> deffered;

		std::mutex deffered_mutex;
	public:
		template<typename T, typename... Arg>
		T* create(Arg... arg)
		{
			return new T(arg...);
		}

		template<typename T>
		void destroy(T* resource)
		{
			std::unique_lock<std::mutex> lock(deffered_mutex);
			deffered[sVulkan::Order().getCurrentFrame()].push_back(resource);
		}

		void swap(int frame)
		{
			frame = ++frame % 3;
			auto& l = deffered[frame];
			for (auto it : l)
			{
				delete it;
			}
			l.clear();
		}
	};
protected:
	GPUResource() = default;
	virtual ~GPUResource() = default;
// 	GPUResource(const GPUResource&) = delete;
// 	GPUResource& operator=(const GPUResource&) = delete;

	GPUResource(GPUResource&&) = default;
	GPUResource& operator = (GPUResource&&) = default;
protected:

	virtual bool isEnd() const = 0;
private:

public:
};

struct UniformBuffer
{
	vk::Buffer mBuffer;
	vk::DeviceMemory mMemory;
	vk::DescriptorBufferInfo mBufferInfo;

};

vk::ShaderModule loadShader(const vk::Device& device, const std::string& filename);
