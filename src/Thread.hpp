/*
文件名称：Thread.hpp
摘要：
1. 线程类Thread定义于此文件，实现于Thread.cpp。
2. Thread提供线程重用方案，支持销毁再创建，一次创建反复使用。
3. 线程在创建之后进入阻塞状态，先调用函数configure分配任务，再调用函数notify激活线程。
4. 支持选择性配置单任务或者任务队列，以及回调函数子。
	任务队列用于自动获取任务，回调函数子用于通知线程池，线程执行完单个任务，以及当前闲置状态。
5. 执行任务之时捕获异常，防止线程泄漏。
6. 线程执行任务之后，若配有任务队列，主动获取任务，否则进入阻塞状态。
	若获取任务失败，等待分配任务；否则执行新任务，从而提高执行效率。
7. 线程在退出之前，若配有任务队列，确保执行队列的所有任务，否则仅执行配置任务。
8. 以互斥元确保除移动构造之外，其它接口的线程安全性，以单状态枚举确保接口的执行顺序。
9. 引入条件类模板Condition，当激活先于阻塞之时，确保线程正常退出。
10.线程主函数声明为静态成员，除去与类成员指针this的关联性。

版本：v2.0.0
作者：许聪
邮箱：2592419242@qq.com
创建日期：2017年09月22日
更新日期：2021年04月04日
*/

#pragma once

#include <functional>
#include <memory>
#include <thread>

#include "Core.hpp"

ETERFREE_BEGIN

template <typename _DataType>
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
	using TaskQueue = std::shared_ptr<Queue<Functor>>;
	using ThreadID = std::thread::id;
	using Callback = std::function<void(bool, ThreadID)>;

private:
	DataType _data;

private:
	static bool setTask(DataType& _data);
	static void execute(DataType _data);

public:
	Thread();
	Thread(const Thread&) = delete;
	/*
	 * 非线程安全。一旦启用移动构造函数，无法确保接口的线程安全性。
	 * 解决方案：
	 *     1.类外增加互斥操作，确保所有接口的线程安全。
	 *     2.类内添加静态成员变量，确保接口的原子性。不过，此法影响类的所有对象，可能降低执行效率。
	 *     3.类外传递互斥元至类内，确保移动语义的线程安全。
	 */
	Thread(Thread&&) noexcept = default;
	~Thread();

	Thread& operator=(const Thread&) = delete;
	// 非线程安全，同移动构造函数。
	Thread& operator=(Thread&&) noexcept = default;

	// 获取线程唯一标识（调用之前不可销毁线程）
	ThreadID getID();
	// 是否闲置
	bool idle() const noexcept;

	// 创建线程
	bool create();
	// 销毁线程
	void destroy();

	// 配置任务队列与回调函数子
	bool configure(TaskQueue _taskQueue, Callback _callback);
	// 配置单任务与回调函数子
	bool configure(const Functor& _task, Callback _callback);

	// 激活线程
	bool notify();
};

ETERFREE_END
