#undef THREAD

#define THREAD
#define ASIO

#include "Common.h"

#if defined THREAD
#include <boost/threadpool.hpp>

#elif defined ASIO
#ifdef _WIN32
#include <WinSock2.h>
#endif // _WIN32

#include <boost/asio.hpp>
#endif

#include <chrono>
#include <cstdlib>
#include <utility>
#include <iostream>
#include <atomic>
#include <thread>

#if defined THREAD
using ThreadPool = boost::threadpool::thread_pool<>;
#elif defined ASIO
using ThreadPool = boost::asio::thread_pool;
#endif

static constexpr std::chrono::nanoseconds::rep SLEEP_TIME = 1000000;

static std::atomic_ulong counter = 0;

static auto getConcurrency() noexcept
{
	auto concurrency = std::thread::hardware_concurrency();
	return concurrency > 0 ? \
		concurrency : static_cast<decltype(concurrency)>(1);
}

static void task()
{
	for (volatile auto index = 0UL; \
		index < 10000UL; ++index);
	Eterfree::sleepFor(SLEEP_TIME);
	counter.fetch_add(1, \
		std::memory_order::relaxed);
}

#if defined THREAD
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

#elif defined ASIO
static void execute(ThreadPool& _threadPool)
{
	for (auto index = 0UL; index < 50000UL; ++index)
		boost::asio::post(_threadPool, task);

	for (auto index = 0UL; index < 50000UL; ++index)
		boost::asio::post(_threadPool, task);
}

static void terminate(ThreadPool&& _threadPool)
{
	_threadPool.join();
}
#endif

int main()
{
	using namespace std::chrono;

	using std::cout, std::endl;

	constexpr auto load = []() noexcept
	{ return counter.load(std::memory_order::relaxed); };

#ifdef FILE_STREAM
	constexpr auto FILE = "Boost.txt";

#ifdef FILE_SYSTEM
	if (std::filesystem::exists(FILE))
		std::filesystem::remove(FILE);
#endif // FILE_SYSTEM

	std::ofstream ofs(FILE, std::ios::app);
	auto os = cout.rdbuf(ofs.rdbuf());
#endif // FILE_STREAM

	ThreadPool threadPool(getConcurrency());

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
