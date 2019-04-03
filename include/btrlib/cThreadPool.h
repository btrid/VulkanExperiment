#pragma once

#include <Windows.h>
#include <memory>
#include <vector>
#include <array>
#include <queue>
#include <functional>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <mutex>
#include <cassert>

struct cThreadJob {
	std::string file;
	int line;
	std::vector<std::function<void()>> mJob;
	std::function<void()> mFinish;
};
#define MAKE_THREAD_JOB(_name) cThreadJob _name; _name.file = __FILE__; _name.line = __LINE__; 

struct cThreadWorker
{
	void operator()(const cThreadJob&& job) {
		for (auto&& j : job.mJob)
		{
			if (j) {
				j();
			}
		}

		if (job.mFinish) {
			job.mFinish();
		}
	}
};

struct SynchronizedPoint
{
	std::atomic_uint m_count;
	std::condition_variable_any m_condition;
	std::mutex m_mutex;

	SynchronizedPoint() : SynchronizedPoint(0) {}
	SynchronizedPoint(uint32_t count)
		: m_count(count)
	{}

	void reset(uint32_t count)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		assert(m_count == 0);
		m_count = count;
	}
	void arrive()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		assert(m_count > 0);
		m_count--;
		m_condition.notify_one();
	}

	void wait()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_condition.wait(lock, [&]() { return m_count == 0; });
	}
};

class cThreadPool
{
private:
	std::vector<std::thread> m_thread;
	std::queue<cThreadJob> m_jobs;
	std::mutex m_job_mutex;
	std::condition_variable_any m_job_condition;

	std::atomic<int> m_process_job_count;
	std::condition_variable_any m_is_finished;

	std::atomic_bool m_is_terminate;
public:

	struct InitParam
	{
		int m_index;
	};

	cThreadPool()
		: m_process_job_count(0)
		, m_is_terminate(false)
	{}
	void start(int num, const std::function<void(const InitParam&)>& initialize = nullptr)
	{
		// 二度スタート禁止。todo:reset todo:wait
		assert(m_thread.empty());
		m_thread.clear();
		m_thread.resize(num);
		if (initialize) {
			InitParam param;
			param.m_index = 0;
			initialize(param);
		}

		for (int i = 0; i < num; i++)
		{
			m_thread[i] = std::thread([=]()
			{
				if (initialize) {
					InitParam param;
					param.m_index = i + 1;
					initialize(param);
				}
				this->work();
			});
		}
		// 初期化完了を待つ
		wait();
	}

	~cThreadPool()
	{
		m_is_terminate.store(true);
		m_job_condition.notify_all();
		for (auto& t : m_thread) {
			t.join();
		}
		wait();
	}

public:
	void enque(const cThreadJob& job)
	{
		// 終了準備中に追加しようとした
		assert(!m_is_terminate.load());

		{
			std::lock_guard<std::mutex> lk(m_job_mutex);
			m_jobs.push(job);
		}
		m_job_condition.notify_one();
	}

	void enque(cThreadJob&& job)
	{
		// 終了準備中に追加しようとした
		assert(!m_is_terminate.load());

		{
			std::lock_guard<std::mutex> lk(m_job_mutex);
			m_jobs.push(std::move(job));
		}
		m_job_condition.notify_one();
	}

	bool wait()
	{
		// スレッドとワークが全部終わるまで待つ
		// unk実装
		std::unique_lock<std::mutex> lock(m_job_mutex);
		auto wait_time = std::chrono::milliseconds(0);
		return m_is_finished.wait_for(lock, wait_time, [&]() { return m_jobs.empty() && (m_process_job_count == 0); });
	}

	uint32_t getThreadNum()const { return (uint32_t)m_thread.size(); }

private:
	bool pop(cThreadJob& job)
	{
		std::unique_lock<std::mutex> lock(m_job_mutex);
		m_job_condition.wait(lock, [this]() { return !m_jobs.empty() || m_is_terminate.load(); });
		if (m_jobs.empty() && m_is_terminate.load())
		{
			return false;
		}
		m_process_job_count++;
		job = m_jobs.front();
		m_jobs.pop();
		return true;
	}

	void work()
	{
		cThreadWorker worker;
		cThreadJob job;
		for (; pop(job);)
		{
			worker(std::forward<cThreadJob>(job));
			m_process_job_count--;
			m_is_finished.notify_one();
		}
	}



};

