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

class sDeleter : public Singleton<sDeleter>
{
	friend Singleton<sGlobal>;
public:
	template<typename... T>
	void enque(T&&... handle)
	{
		struct HandleHolder : Deleter
		{
			std::tuple<T...> m_handle;
			HandleHolder(T&&... arg) : m_handle(std::move(arg)...) {}
		};

		auto ptr = std::make_unique<HandleHolder>(std::move(handle)...);
		{
			std::lock_guard<std::mutex> lk(m_deleter_mutex);
			m_deleter_list.push_back(std::move(ptr));
		}
	};

	void swap()
	{
		std::lock_guard<std::mutex> lk(m_deleter_mutex);
		m_deleter_list.erase(std::remove_if(m_deleter_list.begin(), m_deleter_list.end(), [&](auto& d) { return d->count-- == 0; }), m_deleter_list.end());
	}

private:
	struct Deleter
	{
		uint32_t count;
		Deleter() : count(5) {}
		virtual ~Deleter() { ; }
	};
	std::vector<std::unique_ptr<Deleter>> m_deleter_list;
	std::mutex m_deleter_mutex;


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

	cStopWatch m_timer;
	float m_deltatime;
public:
	cThreadData& getThreadLocal();
	std::vector<cThreadData>& getThreadLocalList() { return m_thread_local; }

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
