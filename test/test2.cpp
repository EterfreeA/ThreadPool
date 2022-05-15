#include "ThreadPool.hpp"
#include "Condition.hpp"

#include <cstdlib>
#include <chrono>
#include <iostream>
#include <atomic>
#include <thread>

USING_ETERFREE_SPACE

static std::atomic_flag flag;
static Condition condition;

static void task()
{
	condition.wait([] { return flag.test(std::memory_order::relaxed); });
}

template <typename _Functor, typename _Queue>
static void print(const ThreadPool<_Functor, _Queue>& _threadPool)
{
	auto [capacity, size, idleSize, taskSize] = _threadPool.getSnapshot();
	std::cout << capacity << ' ' << size << ' ' \
		<< idleSize << ' ' << taskSize << std::endl;
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

	flag.test_and_set();
	condition.exit();
	return EXIT_SUCCESS;
}
