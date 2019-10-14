#include "Thread.h"

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <exception>
#include <iostream>

ETERFREE_BEGIN

// 线程数据结构体
struct Thread::ThreadStructure
{
	std::thread thread;										// 工作线程
	std::mutex mutex;										// 互斥元
	std::condition_variable condition;						// 条件变量
	std::atomic_bool closed;								// 关闭状态标志
	std::atomic_bool running;								// 运行状态标志
	//ThreadPool *threadPool;								// 线程池指针
	std::shared_ptr<Queue<Thread::functor>> taskQueue;		// 任务队列
	std::function<void(bool, Thread::ThreadID)> callback;	// 回调函数子
	Thread::functor task;									// 任务函数子
	// 线程过程参数解决方案：虚基类指针，过程类继承虚基类，通过强制类型转换，在过程函数中访问
	//void *vpParameters;
};

// 默认构造函数
Thread::Thread()
	: data(std::make_shared<ThreadStructure>())
{
	// 线程设为未关闭未运行状态
	setClosed(data, false);
	setRunning(data, false);
	// 创建std::thread对象，即工作线程，以data为参数，执行execute函数
	data->thread = std::thread(Thread::execute, data);
}

// 默认析构函数
Thread::~Thread()
{
	destroy();
}

// 任务队列及回调函数子配置方法
bool Thread::configure(std::shared_ptr<Queue<functor>> taskQueue,
	std::function<void(bool, ThreadID)> callback)
{
	if (getRunning(data))
		return false;
	//data->threadPool = threadPool;	// 指向线程池，便于自动从任务队列获取任务
	data->taskQueue = taskQueue;	// 指向任务队列，便于自动获取任务
	data->callback = callback;	// 回调函数子，用于通知守护线程获取任务失败，线程进入阻塞状态
	return true;
}

// 任务配置方法
bool Thread::configure(const functor& task)
{
	// 若处于运行状态，标志正在执行任务，配置新任务失败
	if (getRunning(data))
		return false;
	data->task = task;	// 配置任务函数子
	//data->vpParameters = vpParameters;
	return true;
}

// 启动工作线程
bool Thread::start()
{
	// 若处于运行状态，标志正在执行任务，不必唤醒工作线程
	if (getRunning(data))
		return false;
	data->condition.notify_one();	// 通过条件变量唤醒工作线程
	return true;
}

// 获取线程ID
Thread::ThreadID Thread::getThreadID() const
{
	return data->thread.get_id();
}

// 获取空闲状态
bool Thread::free() const
{
	return !getRunning(data);
}

//const void *Thread::getParameters()
//{
//	return data->vpParameters;
//}

// 设置关闭状态
inline void Thread::setClosed(data_type& data, bool closed)
{
	data->closed = closed;
}

// 获取关闭状态
inline bool Thread::getClosed(const data_type& data)
{
	return data->closed;
}

// 设置运行状态
inline void Thread::setRunning(data_type& data, bool running)
{
	data->running = running;
}

// 获取运行状态
inline bool Thread::getRunning(const data_type& data)
{
	return data->running;
}

// 工作线程主函数
void Thread::execute(data_type data)
{
	// 调用mem_fn获取函数子getTask，其拥有指向ThreadPool::getTask的指针，并且重载运算符operator()
	//auto &&getTask = std::mem_fn(&ThreadPool::getTask);

	// 创建unique_lock互斥锁，用于阻塞和唤醒工作线程，未指定锁策略默认立即锁住互斥元
	using lock_type = std::unique_lock<std::mutex>;
	lock_type threadLocker(data->mutex);
	// 运用条件变量锁定线程互斥锁，若之前锁定过互斥锁，工作线程进入阻塞状态，等待条件变量的唤醒消息
	data->condition.wait(threadLocker);

	// 若任务队列指针非空，创建互斥锁，指定延迟锁定策略，用于从任务队列互斥读取任务
	lock_type taskLocker;
	if (data->taskQueue)
		taskLocker = decltype(taskLocker)(data->taskQueue->mutex(), std::defer_lock);

	while (!getClosed(data))	// 工作线程退出通道
	{
		setRunning(data, true);	// 线程设为运行状态
		try
		{
			if (data->task)	// 若任务函数子非空
				data->task();	// 执行任务函数子
			//else
			//	process();	// 执行默认任务函数
		}
		catch (std::exception& exception)	// 执行函数子时捕获异常，防止线程泄漏
		{
			std::cerr << exception.what() << std::endl;
		}
		data->task = nullptr;	// 执行完毕清除任务

		/* 以data->threadPool->getTask(this->shared_from_this())的形式
		调用ThreadPool::getTask函数获取线程池任务队列的任务
		若未成功获取任务，阻塞线程，等待条件变量的唤醒信号 */
		//if (!(data->threadPool
		//	&& getTask(data->threadPool, this->shared_from_this())))
		//{
		//	if (getClosed())	// 工作线程退出通道
		//		break;
		//	setRunning(false);	// 允许守护线程分配任务
		//	data->condition.wait(threadLocker);	// 以条件变量锁定互斥锁，进入阻塞状态
		//}

		if (getClosed(data))	// 工作线程退出通道
			break;

		bool empty = true;	// 任务获取标志，用于判断是否进入阻塞状态
		// 若任务队列指针非空，并且队列非空，则配置新任务
		if (data->taskQueue)
		{
			taskLocker.lock();
			if (empty = data->taskQueue->empty(); !empty)
			{
				data->task = data->taskQueue->front();
				data->taskQueue->pop();
			}
			taskLocker.unlock();
		}

		if (empty)	// 任务队列指针非空且队列为空
		{
			setRunning(data, false);	// 线程设为未运行状态，即允许分配任务
			if (data->callback)	// 若回调函数子非空
				data->callback(empty, data->thread.get_id());	// 执行回调函数子
			// 进入阻塞状态，等待条件变量的唤醒消息，直到配置新任务或者关闭线程
			data->condition.wait(threadLocker,
				[&data] { return data->task || getClosed(data); });
		}
		else if (data->callback)	// 无后续任务，同样在回调函数子非空时执行回调函数子
			data->callback(empty, data->thread.get_id());
	}
}

//// 线程默认任务函数，用于继承扩展线程，形成钩子
//void Thread::process()
//{
//}

// 销毁线程
void Thread::destroy()
{
	// 若数据为空或者已经关闭线程，无需销毁线程，以支持移动语义
	if (data == nullptr || getClosed(data))
		return;
	setClosed(data, true);	// 线程设为关闭状态，即销毁状态

	// 工作线程或许处于阻塞状态，通过条件变量唤醒工作线程
	data->condition.notify_one();
	// 阻塞线程直到工作线程结束
	if (data->thread.joinable())
		data->thread.join();
	// 预留等待时间
	//std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

ETERFREE_END
