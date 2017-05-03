#pragma once

#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <climits>

#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <btrlib/sValidationLayer.h>
#include <btrlib/cThreadPool.h>
#include <btrlib/sDebug.h>

namespace vk
{

struct FenceShared
{
	std::shared_ptr<vk::Fence> m_fence;
	void create(vk::Device device, const vk::FenceCreateInfo& fence_info);

	vk::Fence getHandle()const { return m_fence ? *m_fence : vk::Fence(); }
	operator VkFence() const { return getHandle(); }
	vk::Fence operator*() const { return getHandle(); }
};

}

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


	const std::vector<uint32_t>& getQueueFamilyIndex()const { return m_family_index; }
	uint32_t getQueueFamilyIndex(vk::QueueFlags reqest_queue)const 
	{
//		std::vector<uint32_t> index;
		auto queueProp = m_gpu.getQueueFamilyProperties();
		for (size_t i = 0; i < queueProp.size(); i++) {
			if (btr::isOn(queueProp[i].queueFlags, reqest_queue)) 
			{
				if (std::find(m_family_index.begin(), m_family_index.end(), i) != m_family_index.end())
				{
					return (uint32_t)i;
				}
			}
		}
		return (uint32_t)-1;
	}
	uint32_t getQueueNum(uint32_t family_index)const { return (uint32_t)m_queue_info[family_index].size(); }
	uint32_t getQueueNum(vk::QueueFlags reqest_queue)const
	{ 
		return (uint32_t)m_queue_info[getQueueFamilyIndex(reqest_queue)].size();
	}
	const vk::Device& getHandle()const { return m_handle; }
	const vk::Device& operator*()const { return m_handle; }
	const vk::PhysicalDevice& getGPU()const { return m_gpu; }

	void DebugMarkerSetObjectTag(vk::DebugMarkerObjectTagInfoEXT* pTagInfo)const 
	{ m_vk_debug_marker_set_object_tag(m_handle, (VkDebugMarkerObjectTagInfoEXT*)pTagInfo); }
	void DebugMarkerSetObjectName(vk::DebugMarkerObjectNameInfoEXT* pTagInfo)const 
	{ m_vk_debug_marker_set_object_name(m_handle, (VkDebugMarkerObjectNameInfoEXT*)pTagInfo); }
	void CmdDebugMarkerBegin(vk::CommandBuffer cmd, vk::DebugMarkerMarkerInfoEXT* pTagInfo)const 
	{ m_vk_cmd_debug_marker_begin(cmd, (VkDebugMarkerMarkerInfoEXT*)pTagInfo); }
	void CmdDebugMarkerEnd(vk::CommandBuffer cmd)const
	{ m_vk_cmd_debug_marker_end(cmd);}
	void CmdDebugMarkerInsert(vk::CommandBuffer cmd, vk::DebugMarkerMarkerInfoEXT* pTagInfo)const
	{ m_vk_cmd_debug_marker_insert(cmd, (VkDebugMarkerMarkerInfoEXT*)pTagInfo); }
private:
	vk::PhysicalDevice m_gpu;
	vk::Device m_handle;
	std::vector<uint32_t> m_family_index;
	std::vector<std::vector<float>> m_queue_info;
	PFN_vkDebugMarkerSetObjectTagEXT m_vk_debug_marker_set_object_tag;
	PFN_vkDebugMarkerSetObjectNameEXT m_vk_debug_marker_set_object_name;
	PFN_vkCmdDebugMarkerBeginEXT m_vk_cmd_debug_marker_begin;
	PFN_vkCmdDebugMarkerEndEXT m_vk_cmd_debug_marker_end;
	PFN_vkCmdDebugMarkerInsertEXT m_vk_cmd_debug_marker_insert;

};

class cGPU
{
public:
	cGPU() = default;
	~cGPU() = default;
	friend class sGlobal;
public:
	vk::PhysicalDevice*			operator->() { return &m_handle; }
	const vk::PhysicalDevice*	operator->()const { return &m_handle; }
	const vk::PhysicalDevice&	getHandle()const { return m_handle; }

	std::vector<uint32_t> getQueueFamilyIndexList(vk::QueueFlags flag)const;
	std::vector<uint32_t> getQueueFamilyIndexList(vk::QueueFlags flag, const std::vector<uint32_t>& useIndex);
	int getMemoryTypeIndex(const vk::MemoryRequirements& request, vk::MemoryPropertyFlags flag)const;
	vk::Format getSupportedDepthFormat(const std::vector<vk::Format>& depthFormat)const;

	// @deplicated
	std::vector<cDevice> getDevice(vk::QueueFlags flag) const;
	// @deplicated
	cDevice getDeviceByFamilyIndex(uint32_t index) const;
private:

	struct Device
	{
		vk::Device m_device;
		std::vector<uint32_t> m_family_index;
		std::vector<std::vector<float>> m_queue;
		PFN_vkDebugMarkerSetObjectTagEXT m_vk_debug_marker_set_object_tag;
		PFN_vkDebugMarkerSetObjectNameEXT m_vk_debug_marker_set_object_name;
		PFN_vkCmdDebugMarkerBeginEXT m_vk_cmd_debug_marker_begin;
		PFN_vkCmdDebugMarkerEndEXT m_vk_cmd_debug_marker_end;
		PFN_vkCmdDebugMarkerInsertEXT m_vk_cmd_debug_marker_insert;
	};
	vk::PhysicalDevice m_handle;
	std::vector<Device> m_device_list;
public:
	struct Helper
	{
		static uint32_t getMemoryTypeIndex(const vk::PhysicalDevice& gpu, const vk::MemoryRequirements& request, vk::MemoryPropertyFlags flag)
		{
			auto& prop = gpu.getMemoryProperties();
			auto memory_type_bits = request.memoryTypeBits;
			for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
			{
				auto bit = memory_type_bits >> i;
				if ((bit & 1) == 0) {
					continue;
				}

				if ((prop.memoryTypes[i].propertyFlags & flag) == flag)
				{
					return i;
				}
			}
			return 0xffffffffu;
		}
	};
};
struct Deleter {
	vk::Device device;

	vk::CommandPool pool;
	std::vector<vk::CommandBuffer> cmd;

	std::vector<vk::Fence> fence;
	std::vector<vk::FenceShared> fence_shared;
	std::vector<vk::Buffer> buffer;
	std::vector<vk::Image> image;
	std::vector<vk::DeviceMemory> memory;
	std::vector<vk::Sampler> sampler;

	~Deleter() 
	{
		for (auto& f : fence) {
			device.destroyFence(f);
		}

		if (!cmd.empty())
		{
			device.freeCommandBuffers(pool, cmd);
		}

		for (auto& b : buffer)
		{
			device.destroyBuffer(b);
		}
		for (auto& i : image)
		{
			device.destroyImage(i);
		}
		for (auto& m : memory)
		{
			device.freeMemory(m);
		}
		for (auto& s : sampler)
		{
			device.destroySampler(s);
		}
	};
	bool isReady()const
	{
		for (auto& f : fence)
		{
			// todo : device lost?
			if (device.getFenceStatus(f) != vk::Result::eSuccess) {
				return false;
			}
		}
		for (auto& f : fence_shared)
		{
			// todo : device lost?
			if (device.getFenceStatus(f.getHandle()) != vk::Result::eSuccess) {
				return false;
			}
		}
		return true;
	}
};

class sGlobal : public Singleton<sGlobal>
{
	friend Singleton<sGlobal>;
public:
	enum
	{
		FRAME_MAX = 3,
	};

protected:

	sGlobal();
public:

	vk::Instance& getVKInstance() { return m_instance; }
	cGPU& getGPU(int index) { return m_gpu[index]; }
	void swap();
	int32_t getCurrentFrame()const { return m_current_frame; }
	int32_t getPrevFrame()const { return (m_current_frame == 0 ? FRAME_MAX : m_current_frame)-1; }

public:
	cThreadPool& getThreadPool() { return m_thread_pool; }
private:
	vk::Instance m_instance;
	std::vector<cGPU> m_gpu;

	int32_t m_current_frame;
	cThreadPool m_thread_pool;
	std::vector<vk::CommandPool>	m_cmd_pool_tempolary;
	std::array<std::vector<std::unique_ptr<Deleter>>, FRAME_MAX> m_cmd_delete;

	float m_deltatime;
public:
	vk::CommandPool getCmdPoolTempolary(uint32_t device_family_index)const
	{
		return m_cmd_pool_tempolary[device_family_index];
	}

	void destroyResource(std::unique_ptr<Deleter>&& deleter) {
		// @todo thread-safe‘Î‰ž
		assert(deleter->device);
		for (auto& fence : deleter->fence) {
			assert(fence);
		}
		for (auto& fence : deleter->fence_shared){
			assert(fence.getHandle());
		}
		m_cmd_delete[getCurrentFrame()].emplace_back(std::move(deleter));
	}

	float getDeltaTime()const { return m_deltatime; }
};

struct sThreadLocal : public SingletonTLS<sThreadLocal>
{
	friend SingletonTLS<sThreadLocal>;
	cGPU m_gpu;
	int m_thread_index;

	enum DeviceIndex {
		DEVICE_GRAPHICS,
		DEVICE_COMPUTE,
		DEVICE_TRANSFAR,
		DEVICE_MAX,
	};
	cDevice m_device[DEVICE_MAX];

	std::vector<vk::CommandPool>	m_cmd_pool_tempolary;
	std::vector<std::vector<vk::CommandBuffer>>	m_cmd_tempolary;
	std::vector<std::array<vk::CommandPool, sGlobal::FRAME_MAX>>	m_cmd_pool_onetime;
	std::vector<vk::CommandPool>	m_cmd_pool_compiled;

public:
	std::array<vk::CommandPool, sGlobal::FRAME_MAX> getCmdPoolOnetime(int device_family_index)const { return m_cmd_pool_onetime[device_family_index]; }
	vk::CommandPool getCmdPoolCompiled(int device_family_index)const { return m_cmd_pool_compiled[device_family_index]; }
	vk::CommandPool getCmdPoolTempolary(uint32_t device_family_index)const { return m_cmd_pool_tempolary[device_family_index]; }
	vk::CommandBuffer getCmdTempolary(uint32_t device_family_index)const 
	{
		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandBufferCount = 1;
		cmd_buffer_info.commandPool = m_cmd_pool_tempolary[device_family_index];
		cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
		auto cmd = m_gpu.getDeviceByFamilyIndex(device_family_index)->allocateCommandBuffers(cmd_buffer_info)[0];
		return cmd;
	}

};


vk::ShaderModule loadShader(const vk::Device& device, const std::string& filename);
