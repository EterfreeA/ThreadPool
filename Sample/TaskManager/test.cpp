#include "Concurrency/TaskQueue.h"
#include "Concurrency/TaskMapper.hpp"
#include "Concurrency/ThreadPool.h"
#include "Platform/Core/Common.h"

#include <chrono>
#include <cstdlib>
#include <utility>
#include <memory>
#include <iostream>
#include <atomic>
#include <thread>

#undef TASK_QUEUE
#undef TASK_MAPPER

using ThreadPool = Eterfree::ThreadPool;
using SizeType = ThreadPool::SizeType;

using IndexType = Eterfree::TaskPool::IndexType;
using TimeType = std::chrono::nanoseconds::rep;

using TaskQueue = Eterfree::TaskQueue;

using Message = TimeType;
using TaskMapper = Eterfree::TaskMapper<Message>;

static constexpr IndexType TASK_QUEUE = 1;
static constexpr IndexType TASK_MAPPER = 2;
static constexpr TimeType SLEEP_TIME = 1000000;

static std::atomic_ulong counter = 0;

static void add(unsigned long _size) noexcept
{
	counter.fetch_add(_size, \
		std::memory_order::relaxed);
}

static void task()
{
	for (volatile auto index = 0UL; \
		index < 10000UL; ++index);

	Eterfree::sleepFor(SLEEP_TIME);
	add(1);
}

static void execute(ThreadPool& _threadPool)
{
	auto taskManager = _threadPool.getTaskManager();
	if (taskManager == nullptr) return;

	auto taskPool = taskManager->find(TASK_QUEUE);
	auto taskQueue = dynamic_cast<TaskQueue*>(taskPool.get());
	if (taskQueue == nullptr) return;

	for (auto index = 0UL; index < 10000UL; ++index)
		taskQueue->put(task);

	for (auto index = 0UL; index < 400UL; ++index)
	{
		TaskQueue::TaskList taskList(100, task);
		taskQueue->put(std::move(taskList));
	}
}

static void handle(Message& _message)
{
	for (volatile auto index = 0UL; \
		index < 10000UL; ++index);

	Eterfree::sleepFor(_message);
	add(1);
}

static void execute(ThreadPool* _threadPool)
{
	using QueueType = TaskMapper::MessageQueue;

	if (_threadPool == nullptr) return;

	constexpr SizeType HANDLER_NUMBER = 5;

	auto taskManager = _threadPool->getTaskManager();
	if (taskManager == nullptr) return;

	auto taskPool = taskManager->find(TASK_MAPPER);
	auto taskMapper = dynamic_cast<TaskMapper*>(taskPool.get());
	if (taskMapper == nullptr) return;

	for (SizeType index = 0; \
		index < HANDLER_NUMBER; ++index)
		taskMapper->set(index, handle, true);

	for (auto index = 0UL; index < 100UL; ++index)
		for (SizeType index = 0; \
			index < HANDLER_NUMBER; ++index)
		{
			QueueType queue(100, SLEEP_TIME);
			taskMapper->put(index, std::move(queue));
		}
}

static void terminate(ThreadPool&& _threadPool)
{
	auto threadPool(std::forward<ThreadPool>(_threadPool));
	(void)threadPool;
}

int main()
{
	using namespace std::chrono;

	using std::cout, std::endl;

	constexpr auto load = []() noexcept
	{ return counter.load(std::memory_order::relaxed); };

	ThreadPool threadPool;
	auto proxy = threadPool.getProxy();

	auto taskManager = proxy.getTaskManager();
	if (taskManager != nullptr)
	{
		auto taskQueue = std::make_shared<TaskQueue>(TASK_QUEUE);
		taskManager->insert(taskQueue);

		auto taskMapper = std::make_shared<TaskMapper>(TASK_MAPPER);
		taskManager->insert(taskMapper);
	}

	auto begin = system_clock::now();
	auto functor = static_cast<void(*)(ThreadPool*)>(execute);
	auto thread = std::thread(functor, &threadPool);

	execute(threadPool);
	std::this_thread::sleep_for(seconds(10));

	auto count = load();
	auto end = system_clock::now();
	auto duration = duration_cast<milliseconds>(end - begin);

	cout << "任务数量：" << count << endl;
	cout << "执行时间：" << duration.count() << endl;

	if (thread.joinable()) thread.join();
	terminate(std::move(threadPool));
	cout << "任务总数：" << load() << endl;
	return EXIT_SUCCESS;
}
