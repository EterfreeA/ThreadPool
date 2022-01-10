#define ETERFREE
#define BOOST
//#define FILE_STREAM
//#define FILE_SYSTEM

#if defined ETERFREE
#include "ThreadPool.hpp"
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
static void execute(eterfree::ThreadPool<>& _threadPool)
{
	auto proxy = _threadPool.getProxy();
	for (auto index = 0UL; index < 20000UL; ++index)
		proxy.pushTask(task);

	std::list<eterfree::ThreadPool<>::Functor> taskList;
	for (auto index = 0UL; index < 30000UL; ++index)
		taskList.push_back(task);
	_threadPool.pushTask(taskList);
}

static void terminate(eterfree::ThreadPool<>&& _threadPool)
{
	_threadPool.clearTask();
	auto threadPool(std::move(_threadPool));
}

#elif defined BOOST
static void execute(boost::threadpool::thread_pool<>& _threadPool)
{
	for (auto index = 0UL; index < 20000UL; ++index)
		_threadPool.schedule(task);
	for (auto index = 0UL; index < 30000UL; ++index)
		_threadPool.schedule(task);
}

static void terminate(boost::threadpool::thread_pool<>&& _threadPool)
{
	auto threadPool(std::move(_threadPool));
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
	eterfree::ThreadPool threadPool;
#elif defined BOOST
	boost::threadpool::thread_pool<> threadPool(16);
#endif

	using namespace std::chrono;
	auto begin = system_clock::now();
	execute(threadPool);
	std::this_thread::sleep_for(milliseconds(10000));

	using std::endl;
	cout << "任务数量：" << counter << endl;
	auto end = system_clock::now();
	auto duration = duration_cast<milliseconds>(end - begin);
	cout << "执行时间：" << duration.count() << endl;

#ifdef FILE_STREAM
	cout << endl;
	std::cout.rdbuf(os);
#endif

	terminate(std::move(threadPool));
	cout << "任务总数：" << counter << endl;
	return 0;
}
