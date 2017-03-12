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

struct ThreadJob {
	std::vector<std::function<void()>> mJob;
	std::function<void()> mFinish;
};
struct ThreadWorker
{
	void operator()(const ThreadJob&& job) {
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

class ThreadPoolBase
{
protected:
	ThreadPoolBase() = default;
	~ThreadPoolBase() = default;
};
template<
	typename Worker,
	typename _Job,
	typename ThreadData,
	int thread_num = std::thread::hardware_concurrency() - 1
>
class ThreadPool_t /*: public ThreadPoolBase*/
{
public:
	using Job = _Job;
	using type = ThreadPool_t<Worker, Job, ThreadData, thread_num>;

	enum { THREAD_NUM = thread_num, };
private:
	std::array<std::thread, THREAD_NUM> m_thread;
	std::queue<Job> m_jobs;
	std::mutex m_job_mutex;
	std::condition_variable_any m_job_condition;

	std::atomic<int> m_process_job_count;
	std::condition_variable_any m_is_finished;

	std::atomic_bool m_is_terminate;
	struct ThreadLocal : public ThreadData {
		int m_thread_index;
		std::thread::id m_thread_ID;

		ThreadLocal()
			: m_thread_index(-1)
			, m_thread_ID(std::this_thread::get_id())
		{

		}
	};
	using ThreadLocalList = std::array<ThreadLocal, thread_num + 1>;
	ThreadLocalList m_thread_local_storage;
public:
	ThreadPool_t()
		: m_process_job_count(0)
		, m_is_terminate(false)
	{
		int num = THREAD_NUM;
		for (int i = 0; i < num; i++) {
			m_thread[i] = std::thread([&, i]()
			{
				// ThreadLocalの初期化
				this->getThreadLocalList()[i + 1].m_thread_index = i + 1;
				this->getThreadLocalList()[i + 1].m_thread_ID = std::this_thread::get_id();

				this->work();
			});
			SetThreadIdealProcessor(m_thread[i].native_handle(), i + 1);
		}
		getThreadLocalList()[0].m_thread_index = 0;
		getThreadLocalList()[0].m_thread_ID = std::this_thread::get_id();
		SetThreadIdealProcessor(::GetCurrentThread(), 0);

		// 初期化完了を待つ
		wait();
	}

	~ThreadPool_t()
	{
		m_is_terminate.store(true);
		m_job_condition.notify_all();
		for (auto& t : m_thread) {
			t.join();
		}
		wait();
	}

public:
	void enque(const Job& job)
	{
		// 終了準備中に追加しようとした
		assert(!m_is_terminate.load());

		{
			std::lock_guard<std::mutex> lk(m_job_mutex);
			m_jobs.push(job);
		}
		m_job_condition.notify_one();
	}

	void enque(Job&& job)
	{
		// 終了準備中に追加しようとした
		assert(!m_is_terminate.load());

		{
			std::lock_guard<std::mutex> lk(m_job_mutex);
			m_jobs.push(std::move(job));
		}
		m_job_condition.notify_one();
	}

	void wait()
	{
		// スレッドとワークが全部終わるまで待つ
		// unk実装
		std::unique_lock<std::mutex> lock(m_job_mutex);
		auto wait_time = std::chrono::milliseconds(1000);
		while (true)
		{
			switch (m_is_finished.wait_for(lock, wait_time, [&]() { return m_jobs.empty() && (m_process_job_count == 0); }))
			{
			case std::cv_status::timeout:
			case std::cv_status::no_timeout:
				if (m_jobs.empty() && (m_process_job_count == 0))
				{
					return;
				}
				break;
			}
		}
	}

	bool isMainThread()const {
		return getThreadLocal().m_thread_index == 0;
	}

	ThreadLocalList& getThreadLocalList()
	{
		return m_thread_local_storage;
	}
	const ThreadLocal& getThreadLocal()const
	{
		auto id = std::this_thread::get_id();
		for (auto& tls : m_thread_local_storage)
		{
			if (tls.m_thread_ID == id) {
				return tls;
			}
		}

		assert(0);
		return m_thread_local_storage[0];
	}
	ThreadLocal& getThreadLocal()
	{
		return const_cast<ThreadLocal&>(((const type*)this)->getThreadLocal());
	}

	int getThreadNum()const { return THREAD_NUM; }
private:
	bool pop(Job& job)
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
		Worker worker;
		Job job;
		for (; pop(job);)
		{
			worker(std::forward<Job>(job));
			m_process_job_count--;
			m_is_finished.notify_one();
		}
	}



};

struct _dummy {};
//using ThreadPool = ThreadPool_t<ThreadWorker, ThreadJob, _dummy, 7>;