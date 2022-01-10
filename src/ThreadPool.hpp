/*
文件名称：ThreadPool.hpp
摘要：
1.定义线程池类模板ThreadPool。
2.当任务队列为空，阻塞守护线程，在新增任务之时，激活守护线程，通知线程获取任务。
3.当无闲置线程，阻塞守护线程，当存在闲置线程，激活守护线程，通知闲置线程获取任务。
4.当销毁线程池，等待守护线程退出，而守护线程在退出之前，等待所有线程退出。
	线程在退出之前，默认完成任务队列的所有任务，可选弹出所有任务或者清空队列，使得线程立即退出。
5.提供增删线程策略，由守护线程增删线程。
	在任务队列非空之时，一次性增加线程，当存在闲置线程之时，逐个删减线程。
6.以原子操作确保接口的线程安全性，并且新增成员类Proxy，用于减少原子操作，针对频繁操作提升性能。
7.引入双缓冲队列类模板Queue，降低读写任务的相互影响，提高放入和取出任务的效率。
8.引入条件类模板Condition，当激活先于阻塞之时，确保守护线程正常退出。
9.守护线程主函数声明为静态成员，除去与类成员指针this的关联性。

版本：v2.0.4
作者：许聪
邮箱：2592419242@qq.com
创建日期：2017年09月22日
更新日期：2022年01月11日

变化：
v2.0.1
1.运用Condition的宽松策略，提升激活守护线程的效率。
v2.0.2
1.消除谓词对条件实例有效性的重复判断。
v2.0.3
1.修复条件谓词异常。
	在延迟减少线程之时，未减少闲置线程数量，导致守护线程不必等待通知的条件谓词异常。
v2.0.4
1.以原子操作确保移动语义的线程安全性。
2.新增成员类Proxy，提供轻量接口，减少原子操作。
3.新增任务可选复制语义或者移动语义。
*/

#pragma once

#include <type_traits>
#include <functional>
#include <utility>
#include <tuple>
#include <memory>
#include <list>
#include <atomic>
#include <thread>

#include "Core.hpp"
#include "Thread.hpp"
#include "Condition.hpp"
#include "Queue.hpp"

ETERFREE_SPACE_BEGIN

// 生成原子的设置函数
#define SET_ATOMIC(SizeType, Arithmetic, funtor, field) \
SizeType funtor(SizeType _size, Arithmetic _arithmetic) noexcept \
{ \
	switch (_arithmetic) \
	{ \
	case Arithmetic::REPLACE: \
		field.store(_size, std::memory_order::relaxed); \
		return _size; \
	case Arithmetic::INCREASE: \
		return field.fetch_add(_size, std::memory_order::relaxed); \
	case Arithmetic::DECREASE: \
		return field.fetch_sub(_size, std::memory_order::relaxed); \
	default: \
		return field.load(std::memory_order::relaxed); \
	} \
}

template <typename _Functor = std::function<void()>, typename _Queue = Queue<_Functor>>
class ThreadPool
{
public:
	class Proxy;

	using Functor = _Functor;
	using Queue = _Queue;
	using TaskQueue = Queue::QueueType;
	using SizeType = Queue::SizeType;

private:
	using Thread = Thread<>;
	using Condition = Condition<>;

	// 线程池数据结构体
	struct Structure
	{
		std::list<Thread> _threadTable;							// 线程表
		std::shared_ptr<Queue> _taskQueue;						// 任务队列
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
		Structure() : _taskQueue(std::make_shared<Queue>()) {}

		// 过滤任务
		template <typename _TaskQueue>
		static auto filterTask(_TaskQueue&& _taskQueue) -> decltype(_taskQueue.size());

		// 放入任务
		bool pushTask(const Functor& _task);
		bool pushTask(Functor&& _task);

		// 批量放入任务
		bool pushTask(TaskQueue& _taskQueue);
		bool pushTask(TaskQueue&& _taskQueue);

		// 设置线程池容量
		inline void setCapacity(SizeType _capacity) noexcept
		{
			this->_capacity.store(_capacity, std::memory_order::relaxed);
		}

		// 更新线程池容量
		void updateCapacity(SizeType _capacity)
		{
			setCapacity(_capacity);
			_condition.notify_one(Condition::Strategy::RELAXED);
		}

		// 获取线程池容量
		inline auto getCapacity() const noexcept
		{
			return _capacity.load(std::memory_order::relaxed);
		}

		// 算术枚举
		enum class Arithmetic { REPLACE, INCREASE, DECREASE };

		// 设置线程数量
		SET_ATOMIC(SizeType, Arithmetic, setSize, this->_size);

		// 获取线程数量
		inline auto getSize() const noexcept
		{
			return _size.load(std::memory_order::relaxed);
		}

		// 设置闲置线程数量
		SET_ATOMIC(SizeType, Arithmetic, setIdleSize, _idleSize);

		// 获取闲置线程数量
		inline auto getIdleSize() const noexcept
		{
			return _idleSize.load(std::memory_order::relaxed);
		}
	};
	using DataType = std::shared_ptr<Structure>;

private:
	std::atomic<DataType> _data;

private:
	// 创建线程池
	static void create(DataType&& _data, SizeType _capacity);
	// 销毁线程池
	static void destroy(DataType&& _data);
	// 调整线程数量
	static SizeType adjust(DataType& _data);
	// 守护线程主函数
	static void execute(DataType _data);

	// 加载非原子数据
	inline DataType load() const
	{
		return _data.load(std::memory_order::relaxed);
	}

public:
	// 默认构造函数
	ThreadPool(SizeType _capacity = getConcurrency())
		: _data(std::make_shared<Structure>()) { create(load(), _capacity); }

	// 删除默认复制构造函数
	ThreadPool(const ThreadPool&) = delete;

	// 默认移动构造函数
	ThreadPool(ThreadPool&& _thread) noexcept
		: _data(_thread._data.exchange(nullptr, std::memory_order::relaxed)) {}

	// 默认析构函数
	~ThreadPool()
	{
		// 数据非空才进行销毁，以支持移动语义
		if (auto data = _data.exchange(nullptr, std::memory_order::relaxed))
			destroy(std::move(data));
	}

	// 删除默认复制赋值运算符函数
	ThreadPool& operator=(const ThreadPool&) = delete;

	// 默认移动赋值运算符函数
	ThreadPool& operator=(ThreadPool&& _thread) noexcept
	{
		constexpr auto memoryOrder = std::memory_order::relaxed;
		auto data = _thread._data.exchange(nullptr, memoryOrder);
		_data.exchange(data, memoryOrder);
		return *this;
	}

	// 获取支持的并发线程数量
	static SizeType getConcurrency() noexcept
	{
		auto concurrency = std::thread::hardware_concurrency();
		return concurrency > 0 ? concurrency : 1;
	}

	// 获取代理
	Proxy getProxy()
	{
		auto data = load();
		return data ? data : nullptr;
	}

	// 设置线程池容量
	void setCapacity(SizeType _capacity)
	{
		if (_capacity > 0)
			if (auto data = load())
				data->updateCapacity(_capacity);
	}

	// 获取快照（包括线程池容量、线程数量、闲置线程数量、任务数量）
	auto getSnapshot()
	{
		auto data = load();
		using CapacityType = decltype(data->getCapacity());
		using SizeType = decltype(data->getSize());
		using IdleSizeType = decltype(data->getIdleSize());
		using TaskSizeType = decltype(data->_taskQueue->size());

		if (not data)
			return std::make_tuple(static_cast<CapacityType>(0), static_cast<SizeType>(0), static_cast<IdleSizeType>(0), static_cast<TaskSizeType>(0));
		return std::make_tuple(data->getCapacity(), data->getSize(), data->getIdleSize(), data->_taskQueue->size());
	}

	// 放入任务
	bool pushTask(const Functor& _task);
	bool pushTask(Functor&& _task);

	// 适配不同任务接口，推进线程池模板化
	template <typename _Functor>
	bool pushTask(_Functor&& task)
	{
		auto data = load();
		return data and data->pushTask(Functor(std::forward<_Functor>(task)));
	}
	template <typename _Functor, typename... _Args>
	bool pushTask(_Functor&& _task, _Args&&... _args)
	{
		auto data = load();
		//return data and data->pushTask(Functor([_task, _args = std::make_tuple(std::forward<_Args>(_args)...)]{ std::apply(_task, _args); }));
		return data and data->pushTask(Functor([_task, _args...]{ _task(_args...); }));
	}

	// 批量放入任务
	bool pushTask(TaskQueue& _taskQueue)
	{
		auto data = load();
		return data and data->pushTask(_taskQueue);
	}
	bool pushTask(TaskQueue&& _taskQueue)
	{
		auto data = load();
		using TaskQueue = std::remove_reference_t<decltype(_taskQueue)>;
		return data and data->pushTask(std::forward<TaskQueue>(_taskQueue));
	}

	// 批量弹出任务
	bool popTask(TaskQueue& _taskQueue)
	{
		auto data = load();
		return data and data->_taskQueue->pop(_taskQueue);
	}

	// 清空任务
	void clearTask()
	{
		if (auto data = load())
			data->_taskQueue->clear();
	}
};

#undef SET_ATOMIC

template <typename _Functor, typename _Queue>
class ThreadPool<_Functor, _Queue>::Proxy
{
	DataType _data;

public:
	Proxy(DataType _data) noexcept : _data(_data) {}

	inline explicit operator bool() { return static_cast<bool>(_data); }

	// 设置线程池容量
	void setCapacity(SizeType _capacity)
	{
		if (_capacity > 0 and _data)
			_data->updateCapacity(_capacity);
	}

	// 获取线程池容量
	auto getCapacity() const noexcept
	{
		using SizeType = decltype(_data->getCapacity());
		return _data ? _data->getCapacity() : static_cast<SizeType>(0);
	}

	// 获取线程数量
	auto getSize() const noexcept
	{
		using SizeType = decltype(_data->getSize());
		return _data ? _data->getSize() : static_cast<SizeType>(0);
	}

	// 获取闲置线程数量
	auto getIdleSize() const noexcept
	{
		using SizeType = decltype(_data->getIdleSize());
		return _data ? _data->getIdleSize() : static_cast<SizeType>(0);
	}

	// 获取任务数量
	auto getTaskSize() const noexcept
	{
		using SizeType = decltype(_data->_taskQueue->size());
		return _data ? _data->_taskQueue->size() : static_cast<SizeType>(0);
	}

	// 放入任务
	bool pushTask(const Functor& _task)
	{
		return _task and _data and _data->pushTask(_task);
	}
	bool pushTask(Functor&& _task)
	{
		using Functor = std::remove_reference_t<decltype(_task)>;
		return _task and _data and _data->pushTask(std::forward<Functor>(_task));
	}

	// 适配不同任务接口，推进线程池模板化
	template <typename _Functor>
	bool pushTask(_Functor&& task)
	{
		return _data and _data->pushTask(Functor(std::forward<_Functor>(task)));
	}
	template <typename _Functor, typename... _Args>
	bool pushTask(_Functor&& _task, _Args&&... _args)
	{
		//return _data and _data->pushTask(Functor([_task, _args = std::make_tuple(std::forward<_Args>(_args)...)]{ std::apply(_task, _args); }));
		return _data and _data->pushTask(Functor([_task, _args...]{ _task(_args...); }));
	}

	// 批量放入任务
	bool pushTask(TaskQueue& _taskQueue)
	{
		return _data and _data->pushTask(_taskQueue);
	}
	bool pushTask(TaskQueue&& _taskQueue)
	{
		using TaskQueue = std::remove_reference_t<decltype(_taskQueue)>;
		return _data and _data->pushTask(std::forward<TaskQueue>(_taskQueue));
	}

	// 批量弹出任务
	bool popTask(TaskQueue& _taskQueue)
	{
		return _data and _data->_taskQueue->pop(_taskQueue);
	}

	// 清空任务
	void clearTask()
	{
		if (_data)
			_data->_taskQueue->clear();
	}
};

// 过滤无效任务
template <typename _Functor, typename _Queue>
template <typename _TaskQueue>
auto ThreadPool<_Functor, _Queue>::Structure::filterTask(_TaskQueue&& _taskQueue) -> decltype(_taskQueue.size())
{
	decltype(_taskQueue.size()) size = 0;
	for (auto iterator = _taskQueue.cbegin(); iterator != _taskQueue.cend();)
		if (not *iterator)
			iterator = _taskQueue.erase(iterator);
		else
		{
			++iterator;
			++size;
		}
	return size;
}

// 向任务队列放入单任务
template <typename _Functor, typename _Queue>
bool ThreadPool<_Functor, _Queue>::Structure::pushTask(const Functor& _task)
{
	// 若放入任务之前，任务队列为空，则通知守护线程
	auto result = _taskQueue->push(_task);
	if (result and result.value() == 0)
		_condition.notify_one(Condition::Strategy::RELAXED);
	return result.has_value();
}

// 向任务队列放入单任务
template <typename _Functor, typename _Queue>
bool ThreadPool<_Functor, _Queue>::Structure::pushTask(Functor&& _task)
{
	// 若放入任务之前，任务队列为空，则通知守护线程
	using Functor = std::remove_reference_t<decltype(_task)>;
	auto result = _taskQueue->push(std::forward<Functor>(_task));
	if (result and result.value() == 0)
		_condition.notify_one(Condition::Strategy::RELAXED);
	return result.has_value();
}

// 向任务队列批量放入任务
template <typename _Functor, typename _Queue>
bool ThreadPool<_Functor, _Queue>::Structure::pushTask(TaskQueue& _taskQueue)
{
	// 过滤无效任务
	if (filterTask(_taskQueue) <= 0)
		return false;

	// 若放入任务之前，任务队列为空，则通知守护线程
	auto result = this->_taskQueue->push(_taskQueue);
	if (result and result.value() == 0)
		_condition.notify_one(Condition::Strategy::RELAXED);
	return result.has_value();
}

// 向任务队列批量放入任务
template <typename _Functor, typename _Queue>
bool ThreadPool<_Functor, _Queue>::Structure::pushTask(TaskQueue&& _taskQueue)
{
	// 过滤无效任务
	using TaskQueue = std::remove_reference_t<decltype(_taskQueue)>;
	if (filterTask(std::forward<TaskQueue>(_taskQueue)) <= 0)
		return false;

	// 若放入任务之前，任务队列为空，则通知守护线程
	auto result = this->_taskQueue->push(std::forward<TaskQueue>(_taskQueue));
	if (result and result.value() == 0)
		_condition.notify_one(Condition::Strategy::RELAXED);
	return result.has_value();
}

// 创建线程池
template <typename _Functor, typename _Queue>
void ThreadPool<_Functor, _Queue>::create(DataType&& _data, SizeType _capacity)
{
	using Arithmetic = Structure::Arithmetic;

	// 定义回调函数子
	_data->_callback = [_data = std::weak_ptr(_data)](bool idle, Thread::ThreadID id)
	{
		// 线程并非闲置状态
		if (not idle)
			return;

		// 若未增加之前，无闲置线程，则通知守护线程
		if (auto data = _data.lock(); \
			data and data->setIdleSize(1, Arithmetic::INCREASE) == 0)
			data->_condition.notify_one(Condition::Strategy::RELAXED);
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
template <typename _Functor, typename _Queue>
void ThreadPool<_Functor, _Queue>::destroy(DataType&& _data)
{
	// 避免重复销毁
	if (not _data->_condition)
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
template <typename _Functor, typename _Queue>
typename ThreadPool<_Functor, _Queue>::SizeType ThreadPool<_Functor, _Queue>::adjust(DataType& _data)
{
	auto capacity = _data->getCapacity();
	auto size = _data->getSize();

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
template <typename _Functor, typename _Queue>
void ThreadPool<_Functor, _Queue>::execute(DataType _data)
{
	/*
	 * 条件变量的谓词，不必等待通知的条件
	 * 1.存在闲置线程并且任务队列非空。
	 * 2.存在闲置线程并且需要删减线程。
	 * 3.任务队列非空并且需要增加线程。
	 * 4.条件无效。
	 */
	auto predicate = [&_data] {
		bool idle = _data->getIdleSize() > 0;
		bool empty = _data->_taskQueue->empty();
		auto size = _data->getSize();
		auto capacity = _data->getCapacity();
		return idle and (not empty or size > capacity) \
			or not empty and (size < capacity); };

	// 若谓词非真，自动解锁互斥元，阻塞守护线程，直至通知激活，再次锁定互斥元
	_data->_condition.wait(predicate);

	// 守护线程退出通道
	while (_data->_condition)
	{
		// 调整线程数量
		auto size = adjust(_data);

		// 遍历线程表，访问闲置线程
		for (auto iterator = _data->_threadTable.begin(); \
			iterator != _data->_threadTable.end() and _data->getIdleSize() > 0;)
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

// 向任务队列放入单任务
template <typename _Functor, typename _Queue>
bool ThreadPool<_Functor, _Queue>::pushTask(const Functor& _task)
{
	// 过滤无效任务
	if (not _task)
		return false;

	auto data = load();
	return data and data->pushTask(_task);
}

template <typename _Functor, typename _Queue>
bool ThreadPool<_Functor, _Queue>::pushTask(Functor&& _task)
{
	// 过滤无效任务
	if (not _task)
		return false;

	auto data = load();
	using Functor = std::remove_reference_t<decltype(_task)>;
	return data and data->pushTask(std::forward<Functor>(_task));
}

ETERFREE_SPACE_END
