#include "ThreadPool.h"
#include "Thread.h"
#include "Queue.h"
#include "Condition.hpp"

#include <atomic>
#include <thread>

ETERFREE_BEGIN

// 生成原子的设置函数
#define SET_ATOMIC(SizeType, Arithmetic, funtor, field) \
SizeType funtor(SizeType size, Arithmetic arithmetic) noexcept \
{ \
	switch (arithmetic) \
	{ \
	case Arithmetic::REPLACE: \
		field.store(size, std::memory_order::memory_order_relaxed); \
		return size; \
	case Arithmetic::INCREASE: \
		return field.fetch_add(size, std::memory_order::memory_order_relaxed); \
	case Arithmetic::DECREASE: \
		return field.fetch_sub(size, std::memory_order::memory_order_relaxed); \
	default: \
		return field.load(std::memory_order::memory_order_relaxed); \
	} \
}

// 线程池数据结构体
struct ThreadPool::Structure
{
	using QueueType = Queue<Functor>;
	using Condition = Condition<>;

	std::list<Thread> threadTable;							// 线程表
	std::shared_ptr<QueueType> taskQueue;					// 任务队列
	std::function<void(bool, Thread::ThreadID)> callback;	// 回调函数子

	std::thread thread;										// 守护线程
	Condition condition;									// 强化条件变量

	std::atomic<SizeType> capacity;							// 线程池容量
	std::atomic<SizeType> size;								// 线程数量
	std::atomic<SizeType> idleSize;							// 闲置线程数量

	/*
	 * 默认构造函数
	 * 若先以运算符new创建实例，再交由共享指针std::shared_ptr托管，
	 * 则至少二次分配内存，先为实例分配内存，再为共享指针的控制块分配内存。
	 * 而std::make_shared典型地仅分配一次内存，实例内存和控制块内存连续。
	 */
	Structure() : taskQueue(std::make_shared<QueueType>()) {}

	// 设置线程池容量
	void setCapacity(SizeType capacity) noexcept
	{
		this->capacity.store(capacity, std::memory_order::memory_order_relaxed);
	}

	// 获取线程池容量
	SizeType getCapacity() const noexcept
	{
		return capacity.load(std::memory_order::memory_order_relaxed);
	}

	// 算术枚举
	enum class Arithmetic { REPLACE, INCREASE, DECREASE };

	// 设置线程数量
	SET_ATOMIC(SizeType, Arithmetic, setSize, this->size);

	// 获取线程数量
	SizeType getSize() const noexcept
	{
		return size.load(std::memory_order::memory_order_relaxed);
	}

	// 设置闲置线程数量
	SET_ATOMIC(SizeType, Arithmetic, setIdleSize, idleSize);

	// 获取闲置线程数量
	SizeType getIdleSize() const noexcept
	{
		return idleSize.load(std::memory_order::memory_order_relaxed);
	}
};

// 调整线程数量
ThreadPool::SizeType ThreadPool::adjust(DataType& data)
{
	auto size = data->getSize();
	auto capacity = data->getCapacity();

	// 1.删减线程
	if (size >= capacity)
		return size - capacity;

	// 2.增加线程
	size = capacity - size;

	// 添加线程至线程表
	for (decltype(size) index = 0; index < size; ++index)
	{
		Thread thread;
		thread.configure(data->taskQueue, data->callback);
		data->threadTable.push_back(std::move(thread));
	}
	return 0;
}

// 守护线程主函数
void ThreadPool::execute(DataType data)
{
	/*
	 * 条件变量的谓词，不必等待通知的条件
	 * 1.存在闲置线程并且任务队列非空。
	 * 2.存在闲置线程并且需要删减线程。
	 * 3.任务队列非空并且需要增加线程。
	 * 4.条件无效。
	 */
	auto predicate = [&data] {
		bool idle = data->getIdleSize() > 0;
		bool empty = data->taskQueue->empty();
		auto size = data->getSize();
		auto capacity = data->getCapacity();
		return idle && (!empty || size > capacity) \
			|| !empty && (size < capacity); };

	// 若谓词非真，自动解锁互斥元，阻塞守护线程，直至通知激活，再次锁定互斥元
	data->condition.wait(predicate);

	// 守护线程退出通道
	while (data->condition)
	{
		// 调整线程数量
		auto size = adjust(data);

		// 遍历线程表，访问闲置线程
		for (auto iterator = data->threadTable.begin(); \
			iterator != data->threadTable.end() && data->getIdleSize() > 0;)
		{
			// 若线程处于闲置状态
			if (auto& thread = *iterator; thread.idle())
			{
				// 若通知线程执行任务成功，则减少闲置线程数量
				if (thread.notify())
					data->setIdleSize(1, Structure::Arithmetic::DECREASE);
				// 删减线程
				else if (size > 0)
				{
					iterator = data->threadTable.erase(iterator);
					data->setIdleSize(1, Structure::Arithmetic::DECREASE);
					--size;
					continue;
				}
			}
			++iterator;
		}

		// 根据谓词真假，决定是否阻塞守护线程
		data->condition.wait(predicate);
	}

	// 清空线程
	data->threadTable.clear();
}

// 销毁线程池
void ThreadPool::destroy()
{
	// 避免重复销毁
	if (!data->condition)
		return;

	// 分离守护线程
	//data->thread.detach();

	// 通知守护线程退出
	data->condition.exit();

	// 挂起直到守护线程退出
	if (data->thread.joinable())
		data->thread.join();
}

// 默认构造函数
ThreadPool::ThreadPool(SizeType size, SizeType capacity)
	: data(std::make_shared<Structure>())
{
	using Arithmetic = Structure::Arithmetic;

	// 定义回调函数子
	data->callback = [weakData = std::weak_ptr(data)](bool idle, Thread::ThreadID id)
	{
		// 线程并非闲置状态
		if (!idle)
			return;

		// 若未增加之前，无闲置线程，则通知守护线程
		if (auto data = weakData.lock(); \
			data != nullptr && data->setIdleSize(1, Arithmetic::INCREASE) == 0)
			data->condition.notify_one(Structure::Condition::Strategy::RELAXED);
	};

	// 初始化线程并放入线程表
	capacity = capacity > 0 ? capacity : 1;
	for (decltype(capacity) index = 0; index < capacity; ++index)
	{
		Thread thread;
		thread.configure(data->taskQueue, data->callback);
		data->threadTable.push_back(std::move(thread));
	}

	setCapacity(capacity); // 设置线程池容量
	data->setSize(capacity, Arithmetic::REPLACE); // 设置线程数量
	data->setIdleSize(capacity, Arithmetic::REPLACE); // 设置闲置线程数量

	// 创建std::thread对象，即守护线程，以data为参数，执行函数execute
	data->thread = std::thread(ThreadPool::execute, data);
}

// 默认析构函数
ThreadPool::~ThreadPool()
{
	// 若数据为空，无需销毁，以支持移动语义
	if (data != nullptr)
		destroy();
}

// 获取支持的并发线程数量
ThreadPool::SizeType ThreadPool::getConcurrency() noexcept
{
	auto concurrency = std::thread::hardware_concurrency();
	return concurrency > 0 ? concurrency : 1;
}

// 设置最大线程数量
void ThreadPool::setMaxThreads(SizeType capacity)
{
	setCapacity(capacity);
}

// 设置线程池容量
void ThreadPool::setCapacity(SizeType capacity)
{
	if (capacity > 0)
	{
		data->setCapacity(capacity);
		data->condition.notify_one(Structure::Condition::Strategy::RELAXED);
	}
}

// 获取最大线程数量
ThreadPool::SizeType ThreadPool::getMaxThreads() const noexcept
{
	return getCapacity();
}

// 获取线程池容量
ThreadPool::SizeType ThreadPool::getCapacity() const noexcept
{
	return data->getCapacity();
}

// 设置线程数量
bool ThreadPool::setThreads(SizeType size) noexcept
{
	return false;
}

// 获取线程数量
ThreadPool::SizeType ThreadPool::getThreads() const noexcept
{
	return getSize();
}

// 获取线程数量
ThreadPool::SizeType ThreadPool::getSize() const noexcept
{
	return data->getSize();
}

// 获取空闲线程数量
ThreadPool::SizeType ThreadPool::getFreeThreads() const noexcept
{
	return getIdleSize();
}

// 获取闲置线程数量
ThreadPool::SizeType ThreadPool::getIdleSize() const noexcept
{
	return data->getIdleSize();
}

// 获取任务数量
ThreadPool::SizeType ThreadPool::getTasks() const noexcept
{
	return getTaskSize();
}

// 获取任务数量
ThreadPool::SizeType ThreadPool::getTaskSize() const noexcept
{
	return data->taskQueue->size();
}

// 向任务队列添加单任务
bool ThreadPool::pushTask(Functor&& task)
{
	// 过滤无效任务
	if (!task)
		return false;

	// 若添加任务之前，任务队列为空，则通知守护线程
	auto result = data->taskQueue->push(std::move(task));
	if (result && result.value() == 0)
		data->condition.notify_one(Structure::Condition::Strategy::RELAXED);
	return result.has_value();
}

// 向任务队列批量添加任务
bool ThreadPool::pushTask(std::list<Functor>& tasks)
{
	// 过滤无效任务
	decltype(tasks.size()) size = 0;
	for (auto iterator = tasks.cbegin(); iterator != tasks.cend();)
		if (!*iterator)
			iterator = tasks.erase(iterator);
		else
		{
			++iterator;
			++size;
		}

	if (size <= 0)
		return false;

	// 若添加任务之前，任务队列为空，则通知守护线程
	auto result = data->taskQueue->push(tasks);
	if (result && result.value() == 0)
		data->condition.notify_one(Structure::Condition::Strategy::RELAXED);
	return result.has_value();
}

ETERFREE_END
