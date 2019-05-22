#include "Thread.h"
#include "ThreadPool.h"

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <exception>
#include <iostream>

// 线程数据结构体
struct ThreadStructure
{
	std::thread thread;					// 线程
	ThreadPool *threadPool;				// 线程池指针
	std::mutex threadMutex;				// 线程互斥元
	std::condition_variable signal;		// 条件变量
	TaskPair task;						// 任务函数子
	std::atomic_bool bClosed;			// 关闭状态标志
	std::atomic_bool bRunning;			// 运行状态标志
	std::atomic_bool bIntervening;		// 允许介入标志
	//void *vpParameters;
	// 线程过程参数解决方案：虚基类指针，过程类继承虚基类，通过强制类型转换，在过程函数中访问
};

// 线程构造函数
Thread::Thread(ThreadPool *threadPool)
{
	/* shared_ptr需要维护引用计数，若调用构造函数（即通过new表达式创建对象，之后传递给shared_ptr），
	一共两次内存申请，先申请对象内存，再申请控制块内存，对象内存和控制块内存不连续。
	而使用make_shared方法只申请一次内存，对象内存和控制块内存在一起。 */
	threadData = std::make_shared<ThreadStructure>();
	threadData->threadPool = threadPool;	// 指向线程池，便于自动调用获取线程池任务队列中的任务
	setCloseStatus(false);	// 设置线程未关闭
	setRunStatus(false);	// 设置线程未运行
	setInterveneStatus(true);	// 允许管理线程分配任务
	threadData->thread = std::thread(&Thread::execute, this);	// 创建thread，在其中调用this对象的execute方法
}

// 线程析构函数
Thread::~Thread()
{
	destroy();
}

// 设置线程关闭状态
inline void Thread::setCloseStatus(bool bClosed)
{
	threadData->bClosed = bClosed;
}

// 获取线程关闭状态
inline bool Thread::getCloseStatus() const
{
	return threadData->bClosed;
}

// 设置线程运行状态
inline void Thread::setRunStatus(bool bRunning)
{
	threadData->bRunning = bRunning;
}

// 获取线程运行状态
inline bool Thread::getRunStatus() const
{
	return threadData->bRunning;
}

// 设置线程介入状态
inline void Thread::setInterveneStatus(bool bIntervening)
{
	threadData->bIntervening = bIntervening;
}

// 获取线程介入状态
inline bool Thread::getInterveneStatus() const
{
	return threadData->bIntervening;
}

// 为工作线程配置新任务
bool Thread::configure(TaskPair &&task)
{
	// 若线程处于运行状态，说明正在执行任务，配置新任务失败
	if (getRunStatus())
		return false;
	threadData->task = std::move(task);	// 赋予工作线程任务函数子
	//threadData->vpParameters = vpParameters;
	return true;
}

// 唤醒工作线程
bool Thread::start()
{
	if (getRunStatus())	// 若工作线程处于运行状态，标志着已经配置任务
		return false;
	threadData->signal.notify_one();	// 通过条件变量发送信号，唤醒工作线程
	return true;
}

// 销毁工作线程
void Thread::destroy()
{
	// 若工作线程已经销毁，忽略以下步骤
	if (getCloseStatus())
		return;
	setCloseStatus(true);	// 设置工作线程为关闭状态，即销毁状态
	// 工作线程可能处于阻塞状态，通过条件变量发送信号唤醒工作线程
	threadData->signal.notify_one();
	// 等待未执行完成的工作线程结束
	if (threadData->thread.joinable())
		threadData->thread.join();
	// 预留等待时间
	//std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

// 获取工作线程id
std::thread::id Thread::getThreadId() const
{
	return threadData->thread.get_id();
}

//const void *Thread::getParameters()
//{
//	return threadData->vpParameters;
//}

// 返回线程是否处于空闲状态
bool Thread::isFree() const
{
	return getInterveneStatus();
}

// 工作线程体
void Thread::execute()
{
	// 调用mem_fun获取函数对象适配器mem_fun1_t类型的对象getTask，此对象中保存指向ThreadPool::getTask的指针，并且重载运算符operator()
	auto &&getTask = std::mem_fun(&ThreadPool::getTask);
	std::unique_lock<std::mutex> threadLocker(threadData->threadMutex);	// 创建unique_lock互斥锁，并在构造函数中获取互斥锁
	threadData->signal.wait(threadLocker);	// 获取互斥锁，若之前已经获取过互斥锁，阻塞线程，等待条件变量的唤醒信号
	while (!getCloseStatus())	// 工作线程体退出通道
	{
		setRunStatus(true);	// 设置线程为运行状态
		try
		{
			if (threadData->task.first)	// 若任务函数子不为空
				threadData->task.first();	// 执行任务函数子
			else
				run();	// 执行默认任务函数
			if (threadData->task.second)	// 若回调函数子不为空
				threadData->task.second();	// 执行回调函数子
			else
				callback();	// 执行默认回调函数
		}
		catch (std::exception exception)	// 捕获执行任务函数子时出现的异常，防止线程泄漏
		{
			std::cerr << exception.what() << std::endl;
		}
		setRunStatus(false);
		/* 以threadData->threadPool->getTask(this->shared_from_this())的形式调用ThreadPool::getTask函数获取线程池任务队列中的任务
		若未成功获取任务，阻塞线程，等待条件变量的唤醒信号 */
		if (!(threadData->threadPool
			&& getTask(threadData->threadPool, this->shared_from_this())))
		{
			if (getCloseStatus())	// 工作线程退出通道
				break;
			setInterveneStatus(true);	// 设置允许管理线程分配任务
			threadData->signal.wait(threadLocker);
			setInterveneStatus(false);
		}
	}
}

// 线程默认任务函数，用于继承扩展线程，形成钩子
void Thread::run()
{
}

// 线程默认回调函数，用于继承扩展线程，形成钩子
void Thread::callback()
{
}
