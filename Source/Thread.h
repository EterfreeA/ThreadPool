﻿/*
* 文件名称：Thread.h
* 语言标准：C++17
* 
* 创建日期：2017年09月22日
* 更新日期：2023年10月15日
* 
* 摘要
* 1. 线程类Thread定义于此文件，实现于Thread.cpp。
* 2. 线程类提供线程重用方案，支持销毁再创建，一次创建反复使用。
* 3. 线程在创建之后进入阻塞状态，先调用函数configure分配任务，再调用函数notify激活线程。
* 4. 可选配置单任务或者任务队列，以及回调函数子。
*   任务队列用于自动获取任务，回调函数子用于通知线程池，告知已执行完单个任务，以及当前的闲置状态。
* 5. 在执行任务时捕获异常，防止线程泄漏。
* 6. 线程在执行任务之后，倘若配置有任务队列，则主动获取任务，否则进入阻塞状态。
*   倘若获取任务失败，等待分配任务；否则执行新任务，从而提高执行效率。
* 7. 线程在退出之前，倘若配置有任务队列，则确保完成所有任务，否则仅执行配置的单个任务。
* 8. 以原子操作确保接口的线程安全性，以单状态枚举确保接口的执行顺序。
* 9. 引入强化条件类模板Condition，当激活先于阻塞时，确保线程正常退出。
* 10.线程主函数声明为静态成员，除去与类成员指针this的关联性。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v2.3.0
* 变化
* v2.0.1
* 1.运用Condition的宽松策略，提升激活线程的效率。
* v2.0.2
* 1.消除谓词对条件实例有效性的重复判断。
* v2.0.3
* 1.以原子操作确保移动语义的线程安全性。
* 2.消除配置先于回调的隐患。
* v2.1.0
* 1.解决线程在销毁又创建之时的直接退出问题。
* v2.2.0
* 1.配置任务支持复制语义和移动语义。
* 2.解决线程在销毁又创建之时可能出现的状态错误问题。
* 3.判断获取的任务是否有效，以防止线程泄漏。
* v2.2.1
* 1.修复移动赋值运算符函数的资源泄漏问题。
* v2.2.2
* 1.优化移动语义。
* 2.确保移动构造函数和析构函数的异常安全性。
* v2.3.0
* 1.确保移动赋值运算符函数的异常安全性。
* 2.防止获取到无效任务而非预期阻塞线程。
*/

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "Common.hpp"

ETERFREE_SPACE_BEGIN

template <typename _Element>
class DoubleQueue;

class Thread final
{
	// 线程数据结构体
	struct Structure;

private:
	using DataType = std::shared_ptr<Structure>;

public:
	using TaskType = std::function<void()>;
	using QueueType = std::shared_ptr<DoubleQueue<TaskType>>;

	using ThreadID = std::thread::id;
	using Callback = std::function<void(ThreadID, bool)>;

private:
	mutable std::mutex _mutex;
	DataType _data;

private:
	// 移动数据
	static DataType move(Thread& _left, \
		Thread&& _right);

	// 销毁线程
	static void destroy(DataType&& _data);

	// 获取任务
	static bool getTask(DataType& _data);

	// 线程主函数
	static void execute(DataType _data);

private:
	// 加载非原子数据
	auto load() const
	{
		std::lock_guard lock(_mutex);
		return _data;
	}

public:
	// 默认构造函数
	Thread();

	// 删除默认复制构造函数
	Thread(const Thread&) = delete;

	// 默认移动构造函数
	Thread(Thread&& _another) noexcept;

	// 默认析构函数
	~Thread() noexcept;

	// 删除默认复制赋值运算符函数
	Thread& operator=(const Thread&) = delete;

	// 默认移动赋值运算符函数
	Thread& operator=(Thread&& _thread) noexcept;

	// 获取线程唯一标识
	ThreadID getID() const;

	// 是否空闲
	REPLACEMENT(idle)
	bool free() const
	{
		return idle();
	}

	// 是否闲置
	bool idle() const;

	// 创建线程
	bool create();

	// 销毁线程
	void destroy()
	{
		destroy(load());
	}

	// 配置任务队列与回调函数子
	bool configure(const QueueType& _taskQueue, \
		const Callback& _callback);

	// 配置单任务与回调函数子
	bool configure(const TaskType& _task, \
		const Callback& _callback);

	// 配置单任务与回调函数子
	bool configure(TaskType&& _task, \
		const Callback& _callback);

	// 启动线程
	REPLACEMENT(notify)
	bool start()
	{
		return notify();
	}

	// 激活线程
	bool notify();
};

ETERFREE_SPACE_END
