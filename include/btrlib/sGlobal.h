#pragma once

#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <climits>

#include <btrlib/Define.h>
#include <btrlib/GPU.h>
#include <btrlib/Singleton.h>
#include <btrlib/cThreadPool.h>
#include <btrlib/sDebug.h>
#include <btrlib/cStopWatch.h>

using GameFrame = uint32_t;
namespace vk
{

// @deprecated
struct FenceShared
{
	std::shared_ptr<vk::Fence> m_fence;
	void create(vk::Device device, const vk::FenceCreateInfo& fence_info);

	vk::Fence getHandle()const { return m_fence ? *m_fence : vk::Fence(); }
	operator VkFence() const { return getHandle(); }
	vk::Fence operator*() const { return getHandle(); }
};

}

struct DeleterEx {
	uint32_t count;
	DeleterEx() : count(5) {}
	virtual ~DeleterEx() { ; }
};

extern std::vector<std::unique_ptr<DeleterEx>> g_deleter;
template<typename... T>
void enqueDeleter(T&&... handle)
{
	struct HandleHolder : DeleterEx
	{
		std::tuple<T...> m_handle;
		HandleHolder(T&&... arg) : m_handle(std::move(arg)...) {}
	};

	auto ptr = std::make_unique<HandleHolder>(std::move(handle)...);
	g_deleter.push_back(std::move(ptr));
};

void swapDeleter();
// @deprecated?
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
//		THREAD_NUM = std::thread::hardware_concurrency() - 1,
	};
	enum CmdPoolType
	{
		CMD_POOL_TYPE_ONETIME,	//!< 1フレームに一度poolがリセットされる 
		CMD_POOL_TYPE_TEMPORARY,		//!< 数フレームに渡ってcmdが使われる。自分で管理する 
		CMD_POOL_TYPE_COMPILED,		//!< cmdは自分で管理する
	};
protected:

	sGlobal();
public:

	vk::Instance& getVKInstance() { return m_instance; }
	cGPU& getGPU(int index) { return m_gpu[index]; }
	void swap();
	uint32_t getCurrentFrame()const { return m_current_frame; }
	uint32_t getPrevFrame()const { return (m_current_frame == 0 ? FRAME_MAX : m_current_frame) - 1; }
	uint32_t getCPUFrame()const { return (m_current_frame+1) % FRAME_MAX; }
	uint32_t getGPUFrame()const { return m_current_frame; }
	uint32_t getGameFrame()const { return m_game_frame; }
	uint32_t getCPUIndex()const { return m_tick_tock; }
	uint32_t getGPUIndex()const { return (m_tick_tock+1) % 2; }
	bool isElapsed(GameFrame time, GameFrame offset = FRAME_MAX)
	{
		uint64_t game_frame = (uint64_t)m_game_frame;
		if (game_frame < time) {
			game_frame += std::numeric_limits<decltype(m_game_frame)>::max() / FRAME_MAX*FRAME_MAX;
		}
		return game_frame - time >= offset;
	}

public:
	cThreadPool& getThreadPool() { return m_thread_pool; }
private:
	vk::Instance m_instance;
	std::vector<cGPU> m_gpu;

	GameFrame m_current_frame;
	GameFrame m_game_frame;
	uint32_t m_tick_tock;

	cThreadPool m_thread_pool;

	struct cThreadData
	{
		struct CmdPoolPerFamily
		{
			std::array<vk::UniqueCommandPool, sGlobal::FRAME_MAX> m_cmd_pool_onetime;
			vk::UniqueCommandPool	m_cmd_pool_temporary;
			vk::UniqueCommandPool	m_cmd_pool_compiled;
		};
		std::vector<CmdPoolPerFamily>	m_cmd_pool;
	};
	std::vector<cThreadData> m_thread_local;
	std::array<std::vector<std::unique_ptr<Deleter>>, FRAME_MAX> m_cmd_delete;

	cStopWatch m_timer;
	float m_deltatime;
public:
	cThreadData& getThreadLocal();
	std::vector<cThreadData>& getThreadLocalList() { return m_thread_local; }

	void destroyResource(std::unique_ptr<Deleter>&& deleter) {
		// @todo thread-safe対応
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
	uint32_t m_thread_index;
	uint32_t getThreadIndex()const { return m_thread_index; }

	vk::CommandBuffer getCmdOnetime(int device_family_index)const;
	vk::CommandPool getCmdPool(sGlobal::CmdPoolType type, int device_family_index)const;

};


vk::UniqueShaderModule loadShaderUnique(const vk::Device& device, const std::string& filename);
vk::ShaderModule loadShader(const vk::Device& device, const std::string& filename);

struct Descriptor
{
	struct Set {
		vk::DescriptorSetLayout m_descriptor_set_layout;
		vk::DescriptorSet m_descriptor_set;
	};
	vk::DescriptorPool m_descriptor_pool;
	std::vector<vk::DescriptorSetLayout> m_descriptor_set_layout;
	std::vector<vk::DescriptorSet> m_descriptor_set;

};
std::unique_ptr<Descriptor> createDescriptor(vk::Device device, const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& bindings);
std::unique_ptr<Descriptor> createDescriptor(vk::Device device, vk::DescriptorPool pool, const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& bindings);
vk::DescriptorPool createPool(vk::Device device, const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& bindings);
