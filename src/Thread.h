﻿/*
* 文件名称：Thread.h
* 语言标准：C++17
* 
* 创建日期：2017年09月22日
* 更新日期：2022年03月13日
* 
* 摘要
* 1. 线程类Thread定义于此文件，实现于Thread.cpp。
* 2. Thread提供线程重用方案，支持销毁再创建，一次创建反复使用。
* 3. 线程在创建之后进入阻塞状态，先调用函数configure分配任务，再调用函数notify激活线程。
* 4. 可选配置单任务或者任务队列，以及回调函数子。
*   任务队列用于自动获取任务，回调函数子用于通知线程池，线程执行完单个任务，以及当前闲置状态。
* 5. 执行任务之时捕获异常，防止线程泄漏。
* 6. 线程执行任务之后，倘若配置有任务队列，主动获取任务，否则进入阻塞状态。
*   倘若获取任务失败，等待分配任务；否则执行新任务，从而提高执行效率。
* 7. 线程在退出之前，倘若配置有任务队列，确保完成所有任务，否则仅执行配置任务。
* 8. 以原子操作确保接口的线程安全性，以单状态枚举确保接口的执行顺序。
* 9. 引入条件类模板Condition，当激活先于阻塞之时，确保线程正常退出。
* 10.线程主函数声明为静态成员，除去与类成员指针this的关联性。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v2.0.3
* 变化
* v2.0.1
* 1.运用Condition的宽松策略，提升激活线程的效率。
* v2.0.2
* 1.消除谓词对条件实例有效性的重复判断。
* v2.0.3
* 1.以原子操作确保移动语义的线程安全性。
* 2.解决配置先于回调隐患。
*/

#pragma once

#include <functional>
#include <utility>
#include <memory>
#include <thread>
#include <mutex>

#include "Core.hpp"

ETERFREE_SPACE_BEGIN

template <typename _Element>
class Queue;

/*
继承类模板enable_shared_from_this，当Thread被shared_ptr托管，而需要传递this给其它函数之时，
需要传递指向this的shared_ptr，调用this->shared_from_this获取指向this的shared_ptr。
不可直接传递裸指针this，否则无法确保shared_ptr的语义，也许会导致已被释放的错误。
不可单独创建另一shared_ptr，否则多个shared_ptr的控制块不同，导致释放多次同一对象。
*/
class Thread
	//: public std::enable_shared_from_this<Thread>
{
	struct Structure;
	using DataType = std::shared_ptr<Structure>;

public:
	using Functor = std::function<void()>;
	using Queue = std::shared_ptr<Queue<Functor>>;
	using ThreadID = std::thread::id;
	using Callback = std::function<void(bool, ThreadID)>;

private:
	mutable std::mutex _mutex;
	DataType _data;

private:
	// 获取任务
	static bool setTask(DataType& _data);

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
	Thread(Thread&& _thread)
	{
		std::lock_guard lock(_thread._mutex);
		_data = std::move(_thread._data);
	}

	// 默认析构函数
	~Thread() { destroy(); }

	// 删除默认复制赋值运算符函数
	Thread& operator=(const Thread&) = delete;

	// 默认移动赋值运算符函数
	Thread& operator=(Thread&& _thread);

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
	void destroy();

	// 配置任务队列与回调函数子
	bool configure(const Queue& _taskQueue, const Callback& _callback);

	// 配置单任务与回调函数子
	bool configure(const Functor& _task, const Callback& _callback);

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
