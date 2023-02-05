﻿#define ETERFREE
#define BOOST

#define TASK_QUEUE
#define TASK_POOL

//#define FILE_STREAM
//#define FILE_SYSTEM

#if defined ETERFREE
#include "ThreadPool.hpp"

#if defined TASK_QUEUE
#include "TaskQueue.h"

#elif defined TASK_POOL
#include "TaskPool.hpp"

#include <type_traits>
#endif

#include <memory>

#elif defined BOOST
#include <threadpool/threadpool.hpp>
#endif

#include <cstdlib>
#include <utility>
#include <chrono>
#include <iostream>
#include <atomic>
#include <thread>

#ifdef FILE_STREAM
#include <fstream>

#ifdef FILE_SYSTEM
#include <filesystem>
#endif // FILE_SYSTEM
#endif // FILE_STREAM

constexpr auto SLEEP_TIME = std::chrono::milliseconds(3);
static std::atomic_ulong counter = 0;

#if not defined(ETERFREE) or defined(TASK_QUEUE)
static void task()
{
	for (volatile auto index = 0UL; index < 10000UL; ++index);
	std::this_thread::sleep_for(SLEEP_TIME);
	counter.fetch_add(1, std::memory_order::relaxed);
}

#else
using EventType = std::remove_const_t<decltype(SLEEP_TIME)>;
static void handle(EventType& _event)
{
	for (volatile auto index = 0UL; index < 10000UL; ++index);
	std::this_thread::sleep_for(SLEEP_TIME);
	counter.fetch_add(1, std::memory_order::relaxed);
}
#endif

#if defined ETERFREE
#if defined TASK_QUEUE
using TaskManager = eterfree::TaskQueue;
#elif defined TASK_POOL
using TaskManager = eterfree::TaskPool<EventType>;
#endif

using ThreadPool = eterfree::ThreadPool<TaskManager>;

#if defined TASK_QUEUE
static void execute(ThreadPool& _threadPool)
{
	auto taskManager = _threadPool.getTaskManager();
	if (not taskManager) return;

	for (auto index = 0UL; index < 20000UL; ++index)
		taskManager->put(task);

	TaskManager::QueueType queue;
	for (auto index = 0UL; index < 30000UL; ++index)
		queue.push_back(task);
	taskManager->put(std::move(queue));
}

#elif defined TASK_POOL
static void execute(ThreadPool& _threadPool)
{
	auto taskManager = _threadPool.getTaskManager();
	if (not taskManager) return;

	constexpr TaskManager::IndexType INDEX = 0;
	taskManager->set(INDEX, handle, true);

	for (auto index = 0UL; index < 20000UL; ++index)
		taskManager->put(INDEX, SLEEP_TIME);

	TaskManager::EventQueue queue;
	for (auto index = 0UL; index < 30000UL; ++index)
		queue.push_back(SLEEP_TIME);
	taskManager->put(INDEX, std::move(queue));
}
#endif

static void terminate(ThreadPool&& _threadPool)
{
	_threadPool.setTaskManager(nullptr);
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

using ThreadPool = boost::threadpool::thread_pool<>;
static void execute(ThreadPool& _threadPool)
{
	for (auto index = 0UL; index < 20000UL; ++index)
		_threadPool.schedule(task);

	for (auto index = 0UL; index < 30000UL; ++index)
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

	constexpr auto load = []() noexcept
	{ return counter.load(std::memory_order::relaxed); };

#ifdef FILE_STREAM
	constexpr auto FILE = "ThreadPool.log";

#ifdef FILE_SYSTEM
	std::filesystem::remove(FILE);
#endif // FILE_SYSTEM

	std::ofstream ofs(FILE, std::ios::app);
	auto os = cout.rdbuf(ofs.rdbuf());
#endif // FILE_STREAM

#if defined ETERFREE
	ThreadPool threadPool;
	auto taskManager = std::make_shared<TaskManager>();
	threadPool.setTaskManager(taskManager);

#elif defined BOOST
	ThreadPool threadPool(getConcurrency());
#endif

	using namespace std::chrono;
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
