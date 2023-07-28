#define ETERFREE
#define BOOST

//#define FILE_STREAM
//#define FILE_SYSTEM

#if defined ETERFREE
#include "ThreadPool.h"
#elif defined BOOST
#include <boost/threadpool.hpp>
#endif

#include <chrono>
#include <cstdlib>
#include <utility>
#include <iostream>
#include <atomic>
#include <thread>

#ifdef FILE_STREAM
#include <fstream>

#ifdef FILE_SYSTEM
#include <filesystem>
#endif // FILE_SYSTEM
#endif // FILE_STREAM

#ifdef _WIN32
#include <Windows.h>
#pragma comment(lib, "Winmm.lib")
#endif // _WIN32

#if defined ETERFREE
using ThreadPool = Eterfree::ThreadPool;
#elif defined BOOST
using ThreadPool = boost::threadpool::thread_pool<>;
#endif

static constexpr auto SLEEP_TIME = std::chrono::milliseconds(1);

static std::atomic_ulong counter = 0;

static void task()
{
	for (volatile auto index = 0UL; \
		index < 10000UL; ++index);

#ifdef _WIN32
	constexpr UINT PERIOD = 1;
	auto result = ::timeBeginPeriod(PERIOD);
	if (result != TIMERR_NOERROR)
		std::cerr << "timeBeginPeriod error " \
		<< result << std::endl;
#endif // _WIN32

	std::this_thread::sleep_for(SLEEP_TIME);

#ifdef _WIN32
	result = ::timeEndPeriod(PERIOD);
	if (result != TIMERR_NOERROR)
		std::cerr << "timeEndPeriod error " \
		<< result << std::endl;
#endif // _WIN32

	counter.fetch_add(1, \
		std::memory_order_relaxed);
}

#if defined ETERFREE
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

#elif defined BOOST
static auto getConcurrency() noexcept
{
	auto concurrency = std::thread::hardware_concurrency();
	return concurrency > 0 ? \
		concurrency : static_cast<decltype(concurrency)>(1);
}

static void execute(ThreadPool& _threadPool)
{
	for (auto index = 0UL; index < 50000UL; ++index)
		_threadPool.schedule(task);

	for (auto index = 0UL; index < 50000UL; ++index)
		_threadPool.schedule(task);
}

static void terminate(ThreadPool&& _threadPool)
{
	auto threadPool(std::forward<ThreadPool>(_threadPool));
	(void)threadPool;
}
#endif

int main()
{
	using std::cout, std::endl;
	using namespace std::chrono;

	constexpr auto load = []() noexcept
	{ return counter.load(std::memory_order_relaxed); };

#ifdef FILE_STREAM
	constexpr auto FILE = "ThreadPool.txt";

#ifdef FILE_SYSTEM
	std::filesystem::remove(FILE);
#endif // FILE_SYSTEM

	std::ofstream ofs(FILE, std::ios::app);
	auto os = cout.rdbuf(ofs.rdbuf());
#endif // FILE_STREAM

#if defined ETERFREE
	ThreadPool threadPool;
#elif defined BOOST
	ThreadPool threadPool(getConcurrency());
#endif

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
