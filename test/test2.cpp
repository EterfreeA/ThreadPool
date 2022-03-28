#include "ThreadPool.h"
#include "Condition.hpp"

#include <cstdlib>
#include <chrono>
#include <iostream>
#include <atomic>
#include <thread>

ETERFREE_SPACE

static std::atomic_bool valid = true;
static Condition condition;

static void task()
{
	condition.wait([] { return !valid.load(std::memory_order_relaxed); });
}

static void print(const ThreadPool& _threadPool)
{
	std::cout << _threadPool.getCapacity() << ' ' << _threadPool.getSize() << ' ' \
		<< _threadPool.getIdleSize() << ' ' << _threadPool.getTaskSize() << std::endl;
}

int main()
{
	ThreadPool threadPool;
	auto proxy = threadPool.getProxy();
	auto capacity = proxy.getCapacity();
	for (decltype(capacity) index = 0; index < capacity; ++index)
		threadPool.pushTask(task);

	using namespace std::this_thread;
	using namespace std::chrono;
	sleep_for(seconds(2));
	print(threadPool);

	threadPool.pushTask([] { std::cout << "eterfree::ThreadPool" << std::endl; });

	sleep_for(seconds(1));
	print(threadPool);

	threadPool.setCapacity(capacity + 1);

	sleep_for(seconds(2));
	print(threadPool);

	valid.store(false, std::memory_order_relaxed);
	condition.exit();
	return EXIT_SUCCESS;
}
