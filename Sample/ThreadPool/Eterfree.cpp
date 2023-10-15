#include "Common.h"
#include "ThreadPool.h"

#include <chrono>
#include <cstdlib>
#include <utility>
#include <iostream>
#include <atomic>
#include <thread>

using ThreadPool = Eterfree::ThreadPool;

static constexpr std::chrono::nanoseconds::rep SLEEP_TIME = 1000000;

static std::atomic_ulong counter = 0;

static void task()
{
	for (volatile auto index = 0UL; \
		index < 10000UL; ++index);

	sleepFor(SLEEP_TIME);

	counter.fetch_add(1, \
		std::memory_order_relaxed);
}

static void execute(ThreadPool& _threadPool)
{
	auto proxy = _threadPool.getProxy();
	for (auto index = 0UL; index < 50000UL; ++index)
		proxy.pushTask(task);

	ThreadPool::TaskQueue taskQueue;
	for (auto index = 0UL; index < 50000UL; ++index)
		taskQueue.push_back(task);
	proxy.pushTask(std::move(taskQueue));
}

static void terminate(ThreadPool&& _threadPool)
{
	_threadPool.clearTask();

	auto threadPool(std::forward<ThreadPool>(_threadPool));
	(void)threadPool;
}

int main()
{
	using namespace std::chrono;

	using std::cout, std::endl;

	constexpr auto load = []() noexcept
	{ return counter.load(std::memory_order_relaxed); };

#ifdef FILE_STREAM
	constexpr auto FILE = "Eterfree.txt";

#ifdef FILE_SYSTEM
	if (std::filesystem::exists(FILE))
		std::filesystem::remove(FILE);
#endif // FILE_SYSTEM

	std::ofstream ofs(FILE, std::ios::app);
	auto os = cout.rdbuf(ofs.rdbuf());
#endif // FILE_STREAM

	ThreadPool threadPool;

	auto begin = system_clock::now();
	execute(threadPool);
	std::this_thread::sleep_for(seconds(10));

	auto count = load();
	auto end = system_clock::now();
	auto duration = duration_cast<milliseconds>(end - begin);

	cout << "任务数量：" << count << endl;
	cout << "执行时间：" << duration.count() << endl;

#ifdef FILE_STREAM
	cout << endl;
	cout.rdbuf(os);
#endif // FILE_STREAM

	terminate(std::move(threadPool));
	cout << "任务总数：" << load() << endl;
	return EXIT_SUCCESS;
}

#include "Common.cpp"
