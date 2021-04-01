/*
文件名称：Thread.h
摘要：
1.线程类Thread定义于此文件，实现于Thread.cpp。
2.Thread提供线程重用方案，一次创建反复使用，支持销毁再创建。
3.线程在创建之后进入阻塞状态，先调用函数configure分配任务，再调用函数notify激活线程。
4.线程执行任务之后，若配置有任务队列，主动从任务队列获取任务，否则进入阻塞状态。
5.线程在退出之前，若配置有任务队列，确保执行队列的所有任务，否则仅执行配置任务。
6.执行函数子之时捕获异常，确保正常执行任务，防止线程泄漏。
7.以互斥元确保除移动构造之外，其它接口线程安全。

版本：v1.7
作者：许聪
邮箱：2592419242@qq.com
创建日期：2017年09月22日
更新日期：2021年03月21日

历史：
v1.1
1.线程执行任务之后，自动从任务队列获取任务。
	若获取任务失败，激活守护线程，进入阻塞状态，等待分配任务，否则执行新任务，从而提高执行效率。
v1.2
1.精简代码，取消构造函数的任务参数。
2.以移动语义优化配置任务操作，减少不必要的复制步骤。
v1.3
1.调整命名风格，优化类接口。
2.清除多余线程状态，合并介入标志（即允许分配任务）至运行标志，使线程状态转换逻辑清晰。
3.配置任务放弃移动语义，换回复制语义，任务形式乃函数子，存放数据少，复制操作对效率影响小。
	支持任务来源留存备份，避免线程异常而丢失任务。
4.新增移动语义构造和赋值线程，线程主函数声明为静态成员，除去与类成员指针this的关联性。
5.支持选择性配置任务队列和回调函数子。
	分别用于自动获取任务，回调通知线程池，线程执行完单个任务，以及当前闲置状态。
v1.4
1.调整成员访问权限。
2.归入名称空间eterfree。
3.类Thread内外分别声明和定义结构体Structure，避免污染类外名称空间，从而增强类的封装性。
v1.5
1.优化构造函数，运用初始化列表，初始化成员变量，删除多余赋值步骤。
2.精简任务形式为函数子，不再含有回调函数子，若执行任务需要回调，可以在任务函数子之内进行回调步骤。
v1.6
1.优化虚假唤醒解决方案，启动方法判断任务是否为空，取消条件变量成立判别式。
2.尝试解决激活先于阻塞的隐患，若满足条件就不必等待。
3.退出通道增设条件，确保线程在退出之前，执行已经配置的任务。
4.移动Queue.h的引用至源文件，以双缓冲队列模板的声明式替换其定义式，除去Thread.h对Queue.h的编译依存性。
v1.7
1.提供多次创建和销毁线程的方案。
2.以单个状态枚举变量代替多个状态变量，确保接口的执行顺序。
3.以互斥元确保接口线程安全。
4.引入条件类模板Condition，当激活先于阻塞之时，确保线程正常退出。
5.修改退出通道条件，确保线程在退出之前，执行所有任务。
*/

#pragma once

#include <functional>
#include <memory>
#include <thread>

#include "core.h"

ETERFREE_BEGIN

template <typename DataType>
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
	DataType data;

private:
	static bool setTask(DataType& data);
	static void execute(DataType data);

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
	// 是否空闲
	REPLACEMENT(idle)
	bool free() const noexcept;
	// 是否闲置
	bool idle() const noexcept;

	// 创建线程
	bool create();
	// 销毁线程
	void destroy();

	// 配置任务队列与回调函数子
	bool configure(TaskQueue taskQueue, Callback callback);
	// 配置单任务与回调函数子
	bool configure(const Functor& task, Callback callback);

	// 启动线程
	REPLACEMENT(notify)
	bool start();
	// 激活线程
	bool notify();
};

ETERFREE_END
