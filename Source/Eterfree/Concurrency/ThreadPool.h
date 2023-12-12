/*
* 文件名称：ThreadPool.h
* 语言标准：C++20
* 
* 创建日期：2017年09月22日
* 更新日期：2023年10月13日
* 
* 摘要
* 1.线程池类ThreadPool定义于此文件，实现于ThreadPool.cpp。
* 2.当无任务时，阻塞守护线程；当新增任务时，激活守护线程，通知线程执行任务。
* 3.当无闲置线程时，阻塞守护线程；当存在闲置线程时，激活守护线程，通知闲置线程执行任务。
* 4.当销毁线程池时，等待守护线程退出。而守护线程在退出之前，等待所有线程退出。
*   线程在退出之前，默认执行剩余的所有任务。可选清空任务管理器的所有任务池，或者清空任务池的所有任务，以实现线程立即退出。
* 5.提供增删线程策略，由守护线程增删线程。
*   当存在任务时，一次性增加线程；当存在闲置线程时，逐个删减线程。
* 6.以原子操作确保接口的线程安全性，并且新增成员类Proxy，用于减少原子操作，针对频繁操作提升性能。
* 7.守护线程主函数声明为静态成员，除去与类成员指针this的关联性。
* 8.引入强化条件类模板Condition，当激活先于阻塞时，确保守护线程正常退出。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v4.0.0
* 变化
* v3.0.0
* 1.抽象任务池为模板，以支持自定义任务池。
* 2.在销毁线程池时，当任务管理器为空或者无效，并且所有线程闲置，守护线程才退出，否则守护线程轮询等待，直至满足退出条件。
* v3.0.1
* 1.修复移动赋值运算符函数的资源泄漏问题。
* v3.1.0
* 1.封装从类模板改为类，降低编译依存性，同时支持模块化。
* 2.在销毁线程池时，守护线程等待退出条件由轮询改为阻塞与激活。
* v4.0.0
* 1.完善并发模型，实现统一调度线程，执行不同类型任务。
* 2.引入新任务管理器，支持管理多个任务池派生类实例。
*/

#pragma once

#include <memory>
#include <atomic>

#include "Common.h"
#include "TaskManager.h"

CONCURRENCY_SPACE_BEGIN

class ThreadPool final
{
	// 线程池数据结构体
	struct Structure;

public:
	// 线程池代理类
	class Proxy;

private:
	using DataType = std::shared_ptr<Structure>;
	using Atomic = std::atomic<DataType>;

public:
	using SizeType = TaskManager::SizeType;

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

	// 创建线程池
	static void create(DataType&& _data, \
		SizeType _capacity);

	// 销毁线程池
	static void destroy(DataType&& _data);

	// 调整线程数量
	static SizeType adjust(DataType& _data);

	// 守护线程主函数
	static void execute(DataType _data);

public:
	// 获取支持的并发线程数量
	static SizeType getConcurrency() noexcept;

private:
	// 加载非原子数据
	auto load() const noexcept
	{
		return _atomic.load(std::memory_order::relaxed);
	}

public:
	// 默认构造函数
	ThreadPool(SizeType _capacity = getConcurrency());

	// 删除默认复制构造函数
	ThreadPool(const ThreadPool&) = delete;

	// 默认移动构造函数
	ThreadPool(ThreadPool&& _another) noexcept : \
		_atomic(exchange(_another._atomic, nullptr)) {}

	// 默认析构函数
	~ThreadPool() noexcept;

	// 删除默认复制赋值运算符函数
	ThreadPool& operator=(const ThreadPool&) = delete;

	// 默认移动赋值运算符函数
	ThreadPool& operator=(ThreadPool&& _threadPool) noexcept;

	// 获取线程池容量
	SizeType getCapacity() const noexcept;

	// 设置线程池容量
	bool setCapacity(SizeType _capacity);

	// 获取总线程数量
	SizeType getTotalSize() const noexcept;

	// 获取闲置线程数量
	SizeType getIdleSize() const noexcept;

	// 获取任务管理器
	TaskManager* getTaskManager() const;

	// 获取代理
	Proxy getProxy() const noexcept;
};

class ThreadPool::Proxy final
{
	DataType _data;

public:
	Proxy(const decltype(_data)& _data) noexcept : \
		_data(_data) {}

	explicit operator bool() const noexcept { return valid(); }

	// 是否有效
	bool valid() const noexcept
	{
		return static_cast<bool>(_data);
	}

	// 获取线程池容量
	SizeType getCapacity() const noexcept;

	// 设置线程池容量
	bool setCapacity(SizeType _capacity);

	// 获取总线程数量
	SizeType getTotalSize() const noexcept;

	// 获取闲置线程数量
	SizeType getIdleSize() const noexcept;

	// 获取任务管理器
	TaskManager* getTaskManager() const;
};

CONCURRENCY_SPACE_END
