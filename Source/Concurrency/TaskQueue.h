/*
* 文件名称：TaskQueue.h
* 语言标准：C++20
* 
* 创建日期：2023年02月04日
* 
* 摘要
* 1.任务队列类TaskQueue定义于此文件，实现于TaskQueue.cpp。
* 2.任务队列类继承任务管理器抽象类，确保接口的线程安全性。
* 3.引入双缓冲队列类模板DoubleQueue，提高放入和取出任务的效率。
* 4.由于考虑不降低批量放入任务性能，因此未检查任务有效性，使用者需要确保任务函数子有效，否则会导致线程泄漏。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v1.0.0
*/

#pragma once

//#include <tuple>
#include <utility>
#include <memory>
#include <mutex>

#include "DoubleQueue.hpp"
#include "TaskManager.h"

ETERFREE_SPACE_BEGIN

class TaskQueue : public TaskManager, \
	public std::enable_shared_from_this<TaskQueue>
{
private:
	using DoubleQueue = DoubleQueue<TaskType>;

public:
	using QueueType = DoubleQueue::QueueType;

private:
	mutable std::mutex _mutex; // 通知互斥元
	Notify _notify; // 通知函数子

	DoubleQueue _queue; // 任务队列

public:
	// 配置通知函数子
	virtual void configure(const Notify& _notify) override
	{
		std::lock_guard lock(_mutex);
		this->_notify = _notify;
	}
	virtual void configure(Notify&& _notify) override
	{
		std::lock_guard lock(_mutex);
		this->_notify = std::forward<Notify>(_notify);
	}

	// 是否为空
	virtual bool empty() const noexcept override
	{
		return _queue.empty();
	}

	// 总任务数量
	virtual SizeType size() const noexcept override
	{
		return _queue.size();
	}

	// 取出单任务
	virtual bool take(TaskType& _task) override
	{
		return _queue.pop(_task);
	}

private:
	// 执行通知函数子
	void notify() const;

	// 放入单任务
	bool push(const TaskType& _task);
	bool push(TaskType&& _task);

public:
	TaskQueue() = default;

	TaskQueue(const TaskQueue&) = delete;

	virtual ~TaskQueue() = default;

	TaskQueue& operator=(const TaskQueue&) = delete;

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
	bool put(QueueType& _queue);
	bool put(QueueType&& _queue);

	// 批量取出任务
	bool take(QueueType& _queue)
	{
		return this->_queue.pop(_queue);
	}

	// 清空所有任务
	auto clear()
	{
		return _queue.clear();
	}
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

ETERFREE_SPACE_END
