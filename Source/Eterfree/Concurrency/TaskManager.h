/*
* 文件名称：TaskManager.h
* 语言标准：C++20
* 
* 创建日期：2023年10月02日
* 
* 摘要
* 1.任务管理器类TaskManager定义于此文件，实现于TaskManager.cpp。
* 2.线程池支持指定任务池，任务池抽象类接口对应线程池调用的隐式接口。
*   自定义任务池可选继承此抽象类，由于被多线程并发调用，因此需要确保接口的线程安全性。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v1.0.0
*/

#pragma once

#include <functional>
#include <memory>

#include "Common.h"
#include "TaskPool.h"

CONCURRENCY_SPACE_BEGIN

class TaskManager final
{
	struct Structure;

private:
	using DataType = std::shared_ptr<Structure>;

public:
	using SizeType = TaskPool::SizeType;
	using IndexType = TaskPool::IndexType;

	using ThreadNotify = std::function<void()>;
	using TaskNotify = TaskPool::Notify;

	using TaskType = TaskPool::TaskType;
	using PoolType = std::shared_ptr<TaskPool>;

private:
	DataType _data;

public:
	TaskManager();

	void configure(const ThreadNotify& _notify);

	bool valid() const;

	bool empty() const noexcept;

	SizeType size() const;

	bool take(TaskType& _task);

	PoolType find(IndexType _index);

	bool insert(const PoolType& _pool);

	bool remove(IndexType _index);

	void clear();
};

CONCURRENCY_SPACE_END
