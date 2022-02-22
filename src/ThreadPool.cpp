#include "ThreadPool.h"
#include "Thread.h"
#include "Condition.hpp"
#include "Queue.hpp"

#include <atomic>
#include <thread>

ETERFREE_SPACE_BEGIN

// 生成原子的设置函数
#define SET_ATOMIC(SizeType, Arithmetic, functor, field) \
SizeType functor(SizeType _size, Arithmetic _arithmetic) noexcept \
{ \
	constexpr auto memoryOrder = std::memory_order_relaxed; \
	switch (_arithmetic) \
	{ \
	case Arithmetic::REPLACE: \
		field.store(_size, memoryOrder); \
		return _size; \
	case Arithmetic::INCREASE: \
		return field.fetch_add(_size, memoryOrder); \
	case Arithmetic::DECREASE: \
		return field.fetch_sub(_size, memoryOrder); \
	default: \
		return field.load(memoryOrder); \
	} \
}

// 线程池数据结构体
struct ThreadPool::Structure
{
	// 算术枚举
	enum class Arithmetic { REPLACE, INCREASE, DECREASE };

	using QueueType = Queue<Functor>;
	using Condition = Condition<>;

	std::list<Thread> _threadTable;							// 线程表
	std::shared_ptr<QueueType> _taskQueue;					// 任务队列
	std::function<void(bool, Thread::ThreadID)> _callback;	// 回调函数子

	std::thread _thread;									// 守护线程
	Condition _condition;									// 强化条件变量

	std::atomic<SizeType> _capacity;						// 线程池容量
	std::atomic<SizeType> _size;							// 线程数量
	std::atomic<SizeType> _idleSize;						// 闲置线程数量

	/*
	 * 默认构造函数
	 * 若先以运算符new创建实例，再交由共享指针std::shared_ptr托管，
	 * 则至少二次分配内存，先为实例分配内存，再为共享指针的控制块分配内存。
	 * 而std::make_shared典型地仅分配一次内存，实例内存和控制块内存连续。
	 */
	Structure() : _taskQueue(std::make_shared<QueueType>()) {}

	// 过滤任务
	template <typename _TaskQueue>
	static auto filterTask(_TaskQueue& _taskQueue) noexcept -> decltype(_taskQueue.size());

	// 放入任务
	bool pushTask(const Functor& _task);
	bool pushTask(Functor&& _task);

	// 批量放入任务
	bool pushTask(TaskQueue& _taskQueue);
	bool pushTask(TaskQueue&& _taskQueue);

	// 设置线程池容量
	void setCapacity(SizeType _capacity) noexcept
	{
		this->_capacity.store(_capacity, std::memory_order_relaxed);
	}

	// 更新线程池容量
	void updateCapacity(SizeType _capacity)
	{
		setCapacity(_capacity);
		_condition.notify_one(Condition::Strategy::RELAXED);
	}

	// 获取线程池容量
	auto getCapacity() const noexcept
	{
		return _capacity.load(std::memory_order_relaxed);
	}

	// 设置线程数量
	SET_ATOMIC(SizeType, Arithmetic, setSize, this->_size);

	// 获取线程数量
	auto getSize() const noexcept
	{
		return _size.load(std::memory_order_relaxed);
	}

	// 设置闲置线程数量
	SET_ATOMIC(SizeType, Arithmetic, setIdleSize, _idleSize);

	// 获取闲置线程数量
	auto getIdleSize() const noexcept
	{
		return _idleSize.load(std::memory_order_relaxed);
	}
};

#undef SET_ATOMIC

// 过滤无效任务
template <typename _TaskQueue>
auto ThreadPool::Structure::filterTask(_TaskQueue& _taskQueue) noexcept -> decltype(_taskQueue.size())
{
	decltype(_taskQueue.size()) size = 0;
	for (auto iterator = _taskQueue.cbegin(); iterator != _taskQueue.cend();)
		if (!*iterator)
			iterator = _taskQueue.erase(iterator);
		else
		{
			++iterator;
			++size;
		}
	return size;
}

// 放入单任务
bool ThreadPool::Structure::pushTask(const Functor& _task)
{
	// 若放入任务之前，任务队列为空，则通知守护线程
	auto result = _taskQueue->push(_task);
	if (result && result.value() == 0)
		_condition.notify_one(Condition::Strategy::RELAXED);
	return result.has_value();
}

// 放入单任务
bool ThreadPool::Structure::pushTask(Functor&& _task)
{
	// 若放入任务之前，任务队列为空，则通知守护线程
	auto result = _taskQueue->push(std::forward<Functor>(_task));
	if (result && result.value() == 0)
		_condition.notify_one(Condition::Strategy::RELAXED);
	return result.has_value();
}

// 批量放入任务
bool ThreadPool::Structure::pushTask(TaskQueue& _taskQueue)
{
	// 过滤无效任务
	if (filterTask(_taskQueue) <= 0)
		return false;

	// 若放入任务之前，任务队列为空，则通知守护线程
	auto result = this->_taskQueue->push(_taskQueue);
	if (result && result.value() == 0)
		_condition.notify_one(Condition::Strategy::RELAXED);
	return result.has_value();
}

// 批量放入任务
bool ThreadPool::Structure::pushTask(TaskQueue&& _taskQueue)
{
	// 过滤无效任务
	if (filterTask(_taskQueue) <= 0)
		return false;

	// 若放入任务之前，任务队列为空，则通知守护线程
	auto result = this->_taskQueue->push(std::forward<TaskQueue>(_taskQueue));
	if (result && result.value() == 0)
		_condition.notify_one(Condition::Strategy::RELAXED);
	return result.has_value();
}

// 设置线程池容量
void ThreadPool::Proxy::setCapacity(SizeType _capacity)
{
	if (_capacity > 0 && _data)
		_data->updateCapacity(_capacity);
}

// 获取线程池容量
ThreadPool::SizeType ThreadPool::Proxy::getCapacity() const noexcept
{
	return _data ? _data->getCapacity() : 0;
}

// 获取线程数量
ThreadPool::SizeType ThreadPool::Proxy::getSize() const noexcept
{
	return _data ? _data->getSize() : 0;
}

// 获取闲置线程数量
ThreadPool::SizeType ThreadPool::Proxy::getIdleSize() const noexcept
{
	return _data ? _data->getIdleSize() : 0;
}

// 获取任务数量
ThreadPool::SizeType ThreadPool::Proxy::getTaskSize() const noexcept
{
	return _data ? _data->_taskQueue->size() : 0;
}

// 放入单任务
bool ThreadPool::Proxy::pushTask(const Functor& _task)
{
	return _task && _data && _data->pushTask(_task);
}

// 放入单任务
bool ThreadPool::Proxy::pushTask(Functor&& _task)
{
	return _task && _data && _data->pushTask(std::forward<Functor>(_task));
}

// 批量放入任务
bool ThreadPool::Proxy::pushTask(TaskQueue& _taskQueue)
{
	return _data && _data->pushTask(_taskQueue);
}

// 批量放入任务
bool ThreadPool::Proxy::pushTask(TaskQueue&& _taskQueue)
{
	return _data && _data->pushTask(std::forward<TaskQueue>(_taskQueue));
}

// 批量弹出任务
bool ThreadPool::Proxy::popTask(TaskQueue& _taskQueue)
{
	return _data && _data->_taskQueue->pop(_taskQueue);
}

// 清空任务
void ThreadPool::Proxy::clearTask()
{
	if (_data)
		_data->_taskQueue->clear();
}

// 创建线程池
void ThreadPool::create(DataType&& _data, SizeType _capacity)
{
	using Arithmetic = Structure::Arithmetic;

	// 定义回调函数子
	_data->_callback = [_data = std::weak_ptr(_data)](bool _idle, Thread::ThreadID _id) \
	{
		// 线程并非闲置状态
		if (!_idle)
			return;

		// 若未增加之前，无闲置线程，则通知守护线程
		if (auto data = _data.lock(); \
			data && data->setIdleSize(1, Arithmetic::INCREASE) == 0)
			data->_condition.notify_one(Structure::Condition::Strategy::RELAXED);
	};

	// 初始化线程并放入线程表
	_capacity = _capacity > 0 ? _capacity : 1;
	for (decltype(_capacity) index = 0; index < _capacity; ++index)
	{
		Thread thread;
		thread.configure(_data->_taskQueue, _data->_callback);
		_data->_threadTable.push_back(std::move(thread));
	}

	_data->setCapacity(_capacity); // 设置线程池容量
	_data->setSize(_capacity, Arithmetic::REPLACE); // 设置线程数量
	_data->setIdleSize(_capacity, Arithmetic::REPLACE); // 设置闲置线程数量

	// 创建std::thread对象，即守护线程，以_data为参数，执行函数execute
	_data->_thread = std::thread(ThreadPool::execute, _data);
}

// 销毁线程池
void ThreadPool::destroy(DataType&& _data)
{
	// 避免重复销毁
	if (!_data->_condition)
		return;

	// 分离守护线程
	//_data->_thread.detach();

	// 通知守护线程退出
	_data->_condition.exit();

	// 挂起直到守护线程退出
	if (_data->_thread.joinable())
		_data->_thread.join();

	using Arithmetic = Structure::Arithmetic;
	_data->setCapacity(0); // 设置线程池容量
	_data->setSize(0, Arithmetic::REPLACE); // 设置线程数量
	_data->setIdleSize(0, Arithmetic::REPLACE); // 设置闲置线程数量
}

// 调整线程数量
ThreadPool::SizeType ThreadPool::adjust(DataType& _data)
{
	auto size = _data->getSize();
	auto capacity = _data->getCapacity();

	// 1.删减线程
	if (size >= capacity)
		return size - capacity;

	// 2.增加线程
	size = capacity - size;

	// 添加线程至线程表
	for (decltype(size) index = 0; index < size; ++index)
	{
		Thread thread;
		thread.configure(_data->_taskQueue, _data->_callback);
		_data->_threadTable.push_back(std::move(thread));
	}
	return 0;
}

// 守护线程主函数
void ThreadPool::execute(DataType _data)
{
	/*
	 * 条件变量的谓词，不必等待通知的条件
	 * 1.存在闲置线程并且任务队列非空。
	 * 2.存在闲置线程并且需要删减线程。
	 * 3.任务队列非空并且需要增加线程。
	 * 4.条件无效。
	 */
	auto predicate = [&_data] \
	{
		bool idle = _data->getIdleSize() > 0;
		bool empty = _data->_taskQueue->empty();
		auto size = _data->getSize();
		auto capacity = _data->getCapacity();
		return idle && (!empty || size > capacity) \
			|| !empty && (size < capacity);
	};

	// 若谓词非真，自动解锁互斥元，阻塞守护线程，直至通知激活，再次锁定互斥元
	_data->_condition.wait(predicate);

	// 守护线程退出通道
	while (_data->_condition)
	{
		// 调整线程数量
		auto size = adjust(_data);

		// 遍历线程表，访问闲置线程
		for (auto iterator = _data->_threadTable.begin(); \
			iterator != _data->_threadTable.end() && _data->getIdleSize() > 0;)
		{
			// 若线程处于闲置状态
			if (auto& thread = *iterator; thread.idle())
			{
				using Arithmetic = Structure::Arithmetic;
				// 若通知线程执行任务成功，则减少闲置线程数量
				if (thread.notify())
					_data->setIdleSize(1, Arithmetic::DECREASE);
				// 删减线程
				else if (size > 0)
				{
					iterator = _data->_threadTable.erase(iterator);
					_data->setIdleSize(1, Arithmetic::DECREASE);
					--size;
					continue;
				}
			}
			++iterator;
		}

		// 根据谓词真假，决定是否阻塞守护线程
		_data->_condition.wait(predicate);
	}

	// 清空线程
	_data->_threadTable.clear();
}

// 默认构造函数
ThreadPool::ThreadPool(SizeType _size, SizeType _capacity)
	: _data(std::make_shared<Structure>())
{
	create(load(), _capacity);
}

// 默认移动赋值运算符函数
ThreadPool& ThreadPool::operator=(ThreadPool&& _threadPool)
{
	if (&_threadPool != this)
	{
		std::scoped_lock lock(_mutex, _threadPool._mutex);
		_data = std::move(_threadPool._data);
	}
	return *this;
}

// 获取支持的并发线程数量
ThreadPool::SizeType ThreadPool::getConcurrency() noexcept
{
	auto concurrency = std::thread::hardware_concurrency();
	return concurrency > 0 ? concurrency : 1;
}

// 获取代理
ThreadPool::Proxy ThreadPool::getProxy()
{
	return load();
}

// 设置线程池容量
void ThreadPool::setCapacity(SizeType _capacity)
{
	if (_capacity > 0)
		if (auto data = load())
			data->updateCapacity(_capacity);
}

// 获取线程池容量
ThreadPool::SizeType ThreadPool::getCapacity() const
{
	auto data = load();
	return data ? data->getCapacity() : 0;
}

// 获取线程数量
ThreadPool::SizeType ThreadPool::getSize() const
{
	auto data = load();
	return data ? data->getSize() : 0;
}

// 获取闲置线程数量
ThreadPool::SizeType ThreadPool::getIdleSize() const
{
	auto data = load();
	return data ? data->getIdleSize() : 0;
}

// 获取任务数量
ThreadPool::SizeType ThreadPool::getTaskSize() const
{
	auto data = load();
	return data ? data->_taskQueue->size() : 0;
}

// 放入单任务
bool ThreadPool::pushTask(const Functor& _task)
{
	// 过滤无效任务
	if (!_task)
		return false;

	auto data = load();
	return data && data->pushTask(_task);
}

// 放入单任务
bool ThreadPool::pushTask(Functor&& _task)
{
	// 过滤无效任务
	if (!_task)
		return false;

	auto data = load();
	return data && data->pushTask(std::forward<Functor>(_task));
}

// 批量放入任务
bool ThreadPool::pushTask(TaskQueue& _taskQueue)
{
	auto data = load();
	return data && data->pushTask(_taskQueue);
}

// 批量放入任务
bool ThreadPool::pushTask(TaskQueue&& _taskQueue)
{
	auto data = load();
	return data && data->pushTask(std::forward<TaskQueue>(_taskQueue));
}

// 批量弹出任务
bool ThreadPool::popTask(TaskQueue& _taskQueue)
{
	auto data = load();
	return data && data->_taskQueue->pop(_taskQueue);
}

// 清空任务
void ThreadPool::clearTask()
{
	if (auto data = load())
		data->_taskQueue->clear();
}

ETERFREE_SPACE_END
