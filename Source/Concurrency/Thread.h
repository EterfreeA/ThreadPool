/*
* 文件名称：Thread.h
* 语言标准：C++20
* 
* 创建日期：2017年09月22日
* 更新日期：2023年09月03日
* 
* 摘要
* 1. 线程类Thread定义于此文件，实现于Thread.cpp。
* 2. 线程类提供线程重用方案，支持销毁再创建，一次创建反复使用。
* 3. 线程在创建之后进入阻塞状态，先调用函数configure分配任务，再调用函数notify激活线程。
* 4. 可选配置单任务或者获取函数子，以及回复函数子。
*   获取函数子用于自动获取任务；回复函数子用于回复线程池，告知已执行完单个任务，以及当前的闲置状态。
* 5. 在执行任务时捕获异常，防止线程泄漏。
* 6. 线程在执行任务之后，倘若配置有获取函数子，则主动获取任务，否则进入阻塞状态。
*   倘若获取任务失败，等待分配任务；否则执行新任务，从而提高执行效率。
* 7. 线程在退出之前，倘若配置有获取函数子，则确保完成所有任务，否则仅执行配置的单个任务。
* 8. 以原子操作确保接口的线程安全性，以单状态枚举确保接口的执行顺序。
* 9. 引入强化条件类模板Condition，当激活先于阻塞时，确保线程正常退出。
* 10.线程主函数声明为静态成员，除去与类成员指针this的关联性。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v3.1.0
* 变化
* v3.0.0
* 1.以获取函数子替换任务队列。
* 2.配置任务支持复制语义和移动语义。
* 3.解决线程在销毁又创建之时可能出现的状态错误问题。
* 4.判断获取的任务是否有效，以防止线程泄漏。
* v3.0.1
* 1.修复移动赋值运算符函数的资源泄漏问题。
* v3.1.0
* 1.封装从类模板改为类，降低编译依存性，同时支持模块化。
* 2.防止获取到无效任务而非预期阻塞线程。
*/

#pragma once

#include <functional>
#include <memory>
#include <atomic>
#include <thread>

#include "Core/Common.hpp"

ETERFREE_SPACE_BEGIN

class Thread final
{
	// 线程数据结构体
	struct Structure;

private:
	using DataType = std::shared_ptr<Structure>;
	using Atomic = std::atomic<DataType>;

public:
	using ThreadID = std::thread::id;

	using TaskType = std::function<void()>;
	using FetchType = std::function<bool(TaskType&)>;
	using ReplyType = std::function<void(ThreadID, bool)>;

private:
	Atomic _atomic;

private:
	// 交换数据
	static auto exchange(Atomic& _atomic, \
		const DataType& _data) noexcept
	{
		return _atomic.exchange(_data, \
			std::memory_order::relaxed);
	}

	// 销毁线程
	static void destroy(DataType&& _data);

	// 获取任务
	static bool getTask(DataType& _data);

	// 线程主函数
	static void execute(DataType _data);

private:
	// 加载非原子数据
	auto load() const noexcept
	{
		return _atomic.load(std::memory_order::relaxed);
	}

public:
	// 默认构造函数
	Thread();

	// 删除默认复制构造函数
	Thread(const Thread&) = delete;

	// 默认移动构造函数
	Thread(Thread&& _another) noexcept : \
		_atomic(exchange(_another._atomic, nullptr)) {}

	// 默认析构函数
	~Thread() noexcept;

	// 删除默认复制赋值运算符函数
	Thread& operator=(const Thread&) = delete;

	// 默认移动赋值运算符函数
	Thread& operator=(Thread&& _another) noexcept;

	// 获取线程唯一标识
	ThreadID getID() const;

	// 是否闲置
	bool idle() const noexcept;

	// 创建线程
	bool create();

	// 销毁线程
	void destroy()
	{
		destroy(load());
	}

	// 配置获取与回复函数子
	bool configure(const FetchType& _fetch, \
		const ReplyType& _reply);

	// 配置任务与回复函数子
	bool configure(const TaskType& _task, \
		const ReplyType& _reply);

	// 配置任务与回复函数子
	bool configure(TaskType&& _task, \
		const ReplyType& _reply);

	// 激活线程
	bool notify();
};

ETERFREE_SPACE_END
