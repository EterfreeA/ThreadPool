#define ETERFREE
#define BOOST
//#define FILE_STREAM
//#define FILE_SYSTEM

#if defined ETERFREE
#include "ThreadPool.h"
#elif defined BOOST
#include <threadpool/threadpool.hpp>
#endif

#include <utility>
#include <chrono>
#include <iostream>
#include <atomic>
#include <thread>
#ifdef FILE_STREAM
#ifdef FILE_SYSTEM
#include <filesystem>
#endif
#include <fstream>
#endif

static std::atomic_ulong counter = 0;

static void task()
{
	for (volatile auto index = 0UL; index < 10000UL; ++index);
	std::this_thread::sleep_for(std::chrono::milliseconds(3));
	++counter;
}

#if defined ETERFREE
static void process(eterfree::ThreadPool& threadPool)
{
	for (auto index = 0UL; index < 20000UL; ++index)
		threadPool.pushTask(task);
	std::list<eterfree::ThreadPool::Functor> tasks;
	for (auto index = 0UL; index < 30000UL; ++index)
		tasks.push_back(task);
	threadPool.pushTask(tasks);
}
#elif defined BOOST
static void process(boost::threadpool::thread_pool<>& threadPool)
{
	for (auto index = 0UL; index < 20000UL; ++index)
		threadPool.schedule(task);
	for (auto index = 0UL; index < 30000UL; ++index)
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
	eterfree::ThreadPool threadPool(16, 16);
#elif defined BOOST
	boost::threadpool::thread_pool<> threadPool(16);
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
	cout << "任务总数：" << counter << endl;
	return 0;
}
