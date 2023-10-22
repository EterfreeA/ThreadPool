#undef TASK_QUEUE
#undef TASK_MAPPER

#define TASK_QUEUE
#define TASK_MAPPER

#include "Common.h"
#include "Concurrency/ThreadPool.h"

#if defined TASK_QUEUE
#include "Concurrency/TaskQueue.h"
#elif defined TASK_MAPPER
#include "Concurrency/TaskMapper.hpp"
#endif

#include <chrono>
#include <cstdlib>
#include <utility>
#include <memory>
#include <iostream>
#include <atomic>
#include <thread>

USING_ETERFREE_SPACE

using ThreadPool = Concurrency::ThreadPool;
using TimeType = std::chrono::nanoseconds::rep;

#if defined TASK_QUEUE
using TaskPool = Concurrency::TaskQueue;

#elif defined TASK_MAPPER
using Message = TimeType;
using TaskPool = Concurrency::TaskMapper<Message>;

static constexpr bool PARALLEL = true;
#endif

static constexpr TaskPool::IndexType INDEX = 0;
static constexpr TimeType SLEEP_TIME = 1000000;

static std::atomic_ulong counter = 0;

#if defined TASK_QUEUE
static void task()
{
	for (volatile auto index = 0UL; \
		index < 10000UL; ++index);

	Platform::sleepFor(SLEEP_TIME);

	counter.fetch_add(1, \
		std::memory_order::relaxed);
}

static void execute(ThreadPool& _threadPool)
{
	auto taskManager = _threadPool.getTaskManager();
	if (taskManager == nullptr) return;

	auto taskPool = taskManager->find(INDEX);
	auto taskQueue = dynamic_cast<TaskPool*>(taskPool.get());
	if (taskQueue == nullptr) return;

	for (auto index = 0UL; index < 50000UL; ++index)
		taskQueue->put(task);

	TaskPool::TaskList taskList;
	for (auto index = 0UL; index < 50000UL; ++index)
		taskList.push_back(task);
	taskQueue->put(std::move(taskList));
}

#elif defined TASK_MAPPER
static void handle(Message& _message)
{
	for (volatile auto index = 0UL; \
		index < 10000UL; ++index);

	Platform::sleepFor(_message);

	counter.fetch_add(1, \
		std::memory_order::relaxed);
}

static void execute(ThreadPool& _threadPool)
{
	auto taskManager = _threadPool.getTaskManager();
	if (taskManager == nullptr) return;

	auto taskPool = taskManager->find(INDEX);
	auto taskMapper = dynamic_cast<TaskPool*>(taskPool.get());
	if (taskMapper == nullptr) return;

	taskMapper->set(INDEX, handle, PARALLEL);
	for (auto index = 0UL; index < 50000UL; ++index)
		taskMapper->put(INDEX, SLEEP_TIME);

	TaskPool::MessageQueue queue;
	for (auto index = 0UL; index < 50000UL; ++index)
		queue.push_back(SLEEP_TIME);
	taskMapper->put(INDEX, std::move(queue));
}
#endif

static void terminate(ThreadPool&& _threadPool)
{
	auto taskManager = _threadPool.getTaskManager();
	if (taskManager != nullptr)
		taskManager->clear();

	auto threadPool(std::forward<ThreadPool>(_threadPool));
	(void)threadPool;
}

int main()
{
	using namespace std::chrono;

	using std::cout, std::endl;

	constexpr auto load = []() noexcept
	{ return counter.load(std::memory_order::relaxed); };

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
	auto taskManager = threadPool.getTaskManager();
	if (taskManager != nullptr)
	{
		auto taskPool = std::make_shared<TaskPool>(INDEX);
		taskManager->insert(taskPool);
	}

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
