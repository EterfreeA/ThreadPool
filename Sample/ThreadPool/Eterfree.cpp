#undef TASK_QUEUE
#undef TASK_POOL

#define TASK_QUEUE
#define TASK_POOL

#include "Common.h"
#include "Concurrency/ThreadPool.hpp"

#if defined TASK_QUEUE
#include "Concurrency/TaskQueue.h"

#elif defined TASK_POOL
#include "Concurrency/TaskPool.hpp"

#include <type_traits>
#endif

#include <cstdlib>
#include <utility>
#include <chrono>
#include <memory>
#include <iostream>
#include <atomic>
#include <thread>

static constexpr auto SLEEP_TIME = std::chrono::milliseconds(1);

#if defined TASK_QUEUE
using TaskManager = eterfree::TaskQueue;
#elif defined TASK_POOL
using Message = std::remove_const_t<decltype(SLEEP_TIME)>;
using TaskManager = eterfree::TaskPool<Message>;
#endif

using ThreadPool = eterfree::ThreadPool<TaskManager>;

static std::atomic_ulong counter = 0;

#if defined TASK_QUEUE
static void task()
{
	for (volatile auto index = 0UL; \
		index < 10000UL; ++index);
	sleep_for(SLEEP_TIME);
	counter.fetch_add(1, \
		std::memory_order::relaxed);
}

static void execute(ThreadPool& _threadPool)
{
	auto taskManager = _threadPool.getTaskManager();
	if (not taskManager) return;

	for (auto index = 0UL; index < 50000UL; ++index)
		taskManager->put(task);

	TaskManager::QueueType queue;
	for (auto index = 0UL; index < 50000UL; ++index)
		queue.push_back(task);
	taskManager->put(std::move(queue));
}

#elif defined TASK_POOL
static void handle(Message& _message)
{
	for (volatile auto index = 0UL; \
		index < 10000UL; ++index);
	sleep_for(_message);
	counter.fetch_add(1, \
		std::memory_order::relaxed);
}

static void execute(ThreadPool& _threadPool)
{
	auto taskManager = _threadPool.getTaskManager();
	if (not taskManager) return;

	constexpr TaskManager::IndexType INDEX = 0;
	taskManager->set(INDEX, handle, true);

	for (auto index = 0UL; index < 50000UL; ++index)
		taskManager->put(INDEX, SLEEP_TIME);

	TaskManager::MessageQueue queue;
	for (auto index = 0UL; index < 50000UL; ++index)
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

int main()
{
	using std::cout, std::endl;
	using namespace std::chrono;

	constexpr auto load = []() noexcept
	{ return counter.load(std::memory_order::relaxed); };

#ifdef FILE_STREAM
	constexpr auto FILE = "Eterfree.txt";

#ifdef FILE_SYSTEM
	std::filesystem::remove(FILE);
#endif // FILE_SYSTEM

	std::ofstream ofs(FILE, std::ios::app);
	auto os = cout.rdbuf(ofs.rdbuf());
#endif // FILE_STREAM

	ThreadPool threadPool;
	auto taskManager = std::make_shared<TaskManager>();
	threadPool.setTaskManager(taskManager);

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
