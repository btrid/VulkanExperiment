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

#define USE_DEBUG_REPORT _DEBUG


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
	void sync();
	uint32_t getCurrentFrame()const { return m_current_frame; }
	uint32_t getNextFrame()const { return (m_current_frame + 1) % FRAME_MAX; }
	uint32_t getPrevFrame()const { return (m_current_frame == 0 ? FRAME_MAX : m_current_frame) - 1; }
	uint32_t getWorkerFrame()const { return (m_current_frame+1) % FRAME_MAX; }
	uint32_t getRenderFrame()const { return m_current_frame; }
	uint32_t getGameFrame()const { return m_game_frame; }
	uint32_t getWorkerIndex()const { return m_tick_tock; }
	uint32_t getRenderIndex()const { return (m_tick_tock + 1) % 2; }
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
	cThreadPool& getThreadPoolSound() { return m_thread_pool_sound; }
private:
	vk::Instance m_instance;
	vk::DispatchLoaderDynamic m_dispatch;
	vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> m_debug_messenger;

	std::vector<cGPU> m_gpu;

	GameFrame m_current_frame;
	GameFrame m_game_frame;
	uint32_t m_tick_tock;

	cThreadPool m_thread_pool;
	cThreadPool m_thread_pool_sound;

	cStopWatch m_timer;
	float m_deltatime;
	float m_totaltime;
public:

	float getDeltaTime()const { return m_deltatime; }
	float getTotalTime()const { return m_totaltime; }
};

struct sThreadLocal : public SingletonTLS<sThreadLocal>
{
	friend SingletonTLS<sThreadLocal>;
	uint32_t m_thread_index;
	uint32_t getThreadIndex()const { return m_thread_index; }

};

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

	void sync()
	{
		std::lock_guard<std::mutex> lk(m_deleter_mutex);
		m_deleter_list.erase(std::remove_if(m_deleter_list.begin(), m_deleter_list.end(), [&](auto& d) { return d->count-- == 0; }), m_deleter_list.end());
	}

private:
	struct Deleter
	{
		uint32_t count;
		Deleter() : count(sGlobal::Order().FRAME_MAX+3) {}
		virtual ~Deleter() { ; }
	};
	std::vector<std::unique_ptr<Deleter>> m_deleter_list;
	std::mutex m_deleter_mutex;


};


vk::UniqueShaderModule loadShaderUnique(const vk::Device& device, const std::string& filename);
vk::UniqueDescriptorPool createDescriptorPool(vk::Device device, const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& bindings, uint32_t set_size);
