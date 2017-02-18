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
	std::array<std::thread, THREAD_NUM> mThread;
	std::queue<Job> mJobs;
	std::mutex mJobMutex;
	std::condition_variable_any mJobCondition;

	std::atomic<int> mProcessJobCount;
	std::condition_variable_any mJobFinished;

	std::atomic_bool mTerminate;
	struct ThreadLocal : public ThreadData {
		int mThreadIndex;
		std::thread::id mThreadID;

		ThreadLocal()
			: mThreadIndex(-1)
			, mThreadID(std::this_thread::get_id())
		{

		}
	};
	using ThreadLocalList = std::array<ThreadLocal, thread_num + 1>;
	ThreadLocalList mThreadLocal;
public:
	ThreadPool_t()
		: mProcessJobCount(0)
		, mTerminate(false)
	{
		int num = THREAD_NUM;
		for (int i = 0; i < num; i++) {
			mThread[i] = std::thread([&, i]()
			{
				// ThreadLocalの初期化
				// 				this->getThreadLocal().mThreadIndex = i + 1;
				// 				this->getThreadLocal().mThreadID = std::this_thread::get_id();
				this->getThreadLocalList()[i + 1].mThreadIndex = i + 1;
				this->getThreadLocalList()[i + 1].mThreadID = std::this_thread::get_id();

				this->work();
			});
			SetThreadIdealProcessor(mThread[i].native_handle(), i + 1);
		}
		getThreadLocalList()[0].mThreadIndex = 0;
		getThreadLocalList()[0].mThreadID = std::this_thread::get_id();
		SetThreadIdealProcessor(::GetCurrentThread(), 0);

		// 初期化完了を待つ
		wait();
	}

	~ThreadPool_t()
	{
		mTerminate.store(true);
		mJobCondition.notify_all();
		for (auto& t : mThread) {
			t.join();
		}
		wait();
	}

public:
	void enque(const Job& job)
	{
		// 終了準備中に追加しようとした
		assert(!mTerminate.load());

		{
			std::lock_guard<std::mutex> lk(mJobMutex);
			mJobs.push(job);
		}
		mJobCondition.notify_one();
	}

	void wait()
	{
		// スレッドとワークが全部終わるまで待つ
		// unk実装
		std::unique_lock<std::mutex> lock(mJobMutex);
		auto wait_time = std::chrono::milliseconds(1000);
		while (true)
		{
			switch (mJobFinished.wait_for(lock, wait_time, [&]() { return mJobs.empty() && (mProcessJobCount == 0); }))
			{
			case std::cv_status::timeout:
			case std::cv_status::no_timeout:
				if (mJobs.empty() && (mProcessJobCount == 0))
				{
					return;
				}
				break;
			}
		}
	}

	bool isMainThread()const {
		return getThreadLocal().mThreadIndex == 0;
	}

	ThreadLocalList& getThreadLocalList()
	{
		return mThreadLocal;
	}
	const ThreadLocal& getThreadLocal()const
	{
		auto id = std::this_thread::get_id();
		for (auto& tls : mThreadLocal)
		{
			if (tls.mThreadID == id) {
				return tls;
			}
		}

		assert(0);
		return mThreadLocal[0];
	}
	ThreadLocal& getThreadLocal()
	{
		return const_cast<ThreadLocal&>(((const type*)this)->getThreadLocal());
	}

	int getThreadNum()const { return THREAD_NUM; }
private:
	bool pop(Job& job)
	{
		std::unique_lock<std::mutex> lock(mJobMutex);
		mJobCondition.wait(lock, [this]() { return !mJobs.empty() || mTerminate.load(); });
		if (mJobs.empty() && mTerminate.load())
		{
			return false;
		}
		mProcessJobCount++;
		job = mJobs.front();
		mJobs.pop();
		return true;
	}

	void work()
	{
		Worker worker;
		Job job;
		for (; pop(job);)
		{
			worker(std::forward<Job>(job));
			mProcessJobCount--;
			mJobFinished.notify_one();
		}
	}



};

struct _dummy {};
//using ThreadPool = ThreadPool_t<ThreadWorker, ThreadJob, _dummy, 7>;