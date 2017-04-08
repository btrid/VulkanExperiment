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
	uint32_t getQueueNum()const { return m_queue_num; }
	const vk::Device& getHandle()const { return m_handle; }
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
	uint32_t m_family_index;
	uint32_t m_queue_num;
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

	std::vector<cDevice> getDevice(vk::QueueFlags flag) const;
	cDevice getDeviceByFamilyIndex(uint32_t index) const;
private:

	struct Device
	{
		vk::Device m_device;
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
struct Deleter {
	vk::Device device;

	vk::CommandPool pool;
	std::vector<vk::CommandBuffer> cmd;

	vk::Fence fence;
	vk::Buffer buffer;
	vk::DeviceMemory memory;

	~Deleter() {
		if (fence)
		{
			device.destroyFence(fence);
			fence = vk::Fence();
		}
		if (!cmd.empty())
		{
			device.freeCommandBuffers(pool, cmd);
			cmd.clear();
		}
		if (buffer) {
			device.destroyBuffer(buffer);
			buffer = vk::Buffer();
		}
		if (memory)
		{
			device.freeMemory(memory);
			memory = vk::DeviceMemory();
		}
	};
	bool isReady()const
	{
		return device.getFenceStatus(fence) == vk::Result::eSuccess;
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
	int getCurrentFrame()const { return m_current_frame; }
	int getPrevFrame()const { return (m_current_frame == 0 ? FRAME_MAX : m_current_frame)-1; }

public:
	cThreadPool& getThreadPool() { return m_thread_pool; }
private:
	vk::Instance m_instance;
	std::vector<cGPU> m_gpu;

	int m_current_frame;
	cThreadPool m_thread_pool;

	std::vector<vk::CommandPool>	m_cmd_pool_tempolary;
	std::array<std::vector<std::unique_ptr<Deleter>>, FRAME_MAX> m_cmd_delete;
public:
	std::mutex m_post_swap_function_mutex;
	std::vector<std::vector<std::function<void()>>> m_post_swap_function;
	void queueJobPostSwap(std::function<void()>&& job) {
		std::lock_guard<std::mutex> lk(m_post_swap_function_mutex);
		m_post_swap_function[sGlobal::Order().getCurrentFrame()].emplace_back(std::move(job));
	}

	void pushJobPostSwap()
	{
		std::vector<std::function<void()>> jobs;
		{
			std::lock_guard<std::mutex> lk(m_post_swap_function_mutex);
			jobs = std::move(m_post_swap_function[sGlobal::Order().getPrevFrame()]);
		}
		for (auto&& j : jobs)
		{
			cThreadJob job;
			job.mFinish = std::move(j);
			m_thread_pool.enque(std::move(job));
		}
	}

	vk::CommandPool getCmdPoolTempolary(uint32_t device_family_index)const
	{
		return m_cmd_pool_tempolary[device_family_index];
// 		vk::CommandBufferAllocateInfo cmd_buffer_info;
// 		cmd_buffer_info.commandBufferCount = 1;
// 		cmd_buffer_info.commandPool = m_cmd_pool_tempolary[device_family_index];
// 		cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
// 		auto cmd = m_gpu[0].getDeviceByFamilyIndex(device_family_index)->allocateCommandBuffers(cmd_buffer_info)[0];
// 		return cmd;
	}

	void destroyResource(std::unique_ptr<Deleter>&& deleter) {
		m_cmd_delete[getCurrentFrame()].emplace_back(std::move(deleter));
	}

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
	std::vector<std::array<vk::CommandPool, sGlobal::FRAME_MAX>>	m_cmd_pool_compiled;

public:
	std::array<vk::CommandPool, sGlobal::FRAME_MAX> getCmdPoolOnetime(int device_family_index)const { return m_cmd_pool_onetime[device_family_index]; }
	std::array<vk::CommandPool, sGlobal::FRAME_MAX> getCmdPoolCompiled(int device_family_index)const { return m_cmd_pool_onetime[device_family_index]; }
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
			deffered[sGlobal::Order().getCurrentFrame()].push_back(resource);
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


	template<typename T>
	void create(const cGPU& gpu, const cDevice& device, const std::vector<T>& data, vk::BufferUsageFlags flag)
	{
		auto size = vector_sizeof(data);

		vk::BufferCreateInfo buffer_info;
		buffer_info.usage = flag | vk::BufferUsageFlagBits::eTransferDst;
		buffer_info.size = size;

		mBuffer = device->createBuffer(buffer_info);

		vk::MemoryRequirements memory_request = device->getBufferMemoryRequirements(mBuffer);
		vk::MemoryAllocateInfo memory_alloc;
		memory_alloc.setAllocationSize(memory_request.size);
		memory_alloc.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal));
		mMemory = device->allocateMemory(memory_alloc);

		device->bindBufferMemory(mBuffer, mMemory, 0);

		mBufferInfo.setBuffer(mBuffer);
		mBufferInfo.setOffset(0);
		mBufferInfo.setRange(size);

		vk::BufferCreateInfo staging_buffer_info;
		staging_buffer_info.usage = vk::BufferUsageFlagBits::eTransferSrc;
		staging_buffer_info.size = size;
		auto staging_buffer = device->createBuffer(staging_buffer_info);

		vk::MemoryRequirements staging_memory_request = device->getBufferMemoryRequirements(staging_buffer);
		vk::MemoryAllocateInfo staging_memory_alloc;
		staging_memory_alloc.setAllocationSize(staging_memory_request.size);
		staging_memory_alloc.setMemoryTypeIndex(gpu.getMemoryTypeIndex(staging_memory_request, vk::MemoryPropertyFlagBits::eHostVisible));
		auto staging_memory = device->allocateMemory(staging_memory_alloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(staging_memory, 0, size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, data.data(), size);
		}
		device->unmapMemory(staging_memory);
		device->bindBufferMemory(staging_buffer, staging_memory, 0);

		auto cmd_pool = sGlobal::Order().getCmdPoolTempolary(device.getQueueFamilyIndex());
		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandBufferCount = 1;
		cmd_buffer_info.commandPool = cmd_pool;
		cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
		auto cmd = device->allocateCommandBuffers(cmd_buffer_info)[0];

		vk::CommandBufferBeginInfo begin_info;
		begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit | vk::CommandBufferUsageFlagBits::eSimultaneousUse;
		cmd.begin(begin_info);

		vk::BufferCopy copy_info;
		copy_info.size = size;
		copy_info.srcOffset = 0;
		copy_info.dstOffset = 0;
		cmd.copyBuffer(staging_buffer, mBuffer, copy_info);
		cmd.end();
		auto queue = device->getQueue(device.getQueueFamilyIndex(), device.getQueueNum() - 1);
		vk::SubmitInfo submit_info;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmd;

		vk::FenceCreateInfo fence_info;
		vk::Fence fence = device->createFence(fence_info);
		queue.submit(submit_info, fence);

		std::unique_ptr<Deleter> deleter = std::make_unique<Deleter>();
		deleter->pool = cmd_pool;
		deleter->cmd.push_back(cmd);
		deleter->device = device.getHandle();
		deleter->buffer = staging_buffer;
		deleter->memory = staging_memory;
		deleter->fence = fence;
		sGlobal::Order().destroyResource(std::move(deleter));

// 		vk::DebugMarkerObjectNameInfoEXT debug_info;
// 		debug_info.object = (uint64_t)(VkBuffer)mBuffer;
// 		debug_info.objectType = vk::DebugReportObjectTypeEXT::eBuffer;
// 		debug_info.pObjectName = "hogeee";
// 		device->debugMarkerSetObjectNameEXT(&debug_info);

	}
};

vk::ShaderModule loadShader(const vk::Device& device, const std::string& filename);
