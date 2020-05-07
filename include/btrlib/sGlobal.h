#pragma once

#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <climits>

#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <btrlib/cStopWatch.h>

class sGlobal : public Singleton<sGlobal>
{
	friend Singleton<sGlobal>;
public:

	// éQçl
	// https://www.isus.jp/games/sample-app-for-direct3d-12-flip-model-swap-chains/
	enum
	{
		FRAME_COUNT_MAX = 3,
		BUFFER_COUNT_MAX = 3,
	};
protected:

	sGlobal();
public:

	void sync();
	uint32_t getCurrentFrame()const { return m_current_frame; }
	uint32_t getNextFrame()const { return (m_current_frame + 1) % FRAME_COUNT_MAX; }
	uint32_t getPrevFrame()const { return (m_current_frame == 0 ? FRAME_COUNT_MAX : m_current_frame) - 1; }
	uint32_t getWorkerFrame()const { return (m_current_frame+1) % FRAME_COUNT_MAX; }
	uint32_t getRenderFrame()const { return m_current_frame % FRAME_COUNT_MAX; }
	uint32_t getWorkerIndex()const { return m_tick_tock; }
	uint32_t getRenderIndex()const { return (m_tick_tock + 1) % 2; }

private:

	uint32_t m_current_frame;
	uint32_t m_tick_tock;

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
			m_deleter_list[m_index].push_back(std::move(ptr));
		}
	};

	void sync()
	{
		std::lock_guard<std::mutex> lk(m_deleter_mutex);
		m_index = (m_index + 1) % m_deleter_list.size();
		m_deleter_list[m_index].clear();
		m_deleter_list[m_index].shrink_to_fit();
	}

private:
	struct Deleter
	{
		virtual ~Deleter() { ; }
	};
	std::array<std::vector<std::unique_ptr<Deleter>>, sGlobal::FRAME_COUNT_MAX> m_deleter_list;
	std::mutex m_deleter_mutex;
	int m_index = 0;

};


vk::UniqueShaderModule loadShaderUnique(const vk::Device& device, const std::string& filename);
vk::UniqueDescriptorPool createDescriptorPool(vk::Device device, const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& bindings, uint32_t set_size);
