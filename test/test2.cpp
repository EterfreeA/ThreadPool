#include "ThreadPool.hpp"
#include "TaskQueue.h"
#include "Condition.hpp"

#include <cstdlib>
#include <chrono>
#include <memory>
#include <iostream>
#include <atomic>
#include <thread>

USING_ETERFREE_SPACE

static std::atomic_flag flag;
static Condition condition;

static void task()
{
	condition.wait([] \
	{ return flag.test(std::memory_order::relaxed); });
}

static void print(const ThreadPool<TaskManager>& _threadPool)
{
	auto proxy = _threadPool.getProxy();
	auto capacity = proxy.getCapacity();
	auto totalSize = proxy.getTotalSize();
	auto idleSize = proxy.getIdleSize();
	std::cout << capacity << ' ' << totalSize << ' ' << idleSize;

	if (auto taskManager = proxy.getTaskManager())
	{
		auto taskSize = taskManager->size();
		std::cout << ' ' << taskSize;
	}
	std::cout << std::endl;
}

int main()
{
	auto taskQueue = std::make_shared<TaskQueue>();
	ThreadPool<TaskManager> threadPool;
	auto proxy = threadPool.getProxy();
	proxy.setTaskManager(taskQueue);

	auto capacity = proxy.getCapacity();
	for (decltype(capacity) index = 0; \
		index < capacity; ++index)
		taskQueue->put(task);

	using namespace std::this_thread;
	using namespace std::chrono;
	sleep_for(seconds(2));
	print(threadPool);

	taskQueue->put([] \
	{ std::cout << "eterfree::ThreadPool" << std::endl; });

	sleep_for(seconds(1));
	print(threadPool);

	proxy.setCapacity(capacity + 1);

	sleep_for(seconds(2));
	print(threadPool);

	flag.test_and_set(std::memory_order::relaxed);
	condition.exit();
	return EXIT_SUCCESS;
}
