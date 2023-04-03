﻿/*
* 文件名称：TaskPool.hpp
* 语言标准：C++20
*
* 创建日期：2023年02月04日
*
* 摘要
* 1.定义任务池类模板TaskPool，继承任务管理器抽象类，确保接口的线程安全性。
* 2.任务池由处理者、事件、排序者三元素组成，支持自定义处理者和事件类型。
* 3.任务池支持放入不同处理者，分别对应不同事件队列。
*   处理者默认并发处理事件，可指定并行处理事件。
* 4.任务池支持放入无效函数子，以设置空处理者，但不会清空对应事件队列。
*   空处理者的非空事件队列会阻止线程池的守护线程退出，可以主动调用清空事件函数。
* 5.引入排序者类模板Sorter，按照时间先后对处理者索引排序，以确定任务顺序。
*
* 作者：许聪
* 邮箱：solifree@qq.com
*
* 版本：v1.0.0
*/

#pragma once

#include <type_traits>
#include <utility>
#include <chrono>
#include <optional>
#include <memory>
#include <list>
#include <map>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <shared_mutex>

#include "TaskManager.h"
#include "Sorter.hpp"

ETERFREE_SPACE_BEGIN

/*
继承类模板enable_shared_from_this，当TaskPool被shared_ptr托管，而需要传递this给其它函数之时，
需要传递指向this的shared_ptr，调用this->shared_from_this获取指向this的shared_ptr。
不可直接传递裸指针this，否则无法确保shared_ptr的语义，也许会导致已被释放的错误。
不可单独创建另一shared_ptr，否则多个shared_ptr的控制块不同，导致释放多次同一对象。
*/
template <typename _EventType>
class TaskPool : public TaskManager, \
	public std::enable_shared_from_this<TaskPool<_EventType>>
{
	struct Queue;
	struct Record;

public:
	struct Handler;

public:
	using EventType = _EventType;
	using EventQueue = std::list<EventType>;

	using IndexType = SizeType;
	using HandleType = std::function<void(EventType&)>;

private:
	using AtomicType = std::atomic<SizeType>;
	using TimeType = std::chrono::steady_clock::time_point;

	using MutexMapper = std::map<IndexType, std::shared_ptr<std::mutex>>;
	using HandleMapper = std::unordered_map<IndexType, std::shared_ptr<Handler>>;
	using QueueMapper = std::unordered_map<IndexType, std::shared_ptr<Queue>>;

private:
	mutable std::mutex _notifyMutex; // 通知互斥元
	NotifyType _notify; // 通知函数子

	std::shared_mutex _sharedMutex; // 并行互斥元
	std::mutex _mutex; // 互斥元
	MutexMapper _mutexMapper; // 互斥映射

	std::mutex _handleMutex; // 处理互斥元
	HandleMapper _handleMapper; // 处理映射

	std::mutex _queueMutex; // 队列互斥元
	QueueMapper _queueMapper; // 队列映射

	AtomicType _size; // 任务数量

	mutable std::mutex _sortMutex; // 排序互斥元
	Sorter<IndexType, Record> _sorter; // 排序者

private:
	// 获取时间
	static auto getTime() noexcept
	{
		return std::chrono::steady_clock::now();
	}

	// 执行任务
	static void execute(const std::weak_ptr<TaskPool>& _taskPool, \
		IndexType _index, const HandleType& _handle, EventType& _event);

private:
	// 获取原子
	static auto get(const AtomicType& _atomic) noexcept
	{
		return _atomic.load(std::memory_order::relaxed);
	}

	// 设置原子
	static void set(AtomicType& _atomic, SizeType _size) noexcept
	{
		_atomic.store(_size, std::memory_order::relaxed);
	}

	// 原子加法
	static auto add(AtomicType& _atomic, SizeType _size) noexcept
	{
		return _atomic.fetch_add(_size, std::memory_order::relaxed);
	}

	// 原子减法
	static auto subtract(AtomicType& _atomic, SizeType _size) noexcept
	{
		return _atomic.fetch_sub(_size, std::memory_order::relaxed);
	}

public:
	// 配置通知函数子
	virtual void configure(const NotifyType& _notify) override
	{
		std::lock_guard lock(_notifyMutex);
		this->_notify = _notify;
	}
	virtual void configure(NotifyType&& _notify) override
	{
		std::lock_guard lock(_notifyMutex);
		this->_notify = std::forward<NotifyType>(_notify);
	}

	// 是否无任务
	virtual bool empty() const override
	{
		std::lock_guard lock(_sortMutex);
		return _sorter.empty();
	}

	// 总任务数量
	virtual SizeType size() const noexcept override
	{
		return get(_size);
	}

	// 取出单任务
	virtual bool take(TaskType& _task) override;

private:
	// 获取索引互斥元
	std::shared_ptr<std::mutex> getMutex(IndexType _index);

	// 查找处理者
	std::shared_ptr<Handler> findHandler(IndexType _index);

	// 设置处理者
	void setHandler(IndexType _index, std::shared_ptr<Handler>&& _handler);

	// 查找事件队列
	std::shared_ptr<Queue> findQueue(IndexType _index);

	// 获取事件队列
	std::shared_ptr<Queue> getQueue(IndexType _index);

	// 向调度队列放入索引
	bool pushIndex(IndexType _index);

private:
	// 执行通知函数子
	void notify() const;

	// 回复任务
	void reply(IndexType _index);

	// 放入处理者
	bool push(IndexType _index, const HandleType& _handle, bool _parallel);
	bool push(IndexType _index, HandleType&& _handle, bool _parallel);

	// 向调度队列放入索引
	bool push(IndexType _index);

	// 从调度队列取出索引
	std::optional<IndexType> pop();

public:
	TaskPool() : _size(0) {}

	TaskPool(const TaskPool&) = delete;

	TaskPool& operator=(const TaskPool&) = delete;

	virtual ~TaskPool() = default;

	// 任务数量
	SizeType size(IndexType _index) const
	{
		auto queue = findQueue(_index);
		return queue ? queue->size() : 0;
	}

	// 设置处理者
	bool set(IndexType _index, \
		const HandleType& _handle, bool _parallel = false)
	{
		return push(_index, _handle, _parallel);
	}
	bool set(IndexType _index, HandleType&& _handle, bool _parallel = false)
	{
		return push(_index, std::forward<HandleType>(_handle), _parallel);
	}

	// 适配不同处理接口，推进任务池模板化
	template <typename _Functor>
	bool set(IndexType _index, \
		const _Functor& _functor, bool _parallel = false)
	{
		return push(_index, _functor, _parallel);
	}
	template <typename _Functor>
	bool set(IndexType _index, _Functor&& _functor, bool _parallel = false)
	{
		return push(_index, std::forward<_Functor>(_functor), _parallel);
	}

	// 放入单事件
	bool put(IndexType _index, const EventType& _event);
	bool put(IndexType _index, EventType&& _event);

	// 批量放入事件
	bool put(IndexType _index, EventQueue& _queue);
	bool put(IndexType _index, EventQueue&& _queue);

	// 清空事件
	void clear(IndexType _index);

	// 清空所有事件
	void clear();
};

template <typename _EventType>
struct TaskPool<_EventType>::Queue
{
	mutable std::mutex _mutex;
	EventQueue _eventQueue;
	std::list<TimeType> _timeQueue;

	// 是否为空
	bool empty() const
	{
		std::lock_guard lock(_mutex);
		return _eventQueue.empty();
	}

	// 获取事件数量
	auto size() const
	{
		std::lock_guard lock(_mutex);
		return _eventQueue.size();
	}

	// 获取最小时间
	std::optional<TimeType> time() const;

	// 放入单事件
	std::optional<SizeType> push(const EventType& _event);
	std::optional<SizeType> push(EventType&& _event);

	// 批量放入事件
	std::optional<SizeType> push(EventQueue& _queue);
	std::optional<SizeType> push(EventQueue&& _queue);

	// 取出单事件
	bool pop(EventType& _event);

	// 清空所有事件
	auto clear();
};

template <typename _EventType>
struct TaskPool<_EventType>::Record
{
	IndexType _index;
	TimeType _time;

	explicit operator IndexType() const noexcept
	{
		return _index;
	}

	bool operator<(const Record& _another) const noexcept;
};

template <typename _EventType>
struct TaskPool<_EventType>::Handler
{
	HandleType _handle;
	bool _parallel;
	bool _idle;

	Handler() noexcept : _parallel(false), _idle(true) {}
	Handler(const HandleType& _handle, bool _parallel) : \
		_handle(_handle), _parallel(_parallel), _idle(true) {}
	Handler(HandleType&& _handle, bool _parallel) noexcept : \
		_handle(std::forward<HandleType>(_handle)), \
		_parallel(_parallel), _idle(true) {}

	// 赋值替换
	void assign(const HandleType& _handle, bool _parallel)
	{
		this->_handle = _handle;
		this->_parallel = _parallel;
	}
	void assign(HandleType&& _handle, bool _parallel) noexcept
	{
		this->_handle = std::forward<HandleType>(_handle);
		this->_parallel = _parallel;
	}

	// 是否并行
	bool parallel() const noexcept
	{
		return _parallel;
	}

	// 是否闲置
	bool idle() const noexcept
	{
		return _idle;
	}

	// 设置是否闲置
	bool idle(bool _idle) noexcept;
};

// 获取最小时间
template <typename _EventType>
auto TaskPool<_EventType>::Queue::time() const \
-> std::optional<TimeType>
{
	std::lock_guard lock(_mutex);
	if (_timeQueue.empty()) return std::nullopt;
	return _timeQueue.front();
}

// 放入单事件
template <typename _EventType>
auto TaskPool<_EventType>::Queue::push(const EventType& _event) \
-> std::optional<SizeType>
{
	std::lock_guard lock(_mutex);
	auto size = _eventQueue.size();
	_eventQueue.push_back(_event);
	_timeQueue.push_back(getTime());
	return size;
}

// 放入单事件
template <typename _EventType>
auto TaskPool<_EventType>::Queue::push(EventType&& _event) \
-> std::optional<SizeType>
{
	std::lock_guard lock(_mutex);
	auto size = _eventQueue.size();
	_eventQueue.push_back(std::forward<EventType>(_event));
	_timeQueue.push_back(getTime());
	return size;
}

// 批量放入事件
template <typename _EventType>
auto TaskPool<_EventType>::Queue::push(EventQueue& _queue) \
-> std::optional<SizeType>
{
	std::lock_guard lock(_mutex);
	auto size = _eventQueue.size();
	_eventQueue.splice(_eventQueue.cend(), _queue);
	_timeQueue.resize(_eventQueue.size(), getTime());
	return size;
}

// 批量放入事件
template <typename _EventType>
auto TaskPool<_EventType>::Queue::push(EventQueue&& _queue) \
-> std::optional<SizeType>
{
	std::lock_guard lock(_mutex);
	auto size = _eventQueue.size();
	_eventQueue.splice(_eventQueue.cend(), \
		std::forward<EventQueue>(_queue));
	_timeQueue.resize(_eventQueue.size(), getTime());
	return size;
}

// 取出单事件
template <typename _EventType>
bool TaskPool<_EventType>::Queue::pop(EventType& _event)
{
	std::lock_guard lock(_mutex);
	if (_eventQueue.empty()) return false;

	_event = std::move(_eventQueue.front());
	_eventQueue.pop_front();
	_timeQueue.pop_front();
	return true;
}

// 清空所有事件
template <typename _EventType>
auto TaskPool<_EventType>::Queue::clear()
{
	std::lock_guard lock(_mutex);
	auto size = _eventQueue.size();
	_eventQueue.clear();
	_timeQueue.clear();
	return size;
}

template <typename _EventType>
bool TaskPool<_EventType>::Record::operator<(const Record& _another) const noexcept
{
	return this->_time < _another._time \
		or this->_time == _another._time and this->_index < _another._index;
}

// 设置是否闲置
template <typename _EventType>
bool TaskPool<_EventType>::Handler::idle(bool _idle) noexcept
{
	auto idle = this->_idle;
	this->_idle = _idle;
	return idle;
}

// 执行任务
template <typename _EventType>
void TaskPool<_EventType>::execute(const std::weak_ptr<TaskPool>& _taskPool, \
	IndexType _index, const HandleType& _handle, EventType& _event)
{
	if (_handle) _handle(_event);
	if (auto taskPool = _taskPool.lock())
		taskPool->reply(_index);
}

// 取出单任务
template <typename _EventType>
bool TaskPool<_EventType>::take(TaskType& _task)
{
	std::lock_guard lock(_sharedMutex);

	auto result = pop();
	if (not result) return false;

	auto index = result.value();
	auto handler = findHandler(index);
	if (not handler) return false;

	auto queue = getQueue(index);
	if (not queue) return false;

	EventType event;
	if (not queue->pop(event)) return false;

	subtract(_size, 1);
	_task = std::bind(execute, \
		std::weak_ptr(this->shared_from_this()), \
		index, handler->_handle, std::move(event));
	return true;
}

// 获取索引互斥元
template <typename _EventType>
std::shared_ptr<std::mutex> TaskPool<_EventType>::getMutex(IndexType _index)
{
	std::lock_guard lock(_mutex);
	auto iterator = _mutexMapper.find(_index);
	if (iterator == _mutexMapper.end())
	{
		auto mutex = std::make_shared<std::mutex>();
		auto pair = _mutexMapper.emplace(_index, mutex);
		iterator = pair.first;
	}
	return iterator != _mutexMapper.end() ? iterator->second : nullptr;
}

// 查找处理者
template <typename _EventType>
auto TaskPool<_EventType>::findHandler(IndexType _index) \
-> std::shared_ptr<Handler>
{
	std::lock_guard lock(_handleMutex);
	auto iterator = _handleMapper.find(_index);
	return iterator != _handleMapper.end() ? \
		iterator->second : nullptr;
}

// 设置处理者
template <typename _EventType>
void TaskPool<_EventType>::setHandler(IndexType _index, \
	std::shared_ptr<Handler>&& _handler)
{
	std::lock_guard lock(_handleMutex);
	using Handler = std::remove_reference_t<decltype(_handler)>;
	_handleMapper.insert_or_assign(_index, \
		std::forward<Handler>(_handler));
}

// 查找事件队列
template <typename _EventType>
auto TaskPool<_EventType>::findQueue(IndexType _index) \
-> std::shared_ptr<Queue>
{
	std::lock_guard lock(_queueMutex);
	auto iterator = _queueMapper.find(_index);
	return iterator != _queueMapper.end() ? \
		iterator->second : nullptr;
}

// 获取事件队列
template <typename _EventType>
auto TaskPool<_EventType>::getQueue(IndexType _index) \
-> std::shared_ptr<Queue>
{
	std::lock_guard lock(_queueMutex);
	auto iterator = _queueMapper.find(_index);
	if (iterator == _queueMapper.end())
	{
		auto queue = std::make_shared<Queue>();
		auto pair = _queueMapper.emplace(_index, queue);
		iterator = pair.first;
	}
	return iterator != _queueMapper.end() ? \
		iterator->second : nullptr;
}

// 向调度队列放入索引
template <typename _EventType>
bool TaskPool<_EventType>::pushIndex(IndexType _index)
{
	auto queue = getQueue(_index);
	if (not queue) return false;

	auto time = queue->time();
	if (not time) return false;

	std::lock_guard lock(_sortMutex);
	if (_sorter.exist(_index)) return false;

	_sorter.update({ _index, time.value() });
	return true;
}

// 执行通知函数子
template <typename _EventType>
void TaskPool<_EventType>::notify() const
{
	std::unique_lock lock(_notifyMutex);
	auto notify = _notify;
	lock.unlock();

	if (notify) notify();
}

// 回复任务
template <typename _EventType>
void TaskPool<_EventType>::reply(IndexType _index)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return;

	std::lock_guard indexLock(*mutex);

	if (auto handler = findHandler(_index))
		if (not handler->idle(true))
			pushIndex(_index);
}

// 放入处理者
template <typename _EventType>
bool TaskPool<_EventType>::push(IndexType _index, \
	const HandleType& _handle, bool _parallel)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return false;

	std::lock_guard indexLock(*mutex);

	auto handler = findHandler(_index);
	if (handler)
	{
		if (not _handle)
		{
			std::lock_guard lock(_sortMutex);
			_sorter.remove(_index);
		}

		handler->assign(_handle, _parallel);
		return true;
	}

	if (not _handle) return true;

	handler = std::make_shared<Handler>(_handle, _parallel);
	if (not handler) return false;

	setHandler(_index, std::move(handler));
	pushIndex(_index);
	return true;
}

// 放入处理者
template <typename _EventType>
bool TaskPool<_EventType>::push(IndexType _index, HandleType&& _handle, bool _parallel)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return false;

	std::lock_guard indexLock(*mutex);

	auto handler = findHandler(_index);
	if (handler)
	{
		if (not _handle)
		{
			std::lock_guard lock(_sortMutex);
			_sorter.remove(_index);
		}

		handler->assign(std::forward<HandleType>(_handle), _parallel);
		return true;
	}

	if (not _handle) return true;

	handler = std::make_shared<Handler>(std::forward<HandleType>(_handle), _parallel);
	if (not handler) return false;

	setHandler(_index, std::move(handler));
	pushIndex(_index);
	return true;
}

// 向调度队列放入索引
template <typename _EventType>
bool TaskPool<_EventType>::push(IndexType _index)
{
	auto handler = findHandler(_index);
	return handler and handler->idle() ? \
		pushIndex(_index) : false;
}

// 从调度队列取出索引
template <typename _EventType>
auto TaskPool<_EventType>::pop() -> std::optional<IndexType>
{
	std::lock_guard lock(_sortMutex);
	if (auto record = _sorter.front(true); record != nullptr)
	{
		do
		{
			auto index = record->_index;
			if (auto handler = findHandler(index))
			{
				if (handler->parallel())
				{
					if (auto queue = getQueue(index))
						if (auto time = queue->time())
							_sorter.update({ index, time.value() });
				}
				else
				{
					handler->idle(false);
					_sorter.remove(index);
				}
				return index;
			}

			_sorter.remove(index);
			record = _sorter.front(true);
		} while (record != nullptr);
	}
	return std::nullopt;
}

// 放入单事件
template <typename _EventType>
bool TaskPool<_EventType>::put(IndexType _index, \
	const EventType& _event)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return false;

	std::lock_guard indexLock(*mutex);

	auto queue = getQueue(_index);
	if (not queue) return false;

	auto result = queue->push(_event);
	if (result)
	{
		add(_size, 1);

		if (result.value() == 0 and push(_index))
			notify();
	}
	return result.has_value();
}

// 放入单事件
template <typename _EventType>
bool TaskPool<_EventType>::put(IndexType _index, EventType&& _event)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return false;

	std::lock_guard indexLock(*mutex);

	auto queue = getQueue(_index);
	if (not queue) return false;

	auto result = queue->push(std::forward<EventType>(_event));
	if (result)
	{
		add(_size, 1);

		if (result.value() == 0 and push(_index)) notify();
	}
	return result.has_value();
}

// 批量放入事件
template <typename _EventType>
bool TaskPool<_EventType>::put(IndexType _index, \
	EventQueue& _queue)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return false;

	std::lock_guard indexLock(*mutex);

	auto queue = getQueue(_index);
	if (not queue) return false;

	auto size = _queue.size();
	auto result = queue->push(_queue);
	if (result)
	{
		add(_size, size);

		if (result.value() == 0 and push(_index))
			notify();
	}
	return result.has_value();
}

// 批量放入事件
template <typename _EventType>
bool TaskPool<_EventType>::put(IndexType _index, EventQueue&& _queue)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return false;

	std::lock_guard indexLock(*mutex);

	auto queue = getQueue(_index);
	if (not queue) return false;

	auto size = _queue.size();
	auto result = queue->push(std::forward<EventQueue>(_queue));
	if (result)
	{
		add(_size, size);

		if (result.value() == 0 and push(_index)) notify();
	}
	return result.has_value();
}

// 清空事件
template <typename _EventType>
void TaskPool<_EventType>::clear(IndexType _index)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return;

	std::lock_guard indexLock(*mutex);

	auto queue = findQueue(_index);
	if (not queue) return;

	auto size = queue->clear();
	if (size != 0) subtract(_size, size);

	std::lock_guard sortLock(_sortMutex);
	_sorter.remove(_index);
}

// 清空所有事件
template <typename _EventType>
void TaskPool<_EventType>::clear()
{
	std::lock_guard sharedLock(_sharedMutex);

	std::unique_lock sortLock(_sortMutex);
	_sorter.clear();
	sortLock.unlock();

	set(_size, 0);

	std::lock_guard queueLock(_queueMutex);
	for (auto& [_, queue] : _queueMapper)
		queue->clear();
}

ETERFREE_SPACE_END