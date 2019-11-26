#define ETERFREE
#define BOOST
//#define FILE_STREAM
//#define FILE_SYSTEM

#if defined ETERFREE
#include "ThreadPool.h"
#elif defined BOOST
#include <threadpool/threadpool.hpp>
#endif

#include <thread>
#include <atomic>
#include <chrono>
#ifdef FILE_STREAM
#ifdef FILE_SYSTEM
#include <filesystem>
#endif
#include <fstream>
#endif
#include <iostream>

static std::atomic_ulong counter = 0;

static void task()
{
	for (volatile auto counter = 0U; counter < 10000U; ++counter);
	std::this_thread::sleep_for(std::chrono::milliseconds(3));
	++counter;
}

#if defined ETERFREE
static void process(eterfree::ThreadPool &threadPool)
{
	for (auto counter = 0UL; counter < 100000UL; ++counter)
		threadPool.pushTask(task);
	std::list<eterfree::ThreadPool::Functor> tasks;
	for (auto counter = 0UL; counter < 200000UL; ++counter)
		tasks.push_back(task);
	threadPool.pushTask(tasks);
}
#elif defined BOOST
static void process(boost::threadpool::thread_pool<> &threadPool)
{
	for (auto counter = 0UL; counter < 100000UL; ++counter)
		threadPool.schedule(task);
	for (auto counter = 0UL; counter < 200000UL; ++counter)
		threadPool.schedule(task);
}
#endif

int main()
{
	using std::cout;
#ifdef FILE_STREAM
	constexpr auto file = "ThreadPool.log";
#ifdef FILE_SYSTEM
	std::filesystem::remove(file);
#endif
	std::ofstream ofs(file, std::ios::app);
	auto os = cout.rdbuf(ofs.rdbuf());
#endif

#if defined ETERFREE
	eterfree::ThreadPool threadPool(100, 100);
#elif defined BOOST
	boost::threadpool::thread_pool<> threadPool(100);
#endif

	using namespace std::chrono;
	auto begin = system_clock::now();
	process(threadPool);

	std::this_thread::sleep_for(milliseconds(10000));
	using std::endl;
	cout << "任务数量：" << counter << endl;
	auto end = system_clock::now();
	auto duration = duration_cast<milliseconds>(end - begin);
	cout << "执行时间：" << duration.count() << endl;
#ifdef ETERFREE
	eterfree::ThreadPool(std::move(threadPool));
#endif

#ifdef FILE_STREAM
	cout << endl;
	std::cout.rdbuf(os);
#endif
	return 0;
}
