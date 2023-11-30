#include "Eterfree/Concurrency/TaskMapper.hpp"
#include "Eterfree/Concurrency/ThreadPool.h"
#include "Eterfree/Platform/Core/Common.h"

#include <chrono>
#include <cstdlib>
#include <utility>
#include <memory>
#include <iostream>
#include <atomic>
#include <thread>

USING_ETERFREE_SPACE

using ThreadPool = Concurrency::ThreadPool;
using SizeType = ThreadPool::SizeType;

using Message = std::chrono::nanoseconds::rep;
using TaskMapper = Concurrency::TaskMapper<Message>;

static constexpr Message SLEEP_TIME = 1000000;
static constexpr SizeType HANDLER_NUMBER = 17;

static std::atomic_ulong counter = 0;

static void handle(Message& _message)
{
	for (volatile auto index = 0UL; \
		index < 10000UL; ++index);

	Platform::sleepFor(_message);

	counter.fetch_add(1, \
		std::memory_order::relaxed);
}

class Handler final
{
	TaskMapper& _taskMapper;
	SizeType _index;
	SizeType _counter;

public:
	Handler(TaskMapper& _taskMapper, SizeType _index) noexcept : \
		_taskMapper(_taskMapper), _index(_index), _counter(0) {}

	void operator()(Message& _message);
};

void Handler::operator()(Message& _message)
{
	handle(_message);

	if (_counter++ < 100)
	{
		auto index = (_index + 1) % HANDLER_NUMBER;
		_taskMapper.put(index, SLEEP_TIME);
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
	auto taskMapper = std::make_shared<TaskMapper>(0);

	auto taskManager = threadPool->getTaskManager();
	if (taskManager != nullptr)
		taskManager->insert(taskMapper);

	for (SizeType index = 0; index < HANDLER_NUMBER; ++index)
	{
		auto handler = std::make_shared<Handler>(*taskMapper, index);
		taskMapper->set(index, make(handler));
	}

	for (SizeType index = 0; index < HANDLER_NUMBER; ++index)
		taskMapper->put(index, SLEEP_TIME);

	threadPool.reset();
	std::cout << counter.load(std::memory_order::relaxed) << std::endl;
	return EXIT_SUCCESS;
}
