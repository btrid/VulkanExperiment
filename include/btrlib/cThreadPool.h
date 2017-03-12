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
	std::vector<std::function<void()>> mJob;
	std::function<void()> mFinish;
};
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
	cThreadPool()
		: m_process_job_count(0)
		, m_is_terminate(false)
	{}
	void start(int num, const std::function<void(int)>& initialize = nullptr)
	{
		// 二度スタート禁止。todo:reset todo:wait
		assert(m_thread.empty());
		m_thread.clear();
		m_thread.resize(num);

		for (int i = 0; i < num; i++) {
			m_thread[i] = std::thread([&, i]()
			{
				if (initialize) {
					initialize(i+1);
				}
				this->work();
			});
		}
		if (initialize) {
			initialize(0);
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

