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
#include <btrlib/sDebug.h>

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
	std::vector<vk::Buffer> buffer;
	std::vector<vk::DeviceMemory> memory;

	~Deleter() 
	{
		if (!fence.empty())
		{
			for (auto& f : fence) {
				device.destroyFence(f);
			}
			fence.clear();
		}
		if (!cmd.empty())
		{
			device.freeCommandBuffers(pool, cmd);
			cmd.clear();
		}
		if (!buffer.empty()) {
			for (auto& b : buffer)
			{
				device.destroyBuffer(b);
			}
			buffer.clear();
		}
		if (!memory.empty()) {
			for (auto& m : memory)
			{
				device.freeMemory(m);
			}
			memory.clear();
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


template<typename T>
struct UniformBuffer
{
private:
	vk::PhysicalDevice m_gpu;
	uint32_t m_family_index;
	vk::Queue m_queue;

	vk::DescriptorBufferInfo m_buffer_info;
	vk::DescriptorBufferInfo m_staging_buffer_info;

	struct Private
	{
		vk::Device m_device;

		vk::CommandPool m_cmd_pool;
		std::vector<vk::CommandBuffer> m_cmd;
		std::vector<vk::Fence>	m_fence;

		vk::Buffer m_buffer;
		vk::DeviceMemory m_memory;

		vk::Buffer m_staging_buffer;
		vk::DeviceMemory m_staging_memory;
		~Private()
		{
			if (m_cmd_pool)
			{
				std::unique_ptr<Deleter> deleter = std::make_unique<Deleter>();
				deleter->pool = m_cmd_pool;
				deleter->cmd = std::move(m_cmd);
				deleter->device = m_device;
				deleter->fence = std::move(m_fence);

				deleter->buffer = { m_buffer, m_staging_buffer };
				deleter->memory = { m_memory, m_staging_memory };
				sGlobal::Order().destroyResource(std::move(deleter));

			}
		}
	};
	std::shared_ptr<Private> m_private;


	UniformBuffer(const UniformBuffer&) = delete;
	UniformBuffer& operator=(const UniformBuffer&) = delete;

	UniformBuffer(UniformBuffer &&)noexcept = default;
	UniformBuffer& operator=(UniformBuffer&&)noexcept = default;

public:
	UniformBuffer()
		: m_private(std::make_shared<Private>())
	{}

	~UniformBuffer()
	{
	}
	vk::Buffer getBuffer()const { return m_buffer_info.buffer; }
	vk::DescriptorBufferInfo getBufferInfo()const { return m_buffer_info; }

private:
	void create(const cGPU& gpu, const cDevice& device)
	{
		m_gpu = gpu.getHandle();
		m_private->m_device = device.getHandle();
		m_family_index = device.getQueueFamilyIndex();
		m_queue = device->getQueue(device.getQueueFamilyIndex(), device.getQueueNum() - 1);

		{
			// コマンドバッファの準備
			m_private->m_cmd_pool = sThreadLocal::Order().getCmdPoolCompiled(m_family_index);
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandBufferCount = sGlobal::FRAME_MAX;
			cmd_buffer_info.commandPool = m_private->m_cmd_pool;
			cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
			m_private->m_cmd = m_private->m_device.allocateCommandBuffers(cmd_buffer_info);

			vk::FenceCreateInfo fence_info;
			fence_info.setFlags(vk::FenceCreateFlagBits::eSignaled);
			m_private->m_fence.reserve(sGlobal::FRAME_MAX);
			for (int i = 0; i < sGlobal::FRAME_MAX; i++)
			{
				m_private->m_fence.emplace_back(m_private->m_device.createFence(fence_info));
			}
		}
	}
public:

	void create(const cGPU& gpu, const cDevice& device, vk::BufferUsageFlags flag)
	{
		create(gpu, device);

		size_t data_size = sizeof(T);
		{
			// device_localなバッファの作成
			vk::BufferCreateInfo buffer_info;
			buffer_info.usage = flag | vk::BufferUsageFlagBits::eTransferDst;
			buffer_info.size = data_size;

			m_private->m_buffer = device->createBuffer(buffer_info);

			vk::MemoryRequirements memory_request = device->getBufferMemoryRequirements(m_private->m_buffer);
			vk::MemoryAllocateInfo memory_alloc;
			memory_alloc.setAllocationSize(memory_request.size);
			memory_alloc.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal));
			m_private->m_memory = device->allocateMemory(memory_alloc);

			device->bindBufferMemory(m_private->m_buffer, m_private->m_memory, 0);

			m_buffer_info.setBuffer(m_private->m_buffer);
			m_buffer_info.setOffset(0);
			m_buffer_info.setRange(data_size);
		}
		{
			// host_visibleなバッファの作成
			vk::BufferCreateInfo buffer_info;
			buffer_info.usage = vk::BufferUsageFlagBits::eTransferSrc;
			buffer_info.size = data_size * sGlobal::FRAME_MAX;

			m_private->m_staging_buffer = device->createBuffer(buffer_info);

			vk::MemoryRequirements memory_request = device->getBufferMemoryRequirements(m_private->m_staging_buffer);
			vk::MemoryAllocateInfo memory_alloc;
			memory_alloc.setAllocationSize(memory_request.size);
			memory_alloc.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memory_request, vk::MemoryPropertyFlagBits::eHostVisible));
			m_private->m_staging_memory = device->allocateMemory(memory_alloc);

			device->bindBufferMemory(m_private->m_staging_buffer, m_private->m_staging_memory, 0);

			m_staging_buffer_info.setBuffer(m_private->m_staging_buffer);
			m_staging_buffer_info.setOffset(0);
			m_staging_buffer_info.setRange(data_size);
		}
		{
			// cmd を作っとく
			for (size_t i = 0; i < m_private->m_cmd.size(); i++)
			{
				auto& cmd = m_private->m_cmd[i];
				cmd.reset(vk::CommandBufferResetFlags());
				vk::CommandBufferBeginInfo begin_info;
				cmd.begin(begin_info);
				vk::BufferCopy copy_info;
				copy_info.size = data_size;
				copy_info.srcOffset = data_size*i;
				copy_info.dstOffset = 0;
				cmd.copyBuffer(m_private->m_staging_buffer, m_private->m_buffer, copy_info);
				cmd.end();
			}
		}

	}

	void update(const T& data)
	{
		auto frame = sGlobal::Order().getCurrentFrame();
		auto data_size = sizeof(T);
		char* mem = reinterpret_cast<char*>(m_private->m_device.mapMemory(m_private->m_staging_memory, data_size*frame, data_size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, data_size, &data, data_size);
		}
		m_private->m_device.unmapMemory(m_private->m_staging_memory);

		sDebug::Order().waitFence(m_private->m_device, m_private->m_fence[frame]);
		m_private->m_device.resetFences(m_private->m_fence[frame]);

		vk::SubmitInfo submit_info;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &m_private->m_cmd[frame];

		m_queue.submit(submit_info, m_private->m_fence[frame]);
	}
};

/**
	
*/
struct ConstantBuffer
{
private:

	vk::PhysicalDevice m_gpu;
	uint32_t m_family_index;
	vk::Queue m_queue;

	vk::DescriptorBufferInfo m_buffer_info;

	struct Private
	{
		vk::Device m_device;

		vk::Buffer m_buffer;
		vk::DeviceMemory m_memory;

		~Private()
		{
//			if (m_cmd_pool)
			{
				std::unique_ptr<Deleter> deleter = std::make_unique<Deleter>();
//				deleter->pool = m_cmd_pool;
//				deleter->cmd.emplace_back(std::move(m_cmd));
				deleter->device = m_device;
				deleter->buffer = { m_buffer };
				deleter->memory = { m_memory };
				sGlobal::Order().destroyResource(std::move(deleter));
			}
		}
	};
	std::shared_ptr<Private> m_private;


	ConstantBuffer(const ConstantBuffer&) = delete;
	ConstantBuffer& operator=(const ConstantBuffer&) = delete;

	ConstantBuffer(ConstantBuffer &&)noexcept = default;
	ConstantBuffer& operator=(ConstantBuffer&&)noexcept = default;

public:
	ConstantBuffer()
		: m_private(std::make_shared<Private>())
	{}

	~ConstantBuffer()
	{
		int a = 0;
		a++;
	}
	vk::Buffer getBuffer()const { return m_buffer_info.buffer; }
	vk::DescriptorBufferInfo getBufferInfo()const { return m_buffer_info; }

private:
	void create(const cGPU& gpu, const cDevice& device)
	{
		m_gpu = gpu.getHandle();
		m_private->m_device = device.getHandle();
		m_family_index = device.getQueueFamilyIndex();
		m_queue = device->getQueue(device.getQueueFamilyIndex(), device.getQueueNum() - 1);

	}
	void update(void* data, size_t data_size, size_t offset)
	{
		vk::BufferCreateInfo staging_buffer_info;
		staging_buffer_info.usage = vk::BufferUsageFlagBits::eTransferSrc;
		staging_buffer_info.size = data_size;
		auto staging_buffer = m_private->m_device.createBuffer(staging_buffer_info);

		vk::MemoryRequirements staging_memory_request = m_private->m_device.getBufferMemoryRequirements(staging_buffer);
		vk::MemoryAllocateInfo staging_memory_alloc;
		staging_memory_alloc.setAllocationSize(staging_memory_request.size);
		staging_memory_alloc.setMemoryTypeIndex(cGPU::Helper::getMemoryTypeIndex(m_gpu, staging_memory_request, vk::MemoryPropertyFlagBits::eHostVisible));
		auto staging_memory = m_private->m_device.allocateMemory(staging_memory_alloc);

		char* mem = reinterpret_cast<char*>(m_private->m_device.mapMemory(staging_memory, 0, data_size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, data_size, data, data_size);
		}
		m_private->m_device.unmapMemory(staging_memory);
		m_private->m_device.bindBufferMemory(staging_buffer, staging_memory, 0);

		vk::CommandPool cmd_pool;
		std::vector<vk::CommandBuffer> cmd;
		{
			// コマンドバッファの準備
			cmd_pool = sGlobal::Order().getCmdPoolTempolary(m_family_index);
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandBufferCount = 1;
			cmd_buffer_info.commandPool = cmd_pool;
			cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
			cmd = m_private->m_device.allocateCommandBuffers(cmd_buffer_info);

		}
		cmd[0].reset(vk::CommandBufferResetFlags());
		vk::CommandBufferBeginInfo begin_info;
		begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit | vk::CommandBufferUsageFlagBits::eSimultaneousUse;
		cmd[0].begin(begin_info);
		vk::BufferCopy copy_info;
		copy_info.size = data_size;
		copy_info.srcOffset = 0;
		copy_info.dstOffset = 0;
		cmd[0].copyBuffer(staging_buffer, m_private->m_buffer, copy_info);
		cmd[0].end();
		vk::SubmitInfo submit_info;
		submit_info.commandBufferCount = (uint32_t)cmd.size();
		submit_info.pCommandBuffers = cmd.data();

		vk::FenceCreateInfo fence_info;
		vk::Fence fence = m_private->m_device.createFence(fence_info);
		m_queue.submit(submit_info, fence);

		{
			std::unique_ptr<Deleter> deleter = std::make_unique<Deleter>();
			deleter->pool = cmd_pool;
			deleter->cmd = std::move(cmd);
			deleter->device = m_private->m_device;
			deleter->buffer = { staging_buffer };
			deleter->memory = { staging_memory };
			deleter->fence.push_back(fence);
			sGlobal::Order().destroyResource(std::move(deleter));
		}

	}

public:
	template<typename T>
	void create(const cGPU& gpu, const cDevice& device, const std::vector<T>& data, vk::BufferUsageFlags flag)
	{
		create(gpu, device);
		auto data_size = vector_sizeof(data);
		create(gpu, device, data_size, flag);
		{
			update((void*)data.data(), data_size, 0);
		}
	}

	void create(const cGPU& gpu, const cDevice& device, size_t data_size, vk::BufferUsageFlags flag)
	{
		create(gpu, device);

		{
			// device_localなバッファの作成
			vk::BufferCreateInfo buffer_info;
			buffer_info.usage = flag | vk::BufferUsageFlagBits::eTransferDst;
			buffer_info.size = data_size;

			m_private->m_buffer = device->createBuffer(buffer_info);

			vk::MemoryRequirements memory_request = device->getBufferMemoryRequirements(m_private->m_buffer);
			vk::MemoryAllocateInfo memory_alloc;
			memory_alloc.setAllocationSize(memory_request.size);
			memory_alloc.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal));
			m_private->m_memory = device->allocateMemory(memory_alloc);

			device->bindBufferMemory(m_private->m_buffer, m_private->m_memory, 0);

			m_buffer_info.setBuffer(m_private->m_buffer);
			m_buffer_info.setOffset(0);
			m_buffer_info.setRange(data_size);
		}
	}

};

vk::ShaderModule loadShader(const vk::Device& device, const std::string& filename);
