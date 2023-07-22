#include "Concurrency/ThreadPool.hpp"
#include "Concurrency/TaskPool.hpp"

#include <cstdlib>
#include <utility>
#include <chrono>
#include <memory>
#include <iostream>
#include <atomic>
#include <thread>

using Message = std::chrono::milliseconds;
using TaskPool = Eterfree::TaskPool<Message>;

using ThreadPool = Eterfree::ThreadPool<>;
using SizeType = ThreadPool::SizeType;

static constexpr auto SLEEP_TIME = Message(1);
static constexpr SizeType HANDLER_NUMBER = 17;

static std::atomic_ulong counter = 0;

static void handle(Message& _message)
{
	for (volatile auto index = 0UL; \
		index < 10000UL; ++index);
	std::this_thread::sleep_for(_message);
	counter.fetch_add(1, \
		std::memory_order::relaxed);
}

class Handler final
{
	TaskPool& _taskPool;
	SizeType _index;
	SizeType _counter;

public:
	Handler(TaskPool& _taskPool, SizeType _index) noexcept : \
		_taskPool(_taskPool), _index(_index), _counter(0) {}

	void operator()(Message& _message);
};

void Handler::operator()(Message& _message)
{
	handle(_message);

	if (_counter++ < 100)
	{
		auto index = (_index + 1) % HANDLER_NUMBER;
		_taskPool.put(index, SLEEP_TIME);
	}
}

template <typename _Functor>
static auto make(_Functor&& _functor)
{
	return [functor = std::forward<_Functor>(_functor)](Message& _message)
	{
		(*functor)(_message);
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
