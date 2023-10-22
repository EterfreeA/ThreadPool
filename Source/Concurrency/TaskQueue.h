/*
* 文件名称：TaskQueue.h
* 语言标准：C++20
* 
* 创建日期：2023年02月04日
* 更新日期：2023年10月03日
* 
* 摘要
* 1.任务队列类TaskQueue定义于此文件，实现于TaskQueue.cpp。
* 2.任务队列类继承任务池抽象类，确保接口的线程安全性。
* 3.引入双缓冲队列类模板DoubleQueue，提高放入和取出任务的效率。
* 4.考虑不降低批量放入任务性能，因此未检查任务有效性。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v1.1.0
* 变化
* v1.1.0
* 1.弃用双缓冲队列类模板DoubleQueue，以适配任务管理器类TaskManager，支持获取任务队列的最小时间。
* 2.可选自定义队列容量，默认无容量限制。
* 3.解决通知函数子可能出现的死锁问题。
*/

#pragma once

//#include <tuple>
#include <utility>
#include <memory>
#include <list>
#include <atomic>
#include <mutex>

#include "TaskPool.h"

CONCURRENCY_SPACE_BEGIN

class TaskQueue : public TaskPool, \
	public std::enable_shared_from_this<TaskQueue>
{
	using Atomic = std::atomic<SizeType>;

public:
	using TaskList = std::list<TaskType>;
	using TimeList = std::list<TimeType>;

private:
	mutable std::mutex _notifyMutex; // 通知互斥元
	Notify _notify; // 通知函数子

	Atomic _capacity; // 队列容量
	Atomic _size; // 任务数量
	IndexType _index; // 唯一索引

	mutable std::mutex _entryMutex; // 入口互斥元
	TaskList _entryTask; // 入口任务列表
	TimeList _entryTime; // 入口时间列表

	mutable std::mutex _exitMutex; // 出口互斥元
	TaskList _exitTask; // 出口任务列表
	TimeList _exitTime; // 出口时间列表

private:
	// 获取原子值
	static auto get(const Atomic& _atomic) noexcept
	{
		return _atomic.load(std::memory_order::relaxed);
	}

	// 设置原子值
	static void set(Atomic& _atomic, \
		SizeType _size) noexcept
	{
		_atomic.store(_size, \
			std::memory_order::relaxed);
	}

	// 替换原子值
	static auto exchange(Atomic& _atomic, \
		SizeType _size) noexcept
	{
		return _atomic.exchange(_size, \
			std::memory_order::relaxed);
	}

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

	// 是否为空
	virtual bool empty() const noexcept override
	{
		return size() == 0;
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
	// 执行通知函数子
	void notify() const;

	// 原子加法
	auto add(SizeType _size) noexcept
	{
		return this->_size.fetch_add(_size, \
			std::memory_order::relaxed);
	}

	// 原子减法
	auto subtract(SizeType _size) noexcept
	{
		return this->_size.fetch_sub(_size, \
			std::memory_order::relaxed);
	}

	// 未达到队列容量限制
	bool valid(TaskList& _taskList) const noexcept;

	// 放入单任务
	bool push(const TaskType& _task);
	bool push(TaskType&& _task);

public:
	// 若_capacity小于等于零，则无限制，否则其为上限值
	TaskQueue(IndexType _index, SizeType _capacity = 0) : \
		_index(_index), _capacity(_capacity), _size(0) {}

	TaskQueue(const TaskQueue&) = delete;

	virtual ~TaskQueue() = default;

	TaskQueue& operator=(const TaskQueue&) = delete;

	// 获取队列容量
	auto capacity() const noexcept
	{
		return get(_capacity);
	}

	// 设置队列容量
	void reserve(SizeType _capacity) noexcept
	{
		set(this->_capacity, _capacity);
	}

	// 放入单任务
	bool put(const TaskType& _task)
	{
		return push(_task);
	}
	bool put(TaskType&& _task)
	{
		return push(std::forward<TaskType>(_task));
	}

	// 适配不同任务接口
	template <typename _Functor>
	bool put(const _Functor& _functor)
	{
		return push(_functor);
	}
	template <typename _Functor>
	bool put(_Functor&& _functor)
	{
		return push(std::forward<_Functor>(_functor));
	}
	template <typename _Functor, typename... _Args>
	bool put(_Functor&& _functor, _Args&&... _args);

	// 批量放入任务
	bool put(TaskList& _taskList);
	bool put(TaskList&& _taskList);

	// 批量取出任务
	bool take(TaskList& _taskList);

	// 清空所有任务
	SizeType clear();
};

template <typename _Functor, typename... _Args>
bool TaskQueue::put(_Functor&& _functor, _Args&&... _args)
{
	//return push([_functor, _args...] { _functor(_args...); });
	//return push([_functor = std::forward<_Functor>(_functor), \
	//	_args = std::make_tuple(std::forward<_Args>(_args)...)]
	//{ std::apply(_functor, _args); });
	return push(std::bind(std::forward<_Functor>(_functor), \
		std::forward<_Args>(_args)...));
}

CONCURRENCY_SPACE_END
