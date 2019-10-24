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
	for (volatile int counter = 0; counter < 10000; ++counter);
	std::this_thread::sleep_for(std::chrono::milliseconds(3));
	++counter;
}

#if defined ETERFREE
static void process(eterfree::ThreadPool &threadPool)
{
	for (int i = 0; i < 100000; ++i)
		threadPool.pushTask(task);
	std::list<eterfree::ThreadPool::functor> tasks;
	for (int i = 0; i < 200000; ++i)
		tasks.push_back(task);
	threadPool.pushTask(tasks);
}
#elif defined BOOST
static void process(boost::threadpool::thread_pool<> &threadPool)
{
	for (int i = 0; i < 100000; ++i)
		threadPool.schedule(task);
	for (int i = 0; i < 200000; ++i)
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
