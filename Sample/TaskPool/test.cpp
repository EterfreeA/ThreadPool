#include "Concurrency/TaskPool.hpp"
#include "Concurrency/ThreadPool.h"
#include "Core/Condition.hpp"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <iostream>
#include <atomic>
#include <thread>

USING_ETERFREE_SPACE

static std::atomic_flag flag;
static Condition condition;

static void handle(const char*)
{
	condition.wait([]() noexcept \
	{ return flag.test(std::memory_order::relaxed); });
}

static void print(const ThreadPool& _threadPool)
{
	auto proxy = _threadPool.getProxy();
	auto capacity = proxy.getCapacity();
	auto totalSize = proxy.getTotalSize();
	auto idleSize = proxy.getIdleSize();
	std::cout << capacity << ' ' \
		<< totalSize << ' ' << idleSize;

	if (auto taskManager = proxy.getTaskManager())
	{
		auto taskSize = taskManager->size();
		std::cout << ' ' << taskSize;
	}
	std::cout << std::endl;
}

int main()
{
	using namespace std::chrono;
	using namespace std::this_thread;

	using TaskPool = TaskPool<const char*>;

	ThreadPool threadPool;
	auto taskPool = std::make_shared<TaskPool>();
	auto proxy = threadPool.getProxy();
	proxy.setTaskManager(taskPool);

	auto capacity = proxy.getCapacity();
	for (decltype(capacity) index = 0; \
		index < capacity; ++index)
		taskPool->set(index, handle);

	sleep_for(seconds(1));
	print(threadPool);

	const char* MODULE = "Eterfree::ThreadPool";
	for (decltype(capacity) index = 0; \
		index < capacity; ++index)
		taskPool->put(index, MODULE);

	sleep_for(seconds(1));
	print(threadPool);

	taskPool->set(capacity, [](const char* _module)
		{
			std::cout << _module << std::endl;
		});

	sleep_for(seconds(1));
	print(threadPool);

	taskPool->put(capacity, MODULE);

	sleep_for(seconds(1));
	print(threadPool);

	proxy.setCapacity(capacity + 1);

	sleep_for(seconds(1));
	print(threadPool);

	flag.test_and_set(std::memory_order::relaxed);
	condition.exit();
	return EXIT_SUCCESS;
}
