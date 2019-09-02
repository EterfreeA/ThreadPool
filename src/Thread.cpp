#include "Thread.h"

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <exception>
#include <iostream>

// 线程数据结构体
struct ThreadStructure
{
	std::thread thread;										// 线程体
	//ThreadPool *threadPool;								// 线程池指针
	std::shared_ptr<Queue<Thread::TaskPair>> taskQueue;		// 任务队列
	std::function<void(bool, Thread::ThreadID)> callback;	// 回调函数子
	std::mutex mutex;										// 互斥元
	std::condition_variable signal;							// 条件变量
	Thread::TaskPair task;									// 任务函数子
	std::atomic_bool closed;								// 关闭状态标志
	std::atomic_bool running;								// 运行状态标志
	// 线程过程参数解决方案：虚基类指针，过程类继承虚基类，通过强制类型转换，在过程函数中访问
	//void *vpParameters;
};

// 线程构造函数
Thread::Thread(std::shared_ptr<Queue<TaskPair>> taskQueue, std::function<void(bool, ThreadID)> callback)
{
	data = std::make_shared<ThreadStructure>();
	//data->threadPool = threadPool;	// 指向线程池，便于自动获取任务队列的任务
	data->taskQueue = taskQueue;	// 指向任务队列，便于自动获取任务
	data->callback = callback;	// 回调函数子，用于回调通知线程体获取任务失败，进入阻塞状态
	setClosed(data, false);	// 设置线程未关闭
	setRunning(data, false);	// 设置线程未运行
	data->thread = std::thread(Thread::execute, data);	// 创建thread，执行Thread类的execute函数
}

// 线程析构函数
Thread::~Thread()
{
	// 若数据非空则销毁线程，否则不销毁，以支持移动语义
	if (data)
		destroy();
}

// 配置任务
bool Thread::configure(const TaskPair& task)
{
	// 若线程体处于运行状态，说明正在执行任务，配置新任务失败
	if (getRunning(data))
		return false;
	data->task = task;	// 赋予线程任务函数子
	//data->vpParameters = vpParameters;
	return true;
}

// 唤醒线程
bool Thread::start()
{
	// 若线程体处于运行状态，标志正在执行任务，唤醒线程失败
	if (getRunning(data))
		return false;
	data->signal.notify_one();	// 通过条件变量发送信号，唤醒线程
	return true;
}

// 销毁线程
void Thread::destroy()
{
	// 若已经销毁过线程，忽略以下步骤
	if (getClosed(data))
		return;
	setClosed(data, true);	// 设置线程为关闭状态，即销毁状态

	// 线程体或许处于阻塞状态，通过条件变量发送信号唤醒线程体
	data->signal.notify_one();
	// 等待线程体执行结束
	if (data->thread.joinable())
		data->thread.join();
	// 预留等待时间
	//std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

// 获取线程id
std::thread::id Thread::getThreadID() const
{
	return data->thread.get_id();
}

// 返回线程空闲状态
bool Thread::free() const
{
	return !getRunning(data);
}

//const void *Thread::getParameters()
//{
//	return data->vpParameters;
//}

// 线程体主函数
void Thread::execute(data_type data)
{
	// 调用mem_fn获取函数子getTask，保存指向ThreadPool::getTask的指针，并且重载运算符operator()
	//auto &&getTask = std::mem_fn(&ThreadPool::getTask);

	// 创建unique_lock互斥锁，用于阻塞和唤醒线程体，未指定锁策略默认立即锁住互斥元
	using lock_type = std::unique_lock<std::mutex>;
	lock_type threadLocker(data->mutex);
	// 运用条件变量锁定线程互斥锁，若之前锁定过互斥锁，阻塞线程，等待条件变量的唤醒信号
	data->signal.wait(threadLocker);

	lock_type taskLocker;
	// 若任务队列指针非空，创建互斥锁，指定延迟锁定策略，用于互斥读取任务队列的任务
	if (data->taskQueue)
		taskLocker = decltype(taskLocker)(data->taskQueue->mutex(), std::defer_lock);

	while (!getClosed(data))	// 线程体退出通道
	{
		setRunning(data, true);	// 设置线程为运行状态
		try
		{
			if (data->task.first)	// 若任务函数子非空
				data->task.first();	// 执行任务函数子
			//else
			//	process();	// 执行默认任务函数
			if (data->task.second)	// 若回调函数子非空
				data->task.second();	// 执行回调函数子
			//else
			//	callback();	// 执行默认回调函数
		}
		catch (std::exception exception)	// 执行函数子时捕获异常，防止线程泄漏
		{
			std::cerr << exception.what() << std::endl;
		}
		data->task = { nullptr, nullptr };	// 执行完毕之后清除任务
		
		/* 以data->threadPool->getTask(this->shared_from_this())的形式
		调用ThreadPool::getTask函数获取线程池任务队列的任务
		若未成功获取任务，阻塞线程，等待条件变量的唤醒信号 */
		//if (!(data->threadPool 
		//	&& getTask(data->threadPool, this->shared_from_this())))
		//{
		//	if (getClosed())	// 线程体退出通道
		//		break;
		//	setRunning(false);	// 允许管理线程分配任务
		//	data->signal.wait(threadLocker);	// 阻塞线程，等待唤醒信号
		//}

		if (getClosed(data))	// 线程体退出通道
			break;

		bool empty = true;	// 用于判断是否进入阻塞状态的标志
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
			setRunning(data, false);	// 设置线程为未运行状态，即允许分配任务
			if (data->callback)	// 若回调函数子非空
				data->callback(empty, data->thread.get_id());	// 执行回调函数子
			// 阻塞线程，等待条件变量的唤醒信号，直到配置新任务或者关闭线程
			data->signal.wait(threadLocker,
				[&data] { return data->task.first || data->task.second || getClosed(data); });
		}
		else if (data->callback)
			data->callback(empty, data->thread.get_id());
	}
}

//// 线程默认任务函数，用于继承扩展线程，形成钩子
//void Thread::process()
//{
//}
//
//// 线程默认回调函数，用于继承扩展线程，形成钩子
//void Thread::callback()
//{
//}

// 设置线程关闭状态
inline void Thread::setClosed(data_type& data, bool closed)
{
	data->closed = closed;
}

// 获取线程关闭状态
inline bool Thread::getClosed(const data_type& data)
{
	return data->closed;
}

// 设置线程运行状态
inline void Thread::setRunning(data_type& data, bool running)
{
	data->running = running;
}

// 获取线程运行状态
inline bool Thread::getRunning(const data_type& data)
{
	return data->running;
}
