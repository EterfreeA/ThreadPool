﻿#include "Concurrency/TaskMapper.hpp"
#include "Concurrency/ThreadPool.h"
#include "Core/Condition.hpp"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <iostream>
#include <atomic>
#include <thread>

USING_ETERFREE_SPACE

using TaskMapper = Concurrency::TaskMapper<const char*>;
using ThreadPool = Concurrency::ThreadPool;

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

	ThreadPool threadPool;
	auto proxy = threadPool.getProxy();

	auto taskMapper = std::make_shared<TaskMapper>(0);
	auto taskManager = proxy.getTaskManager();
	if (taskManager != nullptr)
		taskManager->insert(taskMapper);

	auto capacity = proxy.getCapacity();
	for (decltype(capacity) index = 0; \
		index < capacity; ++index)
		taskMapper->set(index, handle);

	sleep_for(seconds(1));
	print(threadPool);

	const char* MODULE = "Eterfree::ThreadPool";
	for (decltype(capacity) index = 0; \
		index < capacity; ++index)
		taskMapper->put(index, MODULE);

	sleep_for(seconds(1));
	print(threadPool);

	taskMapper->set(capacity, [](const char* _module)
		{
			std::cout << _module << std::endl;
		});

	sleep_for(seconds(1));
	print(threadPool);

	taskMapper->put(capacity, MODULE);

	sleep_for(seconds(1));
	print(threadPool);

	proxy.setCapacity(capacity + 1);

	sleep_for(seconds(1));
	print(threadPool);

	flag.test_and_set(std::memory_order::relaxed);
	condition.exit();
	return EXIT_SUCCESS;
}