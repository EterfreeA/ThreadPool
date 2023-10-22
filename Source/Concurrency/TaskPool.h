/*
* 文件名称：TaskPool.h
* 语言标准：C++20
* 
* 创建日期：2023年01月21日
* 更新日期：2023年10月02日
* 
* 摘要
* 1.定义任务池抽象类TaskPool。
* 2.线程池支持指定任务池，任务池抽象类接口对应线程池调用的隐式接口。
*   自定义任务池继承此抽象类，由于被多线程并发调用，因此需要确保接口的线程安全性。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v2.0.0
* 变化
* v2.0.0
* 1.重命名任务管理器TaskManager为任务池TaskPool。
*/

#pragma once

#include <cstddef>
#include <functional>
#include <optional>

#include "Core/Timer.h"
#include "Common.h"

CONCURRENCY_SPACE_BEGIN

class TaskPool
{
public:
	using SizeType = std::size_t;
	using IndexType = SizeType;

	using TimeType = TimedTask::SteadyTime;

	using Notify = std::function<void(IndexType)>;
	using TaskType = std::function<void()>;

public:
	virtual ~TaskPool() noexcept {}

	virtual void configure(const Notify&) = 0;

	virtual void configure(Notify&&) = 0;

	virtual IndexType index() const = 0;

	virtual bool empty() const = 0;

	virtual SizeType size() const = 0;

	virtual std::optional<TimeType> time() const = 0;

	virtual bool take(TaskType&) = 0;
};

CONCURRENCY_SPACE_END
