/*
* 文件名称：TaskManager.h
* 语言标准：C++20
* 
* 创建日期：2023年01月21日
* 
* 摘要
* 1.定义任务管理器抽象类TaskManager。
* 2.线程池支持指定任务管理器，任务管理器抽象类接口对应线程池调用的隐式接口。
*   自定义任务管理器可选继承此抽象类，由于被多线程并发调用，因此需要确保接口的线程安全性。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v1.0.0
*/

#pragma once

#include <functional>
#include <cstddef>

#include "Core.hpp"

ETERFREE_SPACE_BEGIN

class TaskManager
{
public:
	using SizeType = std::size_t;
	using NotifyType = std::function<void()>;
	using TaskType = std::function<void()>;

public:
	virtual ~TaskManager() noexcept {}

	virtual void configure(const NotifyType&) = 0;

	virtual void configure(NotifyType&&) = 0;

	virtual bool empty() const = 0;

	virtual SizeType size() const = 0;

	virtual bool take(TaskType&) = 0;
};

ETERFREE_SPACE_END
