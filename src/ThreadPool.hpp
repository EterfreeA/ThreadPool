/*
文件名称：ThreadPool.hpp
摘要：
1.定义线程池类模板ThreadPool。
2.当任务队列为空时，阻塞守护线程，而增加任务之时，激活守护线程，通知线程获取任务。
3.当无闲置线程时，阻塞守护线程，而存在闲置线程之时，激活守护线程，通知闲置线程获取任务。
4.当销毁线程池时，等待守护线程退出，而守护线程在退出之前，等待所有线程退出。
5.提供增删线程策略，由守护线程增删线程。
	在任务队列非空之时一次性增加线程，当存在闲置线程之时，逐个删减线程。
6.引入双缓冲队列类模板Queue，降低读写任务的相互影响，提高放入任务和取出任务的效率。
7.引入条件类模板Condition，当激活先于阻塞之时，确保守护线程正常退出。
8.守护线程主函数声明为静态成员，除去与类成员指针this的关联性。

版本：v2.0.0
作者：许聪
邮箱：2592419242@qq.com
创建日期：2017年09月22日
更新日期：2021年04月04日
*/

#pragma once

#include <functional>
#include <utility>
#include <cstddef>
#include <memory>
#include <list>
#include <atomic>
#include <thread>

#include "Core.hpp"
#include "Thread.hpp"
#include "Queue.hpp"
#include "Condition.hpp"

ETERFREE_BEGIN

// 生成原子的设置函数
#define SET_ATOMIC(SizeType, Arithmetic, funtor, field) \
SizeType funtor(SizeType _size, Arithmetic _arithmetic) noexcept \
{ \
	switch (_arithmetic) \
	{ \
	case Arithmetic::REPLACE: \
		field.store(_size, std::memory_order::memory_order_relaxed); \
		return _size; \
	case Arithmetic::INCREASE: \
		return field.fetch_add(_size, std::memory_order::memory_order_relaxed); \
	case Arithmetic::DECREASE: \
		return field.fetch_sub(_size, std::memory_order::memory_order_relaxed); \
	default: \
		return field.load(std::memory_order::memory_order_relaxed); \
	} \
}

template <typename _SizeType = std::size_t>
class ThreadPool
{
	//friend class Thread;
public:
	using SizeType = _SizeType;
	using Functor = std::function<void()>;

private:
	// 线程池数据结构体
	struct Structure
	{
		using QueueType = Queue<Functor>;

		std::list<Thread> _threadTable;							// 线程表
		std::shared_ptr<QueueType> _taskQueue;					// 任务队列
		std::function<void(bool, Thread::ThreadID)> _callback;	// 回调函数子

		std::thread _thread;									// 守护线程
		Condition<> _condition;									// 强化条件变量

		std::atomic<SizeType> _capacity;						// 线程池容量
		std::atomic<SizeType> _size;								// 线程数量
		std::atomic<SizeType> _idleSize;							// 闲置线程数量

		/*
		 * 默认构造函数
		 * 若先以运算符new创建实例，再交由共享指针std::shared_ptr托管，
		 * 则至少二次分配内存，先为实例分配内存，再为共享指针的控制块分配内存。
		 * 而std::make_shared典型地仅分配一次内存，实例内存和控制块内存连续。
		 */
		Structure() : _taskQueue(std::make_shared<QueueType>()) {}

		// 设置线程池容量
		void setCapacity(SizeType _capacity) noexcept
		{
			this->_capacity.store(_capacity, std::memory_order::memory_order_relaxed);
		}

		// 获取线程池容量
		SizeType getCapacity() const noexcept
		{
			return _capacity.load(std::memory_order::memory_order_relaxed);
		}

		// 算术枚举
		enum class Arithmetic { REPLACE, INCREASE, DECREASE };

		// 设置线程数量
		SET_ATOMIC(SizeType, Arithmetic, setSize, this->_size);

		// 获取线程数量
		SizeType getSize() const noexcept
		{
			return _size.load(std::memory_order::memory_order_relaxed);
		}

		// 设置闲置线程数量
		SET_ATOMIC(SizeType, Arithmetic, setIdleSize, _idleSize);

		// 获取闲置线程数量
		SizeType getIdleSize() const noexcept
		{
			return _idleSize.load(std::memory_order::memory_order_relaxed);
		}
	};
	using DataType = std::shared_ptr<Structure>;

private:
	DataType _data;

private:
	static SizeType adjust(DataType& _data);
	static void execute(DataType _data);
	void destroy();

public:
	ThreadPool(SizeType _capacity = getConcurrency());
	ThreadPool(const ThreadPool&) = delete;
	/*
	 * 非线程安全。一旦启用移动构造函数，无法确保接口的线程安全性。
	 * 解决方案：
	 *     1.类外增加互斥操作，确保所有接口的线程安全。
	 *     2.类内添加静态成员变量，确保接口的原子性。不过，此法影响类的所有对象，可能降低执行效率。
	 *     3.类外传递互斥元至类内，确保移动语义的线程安全。
	 */
	ThreadPool(ThreadPool&&) noexcept = default;

	// 默认析构函数
	~ThreadPool()
	{
		// 若数据为空，无需销毁，以支持移动语义
		if (_data != nullptr)
			destroy();
	}

	ThreadPool& operator=(const ThreadPool&) = delete;
	// 非线程安全，同移动构造函数。
	ThreadPool& operator=(ThreadPool&&) noexcept = default;

	// 获取支持的并发线程数量
	static SizeType getConcurrency() noexcept
	{
		auto concurrency = std::thread::hardware_concurrency();
		return concurrency > 0 ? concurrency : 1;
	}

	// 设置线程池容量
	void setCapacity(SizeType _capacity)
	{
		if (_capacity > 0)
		{
			_data->setCapacity(_capacity);
			_data->_condition.notify_one();
		}
	}

	// 获取线程池容量
	SizeType getCapacity() const noexcept
	{
		return _data->getCapacity();
	}

	// 获取线程数量
	SizeType getSize() const noexcept
	{
		return _data->getSize();
	}

	// 获取闲置线程数量
	SizeType getIdleSize() const noexcept
	{
		return _data->getIdleSize();
	}

	// 获取任务数量
	SizeType getTaskSize() const noexcept
	{
		return _data->_taskQueue->size();
	}

	// 添加任务
	bool pushTask(Functor&& _task);
	// 适配不同接口的任务，推进线程池的模板化
	template <typename _Functor>
	bool pushTask(_Functor&& task)
	{
		return pushTask(Functor(std::forward<_Functor>(task)));
	}
	template <typename _Functor, typename... _Args>
	bool pushTask(_Functor&& _task, _Args&&... _args)
	{
		return pushTask(Functor([_task, _args...]{ _task(_args...); }));
	}
	// 批量添加任务
	bool pushTask(std::list<Functor>& _tasks);
};

#undef SET_ATOMIC

// 调整线程数量
template <typename _SizeType>
typename ThreadPool<_SizeType>::SizeType ThreadPool<_SizeType>::adjust(DataType& _data)
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
template <typename _SizeType>
void ThreadPool<_SizeType>::execute(DataType _data)
{
	/*
	 * 条件变量的谓词，无需等待通知的条件
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
		return idle && (!empty || size > capacity) \
			|| !empty && (size < capacity) \
			|| !_data->_condition.valid(); };

	// 若谓词为真，自动解锁互斥元，阻塞守护线程，直至通知激活，再次锁定互斥元
	_data->_condition.wait(predicate);

	// 守护线程退出通道
	while (_data->_condition.valid())
	{
		// 调整线程数量
		auto size = adjust(_data);

		// 遍历线程表，尝试通知闲置线程
		for (auto iterator = _data->_threadTable.begin(); \
			iterator != _data->_threadTable.end() && _data->getIdleSize() > 0;)
		{
			// 若线程处于闲置状态
			if (auto& thread = *iterator; thread.idle())
			{
				// 若通知线程执行任务成功，则减少闲置线程数量
				if (thread.notify())
					_data->setIdleSize(1, Structure::Arithmetic::DECREASE);
				// 删减线程
				else if (size > 0)
				{
					iterator = _data->_threadTable.erase(iterator);
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

// 销毁线程池
template <typename _SizeType>
void ThreadPool<_SizeType>::destroy()
{
	// 避免重复销毁
	if (!_data->_condition.valid())
		return;

	// 分离守护线程
	//_data->_thread.detach();

	// 通知守护线程退出
	_data->_condition.exit();

	// 挂起直到守护线程退出
	if (_data->_thread.joinable())
		_data->_thread.join();
}

// 默认构造函数
template <typename _SizeType>
ThreadPool<_SizeType>::ThreadPool(SizeType _capacity)
	: _data(std::make_shared<Structure>())
{
	using Arithmetic = typename Structure::Arithmetic;

	// 定义回调函数子
	_data->_callback = [_data = std::weak_ptr(_data)](bool idle, Thread::ThreadID id)
	{
		// 线程并非闲置状态
		if (!idle)
			return;

		// 若未增加之前，无闲置线程，则通知守护线程
		if (auto data = _data.lock(); \
			data != nullptr && data->setIdleSize(1, Arithmetic::INCREASE) == 0)
			data->_condition.notify_one();
	};

	// 初始化线程并放入线程表
	_capacity = _capacity > 0 ? _capacity : 1;
	for (decltype(_capacity) index = 0; index < _capacity; ++index)
	{
		Thread thread;
		thread.configure(_data->_taskQueue, _data->_callback);
		_data->_threadTable.push_back(std::move(thread));
	}

	setCapacity(_capacity); // 设置线程池容量
	_data->setSize(_capacity, Arithmetic::REPLACE); // 设置线程数量
	_data->setIdleSize(_capacity, Arithmetic::REPLACE); // 设置闲置线程数量

	// 创建std::thread对象，即守护线程，以_data为参数，执行函数execute
	_data->_thread = std::thread(ThreadPool::execute, _data);
}

// 向任务队列添加单任务
template <typename _SizeType>
bool ThreadPool<_SizeType>::pushTask(Functor&& _task)
{
	// 过滤无效任务
	if (!_task)
		return false;

	// 若添加任务之前，任务队列为空，则通知守护线程
	auto result = _data->_taskQueue->push(std::move(_task));
	if (result && result.value() == 0)
		_data->_condition.notify_one();
	return result.has_value();
}

// 向任务队列批量添加任务
template <typename _SizeType>
bool ThreadPool<_SizeType>::pushTask(std::list<Functor>& _tasks)
{
	// 过滤无效任务
	decltype(_tasks.size()) size = 0;
	for (auto iterator = _tasks.cbegin(); iterator != _tasks.cend();)
		if (!*iterator)
			iterator = _tasks.erase(iterator);
		else
		{
			++iterator;
			++size;
		}

	if (size <= 0)
		return false;

	// 若添加任务之前，任务队列为空，则通知守护线程
	auto result = _data->_taskQueue->push(_tasks);
	if (result && result.value() == 0)
		_data->_condition.notify_one();
	return result.has_value();
}

ETERFREE_END
