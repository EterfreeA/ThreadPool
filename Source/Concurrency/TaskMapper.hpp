/*
* 文件名称：TaskMapper.hpp
* 语言标准：C++20
*
* 创建日期：2023年02月04日
* 更新日期：2023年10月14日
*
* 摘要
* 1.定义任务映射器类模板TaskMapper，继承任务池抽象类，确保接口的线程安全性。
* 2.任务映射器由处理者、消息、排序者三元素组成，支持自定义处理者和消息类型。
* 3.任务映射器可以设置不同处理者，分别对应不同消息队列。
*   处理者默认并发处理消息，可选并行处理消息。
* 4.处理者可以反复更换，支持以无效函数子作为空处理者，但不会清空对应消息队列。
*   空处理者与非空消息队列会阻止线程池的守护线程退出，可以主动清空非空消息队列，确保线程池的守护线程正常退出。
* 5.引入排序者类模板Sorter，按照时间先后对处理者索引排序，以确定任务顺序。
*
* 作者：许聪
* 邮箱：solifree@qq.com
*
* 版本：v2.0.0
* 变化
* v1.1.0
* 1.在设置处理者之后，判断是否通知线程执行任务。
*   当先放入消息后设置处理者时，或者更换空处理者为非空处理者，避免未及时激活线程。
* v2.0.0
* 1.重命名任务池TaskPool为任务映射器TaskMapper。
* 2.修复并行处理消息未更新索引排序导致线程泄漏。
*/

#pragma once

#include <optional>
#include <type_traits>
#include <utility>
#include <memory>
#include <exception>
#include <list>
#include <map>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <shared_mutex>

#include "Core/Logger.h"
#include "Core/Timer.h"
#include "Sequence/Sorter.hpp"
#include "TaskPool.h"

CONCURRENCY_SPACE_BEGIN

/*
继承类模板enable_shared_from_this，当TaskMapper被shared_ptr托管，传递this给其它函数时，
需要传递指向this的shared_ptr，调用this->shared_from_this获取指向this的shared_ptr。
不可直接传递裸指针this，否则无法确保shared_ptr的语义，也许会导致已被释放的错误。
不可单独创建另一shared_ptr，否则多个shared_ptr的控制块不同，导致多次释放同一对象。
*/
template <typename _Message>
class TaskMapper : public TaskPool, \
	public std::enable_shared_from_this<TaskMapper<_Message>>
{
	struct Queue;
	struct Record;

public:
	struct Handler;

public:
	using QueueType = Queue;

	using Message = _Message;
	using MessageQueue = std::list<Message>;

	using Handle = std::function<void(Message&)>;

private:
	using Atomic = std::atomic<SizeType>;

	using MutexMapper = std::map<IndexType, std::shared_ptr<std::mutex>>;
	using HandleMapper = std::unordered_map<IndexType, std::shared_ptr<Handler>>;
	using QueueMapper = std::unordered_map<IndexType, std::shared_ptr<QueueType>>;

	using Sorter = Sequence::Sorter<IndexType, Record>;

private:
	mutable std::mutex _notifyMutex; // 通知互斥元
	Notify _notify; // 通知函数子

	std::shared_mutex _sharedMutex; // 并行互斥元
	std::mutex _mutex; // 互斥元
	MutexMapper _mutexMapper; // 互斥映射

	mutable std::mutex _handleMutex; // 处理互斥元
	HandleMapper _handleMapper; // 处理映射

	mutable std::mutex _queueMutex; // 队列互斥元
	QueueMapper _queueMapper; // 队列映射

	IndexType _index; // 唯一索引
	Atomic _size; // 任务数量

	mutable std::mutex _sortMutex; // 排序互斥元
	Sorter _sorter; // 排序者

private:
	// 获取原子
	static auto get(const Atomic& _atomic) noexcept
	{
		return _atomic.load(std::memory_order::relaxed);
	}

	// 设置原子
	static void set(Atomic& _atomic, SizeType _size) noexcept
	{
		_atomic.store(_size, std::memory_order::relaxed);
	}

	// 原子加法
	static auto add(Atomic& _atomic, SizeType _size) noexcept
	{
		return _atomic.fetch_add(_size, std::memory_order::relaxed);
	}

	// 原子减法
	static auto subtract(Atomic& _atomic, SizeType _size) noexcept
	{
		return _atomic.fetch_sub(_size, std::memory_order::relaxed);
	}

	// 执行任务
	static void execute(const std::weak_ptr<TaskMapper>& _taskMapper, \
		IndexType _index, const Handle& _handle, Message& _message);

public:
	// 配置通知函数子
	virtual void configure(const Notify& _notify) override
	{
		std::lock_guard lock(_notifyMutex);
		this->_notify = _notify;
	}
	virtual void configure(Notify&& _notify) override
	{
		std::lock_guard lock(_notifyMutex);
		this->_notify = std::forward<Notify>(_notify);
	}

	// 唯一索引
	virtual IndexType index() const noexcept override
	{
		return _index;
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

	// 最小时间
	virtual std::optional<TimeType> time() const override;

	// 取出单任务
	virtual bool take(TaskType& _task) override;

private:
	// 获取索引互斥元
	std::shared_ptr<std::mutex> getMutex(IndexType _index);

	// 查找处理者
	std::shared_ptr<Handler> findHandler(IndexType _index) const;

	// 设置处理者
	void setHandler(IndexType _index, \
		std::shared_ptr<Handler>&& _handler);

	// 查找消息队列
	std::shared_ptr<QueueType> findQueue(IndexType _index) const;

	// 获取消息队列
	std::shared_ptr<QueueType> getQueue(IndexType _index);

private:
	// 执行通知函数子
	void notify() const;

	// 回复任务
	void reply(IndexType _index);

	// 调度队列对索引排序
	bool sort(IndexType _index);

	// 向调度队列放入索引
	bool push(IndexType _index);

	// 从调度队列取出索引
	std::optional<IndexType> pop();

public:
	TaskMapper(IndexType _index) : \
		_index(_index), _size(0) {}

	TaskMapper(const TaskMapper&) = delete;

	virtual ~TaskMapper() = default;

	TaskMapper& operator=(const TaskMapper&) = delete;

	// 任务数量
	SizeType size(IndexType _index) const
	{
		auto queue = findQueue(_index);
		return queue ? queue->size() : 0;
	}

	// 设置处理者
	bool set(IndexType _index, const Handle& _handle, \
		bool _parallel = false);
	bool set(IndexType _index, Handle&& _handle, \
		bool _parallel = false);

	// 适配不同处理接口，推进任务映射器模板化
	template <typename _Functor>
	bool set(IndexType _index, const _Functor& _functor, \
		bool _parallel = false)
	{
		return set(_index, Handle(_functor), _parallel);
	}
	template <typename _Functor>
	bool set(IndexType _index, _Functor&& _functor, bool _parallel = false)
	{
		return set(_index, \
			Handle(std::forward<_Functor>(_functor)), _parallel);
	}

	// 放入单个消息
	bool put(IndexType _index, const Message& _message);
	bool put(IndexType _index, Message&& _message);

	// 批量放入消息
	bool put(IndexType _index, MessageQueue& _queue);
	bool put(IndexType _index, MessageQueue&& _queue);

	// 清空消息
	void clear(IndexType _index);

	// 清空所有消息
	void clear();
};

template <typename _Message>
struct TaskMapper<_Message>::Queue
{
	mutable std::mutex _mutex;
	MessageQueue _messageQueue;
	std::list<TimeType> _timeQueue;

	// 是否为空
	bool empty() const
	{
		std::lock_guard lock(_mutex);
		return _messageQueue.empty();
	}

	// 获取消息数量
	auto size() const
	{
		std::lock_guard lock(_mutex);
		return _messageQueue.size();
	}

	// 获取最小时间
	std::optional<TimeType> time() const;

	// 放入单个消息
	std::optional<SizeType> push(const Message& _message);
	std::optional<SizeType> push(Message&& _message);

	// 批量放入消息
	std::optional<SizeType> push(MessageQueue& _queue);
	std::optional<SizeType> push(MessageQueue&& _queue);

	// 取出单个消息
	bool pop(Message& _message);

	// 清空所有消息
	auto clear();
};

template <typename _Message>
struct TaskMapper<_Message>::Record
{
	IndexType _index;
	TimeType _time;

	explicit operator IndexType() const noexcept
	{
		return _index;
	}

	bool operator<(const Record& _another) const noexcept;
};

template <typename _Message>
struct TaskMapper<_Message>::Handler
{
	Handle _handle;
	bool _parallel;
	bool _idle;

	Handler() noexcept : _parallel(false), _idle(true) {}
	Handler(const Handle& _handle, bool _parallel) : \
		_handle(_handle), _parallel(_parallel), _idle(true) {}
	Handler(Handle&& _handle, bool _parallel) noexcept : \
		_handle(std::forward<Handle>(_handle)), \
		_parallel(_parallel), _idle(true) {}

	// 赋值替换
	void assign(const Handle& _handle, bool _parallel)
	{
		this->_handle = _handle;
		this->_parallel = _parallel;
	}
	void assign(Handle&& _handle, bool _parallel) noexcept
	{
		this->_handle = std::forward<Handle>(_handle);
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
template <typename _Message>
auto TaskMapper<_Message>::Queue::time() const \
-> std::optional<TimeType>
{
	std::lock_guard lock(_mutex);
	if (_timeQueue.empty()) return std::nullopt;
	return _timeQueue.front();
}

// 放入单个消息
template <typename _Message>
auto TaskMapper<_Message>::Queue::push(const Message& _message) \
-> std::optional<SizeType>
{
	std::lock_guard lock(_mutex);
	auto size = _messageQueue.size();
	_messageQueue.push_back(_message);
	_timeQueue.push_back(TimedTask::getSteadyTime());
	return size;
}

// 放入单个消息
template <typename _Message>
auto TaskMapper<_Message>::Queue::push(Message&& _message) \
-> std::optional<SizeType>
{
	std::lock_guard lock(_mutex);
	auto size = _messageQueue.size();
	_messageQueue.push_back(std::forward<Message>(_message));
	_timeQueue.push_back(TimedTask::getSteadyTime());
	return size;
}

// 批量放入消息
template <typename _Message>
auto TaskMapper<_Message>::Queue::push(MessageQueue& _queue) \
-> std::optional<SizeType>
{
	std::lock_guard lock(_mutex);
	auto size = _messageQueue.size();
	_messageQueue.splice(_messageQueue.cend(), _queue);
	_timeQueue.resize(_messageQueue.size(), \
		TimedTask::getSteadyTime());
	return size;
}

// 批量放入消息
template <typename _Message>
auto TaskMapper<_Message>::Queue::push(MessageQueue&& _queue) \
-> std::optional<SizeType>
{
	std::lock_guard lock(_mutex);
	auto size = _messageQueue.size();
	_messageQueue.splice(_messageQueue.cend(), \
		std::forward<MessageQueue>(_queue));
	_timeQueue.resize(_messageQueue.size(), \
		TimedTask::getSteadyTime());
	return size;
}

// 取出单个消息
template <typename _Message>
bool TaskMapper<_Message>::Queue::pop(Message& _message)
{
	std::lock_guard lock(_mutex);
	if (_messageQueue.empty()) return false;

	_message = std::move(_messageQueue.front());
	_messageQueue.pop_front();
	_timeQueue.pop_front();
	return true;
}

// 清空所有消息
template <typename _Message>
auto TaskMapper<_Message>::Queue::clear()
{
	std::lock_guard lock(_mutex);
	auto size = _messageQueue.size();
	_messageQueue.clear();
	_timeQueue.clear();
	return size;
}

template <typename _Message>
bool TaskMapper<_Message>::Record::operator<(const Record& _another) const noexcept
{
	return this->_time < _another._time \
		or this->_time == _another._time and this->_index < _another._index;
}

// 设置是否闲置
template <typename _Message>
bool TaskMapper<_Message>::Handler::idle(bool _idle) noexcept
{
	auto idle = this->_idle;
	this->_idle = _idle;
	return idle;
}

// 执行任务
template <typename _Message>
void TaskMapper<_Message>::execute(const std::weak_ptr<TaskMapper>& _taskMapper, \
	IndexType _index, const Handle& _handle, Message& _message)
{
	if (_handle)
	{
		try
		{
			_handle(_message);
		}
		catch (std::exception& exception)
		{
			Logger::output(Logger::Level::ERROR, \
				std::source_location::current(), exception);
		}
	}

	if (auto taskMapper = _taskMapper.lock()) taskMapper->reply(_index);
}

// 最小时间
template <typename _Message>
auto TaskMapper<_Message>::time() const \
-> std::optional<TimeType>
{
	std::lock_guard lock(_sortMutex);
	auto record = _sorter.front(true);
	if (record != nullptr)
		return record->_time;
	return std::nullopt;
}

// 取出单任务
template <typename _Message>
bool TaskMapper<_Message>::take(TaskType& _task)
{
	std::lock_guard sharedLock(_sharedMutex);

	auto result = pop();
	if (not result) return false;

	auto index = result.value();
	auto handler = findHandler(index);
	if (not handler) return false;

	auto queue = findQueue(index);
	if (not queue) return false;

	Message message;
	if (not queue->pop(message)) return false;

	bool idle = handler->parallel();
	if (not idle) handler->idle(idle);

	std::unique_lock sortLock(_sortMutex);
	if (idle)
		if (auto time = queue->time(); \
			idle = time.has_value())
			_sorter.update({ index, time.value() });

	if (not idle) _sorter.remove(index);
	sortLock.unlock();

	subtract(_size, 1);
	_task = std::bind(execute, \
		std::weak_ptr(this->shared_from_this()), \
		index, handler->_handle, \
		std::move(message));
	return true;
}

// 获取索引互斥元
template <typename _Message>
std::shared_ptr<std::mutex> TaskMapper<_Message>::getMutex(IndexType _index)
{
	std::lock_guard lock(_mutex);
	auto iterator = _mutexMapper.find(_index);
	if (iterator == _mutexMapper.end())
	{
		auto mutex = std::make_shared<std::mutex>();
		auto pair = _mutexMapper.emplace(_index, std::move(mutex));
		iterator = pair.first;
	}
	return iterator != _mutexMapper.end() ? iterator->second : nullptr;
}

// 查找处理者
template <typename _Message>
auto TaskMapper<_Message>::findHandler(IndexType _index) const \
-> std::shared_ptr<Handler>
{
	std::lock_guard lock(_handleMutex);
	auto iterator = _handleMapper.find(_index);
	return iterator != _handleMapper.end() ? \
		iterator->second : nullptr;
}

// 设置处理者
template <typename _Message>
void TaskMapper<_Message>::setHandler(IndexType _index, \
	std::shared_ptr<Handler>&& _handler)
{
	using Handler = std::remove_reference_t<decltype(_handler)>;

	std::lock_guard lock(_handleMutex);
	_handleMapper.insert_or_assign(_index, \
		std::forward<Handler>(_handler));
}

// 查找消息队列
template <typename _Message>
auto TaskMapper<_Message>::findQueue(IndexType _index) const \
-> std::shared_ptr<QueueType>
{
	std::lock_guard lock(_queueMutex);
	auto iterator = _queueMapper.find(_index);
	return iterator != _queueMapper.end() ? \
		iterator->second : nullptr;
}

// 获取消息队列
template <typename _Message>
auto TaskMapper<_Message>::getQueue(IndexType _index) \
-> std::shared_ptr<QueueType>
{
	std::lock_guard lock(_queueMutex);
	auto iterator = _queueMapper.find(_index);
	if (iterator == _queueMapper.end())
	{
		auto queue = std::make_shared<QueueType>();
		auto pair = _queueMapper.emplace(_index, \
			std::move(queue));
		iterator = pair.first;
	}
	return iterator != _queueMapper.end() ? \
		iterator->second : nullptr;
}

// 执行通知函数子
template <typename _Message>
void TaskMapper<_Message>::notify() const
{
	std::unique_lock lock(_notifyMutex);
	auto notify = _notify;
	lock.unlock();

	if (notify) notify(index());
}

// 回复任务
template <typename _Message>
void TaskMapper<_Message>::reply(IndexType _index)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return;

	std::lock_guard indexLock(*mutex);

	if (auto handler = findHandler(_index))
		if (not handler->idle(true) \
			and handler->_handle) sort(_index);
}

// 调度队列对索引排序
template <typename _Message>
bool TaskMapper<_Message>::sort(IndexType _index)
{
	auto queue = findQueue(_index);
	if (not queue) return false;

	auto time = queue->time();
	if (not time) return false;

	std::unique_lock lock(_sortMutex);
	if (_sorter.exist(_index)) return false;

	bool empty = _sorter.empty();
	_sorter.update({ _index, time.value() });
	lock.unlock();

	if (empty) notify();
	return true;
}

// 向调度队列放入索引
template <typename _Message>
bool TaskMapper<_Message>::push(IndexType _index)
{
	auto handler = findHandler(_index);
	return handler and handler->_handle \
		and handler->idle() ? \
		sort(_index) : false;
}

// 从调度队列取出索引
template <typename _Message>
auto TaskMapper<_Message>::pop() -> std::optional<IndexType>
{
	std::lock_guard lock(_sortMutex);
	auto record = _sorter.front(true);
	while (record != nullptr)
	{
		auto index = record->_index;
		if (auto handler = findHandler(index))
			return index;

		_sorter.remove(index);
		record = _sorter.front(true);
	}
	return std::nullopt;
}

// 设置处理者
template <typename _Message>
bool TaskMapper<_Message>::set(IndexType _index, \
	const Handle& _handle, bool _parallel)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return false;

	std::lock_guard indexLock(*mutex);

	auto handler = findHandler(_index);
	if (handler)
	{
		bool invalid = not handler->_handle;
		if (not _handle)
		{
			invalid = false;

			std::lock_guard lock(_sortMutex);
			_sorter.remove(_index);
		}

		handler->assign(_handle, _parallel);
		if (invalid and handler->idle())
			sort(_index);
		return true;
	}

	if (not _handle) return true;

	handler = std::make_shared<Handler>(_handle, \
		_parallel);
	if (not handler) return false;

	setHandler(_index, std::move(handler));
	sort(_index);
	return true;
}

// 设置处理者
template <typename _Message>
bool TaskMapper<_Message>::set(IndexType _index, Handle&& _handle, bool _parallel)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return false;

	std::lock_guard indexLock(*mutex);

	auto handler = findHandler(_index);
	if (handler)
	{
		bool invalid = not handler->_handle;
		if (not _handle)
		{
			invalid = false;

			std::lock_guard lock(_sortMutex);
			_sorter.remove(_index);
		}

		handler->assign(std::forward<Handle>(_handle), _parallel);
		if (invalid and handler->idle()) sort(_index);
		return true;
	}

	if (not _handle) return true;

	handler = std::make_shared<Handler>(std::forward<Handle>(_handle), _parallel);
	if (not handler) return false;

	setHandler(_index, std::move(handler));
	sort(_index);
	return true;
}

// 放入单个消息
template <typename _Message>
bool TaskMapper<_Message>::put(IndexType _index, \
	const Message& _message)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return false;

	std::lock_guard indexLock(*mutex);

	auto queue = getQueue(_index);
	if (not queue) return false;

	auto result = queue->push(_message);
	if (result)
	{
		add(_size, 1);

		if (result.value() == 0) push(_index);
	}
	return result.has_value();
}

// 放入单个消息
template <typename _Message>
bool TaskMapper<_Message>::put(IndexType _index, Message&& _message)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return false;

	std::lock_guard indexLock(*mutex);

	auto queue = getQueue(_index);
	if (not queue) return false;

	auto result = queue->push(std::forward<Message>(_message));
	if (result)
	{
		add(_size, 1);

		if (result.value() == 0) push(_index);
	}
	return result.has_value();
}

// 批量放入消息
template <typename _Message>
bool TaskMapper<_Message>::put(IndexType _index, \
	MessageQueue& _queue)
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

		if (result.value() == 0) push(_index);
	}
	return result.has_value();
}

// 批量放入消息
template <typename _Message>
bool TaskMapper<_Message>::put(IndexType _index, MessageQueue&& _queue)
{
	std::shared_lock sharedLock(_sharedMutex);

	auto mutex = getMutex(_index);
	if (not mutex) return false;

	std::lock_guard indexLock(*mutex);

	auto queue = getQueue(_index);
	if (not queue) return false;

	auto size = _queue.size();
	auto result = queue->push(std::forward<MessageQueue>(_queue));
	if (result)
	{
		add(_size, size);

		if (result.value() == 0) push(_index);
	}
	return result.has_value();
}

// 清空消息
template <typename _Message>
void TaskMapper<_Message>::clear(IndexType _index)
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

// 清空所有消息
template <typename _Message>
void TaskMapper<_Message>::clear()
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

CONCURRENCY_SPACE_END
