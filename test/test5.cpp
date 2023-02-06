#include "ThreadPool.hpp"
#include "TaskPool.hpp"

#include <cstdlib>
#include <utility>
#include <chrono>
#include <memory>
#include <iostream>
#include <atomic>
#include <thread>

using EventType = std::chrono::milliseconds;
using TaskPool = eterfree::TaskPool<EventType>;

using ThreadPool = eterfree::ThreadPool<>;
using SizeType = ThreadPool::SizeType;

constexpr auto SLEEP_TIME = EventType(3);
constexpr SizeType HANDLER_NUMBER = 17;

static std::atomic_ulong counter = 0;

static void handle(EventType& _event)
{
	for (volatile auto index = 0UL; index < 10000UL; ++index);
	std::this_thread::sleep_for(_event);
	counter.fetch_add(1, std::memory_order::relaxed);
}

class Handler
{
	TaskPool& _taskPool;
	SizeType _index;
	SizeType _counter;

public:
	Handler(TaskPool& _taskPool, SizeType _index) noexcept : \
		_taskPool(_taskPool), _index(_index), _counter(0) {}

	void operator()(EventType& _event);
};

void Handler::operator()(EventType& _event)
{
	handle(_event);

	if (_counter++ < 100)
	{
		auto index = (_index + 1) % HANDLER_NUMBER;
		_taskPool.put(index, SLEEP_TIME);
	}
}

template <typename _Functor>
auto make(_Functor&& _functor)
{
	return [functor = std::forward<_Functor>(_functor)](EventType& _event)
	{
		(*functor)(_event);
	};
}

int main()
{
	auto threadPool = std::make_shared<ThreadPool>();
	auto taskPool = std::make_shared<TaskPool>();
	threadPool->setTaskManager(taskPool);

	for (SizeType index = 0; index < HANDLER_NUMBER; ++index)
	{
		auto handler = std::make_shared<Handler>(*taskPool, index);
		taskPool->set(index, make(handler));
	}

	for (SizeType index = 0; index < HANDLER_NUMBER; ++index)
		taskPool->put(index, SLEEP_TIME);

	threadPool.reset();
	std::cout << counter.load(std::memory_order::relaxed) << std::endl;
	return EXIT_SUCCESS;
}
