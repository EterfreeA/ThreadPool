/*
* 文件名称：ThreadPool.hpp
* 语言标准：C++20
* 
* 创建日期：2017年09月22日
* 更新日期：2023年02月07日
* 
* 摘要
* 1.定义线程池类模板ThreadPool。
* 2.当无任务时，阻塞守护线程；当新增任务时，激活守护线程，通知线程获取任务。
* 3.当无闲置线程时，阻塞守护线程；当存在闲置线程时，激活守护线程，通知闲置线程获取任务。
* 4.当销毁线程池时，等待守护线程退出。而守护线程在退出之前，等待所有线程退出。
*   线程在退出之前，默认执行任务管理器的所有任务。可选更换无效任务管理器或者清空任务，以支持线程立即退出。
* 5.提供增删线程策略，由守护线程增删线程。
*   当任务管理器非空时，一次性增加线程；当存在闲置线程时，逐个删减线程。
* 6.以原子操作确保接口的线程安全性，并且新增成员类Proxy，用于减少原子操作，针对频繁操作提升性能。
* 7.守护线程主函数声明为静态成员，除去与类成员指针this的关联性。
* 8.引入强化条件类模板Condition，当激活先于阻塞时，确保守护线程正常退出。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v3.0.1
* 变化
* v3.0.0
* 1.抽象任务管理器为模板隐式接口，以支持自定义任务管理器。
* 2.在任务管理器为空或者无效，并且所有线程闲置时，守护线程才可以退出，否则守护线程轮询等待这两个条件。
*   若任务管理器存在无效任务，则线程可能进行非预期性阻塞，导致在守护线程退出之前，线程无法执行任务管理器的所有任务。
* v3.0.1
* 1.修复移动赋值运算符函数的资源泄漏问题。
*/

#pragma once

#include <utility>
#include <chrono>
#include <memory>
#include <cstdint>
#include <list>
#include <atomic>
#include <thread>
#include <mutex>

#include "Core.hpp"
#include "Thread.hpp"
#include "TaskManager.h"

ETERFREE_SPACE_BEGIN

// 生成原子的设置函数
#define SET_ATOMIC(SizeType, Arithmetic, functor, field) \
SizeType functor(SizeType _size, Arithmetic _arithmetic) noexcept \
{ \
	constexpr auto MEMORY_ORDER = std::memory_order::relaxed; \
	switch (_arithmetic) \
	{ \
	case Arithmetic::REPLACE: \
		return field.exchange(_size, MEMORY_ORDER); \
	case Arithmetic::INCREASE: \
		return field.fetch_add(_size, MEMORY_ORDER); \
	case Arithmetic::DECREASE: \
		return field.fetch_sub(_size, MEMORY_ORDER); \
	default: \
		return field.load(MEMORY_ORDER); \
	} \
}

template <typename _TaskManager = TaskManager>
class ThreadPool
{
	// 算术枚举
	enum class Arithmetic : std::uint8_t
	{
		REPLACE,	// 替换
		INCREASE,	// 自增
		DECREASE	// 自减
	};

	// 线程池数据结构体
	struct Structure;

public:
	// 线程池代理类
	class Proxy;

private:
	using TaskType = _TaskManager::TaskType;
	using NotifyType = _TaskManager::NotifyType;

	using Thread = Thread<TaskType>;
	using Condition = Thread::Condition;

	using FetchType = Thread::FetchType;
	using ReplyType = Thread::ReplyType;

	using Duration = std::chrono::steady_clock::rep;

	using DataType = std::shared_ptr<Structure>;
	using AtomicType = std::atomic<DataType>;

public:
	using SizeType = Thread::SizeType;
	using TaskManager = std::shared_ptr<_TaskManager>;

private:
	AtomicType _atomic;

private:
	// 交换数据
	static auto exchange(AtomicType& _atomic, \
		const DataType& _data) noexcept
	{
		return _atomic.exchange(_data, \
			std::memory_order::relaxed);
	}

	// 创建线程池
	static void create(DataType&& _data, \
		SizeType _capacity);

	// 销毁线程池
	static void destroy(DataType&& _data);

	// 调整线程数量
	static SizeType adjust(DataType& _data);

	// 守护线程主函数
	static void execute(DataType _data);

public:
	// 获取支持的并发线程数量
	static SizeType getConcurrency() noexcept;

private:
	// 加载非原子数据
	auto load() const noexcept
	{
		return _atomic.load(std::memory_order::relaxed);
	}

public:
	/*
	 * 默认构造函数
	 * 若先以运算符new创建实例，再交由共享指针std::shared_ptr托管，
	 * 则至少二次分配内存，先为实例分配内存，再为共享指针的控制块分配内存。
	 * 而std::make_shared典型地仅分配一次内存，实例内存和控制块内存连续。
	 */
	ThreadPool(SizeType _capacity = getConcurrency()) : \
		_atomic(std::make_shared<Structure>())
	{
		create(load(), _capacity);
	}

	// 删除默认复制构造函数
	ThreadPool(const ThreadPool&) = delete;

	// 默认移动构造函数
	ThreadPool(ThreadPool&& _another) noexcept : \
		_atomic(exchange(_another._atomic, nullptr)) {}

	// 默认析构函数
	~ThreadPool()
	{
		// 数据非空才进行销毁，以支持移动语义
		if (auto data = exchange(_atomic, nullptr))
			destroy(std::move(data));
	}

	// 删除默认复制赋值运算符函数
	ThreadPool& operator=(const ThreadPool&) = delete;

	// 默认移动赋值运算符函数
	ThreadPool& operator=(ThreadPool&& _another);

	// 设置轮询间隔
	bool setDuration(Duration _duration) noexcept;

	// 获取线程池容量
	SizeType getCapacity() const noexcept;

	// 设置线程池容量
	bool setCapacity(SizeType _capacity);

	// 获取总线程数量
	SizeType getTotalSize() const noexcept;

	// 获取闲置线程数量
	SizeType getIdleSize() const noexcept;

	// 获取任务管理器
	TaskManager getTaskManager() const;

	// 设置任务管理器
	bool setTaskManager(const TaskManager& _taskManager);

	// 获取代理
	Proxy getProxy() const noexcept { return load(); }
};

// 线程池数据结构体
template <typename _TaskManager>
struct ThreadPool<_TaskManager>::Structure
{
	using TimePoint = std::chrono::steady_clock::time_point;

	std::atomic<Duration> _duration;		// 轮询间隔

	Condition _condition;					// 强化条件变量
	std::thread _thread;					// 守护线程
	std::list<Thread> _threadTable;			// 线程表

	std::atomic<SizeType> _capacity;		// 线程池容量
	std::atomic<SizeType> _totalSize;		// 总线程数量
	std::atomic<SizeType> _idleSize;		// 闲置线程数量

	mutable std::mutex _taskMutex;			// 任务互斥元
	TaskManager _taskManager;				// 任务管理器

	NotifyType _notify;						// 通知函数子
	FetchType _fetch;						// 获取函数子
	ReplyType _reply;						// 回复函数子

	// 获取时间戳
	static auto getTimePoint() noexcept
	{
		return std::chrono::steady_clock::now();
	}

	// 获取轮询间隔
	auto getDuration() const noexcept
	{
		return _duration.load(std::memory_order::relaxed);
	}

	// 设置轮询间隔
	void setDuration(Duration _duration) noexcept
	{
		this->_duration.store(_duration, \
			std::memory_order::relaxed);
	}

	// 等待轮询
	void waitPoll(TimePoint& _timePoint, Duration& _duration);

	// 获取线程池容量
	auto getCapacity() const noexcept
	{
		return _capacity.load(std::memory_order::relaxed);
	}

	// 设置线程池容量
	void setCapacity(SizeType _capacity, bool _notified = false);

	// 获取总线程数量
	auto getTotalSize() const noexcept
	{
		return _totalSize.load(std::memory_order::relaxed);
	}

	// 设置总线程数量
	SET_ATOMIC(SizeType, Arithmetic, setTotalSize, _totalSize);

	// 获取闲置线程数量
	auto getIdleSize() const noexcept
	{
		return _idleSize.load(std::memory_order::relaxed);
	}

	// 设置闲置线程数量
	SET_ATOMIC(SizeType, Arithmetic, setIdleSize, _idleSize);

	// 任务管理器是否为空
	bool isEmptyManager() const;

	// 任务管理器是否有效
	bool isValidManager() const;

	// 获取任务管理器
	auto getTaskManager() const
	{
		std::lock_guard lock(_taskMutex);
		return _taskManager;
	}

	// 设置任务管理器
	void setTaskManager(const TaskManager& _taskManager);
};

#undef SET_ATOMIC

template <typename _TaskManager>
class ThreadPool<_TaskManager>::Proxy
{
	DataType _data;

public:
	Proxy(const decltype(_data)& _data) noexcept : \
		_data(_data) {}

	explicit operator bool() const noexcept { return valid(); }

	// 是否有效
	bool valid() const noexcept
	{
		return static_cast<bool>(_data);
	}

	// 设置轮询间隔
	bool setDuration(Duration _duration) noexcept;

	// 获取线程池容量
	SizeType getCapacity() const noexcept;

	// 设置线程池容量
	bool setCapacity(SizeType _capacity);

	// 获取总线程数量
	SizeType getTotalSize() const noexcept;

	// 获取闲置线程数量
	SizeType getIdleSize() const noexcept;

	// 获取任务管理器
	TaskManager getTaskManager() const
	{
		return _data ? \
			_data->getTaskManager() : nullptr;
	}

	// 设置任务管理器
	bool setTaskManager(const TaskManager& _taskManager);
};

// 等待轮询
template <typename _TaskManager>
void ThreadPool<_TaskManager>::Structure::waitPoll(TimePoint& _timePoint, \
	Duration& _duration)
{
	auto duration = getDuration();
	if (duration <= 0)
	{
		_timePoint = Structure::getTimePoint();
		return;
	}

	_duration %= duration;
	auto difference = (duration - _duration) % duration;

	auto timePoint = Structure::getTimePoint();
	auto realTime = (timePoint - _timePoint).count();
	_timePoint = timePoint;

	realTime %= duration;
	if (realTime >= difference) difference += duration;

	auto sleepTime = difference - realTime;
	std::this_thread::sleep_for(TimePoint::duration(sleepTime));

	timePoint = Structure::getTimePoint();
	realTime = (timePoint - _timePoint).count();
	_timePoint = timePoint;

	_duration = (realTime - sleepTime) % duration;
}

// 设置线程池容量
template <typename _TaskManager>
void ThreadPool<_TaskManager>::Structure::setCapacity(SizeType _capacity, \
	bool _notified)
{
	auto capacity = this->_capacity.exchange(_capacity, \
		std::memory_order::relaxed);
	if (_notified and capacity != _capacity)
		_condition.notify_one(Condition::Strategy::RELAXED);
}

// 任务管理器是否为空
template <typename _TaskManager>
bool ThreadPool<_TaskManager>::Structure::isEmptyManager() const
{
	std::lock_guard lock(_taskMutex);
	return not _taskManager or _taskManager->empty();
}

// 任务管理器是否有效
template <typename _TaskManager>
bool ThreadPool<_TaskManager>::Structure::isValidManager() const
{
	std::lock_guard lock(_taskMutex);
	return _taskManager and _taskManager->size() > 0;
}

// 设置任务管理器
template <typename _TaskManager>
void ThreadPool<_TaskManager>::Structure::setTaskManager(const TaskManager& _taskManager)
{
	std::unique_lock lock(_taskMutex);
	if (this->_taskManager) this->_taskManager->configure(nullptr);
	this->_taskManager = _taskManager;
	if (_taskManager) _taskManager->configure(_notify);
	lock.unlock();

	_condition.notify_one([&_taskManager]
		{ return _taskManager and not _taskManager->empty(); });
}

// 设置轮询间隔
template <typename _TaskManager>
bool ThreadPool<_TaskManager>::Proxy::setDuration(Duration _duration) noexcept
{
	if (_duration < 0 or not _data) return false;

	_data->setDuration(_duration);
	return true;
}

// 获取线程池容量
template <typename _TaskManager>
auto ThreadPool<_TaskManager>::Proxy::getCapacity() const noexcept \
-> SizeType
{
	return _data ? _data->getCapacity() : 0;
}

// 设置线程池容量
template <typename _TaskManager>
bool ThreadPool<_TaskManager>::Proxy::setCapacity(SizeType _capacity)
{
	if (_capacity <= 0 or not _data) return false;

	_data->setCapacity(_capacity, true);
	return true;
}

// 获取总线程数量
template <typename _TaskManager>
auto ThreadPool<_TaskManager>::Proxy::getTotalSize() const noexcept \
-> SizeType
{
	return _data ? _data->getTotalSize() : 0;
}

// 获取闲置线程数量
template <typename _TaskManager>
auto ThreadPool<_TaskManager>::Proxy::getIdleSize() const noexcept \
-> SizeType
{
	return _data ? _data->getIdleSize() : 0;
}

// 设置任务管理器
template <typename _TaskManager>
bool ThreadPool<_TaskManager>::Proxy::setTaskManager(const TaskManager& _taskManager)
{
	if (not _data) return false;

	_data->setTaskManager(_taskManager);
	return true;
}

// 创建线程池
template <typename _TaskManager>
void ThreadPool<_TaskManager>::create(DataType&& _data, SizeType _capacity)
{
	// 设置轮询间隔
	_data->setDuration(std::chrono::nanoseconds(1000000).count());

	// 定义通知函数子
	_data->_notify = [_data = std::weak_ptr(_data)]
	{
		if (auto data = _data.lock())
			data->_condition.notify_one(Condition::Strategy::RELAXED);
	};

	// 定义获取函数子
	_data->_fetch = [_data = std::weak_ptr(_data)](TaskType& _task)
	{
		auto data = _data.lock();
		if (not data) return false;

		std::unique_lock lock(data->_taskMutex);
		auto taskManager = data->_taskManager;
		lock.unlock();

		return taskManager and taskManager->take(_task);
	};

	// 定义回复函数子
	_data->_reply = [_data = std::weak_ptr(_data)](Thread::ThreadID _id, bool _idle)
	{
		// 线程并非闲置状态
		if (not _idle) return;

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
		thread.configure(_data->_fetch, _data->_reply);
		_data->_threadTable.push_back(std::move(thread));
	}

	_data->setCapacity(_capacity); // 设置线程池容量
	_data->setTotalSize(_capacity, Arithmetic::REPLACE); // 设置总线程数量
	_data->setIdleSize(_capacity, Arithmetic::REPLACE); // 设置闲置线程数量

	// 创建std::thread对象，即守护线程，以_data为参数，执行函数execute
	_data->_thread = std::thread(execute, _data);
}

// 销毁线程池
template <typename _TaskManager>
void ThreadPool<_TaskManager>::destroy(DataType&& _data)
{
	// 避免重复销毁
	if (not _data->_condition) return;

	// 分离守护线程
	//_data->_thread.detach();

	// 通知守护线程退出
	_data->_condition.exit();

	// 挂起直到守护线程退出
	if (_data->_thread.joinable())
		_data->_thread.join();

	_data->setCapacity(0); // 设置线程池容量
	_data->setTotalSize(0, Arithmetic::REPLACE); // 设置总线程数量
	_data->setIdleSize(0, Arithmetic::REPLACE); // 设置闲置线程数量
}

// 调整线程数量
template <typename _TaskManager>
auto ThreadPool<_TaskManager>::adjust(DataType& _data) \
-> SizeType
{
	auto size = _data->getTotalSize();
	auto capacity = _data->getCapacity();

	// 1.删减线程
	if (size >= capacity) return size - capacity;

	// 2.增加线程
	size = capacity - size;

	// 添加线程至线程表
	for (decltype(size) index = 0; index < size; ++index)
	{
		Thread thread;
		thread.configure(_data->_fetch, _data->_reply);
		_data->_threadTable.push_back(std::move(thread));
	}

	// 增加总线程数量
	_data->setTotalSize(size, Arithmetic::INCREASE);

	// 增加闲置线程数量
	_data->setIdleSize(size, Arithmetic::INCREASE);
	return 0;
}

// 守护线程主函数
template <typename _TaskManager>
void ThreadPool<_TaskManager>::execute(DataType _data)
{
	auto timeStamp = Structure::getTimePoint(); // 时间戳
	Duration correction = 0; // 修正值

	/*
	 * 条件变量的谓词，不必等待通知的条件
	 * 1.强化条件变量无效。
	 * 2.任务管理器非空并且存在闲置线程。
	 * 3.任务管理器非空并且需要增加线程。
	 * 4.存在闲置线程并且需要删减线程。
	 */
	auto predicate = [&_data]
	{
		bool empty = _data->isEmptyManager();
		bool idle = _data->getIdleSize() > 0;
		auto size = _data->getTotalSize();
		auto capacity = _data->getCapacity();
		return not empty and (idle or size < capacity) \
			or idle and size > capacity;
	};

	// 若谓词非真，自动解锁互斥元，阻塞守护线程，直至通知激活，再次锁定互斥元
	_data->_condition.wait(predicate);

	/*
	 * 守护线程退出条件
	 * 1.强化条件变量无效
	 * 2.任务管理器无效
	 * 3.所有线程闲置
	 */
	while (_data->_condition or _data->isValidManager() \
		or _data->getIdleSize() < _data->getTotalSize())
	{
		// 调整线程数量
		auto size = adjust(_data);

		// 遍历线程表，访问闲置线程
		for (auto iterator = _data->_threadTable.begin(); \
			iterator != _data->_threadTable.end() \
			and _data->getIdleSize() > 0;)
		{
			// 若线程处于闲置状态
			if (auto& thread = *iterator; thread.idle())
			{
				// 若通知线程执行任务成功，则减少闲置线程数量
				if (thread.notify())
					_data->setIdleSize(1, Arithmetic::DECREASE);
				// 删减线程
				else if (size > 0)
				{
					iterator = _data->_threadTable.erase(iterator);
					_data->setIdleSize(1, Arithmetic::DECREASE);
					_data->setTotalSize(1, Arithmetic::DECREASE);
					--size;
					continue;
				}
			}
			++iterator;
		}

		// 根据谓词真假，决定是否阻塞守护线程
		if (_data->_condition)
			_data->_condition.wait(predicate);
		// 在守护线程退出之前，等待其他线程完成任务
		else
			// 由于强化条件变量无效，因此采用时间片轮询方式
			_data->waitPoll(timeStamp, correction);
	}

	// 清空线程表
	_data->_threadTable.clear();
}

// 获取支持的并发线程数量
template <typename _TaskManager>
auto ThreadPool<_TaskManager>::getConcurrency() noexcept -> SizeType
{
	auto concurrency = std::thread::hardware_concurrency();
	return concurrency > 0 ? concurrency : 1;
}

// 默认移动赋值运算符函数
template <typename _TaskManager>
auto ThreadPool<_TaskManager>::operator=(ThreadPool&& _another) \
-> ThreadPool&
{
	if (&_another != this)
	{
		auto data = exchange(_another._atomic, nullptr);
		if (data = exchange(this->_atomic, data))
			destroy(std::move(data));
	}
	return *this;
}

// 设置轮询间隔
template <typename _TaskManager>
bool ThreadPool<_TaskManager>::setDuration(Duration _duration) noexcept
{
	if (_duration >= 0)
		if (auto data = load())
		{
			data->setDuration(_duration);
			return true;
		}
	return false;
}

// 获取线程池容量
template <typename _TaskManager>
auto ThreadPool<_TaskManager>::getCapacity() const noexcept \
-> SizeType
{
	auto data = load();
	return data ? data->getCapacity() : 0;
}

// 设置线程池容量
template <typename _TaskManager>
bool ThreadPool<_TaskManager>::setCapacity(SizeType _capacity)
{
	if (_capacity > 0)
		if (auto data = load())
		{
			data->setCapacity(_capacity, true);
			return true;
		}
	return false;
}

// 获取总线程数量
template <typename _TaskManager>
auto ThreadPool<_TaskManager>::getTotalSize() const noexcept \
-> SizeType
{
	auto data = load();
	return data ? data->getTotalSize() : 0;
}

// 获取闲置线程数量
template <typename _TaskManager>
auto ThreadPool<_TaskManager>::getIdleSize() const noexcept \
-> SizeType
{
	auto data = load();
	return data ? data->getIdleSize() : 0;
}

// 获取任务管理器
template <typename _TaskManager>
auto ThreadPool<_TaskManager>::getTaskManager() const \
-> TaskManager
{
	auto data = load();
	return data ? data->getTaskManager() : nullptr;
}

// 设置任务管理器
template <typename _TaskManager>
bool ThreadPool<_TaskManager>::setTaskManager(const TaskManager& _taskManager)
{
	bool result = false;
	if (auto data = load())
	{
		data->setTaskManager(_taskManager);
		result = true;
	}
	return result;
}

ETERFREE_SPACE_END
