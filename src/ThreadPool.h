/*
文件名称：ThreadPool.h
摘要：
1.线程池类ThreadPool定义于此文件，实现于ThreadPool.cpp。
2.当任务队列为空时，阻塞守护线程，而增加任务之时，激活守护线程，通知线程获取任务。
3.当无闲置线程时，阻塞守护线程，而存在闲置线程之时，激活守护线程，通知闲置线程获取任务。
4.采用双缓冲任务队列，降低读写任务的相互影响，提高放入任务和取出任务的效率。
5.当销毁线程池之时，等待守护线程退出，而守护线程在退出之前，等待所有线程退出。
6.提供增删线程策略，由守护线程增删线程。
	在任务队列非空之时一次性增加线程，当存在闲置线程之时，逐个删减线程。

版本：v1.9
作者：许聪
邮箱：2592419242@qq.com
创建日期：2017年09月22日
更新日期：2021年04月05日

变化：
v1.1
1.设置空闲线程表，线程主动获取任务之时，若任务队列为空，阻塞线程并放入空闲线程表，而守护线程分配任务之时，遍历空闲线程表，取出并激活线程。
v1.2
1.增加空闲线程计数，删除空闲线程表。
2.解决销毁线程池的死锁隐患。
v1.3
1.以移动语义优化配置任务操作，减少不必要的复制步骤。
2.删除分配任务的多余判断步骤。
3.采用双缓冲任务队列类模板Queue，降低读写任务的相互影响，提高放入任务和取出任务的效率。
v1.4
1.调整命名风格，优化类接口。
2.优化任务分配逻辑。
3.支持非阻塞式销毁线程池。
	当销毁线程池之时，先设置线程池为关闭状态，再激活守护线程，由守护线程销毁线程。
4.新增移动语义构造和赋值线程池，守护线程主函数声明为静态成员，除去与类成员指针this的关联性。
v1.5
1.调整成员访问权限。
2.归入名称空间eterfree。
3.运用析构函数自动销毁线程。
4.以weak_ptr解决shared_ptr循环引用问题，防止销毁线程池之时内存泄漏。
5.类ThreadPool内外分别声明和定义结构体Structure，避免污染类外名称空间，从而增强类的封装性。
v1.6
1.优化构造函数，运用初始化列表，初始化成员变量，删除多余赋值步骤。
2.精简任务形式为函数子，不再含有回调函数子，以降低内存开销。
v1.7
1.添加任务之时过滤无效任务，增强线程池的健壮性，防止线程获取任务成功而启动失败。
	无效任务会导致守护线程失去启动线程的能力，在最坏的情况，所有线程处于阻塞状态，线程池无法处理任务，任务堆积过多，内存耗尽，最终程序崩溃。
2.优化条件变量，删除多余判断步骤。
v1.8
1.改为等待式销毁线程池。
	当销毁线程池之时，等待守护线程退出，而守护线程在退出之前，等待所有线程退出。
2.引入条件类模板Condition，当激活先于阻塞之时，确保守护线程正常退出。
3.提供增删线程策略，由守护线程增删线程。
	在任务队列非空之时一次性增加线程，当存在闲置线程之时，逐个删减线程。
v1.9
1.运用Condition的宽松策略，提升激活守护线程的效率。
2.消除谓词对条件实例有效性的重复判断。
*/

#pragma once

#include <functional>
#include <utility>
#include <cstddef>
#include <memory>
#include <list>

#include "core.h"

ETERFREE_BEGIN

class ThreadPool
{
	//friend class Thread;
	struct Structure;
	using DataType = std::shared_ptr<Structure>;

public:
	using SizeType = std::size_t;
	using Functor = std::function<void()>;

private:
	DataType data;

private:
	static SizeType adjust(DataType& data);
	static void execute(DataType data);
	void destroy();

public:
	ThreadPool(SizeType size = getConcurrency(), \
		SizeType capacity = getConcurrency());
	ThreadPool(const ThreadPool&) = delete;
	/*
	 * 非线程安全。一旦启用移动构造函数，无法确保接口的线程安全性。
	 * 解决方案：
	 *     1.类外增加互斥操作，确保所有接口的线程安全。
	 *     2.类内添加静态成员变量，确保接口的原子性。不过，此法影响类的所有对象，可能降低执行效率。
	 *     3.类外传递互斥元至类内，确保移动语义的线程安全。
	 */
	ThreadPool(ThreadPool&&) noexcept = default;
	~ThreadPool();

	ThreadPool& operator=(const ThreadPool&) = delete;
	// 非线程安全，同移动构造函数。
	ThreadPool& operator=(ThreadPool&&) noexcept = default;

	// 获取支持的并发线程数量
	static SizeType getConcurrency() noexcept;

	// 设置最大线程数量
	REPLACEMENT(setCapacity)
	void setMaxThreads(SizeType capacity);

	// 设置线程池容量
	void setCapacity(SizeType capacity);

	// 获取最大线程数量
	REPLACEMENT(getCapacity)
	SizeType getMaxThreads() const noexcept;

	// 获取线程池容量
	SizeType getCapacity() const noexcept;

	// 设置线程数量
	DEPRECATED
	bool setThreads(SizeType size) noexcept;

	// 获取线程数量
	REPLACEMENT(getSize)
	SizeType getThreads() const noexcept;

	// 获取线程数量
	SizeType getSize() const noexcept;

	// 获取空闲线程数量
	REPLACEMENT(getIdleSize)
	SizeType getFreeThreads() const noexcept;

	// 获取闲置线程数量
	SizeType getIdleSize() const noexcept;

	// 获取任务数量
	REPLACEMENT(getTaskSize)
	SizeType getTasks() const noexcept;

	// 获取任务数量
	SizeType getTaskSize() const noexcept;

	// 添加任务
	bool pushTask(Functor&& task);
	// 适配不同接口的任务，推进线程池的模板化
	template <typename Functor>
	bool pushTask(Functor&& task)
	{
		return pushTask(ThreadPool::Functor(std::forward<Functor>(task)));
	}
	template <typename Functor, typename... Args>
	bool pushTask(Functor&& task, Args&&... args)
	{
		return pushTask(ThreadPool::Functor([task, args...]{ task(args...); }));
	}
	// 批量添加任务
	bool pushTask(std::list<Functor>& tasks);
};

ETERFREE_END
